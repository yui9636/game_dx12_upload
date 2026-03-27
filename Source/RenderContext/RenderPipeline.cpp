#include "RenderPipeline.h"
#include "Engine/EngineKernel.h"
#include "Component/CameraComponent.h"
#include "Registry/Registry.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include <Light/LightSystem.h>
#include <Scene/SceneDataUploadSystem.h>
#include "Render/GlobalRootSignature.h"
#include "RHI/ICommandList.h"
#include "Console/Console.h"
#include <typeinfo>
#include <Console/Profiler.h>
#include <RHI/DX11/DX11CommandList.h>
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12RootSignature.h"
#include "RHI/GraphicsAPI.h"
#include <RenderGraph/FrameGraph.h>
#include "RenderGraph/FrameGraphResources.h"
#include "RenderPass/FinalBlitPass.h"
#include "RenderPass/ExtractVisibleInstancesPass.h"
#include "RenderPass/BuildInstanceBufferPass.h"
#include "RenderPass/BuildIndirectCommandPass.h"
#include "RenderPass/ComputeCullingPass.h"
#include "RenderPass/ShadowPass.h"
#include "RenderPass/GBufferPass.h"
#include "RenderPass/GTAOPass.h"
#include "RenderPass/SSGIPass.h"
#include "RenderPass/VolumetricFogPass.h"
#include "RenderPass/SSRPass.h"
#include "RenderPass/DeferredLightingPass.h"
#include "RenderPass/SkyboxPass.h"
#include "RenderPass/ForwardTransparentPass.h"
#include "RenderPass/PostProcessPass.h"
#include "Console/Logger.h"
#include "System/TaskSystem.h"
#include <chrono>
#include <unordered_set>

namespace
{
    uint64_t ResolveHistoryKey(const RenderContext::ViewState& viewState)
    {
        if (viewState.historyKey != 0) {
            return viewState.historyKey;
        }

        if (viewState.mainRenderTarget) {
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(viewState.mainRenderTarget));
        }
        if (viewState.sceneColorTexture) {
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(viewState.sceneColorTexture));
        }
        return 1;
    }

    std::shared_ptr<IRenderPass> ClonePassForView(const std::shared_ptr<IRenderPass>& pass, IResourceFactory* factory)
    {
        if (dynamic_cast<ExtractVisibleInstancesPass*>(pass.get())) return std::make_shared<ExtractVisibleInstancesPass>();
        if (dynamic_cast<BuildInstanceBufferPass*>(pass.get())) return std::make_shared<BuildInstanceBufferPass>();
        if (dynamic_cast<BuildIndirectCommandPass*>(pass.get())) return std::make_shared<BuildIndirectCommandPass>();
        if (dynamic_cast<ComputeCullingPass*>(pass.get())) return std::make_shared<ComputeCullingPass>();
        if (dynamic_cast<ShadowPass*>(pass.get())) return std::make_shared<ShadowPass>();
        if (dynamic_cast<GBufferPass*>(pass.get())) return std::make_shared<GBufferPass>();
        if (dynamic_cast<GTAOPass*>(pass.get())) return std::make_shared<GTAOPass>(factory);
        if (dynamic_cast<SSGIPass*>(pass.get())) return std::make_shared<SSGIPass>(factory);
        if (dynamic_cast<VolumetricFogPass*>(pass.get())) return std::make_shared<VolumetricFogPass>(factory);
        if (dynamic_cast<SSRPass*>(pass.get())) return std::make_shared<SSRPass>(factory);
        if (dynamic_cast<DeferredLightingPass*>(pass.get())) return std::make_shared<DeferredLightingPass>(factory);
        if (dynamic_cast<SkyboxPass*>(pass.get())) return std::make_shared<SkyboxPass>();
        if (dynamic_cast<ForwardTransparentPass*>(pass.get())) return std::make_shared<ForwardTransparentPass>();
        if (dynamic_cast<FinalBlitPass*>(pass.get())) return std::make_shared<FinalBlitPass>(factory);
        if (dynamic_cast<PostProcessPass*>(pass.get())) return std::make_shared<PostProcessPass>();
        return nullptr;
    }
}


