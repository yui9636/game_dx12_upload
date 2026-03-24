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
#include "Console/Logger.h"
#include "System/TaskSystem.h"
#include <chrono>


RenderContext RenderPipeline::BeginFrame(Registry& registry, FrameBuffer* targetBuffer)
{
    Graphics& g = Graphics::Instance();

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
    rc.time = (float)EngineKernel::Instance().GetTime().totalTime;
    rc.shadowMap = g.GetShadowMap();

    rc.mainViewport = RhiViewport(0.0f, 0.0f, (float)g.GetScreenWidth(), (float)g.GetScreenHeight());
    rc.mainRenderTarget = fb->GetColorTexture(0);
    rc.mainDepthStencil = fb->GetDepthTexture();

    // ↓ これを追加しないとポストエフェクトに深度が伝わりません！
    rc.sceneColorTexture = fb->GetColorTexture(0);
    rc.sceneDepthTexture = fb->GetDepthTexture();

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
    float screenH_Safe = g.GetScreenHeight() > 1.0f ? g.GetScreenHeight() : 1.0f;
    rc.aspect = g.GetScreenWidth() / screenH_Safe;

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
    std::vector<RenderContext::ViewState> views;
    views.push_back(BuildPrimaryViewState(rc));
    ExecuteViews(queue, rc, views);
}

void RenderPipeline::ExecuteViews(const RenderQueue& queue, RenderContext& rc, const std::vector<RenderContext::ViewState>& views)
{
    PROFILE_SCOPE("Total RenderPipeline");

    if (views.empty()) {
        return;
    }

    for (const auto& viewState : views) {
        ExecuteSingleView(queue, rc, viewState);
    }
}

