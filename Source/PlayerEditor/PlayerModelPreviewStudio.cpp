#include "PlayerModelPreviewStudio.h"

#include "Render/OffscreenRenderer.h"
#include "Graphics.h"
#include "Model/Model.h"
#include "RHI/ITexture.h"
#include "RHI/IResourceFactory.h"
#include "RenderGraph/FrameGraphTypes.h"
#include "Console/Logger.h"
#include "System/ResourceManager.h"

using namespace DirectX;
// PlayerModelPreviewStudio::Instance はこのモジュールの実行時処理を構成する補助処理を行う。

namespace
{
    constexpr const char* kTrainingStagePath = "Data/Model/TrainingStage/TrainingStage.gltf";
    constexpr float kTrainingStageScale = 2.0f;
    constexpr float kTrainingStagePositionY = 1.0f;

    DirectX::XMFLOAT4X4 BuildTrainingStageWorld()
    {
        using namespace DirectX;

        XMFLOAT4X4 world{};
        XMStoreFloat4x4(
            &world,
            XMMatrixScaling(kTrainingStageScale, kTrainingStageScale, kTrainingStageScale) *
            XMMatrixTranslation(0.0f, kTrainingStagePositionY, 0.0f));
        return world;
    }

    DirectX::XMFLOAT4X4 BuildViewProjection(
        const DirectX::XMFLOAT3& eye,
        const DirectX::XMFLOAT3& target,
        float fovY,
        float aspect,
        float nearZ,
        float farZ)
    {
        using namespace DirectX;
        XMFLOAT4X4 viewProj{};
        XMStoreFloat4x4(
            &viewProj,
            XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) *
            XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ));
        return viewProj;
    }

}

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
    m_trainingStageModel.reset();
    m_trainingStageLoadAttempted = false;

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

Model* PlayerModelPreviewStudio::EnsureTrainingStageModel()
{
    if (m_trainingStageModel) {
        return m_trainingStageModel.get();
    }
    if (m_trainingStageLoadAttempted) {
        return nullptr;
    }

    m_trainingStageLoadAttempted = true;
    m_trainingStageModel = ResourceManager::Instance().CreateModelInstance(kTrainingStagePath);
    if (!m_trainingStageModel) {
        LOG_WARN("[PlayerModelPreviewStudio] Training stage load failed: %s", kTrainingStagePath);
        return nullptr;
    }

    LOG_INFO("[PlayerModelPreviewStudio] Training stage loaded: %s", kTrainingStagePath);
    return m_trainingStageModel.get();
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
    if (!IsReady()) {
        return;
    }
    if (!m_offscreen->IsGpuIdle()) {
        return;
    }

    auto modelResource = model ? model->GetModelResource() : nullptr;
    Model* trainingStage = EnsureTrainingStageModel();
    auto stageResource = trainingStage ? trainingStage->GetModelResource() : nullptr;
    if (!modelResource && !stageResource) {
        return;
    }

    XMFLOAT4X4 identity{};
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    if (model) {
        const_cast<Model*>(model)->UpdateTransform(identity);
    }
    XMFLOAT4X4 scaledWorld{};
    XMStoreFloat4x4(&scaledWorld, XMMatrixScaling(
        (std::max)(previewScale, 0.01f),
        (std::max)(previewScale, 0.01f),
        (std::max)(previewScale, 0.01f)));

    const float safeAspect = aspect > 0.01f ? aspect : 1.0f;
    const float safeNearZ = nearZ > 0.0001f ? nearZ : 0.03f;
    const float safeFarZ = farZ > safeNearZ ? farZ : (safeNearZ + 500.0f);
    const float safeFovY = fovY > 0.01f ? fovY : 0.785398f;

    const XMFLOAT4X4 playerViewProj = BuildViewProjection(
        cameraPosition,
        cameraTarget,
        safeFovY,
        safeAspect,
        safeNearZ,
        safeFarZ);
    const XMFLOAT3 stageCameraPosition = { 0.0f, 110.0f, -360.0f };
    const XMFLOAT3 stageCameraTarget = { 0.0f, 35.0f, -25.0f };
    const XMFLOAT4X4 stageViewProj = BuildViewProjection(
        stageCameraPosition,
        stageCameraTarget,
        DirectX::XMConvertToRadians(48.0f),
        safeAspect,
        0.1f,
        1000.0f);

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
    m_offscreen->BindSampler();

    if (stageResource) {
        const XMFLOAT4X4 stageWorld = BuildTrainingStageWorld();
        trainingStage->UpdateTransform(identity);
        m_offscreen->UploadScene(
            stageViewProj,
            stageCameraPosition,
            lightDir,
            lightColor,
            static_cast<float>(PREVIEW_SIZE),
            static_cast<float>(PREVIEW_SIZE));
        m_offscreen->BindScene();
        m_offscreen->GetModelRenderer().Draw(
            ShaderId::PBR,
            stageResource,
            stageWorld,
            stageWorld,
            white,
            0.0f,
            1.0f,
            0.0f,
            nullptr,
            BlendState::Opaque,
            DepthState::TestAndWrite,
            RasterizerState::SolidCullNone);
        m_offscreen->RenderQueuedDirect(m_previewTexture.get(), m_previewDepth.get());
        m_offscreen->ClearExternalDepth(m_previewDepth.get());
    }

    if (modelResource) {
        m_offscreen->UploadScene(
            playerViewProj,
            cameraPosition,
            lightDir,
            lightColor,
            static_cast<float>(PREVIEW_SIZE),
            static_cast<float>(PREVIEW_SIZE));
        m_offscreen->BindScene();
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
        m_offscreen->RenderQueuedDirect(m_previewTexture.get(), m_previewDepth.get());
    }

    m_offscreen->FinishDirect(m_previewTexture.get());
}