RenderContext RenderPipeline::BeginFrame(Registry& registry, FrameBuffer* targetBuffer)
{
    Graphics& g = Graphics::Instance();
    ++m_frameSerial;
    const int frameSlotIndex = m_inFlightIndex % kMaxInFlight;
    auto& recordedListSlot = m_inFlightRecordedDx12Lists[frameSlotIndex];
    recordedListSlot.clear();

    // 1. ターゲットバッファの決定
    FrameBuffer* fb = targetBuffer ? targetBuffer : g.GetFrameBuffer(FrameBufferId::GBuffer);

    // 2. CommandListの生成（初回のみ）
    if (!m_commandList) {
        if (g.GetAPI() == GraphicsAPI::DX12) {
            if (!m_dx12RootSig) {
                m_dx12RootSig = std::make_unique<DX12RootSignature>(g.GetDX12Device());
            }
            m_commandList = std::make_unique<DX12CommandList>(g.GetDX12Device(), m_dx12RootSig.get());
        } else {
            m_commandList = std::make_unique<DX11CommandList>(g.GetDeviceContext());
        }
    }

    // 2b. DX12: Begin command list + back buffer transition + clear
    if (g.GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(m_commandList.get());
        dx12Cmd->Begin();
        ITexture* bb = g.GetBackBufferTexture();
        if (bb) {
            m_commandList->TransitionBarrier(bb, ResourceState::RenderTarget);
            float bbClear[4] = { 0.0f, 0.2f, 0.4f, 1.0f }; // DX12 test: blue tint
            m_commandList->ClearColor(bb, bbClear);
        }
    }

    // 3. RenderContext の初期化
    RenderContext rc = {};
    rc.commandList = m_commandList.get();
    rc.renderState = g.GetRenderState();
    rc.dx12RootSignature = m_dx12RootSig.get();
    rc.recordedDx12CommandLists = &recordedListSlot;
    if (g.GetAPI() == GraphicsAPI::DX12) {
        const size_t workerCount = TaskSystem::Instance().GetWorkerCount();
        auto& workerPoolSlot = m_workerDx12CommandListPools[frameSlotIndex];
        if (workerPoolSlot.size() != workerCount) {
            workerPoolSlot.clear();
            workerPoolSlot.reserve(workerCount);
            for (size_t i = 0; i < workerCount; ++i) {
                workerPoolSlot.push_back(
                    std::make_shared<DX12CommandList>(g.GetDX12Device(), m_dx12RootSig.get(), false));
            }
        }
        rc.workerDx12CommandListPool = &workerPoolSlot;
    }
    rc.time = (float)EngineKernel::Instance().GetTime().totalTime;
    rc.shadowMap = g.GetShadowMap();

    const bool isDx12 = (g.GetAPI() == GraphicsAPI::DX12);
    const uint32_t defaultRenderWidth = static_cast<uint32_t>(g.GetScreenWidth() * g.GetRenderScale());
    const uint32_t defaultRenderHeight = static_cast<uint32_t>(g.GetScreenHeight() * g.GetRenderScale());
    rc.mainViewport = RhiViewport(0.0f, 0.0f, (float)g.GetScreenWidth(), (float)g.GetScreenHeight());
    rc.mainRenderTarget = (!isDx12 && fb) ? fb->GetColorTexture(0) : nullptr;
    rc.mainDepthStencil = (!isDx12 && fb) ? fb->GetDepthTexture() : nullptr;
    rc.renderWidth = (!isDx12 && fb) ? fb->GetWidth() : defaultRenderWidth;
    rc.renderHeight = (!isDx12 && fb) ? fb->GetHeight() : defaultRenderHeight;
    rc.displayWidth = static_cast<uint32_t>(g.GetScreenWidth());
    rc.displayHeight = static_cast<uint32_t>(g.GetScreenHeight());
    rc.panelWidth = rc.displayWidth;
    rc.panelHeight = rc.displayHeight;

    rc.sceneColorTexture = (!isDx12 && fb) ? fb->GetColorTexture(0) : nullptr;
    rc.sceneDepthTexture = (!isDx12 && fb) ? fb->GetDepthTexture() : nullptr;

    // ====================================================
    // 4. ECSから「メインカメラ」を探すロジック
    // ====================================================
    auto archetypes = registry.GetAllArchetypes();
    Signature mainCamSig = CreateSignature<CameraMatricesComponent, CameraMainTagComponent>();

    bool cameraFound = false;
    for (auto* archetype : archetypes) {
        if (!SignatureMatches(archetype->GetSignature(), mainCamSig)) continue;
        auto* matsCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraMatricesComponent>());
        if (archetype->GetEntityCount() > 0) {
            auto& mats = *static_cast<CameraMatricesComponent*>(matsCol->Get(0));
            rc.viewMatrix = mats.view;
            rc.projectionMatrix = mats.projection;
            rc.cameraPosition = mats.worldPos;
            rc.cameraDirection = mats.cameraFront;
            cameraFound = true;
            break;
        }
    }

    if (!cameraFound) {
        DirectX::XMStoreFloat4x4(&rc.viewMatrix, DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&rc.projectionMatrix, DirectX::XMMatrixIdentity());
        rc.cameraPosition = { 0.0f, 0.0f, 0.0f };
    }

    static bool s_loggedCameraState = false;
    if (!s_loggedCameraState) {
        LOG_INFO("[RenderPipeline] cameraFound=%d pos=(%.3f, %.3f, %.3f) dir=(%.3f, %.3f, %.3f) proj11=%.3f proj22=%.3f",
            cameraFound ? 1 : 0,
            rc.cameraPosition.x, rc.cameraPosition.y, rc.cameraPosition.z,
            rc.cameraDirection.x, rc.cameraDirection.y, rc.cameraDirection.z,
            rc.projectionMatrix._11, rc.projectionMatrix._22);
        s_loggedCameraState = true;
    }

    // 5. ライト情報の抽出
    LightSystem::ExtractLights(registry, rc);

    // 6. 行列計算とジッター処理
    DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.viewMatrix);
    DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.projectionMatrix);
    DirectX::XMMATRIX currentVP = V * P;

    DirectX::XMStoreFloat4x4(&rc.viewProjectionUnjittered, currentVP);
    rc.prevViewProjectionMatrix = rc.viewProjectionUnjittered;
    rc.prevJitterOffset = { 0.0f, 0.0f };

    // DX12 はまだ FSR2/TAA resolve が未接続なので、jitter だけ残すと揺れます。
    if (g.GetAPI() == GraphicsAPI::DX12) {
        rc.jitterOffset = { 0.0f, 0.0f };
    }
    else {
        static int32_t jitterIndex = 0;
        float renderScale = g.GetRenderScale();
        float renderW = (float)(uint32_t)(g.GetScreenWidth() * renderScale);
        float renderH = (float)(uint32_t)(g.GetScreenHeight() * renderScale);
        float displayW = g.GetScreenWidth();

        int32_t phaseCount = ffxFsr2GetJitterPhaseCount((int32_t)renderW, (int32_t)displayW);
        float jitterX = 0.0f, jitterY = 0.0f;
        ffxFsr2GetJitterOffset(&jitterX, &jitterY, jitterIndex++, phaseCount);

        rc.jitterOffset.x = jitterX;
        rc.jitterOffset.y = jitterY;

        float shiftX = 2.0f * jitterX / renderW;
        float shiftY = 2.0f * jitterY / renderH;
        rc.projectionMatrix._31 += shiftX;
        rc.projectionMatrix._32 += shiftY;
    }

    //// ====================================================
    //// 7. フレームバッファのクリアとターゲット設定 (RHI)
    //// ====================================================
    if (g.GetAPI() != GraphicsAPI::DX12) {
        float clearColor[4] = { 0.1f, 0.1f, 0.12f, 1.0f };
        fb->Clear(rc.commandList, clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

        // レンダーターゲットをセットし、同時にビューポートも適用する
        fb->SetRenderTargets(rc.commandList);
    }
    rc.commandList->SetViewport(rc.mainViewport);

    // ====================================================
    // 8. アスペクト比・FOV・Near/Far の計算
    // ====================================================
    float renderH_Safe = rc.renderHeight > 0 ? static_cast<float>(rc.renderHeight) : 1.0f;
    rc.aspect = static_cast<float>(rc.renderWidth) / renderH_Safe;

    float m22 = rc.projectionMatrix._22;
    float m33 = rc.projectionMatrix._33;
    float m43 = rc.projectionMatrix._43;

    float fovY = (fabsf(m22) > 0.0001f) ? (2.0f * atanf(1.0f / m22)) : 0.785f;
    float nearZ = 0.1f;
    float farZ = 1000.0f;

    if (fabsf(m33) > 0.0001f && fabsf(1.0f - m33) > 0.0001f) {
        nearZ = -m43 / m33;
        farZ = m43 / (1.0f - m33);
    }

    if (nearZ <= 0.0f || std::isnan(nearZ)) nearZ = 0.1f;
    if (farZ <= nearZ || std::isnan(farZ))  farZ = nearZ + 1000.0f;
    if (fovY <= 0.0f || std::isnan(fovY))   fovY = 0.785f;

    rc.fovY = fovY;
    rc.nearZ = nearZ;
    rc.farZ = farZ;

    return rc;
}

