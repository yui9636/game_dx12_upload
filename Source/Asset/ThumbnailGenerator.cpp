#include "ThumbnailGenerator.h"
#include "Graphics.h"
#include "FrameBuffer.h"
#include "System/ResourceManager.h"
#include "Registry/Registry.h"
#include "Component/CameraComponent.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "RHI/ICommandList.h"
#include "RHI/ITexture.h"
#include "RHI/IResourceFactory.h"
#include "RHI/GraphicsAPI.h"
#include "RHI/DX11/DX11CommandList.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12RootSignature.h"
#include "RHI/DX12/DX12Device.h"
#include "RenderContext/RenderContext.h"
#include "Scene/SceneDataUploadSystem.h"
#include "Render/GlobalRootSignature.h"
#include "Console/Logger.h"
#include <cmath>

using namespace DirectX;

ThumbnailGenerator& ThumbnailGenerator::Instance() {
    static ThumbnailGenerator instance;
    return instance;
}

ThumbnailGenerator::~ThumbnailGenerator() = default;

void ThumbnailGenerator::Initialize()
{
    m_available = false;
    m_loggedUnavailable = false;
    m_cache.clear();
    m_pendingQueue.clear();
    m_pendingSet.clear();
    m_commandList.reset();
    m_dx12RootSignature.reset();
    m_renderer.reset();

    Graphics& graphics = Graphics::Instance();
    auto* factory = graphics.GetResourceFactory();
    if (!factory) {
        LOG_ERROR("[ThumbnailGenerator] Resource factory is unavailable.");
        return;
    }

    m_renderer = std::make_unique<ModelRenderer>(factory);

    if (!m_thumbRegistry) {
        m_thumbRegistry = std::make_unique<Registry>();
        m_thumbCamera = m_thumbRegistry->CreateEntity();
        m_thumbRegistry->AddComponent(m_thumbCamera, CameraLensComponent{});
        m_thumbRegistry->AddComponent(m_thumbCamera, CameraMatricesComponent{});
        m_thumbRegistry->AddComponent(m_thumbCamera, CameraMainTagComponent{});
    }

    if (graphics.GetAPI() == GraphicsAPI::DX12) {
        auto* device = graphics.GetDX12Device();
        if (!device) {
            LOG_ERROR("[ThumbnailGenerator] DX12 device is unavailable.");
            return;
        }

        m_dx12RootSignature = std::make_unique<DX12RootSignature>(device);
        m_commandList = std::make_unique<DX12CommandList>(device, m_dx12RootSignature.get(), false);
        m_available = true;
        LOG_INFO("[ThumbnailGenerator] Initialized DX12 thumbnail renderer.");
        return;
    }

    auto* context = graphics.GetDeviceContext();
    if (!context) {
        LOG_ERROR("[ThumbnailGenerator] DX11 device context is unavailable.");
        return;
    }

    m_commandList = std::make_unique<DX11CommandList>(context);
    m_available = true;
    LOG_INFO("[ThumbnailGenerator] Initialized DX11 thumbnail renderer.");
}

void ThumbnailGenerator::Request(const std::string& modelPath)
{
    if (!m_available) {
        if (!m_loggedUnavailable) {
            LOG_WARN("[ThumbnailGenerator] Thumbnail generation is unavailable on the current renderer.");
            m_loggedUnavailable = true;
        }
        return;
    }

    if (modelPath.empty() || m_cache.count(modelPath) || m_pendingSet.count(modelPath)) {
        return;
    }

    m_pendingQueue.push_back(modelPath);
    m_pendingSet.insert(modelPath);
}

std::shared_ptr<ITexture> ThumbnailGenerator::Get(const std::string& modelPath) const
{
    auto it = m_cache.find(modelPath);
    return (it != m_cache.end()) ? it->second : nullptr;
}

