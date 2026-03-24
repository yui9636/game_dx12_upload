#include "MaterialPreviewStudio.h"
#include "MaterialAsset.h"
#include "Render/OffscreenRenderer.h"
#include "Graphics.h"
#include "System/ResourceManager.h"
#include "Model/Model.h"
#include "RHI/ITexture.h"
#include "RHI/IResourceFactory.h"
#include "RenderGraph/FrameGraphTypes.h"
#include "Console/Logger.h"
#include <cmath>

using namespace DirectX;

static constexpr float CLEAR_R = 0.18f;
static constexpr float CLEAR_G = 0.18f;
static constexpr float CLEAR_B = 0.20f;
static constexpr float CLEAR_A = 1.0f;

MaterialPreviewStudio& MaterialPreviewStudio::Instance() {
    static MaterialPreviewStudio instance;
    return instance;
}

MaterialPreviewStudio::~MaterialPreviewStudio() = default;

void MaterialPreviewStudio::Initialize(OffscreenRenderer* offscreen)
{
    m_offscreen = offscreen;
    m_pendingMaterial = nullptr;
    m_dirty = false;
    m_previewTexture.reset();
    m_previewDepth.reset();

    if (!m_offscreen || !m_offscreen->IsReady()) {
        LOG_ERROR("[MaterialPreviewStudio] OffscreenRenderer unavailable.");
        return;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return;

    // Preview color texture: RGBA8_UNORM, RT+SRV (persistent)
    {
        TextureDesc desc{};
        desc.width = PREVIEW_SIZE;
        desc.height = PREVIEW_SIZE;
        desc.format = TextureFormat::RGBA8_UNORM;
        desc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
        desc.clearColor[0] = CLEAR_R;
        desc.clearColor[1] = CLEAR_G;
        desc.clearColor[2] = CLEAR_B;
        desc.clearColor[3] = CLEAR_A;
        auto raw = factory->CreateTexture("MaterialPreview", desc);
        m_previewTexture = std::shared_ptr<ITexture>(std::move(raw));
    }

    // Shared depth
    {
        TextureDesc depthDesc{};
        depthDesc.width = PREVIEW_SIZE;
        depthDesc.height = PREVIEW_SIZE;
        depthDesc.format = TextureFormat::D24_UNORM_S8_UINT;
        depthDesc.bindFlags = TextureBindFlags::DepthStencil;
        depthDesc.clearDepth = 1.0f;
        m_previewDepth = factory->CreateTexture("MaterialPreviewDepth", depthDesc);
    }

    m_sphereModel = ResourceManager::Instance().GetModel("Data/Model/sphere/fbx_sphere_001.fbx");
    if (!m_sphereModel) {
        LOG_ERROR("[MaterialPreviewStudio] Failed to load sphere model.");
        return;
    }

    LOG_INFO("[MaterialPreviewStudio] Initialized.");
}

bool MaterialPreviewStudio::IsReady() const {
    return m_offscreen && m_offscreen->IsReady()
        && m_sphereModel != nullptr && m_previewTexture != nullptr;
}

void MaterialPreviewStudio::RequestPreview(MaterialAsset* material)
{
    if (!material) return;
    m_pendingMaterial = material;
    m_dirty = true;
}

void MaterialPreviewStudio::PumpPreview()
{
    if (!m_dirty || !m_pendingMaterial || !IsReady()) return;
    if (!m_offscreen->IsGpuIdle()) return;
    m_dirty = false;
    ExecuteRender();
}

void MaterialPreviewStudio::ExecuteRender()
{
    MaterialAsset* material = m_pendingMaterial;

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    m_sphereModel->UpdateTransform(identity);

    auto& meshMaterials = m_sphereModel->GetMaterialss();
    for (auto& mat : meshMaterials) {
        mat.color = material->baseColor;
        mat.metallicFactor = material->metallic;
        mat.roughnessFactor = material->roughness;

        mat.diffuseTextureFileName = material->diffuseTexturePath;
        mat.normalTextureFileName = material->normalTexturePath;
        mat.metallicTextureFileName = material->metallicRoughnessTexturePath;
        mat.roughnessTextureFileName = material->metallicRoughnessTexturePath;
        mat.emissiveTextureFileName = material->emissiveTexturePath;

        mat.diffuseMap = ResourceManager::Instance().GetTexture(mat.diffuseTextureFileName);
        mat.normalMap = ResourceManager::Instance().GetTexture(mat.normalTextureFileName);
        if (!mat.metallicTextureFileName.empty()) {
            mat.metallicMap = ResourceManager::Instance().GetTexture(mat.metallicTextureFileName);
            mat.roughnessMap = mat.metallicMap;
        } else {
            mat.metallicMap = nullptr;
            mat.roughnessMap = nullptr;
        }
        mat.emissiveMap = ResourceManager::Instance().GetTexture(mat.emissiveTextureFileName);
    }

    BoundingBox aabb = m_sphereModel->GetWorldBounds();
    XMFLOAT3 center = aabb.Center;
    XMFLOAT3 ex = aabb.Extents;

    float radius = XMVectorGetX(XMVector3Length(XMLoadFloat3(&ex)));
    if (radius < 0.01f) radius = 1.0f;

    float fov = XMConvertToRadians(45.0f);
    float distance = (radius / sinf(fov * 0.5f)) * 1.3f;
    float pitch = XMConvertToRadians(20.0f);
    float yaw = XMConvertToRadians(135.0f);

    XMFLOAT3 camPos = {
        center.x + distance * cosf(pitch) * sinf(yaw),
        center.y + distance * sinf(pitch),
        center.z - distance * cosf(pitch) * cosf(yaw)
    };

    XMVECTOR eye = XMLoadFloat3(&camPos);
    XMVECTOR at  = XMLoadFloat3(&center);
    XMVECTOR upV = XMVectorSet(0, 1, 0, 0);
    XMFLOAT4X4 viewProj;
    XMStoreFloat4x4(&viewProj, XMMatrixLookAtLH(eye, at, upV) *
        XMMatrixPerspectiveFovLH(fov, 1.0f, 0.01f, distance * 10.0f));

    XMFLOAT3 lightDir   = { -0.5f, -0.7f, 0.5f };
    XMFLOAT3 lightColor = { 3.0f, 3.0f, 3.0f };

    // Direct rendering to persistent texture (no FrameBuffer, no CopyResource)
    m_offscreen->BeginJob();
    m_offscreen->ClearExternalRT(m_previewTexture.get(), m_previewDepth.get(),
        CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A);
    m_offscreen->SetExternalRenderTarget(m_previewTexture.get(), m_previewDepth.get());
    m_offscreen->SetViewport((float)PREVIEW_SIZE, (float)PREVIEW_SIZE);
    m_offscreen->UploadScene(viewProj, camPos, lightDir, lightColor,
        (float)PREVIEW_SIZE, (float)PREVIEW_SIZE);
    m_offscreen->BindScene();
    m_offscreen->BindSampler();

    auto modelRes = m_sphereModel->GetModelResource();
    m_offscreen->GetModelRenderer().Draw(
        ShaderId::Phong, modelRes,
        identity, identity,
        material->baseColor, material->metallic, material->roughness, material->emissive,
        material, BlendState::Opaque, DepthState::TestAndWrite, RasterizerState::SolidCullNone);

    m_offscreen->SubmitDirect(m_previewTexture.get());
}