void RenderPipeline::Execute(const RenderQueue& queue, RenderContext& rc)
{
    std::vector<RenderViewContext> views;
    views.push_back(BuildPrimaryViewContext(rc));
    ExecuteViews(queue, rc, views);
}

void RenderPipeline::ExecuteViews(const RenderQueue& queue, RenderContext& rc, const std::vector<RenderContext::ViewState>& views)
{
    std::vector<RenderViewContext> wrappedViews;
    wrappedViews.reserve(views.size());
    for (const auto& view : views) {
        RenderViewContext wrapped{};
        wrapped.state = view;
        wrappedViews.push_back(std::move(wrapped));
    }
    ExecuteViews(queue, rc, wrappedViews);
}

void RenderPipeline::ExecuteViews(const RenderQueue& queue, RenderContext& rc, std::vector<RenderViewContext>& views)
{
    PROFILE_SCOPE("Total RenderPipeline");

    if (views.empty()) {
        return;
    }

    rc.allowParallelRecording = views.size() <= 1;

    for (size_t viewIndex = 0; viewIndex < views.size(); ++viewIndex) {
        auto& viewContext = views[viewIndex];
        ExecuteView(queue, rc, viewContext);

        const bool hasMoreViews = (viewIndex + 1) < views.size();
        if (hasMoreViews && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
            auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
            dx12Cmd->End();
            dx12Cmd->Submit();
            if (auto* dx12Device = Graphics::Instance().GetDX12Device()) {
                dx12Device->WaitForGPU();
            }
            dx12Cmd->Begin();
        }
    }

    PruneInactiveViews(views);
}