void ThumbnailGenerator::PumpOne()
{
    if (!m_available || m_pendingQueue.empty()) {
        return;
    }

    const std::string modelPath = m_pendingQueue.front();
    m_pendingQueue.pop_front();
    m_pendingSet.erase(modelPath);

    auto texture = GenerateTexture(modelPath);
    if (texture) {
        m_cache[modelPath] = texture;
        LOG_INFO("[ThumbnailGenerator] Generated thumbnail: %s", modelPath.c_str());
    }
    else {
        LOG_WARN("[ThumbnailGenerator] Failed to generate thumbnail: %s", modelPath.c_str());
    }
}

RenderContext ThumbnailGenerator::BuildThumbnailRenderContext(FrameBuffer* targetBuffer)
{
    RenderContext rc = {};
    rc.commandList = m_commandList.get();
    rc.renderState = Graphics::Instance().GetRenderState();
    rc.shadowMap = nullptr;
    rc.mainRenderTarget = targetBuffer->GetColorTexture(0);
    rc.mainDepthStencil = targetBuffer->GetDepthTexture();
    rc.mainViewport = RhiViewport(0.0f, 0.0f, 256.0f, 256.0f);

    auto archetypes = m_thumbRegistry->GetAllArchetypes();
    Signature camSig = CreateSignature<CameraMatricesComponent, CameraMainTagComponent>();

    for (auto* arch : archetypes) {
        if (!SignatureMatches(arch->GetSignature(), camSig) || arch->GetEntityCount() == 0) continue;

        auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<CameraMatricesComponent>());
        auto& mats = *static_cast<CameraMatricesComponent*>(col->Get(0));
        rc.viewMatrix = mats.view;
        rc.projectionMatrix = mats.projection;
        rc.cameraPosition = mats.worldPos;
        rc.cameraDirection = mats.cameraFront;
        break;
    }

    // サムネイル用ライト（斜め上から白色光）
    rc.directionalLight.direction = { -0.5f, -0.7f, 0.5f };
    rc.directionalLight.color = { 1.0f, 1.0f, 1.0f };

    rc.aspect = 1.0f;
    float m22 = rc.projectionMatrix._22;
    float m33 = rc.projectionMatrix._33;
    float m43 = rc.projectionMatrix._43;

    rc.fovY = (fabsf(m22) > 0.0001f) ? (2.0f * atanf(1.0f / m22)) : 0.785f;
    rc.nearZ = (fabsf(m33) > 0.0001f && fabsf(1.0f - m33) > 0.0001f) ? -m43 / m33 : 0.01f;
    rc.farZ = (fabsf(m33) > 0.0001f && fabsf(1.0f - m33) > 0.0001f) ? m43 / (1.0f - m33) : 1000.0f;

    if (rc.nearZ <= 0.0f) rc.nearZ = 0.01f;
    if (rc.farZ <= rc.nearZ) rc.farZ = rc.nearZ + 1000.0f;

    return rc;
}

