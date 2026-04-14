#include "PlayerModelPreviewStudio.h"

#include "Render/OffscreenRenderer.h"
#include "Graphics.h"
#include "Model/Model.h"
#include "RHI/ITexture.h"
#include "RHI/IResourceFactory.h"
#include "RenderGraph/FrameGraphTypes.h"
#include "Console/Logger.h"

using namespace DirectX;

PlayerModelPreviewStudio& PlayerModelPreviewStudio::Instance()
{
    static PlayerModelPreviewStudio instance;
    return instance;
}

void PlayerModelPreviewStudio::Initialize(OffscreenRenderer* offscreen)
{
    m_offscreen = offscreen;
    m_previewTexture.reset();
    m_previewDepth.reset();

    if (!m_offscreen || !m_offscreen->IsReady()) {
        LOG_ERROR("[PlayerModelPreviewStudio] OffscreenRenderer unavailable.");
        return;
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) {
        return;
    }

    TextureDesc colorDesc{};
    colorDesc.width = PREVIEW_SIZE;
    colorDesc.height = PREVIEW_SIZE;
    colorDesc.format = TextureFormat::RGBA8_UNORM;
    colorDesc.bindFlags = TextureBindFlags::RenderTarget | TextureBindFlags::ShaderResource;
    colorDesc.clearColor[0] = 0.12f;
    colorDesc.clearColor[1] = 0.12f;
    colorDesc.clearColor[2] = 0.12f;
    colorDesc.clearColor[3] = 1.0f;
    m_previewTexture = std::shared_ptr<ITexture>(factory->CreateTexture("PlayerModelPreview", colorDesc));

    TextureDesc depthDesc{};
    depthDesc.width = PREVIEW_SIZE;
    depthDesc.height = PREVIEW_SIZE;
    depthDesc.format = TextureFormat::D24_UNORM_S8_UINT;
    depthDesc.bindFlags = TextureBindFlags::DepthStencil;
    depthDesc.clearDepth = 1.0f;
    m_previewDepth = factory->CreateTexture("PlayerModelPreviewDepth", depthDesc);

    if (!m_previewTexture || !m_previewDepth) {
        LOG_ERROR("[PlayerModelPreviewStudio] Failed to create preview textures.");
        m_previewTexture.reset();
        m_previewDepth.reset();
        return;
    }

    LOG_INFO("[PlayerModelPreviewStudio] Initialized.");
}

bool PlayerModelPreviewStudio::IsReady() const
{
    return m_offscreen && m_offscreen->IsReady() && m_previewTexture && m_previewDepth;
}

void PlayerModelPreviewStudio::RenderPreview(
    const Model* model,
    const DirectX::XMFLOAT3& cameraPosition,
    const DirectX::XMFLOAT3& cameraTarget,
    float aspect,
    float fovY,
    float nearZ,
    float farZ,
    const DirectX::XMFLOAT4& clearColor,
    float previewScale)
{
    if (!IsReady() || !model) {
        return;
    }
    if (!m_offscreen->IsGpuIdle()) {
        return;
    }

    auto modelResource = model->GetModelResource();
    if (!modelResource) {
        return;
    }

    XMFLOAT4X4 identity{};
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    const_cast<Model*>(model)->UpdateTransform(identity);
    XMFLOAT4X4 scaledWorld{};
    XMStoreFloat4x4(&scaledWorld, XMMatrixScaling(
        (std::max)(previewScale, 0.01f),
        (std::max)(previewScale, 0.01f),
        (std::max)(previewScale, 0.01f)));

    const float safeAspect = aspect > 0.01f ? aspect : 1.0f;
    const float safeNearZ = nearZ > 0.0001f ? nearZ : 0.03f;
    const float safeFarZ = farZ > safeNearZ ? farZ : (safeNearZ + 500.0f);
    const float safeFovY = fovY > 0.01f ? fovY : 0.785398f;

    const XMVECTOR eye = XMLoadFloat3(&cameraPosition);
    const XMVECTOR at = XMLoadFloat3(&cameraTarget);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMFLOAT4X4 viewProj{};
    XMStoreFloat4x4(
        &viewProj,
        XMMatrixLookAtLH(eye, at, up) *
        XMMatrixPerspectiveFovLH(safeFovY, safeAspect, safeNearZ, safeFarZ));

    const XMFLOAT3 lightDir = { -0.5f, -0.7f, 0.5f };
    const XMFLOAT3 lightColor = { 3.0f, 3.0f, 3.0f };
    const XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };

    m_offscreen->BeginJob();
    m_offscreen->ClearExternalRT(
        m_previewTexture.get(),
        m_previewDepth.get(),
        clearColor.x,
        clearColor.y,
        clearColor.z,
        clearColor.w);
    m_offscreen->SetExternalRenderTarget(m_previewTexture.get(), m_previewDepth.get());
    m_offscreen->SetViewport(static_cast<float>(PREVIEW_SIZE), static_cast<float>(PREVIEW_SIZE));
    m_offscreen->UploadScene(
        viewProj,
        cameraPosition,
        lightDir,
        lightColor,
        static_cast<float>(PREVIEW_SIZE),
        static_cast<float>(PREVIEW_SIZE));
    m_offscreen->BindScene();
    m_offscreen->BindSampler();
    m_offscreen->GetModelRenderer().Draw(
        ShaderId::PBR,
        modelResource,
        scaledWorld,
        scaledWorld,
        white,
        0.0f,
        1.0f,
        0.0f,
        nullptr,
        BlendState::Opaque,
        DepthState::TestAndWrite,
        RasterizerState::SolidCullNone);
    m_offscreen->SubmitDirect(m_previewTexture.get());
}