void RenderPipeline::EnsureViewFrameBuffers(ViewHistoryState& history, const RenderContext::ViewState& viewState, IResourceFactory* factory)
{
    if (!factory) {
        return;
    }

    const uint32_t renderWidth = viewState.renderWidth;
    const uint32_t renderHeight = viewState.renderHeight;
    const uint32_t displayWidth = viewState.displayWidth > 0 ? viewState.displayWidth : renderWidth;
    const uint32_t displayHeight = viewState.displayHeight > 0 ? viewState.displayHeight : renderHeight;

    const bool renderSizeChanged =
        history.frameBuffers.renderWidth != renderWidth ||
        history.frameBuffers.renderHeight != renderHeight;
    if (renderWidth > 0 && renderHeight > 0 &&
        (!history.frameBuffers.scene || !history.frameBuffers.prevScene || renderSizeChanged)) {
        std::vector<TextureFormat> hdr = { TextureFormat::R16G16B16A16_FLOAT };
        history.frameBuffers.scene = std::make_unique<FrameBuffer>(factory, renderWidth, renderHeight, hdr);
        history.frameBuffers.prevScene = std::make_unique<FrameBuffer>(factory, renderWidth, renderHeight, hdr);
        history.frameBuffers.renderWidth = renderWidth;
        history.frameBuffers.renderHeight = renderHeight;
    }

    const bool displaySizeChanged =
        history.frameBuffers.displayWidth != displayWidth ||
        history.frameBuffers.displayHeight != displayHeight;
    if (displayWidth > 0 && displayHeight > 0 &&
        (!history.frameBuffers.display || displaySizeChanged)) {
        std::vector<TextureFormat> ldr = { TextureFormat::RGBA8_UNORM };
        history.frameBuffers.display = std::make_unique<FrameBuffer>(factory, displayWidth, displayHeight, ldr);
        history.frameBuffers.displayWidth = displayWidth;
        history.frameBuffers.displayHeight = displayHeight;
    }
}

void RenderPipeline::PruneInactiveViews(const std::vector<RenderViewContext>& views)
{
    if (m_viewHistory.empty()) {
        return;
    }

    std::unordered_set<uint64_t> activeKeys;
    activeKeys.reserve(views.size());
    for (const auto& view : views) {
        activeKeys.insert(ResolveHistoryKey(view.state));
    }

    for (auto it = m_viewHistory.begin(); it != m_viewHistory.end();) {
        if (activeKeys.find(it->first) == activeKeys.end()) {
            it = m_viewHistory.erase(it);
        } else {
            ++it;
        }
    }
}