std::shared_ptr<ITexture> ThumbnailGenerator::GenerateTexture(const std::string& modelPath)
{
    auto model = ResourceManager::Instance().GetModel(modelPath);
    if (!model) {
        return nullptr;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        return nullptr;
    }

    auto captureBuffer = std::make_shared<FrameBuffer>(
        factory,
        256,
        256,
        std::vector<TextureFormat>{ TextureFormat::R16G16B16A16_FLOAT },
        TextureFormat::D32_FLOAT);

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    model->UpdateTransform(identity);

    BoundingBox aabb = model->GetWorldBounds();
    XMFLOAT3 center = aabb.Center;
    XMFLOAT3 ex = aabb.Extents;

    float maxTmp = (ex.x > ex.y) ? ex.x : ex.y;
    float maxDim = (maxTmp > ex.z) ? maxTmp : ex.z;
    float minTmp = (ex.x < ex.y) ? ex.x : ex.y;
    float minDim = (minTmp < ex.z) ? minTmp : ex.z;

    bool isEffect = (minDim < maxDim * 0.05f);
    float pitch = XMConvertToRadians(25.0f);
    float yaw = XMConvertToRadians(45.0f);
    if (isEffect) {
        if (ex.y == minDim) pitch = XMConvertToRadians(60.0f);
        else if (ex.z == minDim) yaw = XMConvertToRadians(10.0f);
        else if (ex.x == minDim) yaw = XMConvertToRadians(80.0f);
    }

    float radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&ex)));
    if (radius < 0.01f) radius = 1.0f;

    float fov = XMConvertToRadians(45.0f);
    float distance = (radius / sinf(fov * 0.5f)) * 1.3f;
    float nearZ = 0.01f;
    float farZ = distance * 10.0f;

    XMFLOAT3 camPos = {
        center.x + distance * cosf(pitch) * sinf(yaw),
        center.y + distance * sinf(pitch),
        center.z - distance * cosf(pitch) * cosf(yaw)
    };

    auto* lens = m_thumbRegistry->GetComponent<CameraLensComponent>(m_thumbCamera);
    auto* mats = m_thumbRegistry->GetComponent<CameraMatricesComponent>(m_thumbCamera);
    lens->fovY = fov;
    lens->aspect = 1.0f;
    lens->nearZ = nearZ;
    lens->farZ = farZ;

    XMVECTOR eye = XMLoadFloat3(&camPos);
    XMVECTOR at = XMLoadFloat3(&center);
    XMVECTOR upV = XMVectorSet(0, 1, 0, 0);
    XMStoreFloat4x4(&mats->view, XMMatrixLookAtLH(eye, at, upV));
    XMStoreFloat4x4(&mats->projection, XMMatrixPerspectiveFovLH(fov, 1.0f, nearZ, farZ));
    mats->worldPos = camPos;
    XMStoreFloat3(&mats->cameraFront, XMVector3Normalize(at - eye));

    RenderContext rc = BuildThumbnailRenderContext(captureBuffer.get());

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(m_commandList.get());
        dx12Cmd->Begin();
    }

    float clearColor[4] = { 0.2f, 0.2f, 0.22f, 1.0f };
    captureBuffer->Clear(rc.commandList, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    captureBuffer->SetRenderTargets(rc.commandList);
    rc.commandList->SetViewport(rc.mainViewport);

    SceneDataUploadSystem uploadSystem;
    uploadSystem.Upload(rc, GlobalRootSignature::Instance());
    GlobalRootSignature::Instance().BindAll(rc.commandList, rc.renderState, rc.shadowMap);

    // テクスチャなしメッシュのマテリアルカラーをマゼンタに一時変更
    auto modelRes = model->GetModelResource();
    std::vector<XMFLOAT4> savedColors;
    for (int i = 0; i < modelRes->GetMeshCount(); ++i) {
        auto* mesh = modelRes->GetMeshResource(i);
        savedColors.push_back(mesh->material.color);
        if (!mesh->material.diffuseMap) {
            mesh->material.color = { 1.0f, 0.0f, 1.0f, 1.0f }; // マゼンタ
        }
    }

    XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_renderer->Draw(ShaderId::Phong, modelRes,
        identity, identity, white, 0.0f, 1.0f, 0.0f,
        BlendState::Opaque, DepthState::TestAndWrite, RasterizerState::SolidCullNone);

    RenderQueue emptyQueue;
    m_renderer->Render(rc, emptyQueue);

    // マテリアルカラー復元
    for (int i = 0; i < modelRes->GetMeshCount(); ++i) {
        modelRes->GetMeshResource(i)->material.color = savedColors[i];
    }

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        rc.commandList->TransitionBarrier(captureBuffer->GetColorTexture(0), ResourceState::ShaderResource);
        auto* dx12Cmd = static_cast<DX12CommandList*>(m_commandList.get());
        dx12Cmd->End();
        dx12Cmd->Submit();
        Graphics::Instance().GetDX12Device()->WaitForGPU();
    }

    return std::shared_ptr<ITexture>(captureBuffer, captureBuffer->GetColorTexture(0));
}