void RenderPipeline::ExecuteSingleView(const RenderQueue& queue, RenderContext& baseRc, const RenderContext::ViewState& viewState)
{
    using Clock = std::chrono::high_resolution_clock;

    baseRc.mainViewport = viewState.viewport;
    baseRc.mainRenderTarget = viewState.mainRenderTarget;
    baseRc.mainDepthStencil = viewState.mainDepthStencil;
    baseRc.sceneColorTexture = viewState.sceneColorTexture;
    baseRc.sceneDepthTexture = viewState.sceneDepthTexture;
    baseRc.viewMatrix = viewState.viewMatrix;
    baseRc.projectionMatrix = viewState.projectionMatrix;
    baseRc.viewProjectionUnjittered = viewState.viewProjectionUnjittered;
    baseRc.prevViewProjectionMatrix = viewState.prevViewProjectionMatrix;
    baseRc.cameraPosition = viewState.cameraPosition;
    baseRc.cameraDirection = viewState.cameraDirection;
    baseRc.fovY = viewState.fovY;
    baseRc.aspect = viewState.aspect;
    baseRc.nearZ = viewState.nearZ;
    baseRc.farZ = viewState.farZ;
    baseRc.jitterOffset = viewState.jitterOffset;
    {
        auto& history = m_viewHistory[viewState.historyKey];
        if (!history.initialized) {
            history.prevViewProjection = viewState.viewProjectionUnjittered;
            history.prevJitterOffset = viewState.prevJitterOffset;
            history.initialized = true;
        }
        baseRc.prevViewProjectionMatrix = history.prevViewProjection;
        baseRc.prevJitterOffset = history.prevJitterOffset;
        history.prevViewProjection = viewState.viewProjectionUnjittered;
        history.prevJitterOffset = viewState.jitterOffset;
    }

    ExtractVisibleInstancesPass* visiblePass = nullptr;
    BuildInstanceBufferPass* instancePass = nullptr;
    BuildIndirectCommandPass* indirectPass = nullptr;
    ComputeCullingPass* computePass = nullptr;
    std::vector<IRenderPass*> graphPasses;
    graphPasses.reserve(m_passes.size());
    for (const auto& pass : m_passes) {
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

    if (visiblePass || instancePass || indirectPass || computePass) {
        FrameGraphResources prepResources(m_frameGraph);
        std::vector<TaskSystem::TaskGraphNode> prepGraph;
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
            visibleNode = addNode([&]() { visiblePass->Execute(prepResources, queue, baseRc); });
        }
        if (instancePass) {
            std::vector<size_t> deps;
            if (visibleNode != static_cast<size_t>(-1)) deps.push_back(visibleNode);
            instanceNode = addNode([&]() { instancePass->Execute(prepResources, queue, baseRc); }, std::move(deps));
        }
        if (indirectPass) {
            std::vector<size_t> deps;
            if (instanceNode != static_cast<size_t>(-1)) deps.push_back(instanceNode);
            indirectNode = addNode([&]() { indirectPass->Execute(prepResources, queue, baseRc); }, std::move(deps));
        }
        if (computePass) {
            std::vector<size_t> deps;
            if (indirectNode != static_cast<size_t>(-1)) deps.push_back(indirectNode);
            addNode([&]() { computePass->Execute(prepResources, queue, baseRc); }, std::move(deps));
        }

        if (!prepGraph.empty()) {
            TaskSystem::Instance().RunTaskGraph(prepGraph);
        }
    }

    const auto uploadStart = Clock::now();
    SceneDataUploadSystem uploadSystem;
    uploadSystem.Upload(baseRc, GlobalRootSignature::Instance());
    baseRc.prepMetrics.sceneUploadMs =
        std::chrono::duration<double, std::milli>(Clock::now() - uploadStart).count();

    GlobalRootSignature::Instance().BindAll(
        baseRc.commandList,
        baseRc.renderState,
        baseRc.shadowMap
    );

    for (auto* pass : graphPasses) {
        if (pass) {
            m_frameGraph.AddPass(pass);
        }
    }

    Graphics& g = Graphics::Instance();
    FrameBuffer* sceneBuffer = g.GetFrameBuffer(FrameBufferId::Scene);
    FrameBuffer* prevSceneFB = g.GetFrameBuffer(FrameBufferId::PrevScene);
    FrameBuffer* displayBuffer = g.GetFrameBuffer(FrameBufferId::Display);

    if (sceneBuffer) m_frameGraph.ImportTexture("SceneColor", sceneBuffer->GetColorTexture(0));
    if (prevSceneFB) m_frameGraph.ImportTexture("PrevScene", prevSceneFB->GetColorTexture(0));
    if (displayBuffer) m_frameGraph.ImportTexture("DisplayColor", displayBuffer->GetColorTexture(0));

    m_frameGraph.Execute(queue, baseRc);

    Graphics::Instance().CopyFrameBuffer(
        Graphics::Instance().GetFrameBuffer(FrameBufferId::Scene),
        Graphics::Instance().GetFrameBuffer(FrameBufferId::PrevScene)
    );
}

RenderContext::ViewState RenderPipeline::BuildPrimaryViewState(const RenderContext& rc) const
{
    RenderContext::ViewState view{};
    view.historyKey = 0;
    view.viewport = rc.mainViewport;
    view.mainRenderTarget = rc.mainRenderTarget;
    view.mainDepthStencil = rc.mainDepthStencil;
    view.sceneColorTexture = rc.sceneColorTexture;
    view.sceneDepthTexture = rc.sceneDepthTexture;
    view.viewMatrix = rc.viewMatrix;
    view.projectionMatrix = rc.projectionMatrix;
    view.viewProjectionUnjittered = rc.viewProjectionUnjittered;
    view.prevViewProjectionMatrix = rc.prevViewProjectionMatrix;
    view.cameraPosition = rc.cameraPosition;
    view.cameraDirection = rc.cameraDirection;
    view.fovY = rc.fovY;
    view.aspect = rc.aspect;
    view.nearZ = rc.nearZ;
    view.farZ = rc.farZ;
    view.jitterOffset = rc.jitterOffset;
    view.prevJitterOffset = rc.prevJitterOffset;
    return view;
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