void RenderPipeline::ExecuteView(const RenderQueue& queue, RenderContext& baseRc, RenderViewContext& viewContext)
{
    using Clock = std::chrono::high_resolution_clock;
    RenderContext rc = baseRc;
    const auto& viewState = viewContext.state;
    const uint64_t historyKey = ResolveHistoryKey(viewState);
    const bool usePreparedPath = rc.allowParallelRecording && viewState.enableComputeCulling;

    auto& history = m_viewHistory[historyKey];
    auto* factory = Graphics::Instance().GetResourceFactory();
    EnsureViewFrameBuffers(history, viewState, factory);

    ITexture* mainRenderTarget = viewState.mainRenderTarget;
    ITexture* mainDepthStencil = viewState.mainDepthStencil;
    ITexture* sceneColorTexture = viewState.sceneColorTexture;
    ITexture* sceneDepthTexture = viewState.sceneDepthTexture;
    ITexture* prevSceneTexture = viewState.prevSceneTexture;
    ITexture* displayColorTexture = viewState.displayColorTexture;

    if (!sceneColorTexture && history.frameBuffers.scene) {
        sceneColorTexture = history.frameBuffers.scene->GetColorTexture(0);
        sceneDepthTexture = history.frameBuffers.scene->GetDepthTexture();
    }
    if (!prevSceneTexture && history.frameBuffers.prevScene) {
        prevSceneTexture = history.frameBuffers.prevScene->GetColorTexture(0);
    }
    if (!displayColorTexture && history.frameBuffers.display) {
        displayColorTexture = history.frameBuffers.display->GetColorTexture(0);
    }
    if (!mainRenderTarget) {
        mainRenderTarget = sceneColorTexture;
    }
    if (!mainDepthStencil) {
        mainDepthStencil = sceneDepthTexture;
    }

    rc.mainRenderTarget = mainRenderTarget;
    rc.mainDepthStencil = mainDepthStencil;
    rc.sceneColorTexture = sceneColorTexture;
    rc.sceneDepthTexture = sceneDepthTexture;
    rc.renderWidth = viewState.renderWidth;
    rc.renderHeight = viewState.renderHeight;
    rc.displayWidth = viewState.displayWidth;
    rc.displayHeight = viewState.displayHeight;
    rc.panelWidth = viewState.panelWidth;
    rc.panelHeight = viewState.panelHeight;
    if (rc.sceneColorTexture) {
        rc.mainViewport = RhiViewport(
            0.0f,
            0.0f,
            static_cast<float>(rc.sceneColorTexture->GetWidth()),
            static_cast<float>(rc.sceneColorTexture->GetHeight()));
    } else if (rc.mainRenderTarget) {
        rc.mainViewport = RhiViewport(
            0.0f,
            0.0f,
            static_cast<float>(rc.mainRenderTarget->GetWidth()),
            static_cast<float>(rc.mainRenderTarget->GetHeight()));
    } else {
        rc.mainViewport = viewState.viewport;
    }
    rc.allowGpuDrivenCompute = viewState.enableComputeCulling;
    rc.allowAsyncCompute = viewState.enableAsyncCompute;
    rc.enableGTAO = viewState.enableGTAO;
    rc.enableSSGI = viewState.enableSSGI;
    rc.enableVolumetricFog = viewState.enableVolumetricFog;
    rc.enableSSR = viewState.enableSSR;
    rc.viewMatrix = viewState.viewMatrix;
    rc.projectionMatrix = viewState.projectionMatrix;
    rc.viewProjectionUnjittered = viewState.viewProjectionUnjittered;
    rc.prevViewProjectionMatrix = viewState.prevViewProjectionMatrix;
    rc.cameraPosition = viewState.cameraPosition;
    rc.cameraDirection = viewState.cameraDirection;
    rc.fovY = viewState.fovY;
    rc.aspect = viewState.aspect;
    rc.nearZ = viewState.nearZ;
    rc.farZ = viewState.farZ;
    rc.jitterOffset = viewState.jitterOffset;
    {
        if (!history.initialized) {
            history.prevViewProjection = viewState.viewProjectionUnjittered;
            history.prevJitterOffset = viewState.prevJitterOffset;
            history.initialized = true;
        }
        if (!history.frameGraph) {
            history.frameGraph = std::make_unique<FrameGraph>();
        }
        history.frameGraph->SetFrameIndex(m_frameSerial);
        if (history.passes.empty()) {
            history.passes.reserve(m_passes.size());
            for (const auto& pass : m_passes) {
                if (auto clone = ClonePassForView(pass, factory)) {
                    history.passes.push_back(std::move(clone));
                }
            }
        }
        if (!history.sceneBuffer && factory) {
            history.sceneBuffer = factory->CreateBuffer(sizeof(CbScene), BufferType::Constant);
        }
        if (!history.shadowBuffer && factory) {
            history.shadowBuffer = factory->CreateBuffer(sizeof(CbShadowMap), BufferType::Constant);
        }
        if (!history.shadowMap && factory) {
            history.shadowMap = std::make_shared<ShadowMap>(factory);
        }
        if (!history.modelRenderer && factory) {
            history.modelRenderer = std::make_unique<ModelRenderer>(factory);
        }
        rc.sceneConstantBufferOverride = history.sceneBuffer.get();
        rc.shadowConstantBufferOverride = history.shadowBuffer.get();
        rc.shadowMap = history.shadowMap ? history.shadowMap.get() : rc.shadowMap;
        rc.modelRendererOverride = history.modelRenderer ? history.modelRenderer.get() : rc.modelRendererOverride;
        if (usePreparedPath) {
            rc.preparedInstanceBuffer = history.preparedInstanceBuffer;
            rc.preparedVisibleInstanceStructuredBuffer = history.preparedVisibleInstanceStructuredBuffer;
            rc.preparedIndirectArgumentBuffer = history.preparedIndirectArgumentBuffer;
            rc.preparedIndirectCommandMetadataBuffer = history.preparedIndirectCommandMetadataBuffer;
            rc.preparedInstanceCapacity = history.preparedInstanceCapacity;
            rc.preparedIndirectArgumentCapacity = history.preparedIndirectArgumentCapacity;
            rc.preparedIndirectCommandMetadataCapacity = history.preparedIndirectCommandMetadataCapacity;
        } else {
            rc.preparedInstanceBuffer.reset();
            rc.preparedVisibleInstanceStructuredBuffer.reset();
            rc.preparedIndirectArgumentBuffer.reset();
            rc.preparedIndirectCommandMetadataBuffer.reset();
            rc.preparedInstanceCapacity = 0;
            rc.preparedIndirectArgumentCapacity = 0;
            rc.preparedIndirectCommandMetadataCapacity = 0;
        }
        rc.prevViewProjectionMatrix = history.prevViewProjection;
        rc.prevJitterOffset = history.prevJitterOffset;
        history.prevViewProjection = viewState.viewProjectionUnjittered;
        history.prevJitterOffset = viewState.jitterOffset;
    }

    FrameGraph& frameGraph = *history.frameGraph;
    auto& viewPasses = history.passes;

    ExtractVisibleInstancesPass* visiblePass = nullptr;
    BuildInstanceBufferPass* instancePass = nullptr;
    BuildIndirectCommandPass* indirectPass = nullptr;
    ComputeCullingPass* computePass = nullptr;
    auto& graphPasses = m_graphPassScratch;
    graphPasses.clear();
    graphPasses.reserve(viewPasses.size());
    for (const auto& pass : viewPasses) {
        if (!pass) {
            continue;
        }
        if (auto* typed = dynamic_cast<ExtractVisibleInstancesPass*>(pass.get())) {
            visiblePass = typed;
            continue;
        }
        if (auto* typed = dynamic_cast<BuildInstanceBufferPass*>(pass.get())) {
            instancePass = typed;
            continue;
        }
        if (auto* typed = dynamic_cast<BuildIndirectCommandPass*>(pass.get())) {
            indirectPass = typed;
            continue;
        }
        if (auto* typed = dynamic_cast<ComputeCullingPass*>(pass.get())) {
            computePass = typed;
            continue;
        }
        graphPasses.push_back(pass.get());
    }

    if (usePreparedPath && (visiblePass || instancePass || indirectPass || computePass)) {
        FrameGraphResources prepResources(frameGraph);
        auto& prepGraph = m_prepGraphScratch;
        prepGraph.clear();
        auto addNode = [&](std::function<void()> task, std::vector<size_t> deps = {}) {
            TaskSystem::TaskGraphNode node{};
            node.task = std::move(task);
            node.dependencies = std::move(deps);
            prepGraph.push_back(std::move(node));
            return prepGraph.size() - 1;
        };

        size_t visibleNode = static_cast<size_t>(-1);
        size_t instanceNode = static_cast<size_t>(-1);
        size_t indirectNode = static_cast<size_t>(-1);

        if (visiblePass) {
            visibleNode = addNode([&]() { visiblePass->Execute(prepResources, queue, rc); });
        }
        if (instancePass) {
            std::vector<size_t> deps;
            if (visibleNode != static_cast<size_t>(-1)) deps.push_back(visibleNode);
            instanceNode = addNode([&]() { instancePass->Execute(prepResources, queue, rc); }, std::move(deps));
        }
        if (indirectPass) {
            std::vector<size_t> deps;
            if (instanceNode != static_cast<size_t>(-1)) deps.push_back(instanceNode);
            indirectNode = addNode([&]() { indirectPass->Execute(prepResources, queue, rc); }, std::move(deps));
        }
        if (computePass) {
            std::vector<size_t> deps;
            if (indirectNode != static_cast<size_t>(-1)) deps.push_back(indirectNode);
            addNode([&]() { computePass->Execute(prepResources, queue, rc); }, std::move(deps));
        }

        if (!prepGraph.empty()) {
            TaskSystem::Instance().RunTaskGraph(prepGraph);
        }

        history.preparedInstanceBuffer = rc.preparedInstanceBuffer;
        history.preparedVisibleInstanceStructuredBuffer = rc.preparedVisibleInstanceStructuredBuffer;
        history.preparedIndirectArgumentBuffer = rc.preparedIndirectArgumentBuffer;
        history.preparedIndirectCommandMetadataBuffer = rc.preparedIndirectCommandMetadataBuffer;
        history.preparedInstanceCapacity = rc.preparedInstanceCapacity;
        history.preparedIndirectArgumentCapacity = rc.preparedIndirectArgumentCapacity;
        history.preparedIndirectCommandMetadataCapacity = rc.preparedIndirectCommandMetadataCapacity;
    } else {
        rc.visibleOpaqueInstanceBatches.clear();
        rc.preparedInstanceData.clear();
        rc.preparedOpaqueInstanceBatches.clear();
        rc.preparedIndirectCommands.clear();
        rc.preparedSkinnedCommands.clear();
        rc.gpuDrivenDispatchGroups.clear();
        rc.activeDrawCommands.clear();
        rc.activeSkinnedCommands.clear();
        rc.activeInstanceBuffer = nullptr;
        rc.activeDrawArgsBuffer = nullptr;
        rc.activeCountBuffer = nullptr;
        rc.useGpuCulling = false;
        rc.pendingAsyncComputeFenceValue = 0;
    }

    const auto uploadStart = Clock::now();
    if (auto* shadowMap = const_cast<ShadowMap*>(rc.shadowMap)) {
        shadowMap->UpdateCascades(rc);
    }
    SceneDataUploadSystem uploadSystem;
    uploadSystem.Upload(rc, GlobalRootSignature::Instance());
    rc.prepMetrics.sceneUploadMs =
        std::chrono::duration<double, std::milli>(Clock::now() - uploadStart).count();

    GlobalRootSignature::Instance().BindAll(
        rc.commandList,
        rc.renderState,
        rc.shadowMap,
        rc.sceneConstantBufferOverride,
        rc.shadowConstantBufferOverride
    );

    for (auto* pass : graphPasses) {
        if (pass) {
            frameGraph.AddPass(pass);
        }
    }

    Graphics& g = Graphics::Instance();
    ITexture* sceneColor = sceneColorTexture;
    ITexture* prevScene = prevSceneTexture;
    ITexture* displayColor = displayColorTexture;

    if (!sceneColor) {
        sceneColor = history.frameBuffers.scene ? history.frameBuffers.scene->GetColorTexture(0) : nullptr;
    }
    if (!prevScene) {
        prevScene = history.frameBuffers.prevScene ? history.frameBuffers.prevScene->GetColorTexture(0) : nullptr;
    }
    if (!displayColor) {
        displayColor = history.frameBuffers.display ? history.frameBuffers.display->GetColorTexture(0) : nullptr;
    }

    if (sceneColor) frameGraph.ImportTexture("SceneColor", sceneColor);
    if (prevScene) frameGraph.ImportTexture("PrevScene", prevScene);
    if (displayColor) frameGraph.ImportTexture("DisplayColor", displayColor);

    frameGraph.Execute(queue, rc);

    if (sceneColor && prevScene) {
        if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
            auto* sceneDx12 = dynamic_cast<DX12Texture*>(sceneColor);
            auto* prevDx12 = dynamic_cast<DX12Texture*>(prevScene);
            if (sceneDx12 && prevDx12) {
                rc.commandList->TransitionBarrier(sceneColor, ResourceState::CopySource);
                rc.commandList->TransitionBarrier(prevScene, ResourceState::CopyDest);
                if (auto* dx12Cmd = dynamic_cast<DX12CommandList*>(rc.commandList)) {
                    dx12Cmd->FlushResourceBarriers();
                    dx12Cmd->GetNativeCommandList()->CopyResource(
                        prevDx12->GetNativeResource(),
                        sceneDx12->GetNativeResource());
                }
                rc.commandList->TransitionBarrier(prevScene, ResourceState::ShaderResource);
                rc.commandList->TransitionBarrier(sceneColor, ResourceState::ShaderResource);
            }
        } else {
            FrameBuffer* sourceFB = history.frameBuffers.scene.get();
            FrameBuffer* destFB = history.frameBuffers.prevScene.get();
            if (sourceFB && destFB) {
                Graphics::Instance().CopyFrameBuffer(sourceFB, destFB);
            }
        }
    }

    baseRc.prepMetrics = rc.prepMetrics;
    baseRc.mainRenderTarget = rc.mainRenderTarget;
    baseRc.mainDepthStencil = rc.mainDepthStencil;
    baseRc.sceneColorTexture = rc.sceneColorTexture;
    baseRc.sceneDepthTexture = rc.sceneDepthTexture;
    baseRc.renderWidth = rc.renderWidth;
    baseRc.renderHeight = rc.renderHeight;
    baseRc.displayWidth = rc.displayWidth;
    baseRc.displayHeight = rc.displayHeight;
    baseRc.panelWidth = rc.panelWidth;
    baseRc.panelHeight = rc.panelHeight;
    baseRc.mainViewport = rc.mainViewport;
    baseRc.preparedInstanceBuffer = rc.preparedInstanceBuffer;
    baseRc.preparedVisibleInstanceStructuredBuffer = rc.preparedVisibleInstanceStructuredBuffer;
    baseRc.preparedIndirectArgumentBuffer = rc.preparedIndirectArgumentBuffer;
    baseRc.preparedIndirectCommandMetadataBuffer = rc.preparedIndirectCommandMetadataBuffer;
    baseRc.preparedInstanceCapacity = rc.preparedInstanceCapacity;
    baseRc.preparedIndirectArgumentCapacity = rc.preparedIndirectArgumentCapacity;
    baseRc.preparedIndirectCommandMetadataCapacity = rc.preparedIndirectCommandMetadataCapacity;
    baseRc.preparedVisibleInstanceCount = rc.preparedVisibleInstanceCount;
    baseRc.gpuDrivenDispatchGroups = rc.gpuDrivenDispatchGroups;
    baseRc.activeDrawCommands = rc.activeDrawCommands;
    baseRc.activeSkinnedCommands = rc.activeSkinnedCommands;
    baseRc.activeInstanceBuffer = rc.activeInstanceBuffer;
    baseRc.activeDrawArgsBuffer = rc.activeDrawArgsBuffer;
    baseRc.activeCountBuffer = rc.activeCountBuffer;
    baseRc.useGpuCulling = rc.useGpuCulling;
    baseRc.pendingAsyncComputeFenceValue = rc.pendingAsyncComputeFenceValue;
    baseRc.debugGBuffer0 = rc.debugGBuffer0;
    baseRc.debugGBuffer1 = rc.debugGBuffer1;
    baseRc.debugGBuffer2 = rc.debugGBuffer2;
    baseRc.debugGBufferDepth = rc.debugGBufferDepth;
    viewContext.sceneViewTexture = rc.sceneColorTexture;
    viewContext.prevSceneTexture = prevScene;
    viewContext.displayTexture = displayColor;
    viewContext.sceneDepthTexture = rc.sceneDepthTexture;
    viewContext.debugGBuffer0 = rc.debugGBuffer0;
    viewContext.debugGBuffer1 = rc.debugGBuffer1;
    viewContext.debugGBuffer2 = rc.debugGBuffer2;
    viewContext.debugDepth = rc.debugGBufferDepth;
}

RenderPipeline::RenderViewContext RenderPipeline::BuildPrimaryViewContext(const RenderContext& rc, uint32_t panelWidth, uint32_t panelHeight) const
{
    RenderViewContext viewContext{};
    auto& view = viewContext.state;
    view.mainRenderTarget = nullptr;
    view.mainDepthStencil = nullptr;
    view.sceneColorTexture = nullptr;
    view.sceneDepthTexture = nullptr;
    view.prevSceneTexture = nullptr;
    view.displayColorTexture = nullptr;

    view.panelWidth = panelWidth;
    view.panelHeight = panelHeight;
    view.renderWidth = rc.renderWidth;
    view.renderHeight = rc.renderHeight;
    view.displayWidth = rc.displayWidth > 0 ? rc.displayWidth : rc.renderWidth;
    view.displayHeight = rc.displayHeight > 0 ? rc.displayHeight : rc.renderHeight;
    view.viewport = RhiViewport(
        0.0f,
        0.0f,
        static_cast<float>(view.renderWidth),
        static_cast<float>(view.renderHeight));
    view.historyKey = 1;
    view.viewMatrix = rc.viewMatrix;
    view.aspect = (view.renderHeight > 0)
        ? (static_cast<float>(view.renderWidth) / static_cast<float>(view.renderHeight))
        : rc.aspect;
    view.fovY = rc.fovY;
    view.nearZ = rc.nearZ;
    view.farZ = rc.farZ;
    if (view.fovY > 0.0f && view.nearZ > 0.0f && view.farZ > view.nearZ) {
        DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
            view.fovY,
            view.aspect,
            view.nearZ,
            view.farZ);
        DirectX::XMStoreFloat4x4(&view.projectionMatrix, projection);
        DirectX::XMMATRIX viewMatrix = DirectX::XMLoadFloat4x4(&view.viewMatrix);
        DirectX::XMStoreFloat4x4(&view.viewProjectionUnjittered, viewMatrix * projection);
    } else {
        view.projectionMatrix = rc.projectionMatrix;
        view.viewProjectionUnjittered = rc.viewProjectionUnjittered;
    }
    view.prevViewProjectionMatrix = rc.prevViewProjectionMatrix;
    view.cameraPosition = rc.cameraPosition;
    view.cameraDirection = rc.cameraDirection;
    view.jitterOffset = rc.jitterOffset;
    view.prevJitterOffset = rc.prevJitterOffset;
    view.enableComputeCulling = rc.allowGpuDrivenCompute;
    view.enableAsyncCompute = rc.allowAsyncCompute;
    view.enableGTAO = rc.enableGTAO;
    view.enableSSGI = rc.enableSSGI;
    view.enableVolumetricFog = rc.enableVolumetricFog;
    view.enableSSR = rc.enableSSR;
    return viewContext;
}

RenderContext::ViewState RenderPipeline::BuildPrimaryViewState(const RenderContext& rc, uint32_t panelWidth, uint32_t panelHeight) const
{
    return BuildPrimaryViewContext(rc, panelWidth, panelHeight).state;
}

void RenderPipeline::BlitSceneToDisplay(RenderContext& rc)
{
    Graphics& g = Graphics::Instance();
    FrameBuffer* sceneFB = g.GetFrameBuffer(FrameBufferId::Scene);
    FrameBuffer* displayFB = g.GetFrameBuffer(FrameBufferId::Display);
    if (!sceneFB || !displayFB) return;

    ITexture* sceneColor = sceneFB->GetColorTexture(0);
    ITexture* displayColor = displayFB->GetColorTexture(0);
    if (!sceneColor || !displayColor) return;

    // SceneColor → ShaderResource (読み取り用)
    rc.commandList->TransitionBarrier(sceneColor, ResourceState::ShaderResource);
    // DisplayColor → RenderTarget (書き込み用)
    rc.commandList->TransitionBarrier(displayColor, ResourceState::RenderTarget);

    // FinalBlitPass の PSO を検索して使用
    IPipelineState* blitPSO = nullptr;
    for (auto& pass : m_passes) {
        if (pass->GetName() == "FinalBlitPass") {
            auto* fbPass = static_cast<FinalBlitPass*>(pass.get());
            blitPSO = fbPass->GetPSO();
            break;
        }
    }

    if (!blitPSO) {
        // PSO が見つからない場合はシアンにクリアしてデバッグ
        float cyan[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
        rc.commandList->ClearColor(displayColor, cyan);
        return;
    }

    rc.commandList->SetPipelineState(blitPSO);
    rc.commandList->SetRenderTarget(displayColor, nullptr);
    RhiViewport vp(0.0f, 0.0f, (float)displayColor->GetWidth(), (float)displayColor->GetHeight());
    rc.commandList->SetViewport(vp);
    rc.commandList->PSSetTexture(0, sceneColor);
    rc.commandList->SetPrimitiveTopology(PrimitiveTopology::TriangleStrip);
    rc.commandList->Draw(4, 0);
    rc.commandList->PSSetTexture(0, nullptr);
}

void RenderPipeline::EndFrame(RenderContext& rc)
{
    Graphics& g = Graphics::Instance();

    // ====================================================
    // バックバッファをレンダーターゲットとしてセット
    // DX12: コマンドリストはまだ開いたまま（ImGui描画のため）
    // ====================================================
    ITexture* backBuffer = g.GetBackBufferTexture();

    if (backBuffer) {
        rc.commandList->SetRenderTarget(backBuffer, nullptr);
    }
}

void RenderPipeline::SubmitFrame(RenderContext& rc)
{
    using Clock = std::chrono::high_resolution_clock;
    const auto submitStart = Clock::now();

    Graphics& g = Graphics::Instance();

    // Keep GPU buffers alive until GPU finishes (prevents use-after-free)
    auto& slot = m_inFlightResources[m_inFlightIndex % kMaxInFlight];
    slot.instanceBuffer = rc.preparedInstanceBuffer;
    slot.instanceStructuredBuffer = rc.preparedVisibleInstanceStructuredBuffer;
    slot.drawArgsBuffer = rc.preparedIndirectArgumentBuffer;
    slot.metadataBuffer = rc.preparedIndirectCommandMetadataBuffer;
    ++m_inFlightIndex;

    if (g.GetAPI() == GraphicsAPI::DX12) {
        ITexture* backBuffer = g.GetBackBufferTexture();
        if (backBuffer) {
            rc.commandList->TransitionBarrier(backBuffer, ResourceState::Present);
        }
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        dx12Cmd->End();
        dx12Cmd->Submit();
    }

    rc.prepMetrics.submitFrameMs =
        std::chrono::duration<double, std::milli>(Clock::now() - submitStart).count();
}
