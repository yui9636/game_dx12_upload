#include "EngineKernel.h"
#include "Graphics.h"
#include "Layer/GameLayer.h"
#include "Layer/EditorLayer.h"
#include <imgui.h>
#include "Icon/IconFontManager.h"
#include "Asset/ThumbnailGenerator.h"
#include "RenderPass/DrawObjectsPass.h"
#include "RenderPass/ExtractVisibleInstancesPass.h"
#include "RenderPass/BuildInstanceBufferPass.h"
#include "RenderPass/BuildIndirectCommandPass.h"
#include "RenderPass/ComputeCullingPass.h"
#include "RenderPass/ShadowPass.h"
#include "RenderPass/PostProcessPass.h"
#include <RenderPass\SkyboxPass.h>
#include <RenderPass\GBufferPass.h>
#include <RenderPass\ForwardTransparentPass.h>
#include <RenderPass\DeferredLightingPass.h>
#include <RenderPass\GTAOPass.h>
#include "RenderPass/SSGIPass.h"
#include "Render/GlobalRootSignature.h"
#include <RenderPass\VolumetricFogPass.h>
#include <RenderPass\SSRPass.h>
#include "RenderPass/FinalBlitPass.h"
#include <Material\MaterialPreviewStudio.h>
#include "RHI/IResourceFactory.h"
#include "ImGuiRenderer.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Texture.h"
#include "Console/Logger.h"
#include <wrl/client.h>

namespace {
    constexpr bool kEnableDx12RuntimeDiagnostics = false;

    struct TextureSnapshotState {
        Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
        UINT64 bufferSize = 0;
        UINT width = 0;
        UINT height = 0;
        UINT rowPitch = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool logged = false;
        bool pending = false;
    };

    float HalfToFloat(uint16_t value) {
        const uint32_t sign = static_cast<uint32_t>(value & 0x8000u) << 16;
        uint32_t exponent = (value >> 10) & 0x1Fu;
        uint32_t mantissa = value & 0x03FFu;

        if (exponent == 0) {
            if (mantissa == 0) {
                uint32_t bits = sign;
                float result;
                memcpy(&result, &bits, sizeof(result));
                return result;
            }

            while ((mantissa & 0x0400u) == 0) {
                mantissa <<= 1;
                --exponent;
            }
            ++exponent;
            mantissa &= ~0x0400u;
        } else if (exponent == 31) {
            uint32_t bits = sign | 0x7F800000u | (mantissa << 13);
            float result;
            memcpy(&result, &bits, sizeof(result));
            return result;
        }

        exponent = exponent + (127 - 15);
        uint32_t bits = sign | (exponent << 23) | (mantissa << 13);
        float result;
        memcpy(&result, &bits, sizeof(result));
        return result;
    }

    TextureSnapshotState& GetDisplaySnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    TextureSnapshotState& GetSceneSnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    TextureSnapshotState& GetSceneOpaqueSnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    TextureSnapshotState& GetGBuffer0SnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    TextureSnapshotState& GetGBuffer1SnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    TextureSnapshotState& GetGBuffer2SnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    void ReadSnapshotPixel(const TextureSnapshotState& state, const uint8_t* bytes, UINT x, UINT y,
        double& r, double& g, double& b, double& a)
    {
        r = 0.0;
        g = 0.0;
        b = 0.0;
        a = 1.0;

        const uint8_t* row = bytes + static_cast<size_t>(y) * state.rowPitch;
        if (state.format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
            const auto* pixel = reinterpret_cast<const uint16_t*>(row + x * 8);
            r = HalfToFloat(pixel[0]);
            g = HalfToFloat(pixel[1]);
            b = HalfToFloat(pixel[2]);
            a = HalfToFloat(pixel[3]);
            return;
        }

        if (state.format == DXGI_FORMAT_R32G32B32A32_FLOAT) {
            const auto* pixel = reinterpret_cast<const float*>(row + x * 16);
            r = pixel[0];
            g = pixel[1];
            b = pixel[2];
            a = pixel[3];
            return;
        }

        const uint8_t* pixel = row + x * 4;
        r = pixel[0];
        g = pixel[1];
        b = pixel[2];
        a = pixel[3];
    }

    void ScheduleTextureSnapshot(DX12CommandList* commandList, DX12Texture* texture, TextureSnapshotState& state) {
        if (!commandList || !texture || state.logged) {
            return;
        }

        auto* device = Graphics::Instance().GetDX12Device();
        if (!device) {
            return;
        }

        auto* nativeTexture = texture->GetNativeResource();
        D3D12_RESOURCE_DESC desc = nativeTexture->GetDesc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 totalBytes = 0;
        device->GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

        if (!state.readbackBuffer || state.bufferSize < totalBytes) {
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_READBACK;

            D3D12_RESOURCE_DESC bufferDesc = {};
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Width = totalBytes;
            bufferDesc.Height = 1;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.MipLevels = 1;
            bufferDesc.SampleDesc.Count = 1;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            state.readbackBuffer.Reset();
            HRESULT hr = device->GetDevice()->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&state.readbackBuffer));
            if (FAILED(hr)) {
                LOG_ERROR("[Snapshot] Failed to create readback buffer hr=0x%08X", static_cast<unsigned int>(hr));
                return;
            }
            state.bufferSize = totalBytes;
        }

        state.width = static_cast<UINT>(desc.Width);
        state.height = desc.Height;
        state.rowPitch = footprint.Footprint.RowPitch;
        state.format = desc.Format;

        auto* nativeCommandList = commandList->GetNativeCommandList();

        D3D12_RESOURCE_BARRIER toCopy = {};
        toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopy.Transition.pResource = nativeTexture;
        toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        nativeCommandList->ResourceBarrier(1, &toCopy);

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = state.readbackBuffer.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = nativeTexture;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        nativeCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER toSrv = {};
        toSrv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toSrv.Transition.pResource = nativeTexture;
        toSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toSrv.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        toSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        nativeCommandList->ResourceBarrier(1, &toSrv);

        state.pending = true;
    }

    void LogTextureSnapshot(const char* label, TextureSnapshotState& state) {
        if (!state.pending || !state.readbackBuffer) {
            return;
        }

        void* mapped = nullptr;
        D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(state.bufferSize) };
        HRESULT hr = state.readbackBuffer->Map(0, &readRange, &mapped);
        if (FAILED(hr) || !mapped) {
            LOG_ERROR("[%s] Failed to map readback buffer hr=0x%08X", label, static_cast<unsigned int>(hr));
            state.pending = false;
            state.logged = true;
            return;
        }

        const auto* bytes = static_cast<const uint8_t*>(mapped);
        double sumR = 0.0, sumG = 0.0, sumB = 0.0, sumA = 0.0;
        uint64_t nonBlack = 0;
        double centerR = 0.0, centerG = 0.0, centerB = 0.0, centerA = 0.0;

        for (UINT y = 0; y < state.height; ++y) {
            const uint8_t* row = bytes + static_cast<size_t>(y) * state.rowPitch;
            for (UINT x = 0; x < state.width; ++x) {
                double r = 0.0, g = 0.0, b = 0.0, a = 1.0;
                if (state.format == DXGI_FORMAT_R32G32B32A32_FLOAT) {
                    const auto* pixel = reinterpret_cast<const float*>(row + x * 16);
                    r = pixel[0];
                    g = pixel[1];
                    b = pixel[2];
                    a = pixel[3];
                } else if (state.format == DXGI_FORMAT_R32G32_FLOAT) {
                    const auto* pixel = reinterpret_cast<const float*>(row + x * 8);
                    r = pixel[0];
                    g = pixel[1];
                } else if (state.format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
                    const auto* pixel = reinterpret_cast<const uint16_t*>(row + x * 8);
                    r = HalfToFloat(pixel[0]);
                    g = HalfToFloat(pixel[1]);
                    b = HalfToFloat(pixel[2]);
                    a = HalfToFloat(pixel[3]);
                } else {
                    const uint8_t* pixel = row + x * 4;
                    r = pixel[0];
                    g = pixel[1];
                    b = pixel[2];
                    a = pixel[3];
                }

                sumR += r;
                sumG += g;
                sumB += b;
                sumA += a;
                if (r > 0.0001 || g > 0.0001 || b > 0.0001) {
                    ++nonBlack;
                }

                if (x == state.width / 2 && y == state.height / 2) {
                    centerR = r;
                    centerG = g;
                    centerB = b;
                    centerA = a;
                }
            }
        }

        const double invPixelCount = (state.width && state.height) ? (1.0 / static_cast<double>(state.width * state.height)) : 0.0;
        LOG_INFO("[%s] format=%d avgRGBA=(%.4f, %.4f, %.4f, %.4f) centerRGBA=(%.4f, %.4f, %.4f, %.4f) nonBlack=%llu/%u",
            label,
            static_cast<int>(state.format),
            sumR * invPixelCount,
            sumG * invPixelCount,
            sumB * invPixelCount,
            sumA * invPixelCount,
            centerR, centerG, centerB, centerA,
            static_cast<unsigned long long>(nonBlack),
            state.width * state.height);

        D3D12_RANGE writeRange = { 0, 0 };
        state.readbackBuffer->Unmap(0, &writeRange);
        state.pending = false;
        state.logged = true;
    }

    void LogSceneOverGBufferMask(TextureSnapshotState& maskState, TextureSnapshotState& sceneState) {
        static bool logged = false;
        if (logged || !maskState.readbackBuffer || !sceneState.readbackBuffer ||
            !maskState.logged || !sceneState.logged) {
            return;
        }

        void* maskMapped = nullptr;
        void* sceneMapped = nullptr;
        D3D12_RANGE maskRange = { 0, static_cast<SIZE_T>(maskState.bufferSize) };
        D3D12_RANGE sceneRange = { 0, static_cast<SIZE_T>(sceneState.bufferSize) };
        if (FAILED(maskState.readbackBuffer->Map(0, &maskRange, &maskMapped)) || !maskMapped) {
            return;
        }
        if (FAILED(sceneState.readbackBuffer->Map(0, &sceneRange, &sceneMapped)) || !sceneMapped) {
            D3D12_RANGE writeRange = { 0, 0 };
            maskState.readbackBuffer->Unmap(0, &writeRange);
            return;
        }

        const auto* maskBytes = static_cast<const uint8_t*>(maskMapped);
        const auto* sceneBytes = static_cast<const uint8_t*>(sceneMapped);
        const UINT width = (maskState.width < sceneState.width) ? maskState.width : sceneState.width;
        const UINT height = (maskState.height < sceneState.height) ? maskState.height : sceneState.height;

        double sumMaskR = 0.0, sumMaskG = 0.0, sumMaskB = 0.0;
        double sumSceneR = 0.0, sumSceneG = 0.0, sumSceneB = 0.0;
        uint64_t count = 0;

        for (UINT y = 0; y < height; ++y) {
            for (UINT x = 0; x < width; ++x) {
                double maskR, maskG, maskB, maskA;
                ReadSnapshotPixel(maskState, maskBytes, x, y, maskR, maskG, maskB, maskA);
                if (maskR <= 0.0001 && maskG <= 0.0001 && maskB <= 0.0001) {
                    continue;
                }

                double sceneR, sceneG, sceneB, sceneA;
                ReadSnapshotPixel(sceneState, sceneBytes, x, y, sceneR, sceneG, sceneB, sceneA);
                sumMaskR += maskR;
                sumMaskG += maskG;
                sumMaskB += maskB;
                sumSceneR += sceneR;
                sumSceneG += sceneG;
                sumSceneB += sceneB;
                ++count;
            }
        }

        D3D12_RANGE writeRange = { 0, 0 };
        sceneState.readbackBuffer->Unmap(0, &writeRange);
        maskState.readbackBuffer->Unmap(0, &writeRange);

        if (count == 0) {
            return;
        }

        const double invCount = 1.0 / static_cast<double>(count);
        LOG_INFO("[SceneOverAlbedoMask] maskAvgRGB=(%.4f, %.4f, %.4f) sceneAvgRGB=(%.4f, %.4f, %.4f) count=%llu",
            sumMaskR * invCount,
            sumMaskG * invCount,
            sumMaskB * invCount,
            sumSceneR * invCount,
            sumSceneG * invCount,
            sumSceneB * invCount,
            static_cast<unsigned long long>(count));
        logged = true;
    }
}

EngineKernel& EngineKernel::Instance()
{
    static EngineKernel instance;
    return instance;
}

void EngineKernel::Initialize()
{
    time = EngineTime();
    mode = EngineMode::Editor;

    m_renderPipeline = std::make_unique<RenderPipeline>();

    const bool isDX12 = (Graphics::Instance().GetAPI() == GraphicsAPI::DX12);
    auto* factory = Graphics::Instance().GetResourceFactory();

    m_renderPipeline->AddPass(std::make_shared<ExtractVisibleInstancesPass>());
    m_renderPipeline->AddPass(std::make_shared<BuildInstanceBufferPass>());
    m_renderPipeline->AddPass(std::make_shared<BuildIndirectCommandPass>());
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        m_renderPipeline->AddPass(std::make_shared<ComputeCullingPass>());
    }
    m_renderPipeline->AddPass(std::make_shared<ShadowPass>());
    m_renderPipeline->AddPass(std::make_shared<GBufferPass>());
    m_renderPipeline->AddPass(std::make_shared<GTAOPass>(factory));
    m_renderPipeline->AddPass(std::make_shared<SSGIPass>(factory));
    m_renderPipeline->AddPass(std::make_shared<VolumetricFogPass>(factory));
    m_renderPipeline->AddPass(std::make_shared<SSRPass>(factory));
    m_renderPipeline->AddPass(std::make_shared<DeferredLightingPass>(factory));
    m_renderPipeline->AddPass(std::make_shared<SkyboxPass>());
    m_renderPipeline->AddPass(std::make_shared<ForwardTransparentPass>());
    m_renderPipeline->AddPass(std::make_shared<PostProcessPass>());

    if (isDX12) {
        m_renderPipeline->AddPass(std::make_shared<FinalBlitPass>(factory));
    }

    m_gameLayer = std::make_unique<GameLayer>();
    m_gameLayer->Initialize();

    std::vector<IconFontManager::SizeConfig> configs = {
        { IconFontSize::Mini,   14.0f },
        { IconFontSize::Small,  14.0f },
        { IconFontSize::Medium, 18.0f },
        { IconFontSize::Large,  24.0f },
        { IconFontSize::Extra,  64.0f }
    };
    IconFontManager::Instance().Setup(configs);
    m_sharedOffscreen = std::make_unique<OffscreenRenderer>();
    if (m_sharedOffscreen->Initialize()) {
        ThumbnailGenerator::Instance().Initialize(m_sharedOffscreen.get());
        MaterialPreviewStudio::Instance().Initialize(m_sharedOffscreen.get());
    } else {
        LOG_ERROR("[EngineKernel] Failed to initialize shared OffscreenRenderer.");
        ThumbnailGenerator::Instance().Initialize(nullptr);
        MaterialPreviewStudio::Instance().Initialize(nullptr);
    }

    LOG_INFO("[EngineKernel] Initialize API=%s", isDX12 ? "DX12" : "DX11");

    if (isDX12) {
        m_editorLayer = std::make_unique<EditorLayer>(m_gameLayer.get());
        m_editorLayer->Initialize();
        return;
    }

    ID3D11Device* dx11Dev = Graphics::Instance().GetDevice();

    m_probeBaker = std::make_unique<ReflectionProbeBaker>(dx11Dev);
    GlobalRootSignature::Instance().Initialize(dx11Dev);

    m_editorLayer = std::make_unique<EditorLayer>(m_gameLayer.get());
    m_editorLayer->Initialize();
}

void EngineKernel::Finalize()
{
    if (m_editorLayer) m_editorLayer->Finalize();
    if (m_gameLayer) m_gameLayer->Finalize();
}

void EngineKernel::Update(float rawDt)
{
    time.unscaledDt = rawDt;
    time.frameCount++;

    if (m_editorLayer) m_editorLayer->Update(time);

    if (mode == EngineMode::Play)
        time.dt = rawDt * time.timeScale;
    else
        time.dt = 0.0f;

    if (m_gameLayer) m_gameLayer->Update(time);

    time.totalTime += time.dt;
}

void EngineKernel::Render()
{
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        if (auto* dx12Device = Graphics::Instance().GetDX12Device()) {
            dx12Device->ProcessDeferredFrees();
        }
    }
    if (m_sharedOffscreen) {
        ImGuiRenderer::ProcessDeferredUnregisters(m_sharedOffscreen->GetCompletedFenceValue());
    }

    // Priority dispatch: MaterialPreview (active editing) > Thumbnails (background)
    // 1 job per frame on shared OffscreenRenderer to avoid GPU contention
    static int s_thumbnailSkipCounter = 0;
    if (m_sharedOffscreen && m_sharedOffscreen->IsGpuIdle()) {
        if (MaterialPreviewStudio::Instance().IsDirty())
            MaterialPreviewStudio::Instance().PumpPreview();
        else if (ThumbnailGenerator::Instance().HasPending() && ++s_thumbnailSkipCounter >= 2) {
            s_thumbnailSkipCounter = 0;
            ThumbnailGenerator::Instance().PumpOne();
        }
    }

    Registry& reg = m_gameLayer ? m_gameLayer->GetRegistry() : m_emptyRegistry;
    RenderContext rc = m_renderPipeline->BeginFrame(reg);
    m_renderQueue.Clear();

    if (m_gameLayer) {
        m_gameLayer->Render(rc, m_renderQueue);
    }

    // ReflectionProbeBaker is DX11-only; skip in DX12 mode to avoid crash
    if (m_probeBaker && m_gameLayer && Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        m_probeBaker->BakeAllDirtyProbes(m_gameLayer->GetRegistry(), m_renderQueue, rc);
    }
    m_renderPipeline->Execute(m_renderQueue, rc);

    static uint64_t s_perfLogFrame = 0;
    if ((s_perfLogFrame++ % 120ull) == 0ull) {
        LOG_INFO(
            "[DX12Perf] sceneUpload=%.3fms fg(setup=%.3f compile=%.3f execute=%.3f) submit=%.3fms "
            "extract=%.3fms visible=%.3fms instance=%.3fms indirect=%.3fms "
            "opaque=%u transparent=%u batches=%u visibleBatches=%u visibleInstances=%u "
            "preparedDraw=%u preparedSkinned=%u matResolves=%u maxBatch=%u "
            "packetGrow=(%u,%u) batchGrow=%u visibleScratchGrow=%u preparedGrow=(inst:%u,batch:%u) "
            "indirectGrow=(gpu:%u,scratch:%u,args:%u,meta:%u) split=(nonSkin:%u,skin:%u)",
            rc.prepMetrics.sceneUploadMs,
            rc.prepMetrics.frameGraphSetupMs,
            rc.prepMetrics.frameGraphCompileMs,
            rc.prepMetrics.frameGraphExecuteMs,
            rc.prepMetrics.submitFrameMs,
            m_renderQueue.metrics.meshExtractMs,
            rc.prepMetrics.visibleExtractMs,
            rc.prepMetrics.instanceBuildMs,
            rc.prepMetrics.indirectBuildMs,
            m_renderQueue.metrics.opaquePacketCount,
            m_renderQueue.metrics.transparentPacketCount,
            m_renderQueue.metrics.opaqueBatchCount,
            rc.prepMetrics.visibleBatchCount,
            rc.prepMetrics.visibleInstanceCount,
            rc.prepMetrics.preparedIndirectCount,
            rc.prepMetrics.preparedSkinnedCount,
            m_renderQueue.metrics.materialResolveCount,
            m_renderQueue.metrics.maxInstancesPerBatch,
            m_renderQueue.metrics.opaquePacketVectorGrowths,
            m_renderQueue.metrics.transparentPacketVectorGrowths,
            m_renderQueue.metrics.opaqueBatchVectorGrowths,
            rc.prepMetrics.visibleScratchVectorGrowths,
            rc.prepMetrics.preparedInstanceVectorGrowths,
            rc.prepMetrics.preparedBatchVectorGrowths,
            rc.prepMetrics.indirectBufferReallocs,
            rc.prepMetrics.indirectScratchVectorGrowths,
            rc.prepMetrics.drawArgsVectorGrowths,
            rc.prepMetrics.metadataVectorGrowths,
            rc.prepMetrics.nonSkinnedCommandCount,
            rc.prepMetrics.skinnedCommandCount);
    }

    // ★ DX12: FrameGraph 完了後に SceneColor → DisplayColor のトーンマップBlit
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        m_renderPipeline->BlitSceneToDisplay(rc);
    }

    m_renderPipeline->EndFrame(rc);

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto transitionFramebufferForUi = [&](FrameBufferId id, size_t colorCount, bool includeDepth) {
            FrameBuffer* fb = Graphics::Instance().GetFrameBuffer(id);
            if (!fb) return;

            for (size_t i = 0; i < colorCount; ++i) {
                ITexture* color = fb->GetColorTexture(i);
                if (color) {
                    rc.commandList->TransitionBarrier(color, ResourceState::ShaderResource);
                }
            }

            if (includeDepth) {
                ITexture* depth = fb->GetDepthTexture();
                if (depth) {
                    rc.commandList->TransitionBarrier(depth, ResourceState::ShaderResource);
                }
            }
        };

        transitionFramebufferForUi(FrameBufferId::Scene, 1, false);
        transitionFramebufferForUi(FrameBufferId::Display, 1, false);
        transitionFramebufferForUi(FrameBufferId::GBuffer, 4, true);
    }

    if (m_editorLayer) m_editorLayer->RenderUI();

    TextureSnapshotState& displaySnapshot = GetDisplaySnapshotState();
    TextureSnapshotState& sceneSnapshot = GetSceneSnapshotState();
    TextureSnapshotState& sceneOpaqueSnapshot = GetSceneOpaqueSnapshotState();
    TextureSnapshotState& gbuffer0Snapshot = GetGBuffer0SnapshotState();
    TextureSnapshotState& gbuffer1Snapshot = GetGBuffer1SnapshotState();
    TextureSnapshotState& gbuffer2Snapshot = GetGBuffer2SnapshotState();
    const bool shouldCaptureDisplay = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && !displaySnapshot.logged;
    const bool shouldCaptureScene = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && !sceneSnapshot.logged;
    const bool shouldCaptureSceneOpaque = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && !sceneOpaqueSnapshot.logged && !m_renderQueue.opaquePackets.empty();
    const bool shouldCaptureGBuffer0 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && !gbuffer0Snapshot.logged && !m_renderQueue.opaquePackets.empty() && rc.debugGBuffer0;
    const bool shouldCaptureGBuffer1 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && !gbuffer1Snapshot.logged && !m_renderQueue.opaquePackets.empty() && rc.debugGBuffer1;
    const bool shouldCaptureGBuffer2 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && !gbuffer2Snapshot.logged && !m_renderQueue.opaquePackets.empty() && rc.debugGBuffer2;

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
        dx12Cmd->FlushResourceBarriers();
        ImGuiRenderer::RenderDX12(dx12Cmd->GetNativeCommandList());
        dx12Cmd->RestoreDescriptorHeap();  // ImGui がヒープを切り替えるため復元
        if (shouldCaptureDisplay) {
            FrameBuffer* displayBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::Display);
            auto* displayTexture = displayBuffer ? dynamic_cast<DX12Texture*>(displayBuffer->GetColorTexture(0)) : nullptr;
            ScheduleTextureSnapshot(dx12Cmd, displayTexture, displaySnapshot);
        }
        if (shouldCaptureScene) {
            FrameBuffer* sceneBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::Scene);
            auto* sceneTexture = sceneBuffer ? dynamic_cast<DX12Texture*>(sceneBuffer->GetColorTexture(0)) : nullptr;
            ScheduleTextureSnapshot(dx12Cmd, sceneTexture, sceneSnapshot);
        }
        if (shouldCaptureSceneOpaque) {
            FrameBuffer* sceneBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::Scene);
            auto* sceneTexture = sceneBuffer ? dynamic_cast<DX12Texture*>(sceneBuffer->GetColorTexture(0)) : nullptr;
            ScheduleTextureSnapshot(dx12Cmd, sceneTexture, sceneOpaqueSnapshot);
        }
        if (shouldCaptureGBuffer0) {
            auto* gbuffer0 = dynamic_cast<DX12Texture*>(rc.debugGBuffer0);
            ScheduleTextureSnapshot(dx12Cmd, gbuffer0, gbuffer0Snapshot);
        }
        if (shouldCaptureGBuffer1) {
            auto* gbuffer1 = dynamic_cast<DX12Texture*>(rc.debugGBuffer1);
            ScheduleTextureSnapshot(dx12Cmd, gbuffer1, gbuffer1Snapshot);
        }
        if (shouldCaptureGBuffer2) {
            auto* gbuffer2 = dynamic_cast<DX12Texture*>(rc.debugGBuffer2);
            ScheduleTextureSnapshot(dx12Cmd, gbuffer2, gbuffer2Snapshot);
        }
    }

    m_renderPipeline->SubmitFrame(rc);

    // D3D12 デバッグメッセージをログに出力（初回フレームのみ）
    if (kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        Graphics::Instance().GetDX12Device()->FlushDebugMessages();
    }

    if (shouldCaptureDisplay || shouldCaptureScene || shouldCaptureSceneOpaque || shouldCaptureGBuffer0 || shouldCaptureGBuffer1 || shouldCaptureGBuffer2) {
        Graphics::Instance().GetDX12Device()->WaitForGPU();
        LogTextureSnapshot("DisplaySnapshot", displaySnapshot);
        LogTextureSnapshot("SceneSnapshot", sceneSnapshot);
        LogTextureSnapshot("SceneOpaqueSnapshot", sceneOpaqueSnapshot);
        LogTextureSnapshot("GBuffer0Snapshot", gbuffer0Snapshot);
        LogTextureSnapshot("GBuffer1Snapshot", gbuffer1Snapshot);
        LogTextureSnapshot("GBuffer2Snapshot", gbuffer2Snapshot);
        LogSceneOverGBufferMask(gbuffer0Snapshot, sceneOpaqueSnapshot);
    }
}

void EngineKernel::Play()
{
    if (mode == EngineMode::Editor) {
        mode = EngineMode::Play;
    }
}

void EngineKernel::Stop()
{
    if (mode == EngineMode::Play || mode == EngineMode::Pause) {
        mode = EngineMode::Editor;
    }
}

void EngineKernel::Pause()
{
    if (mode == EngineMode::Play) mode = EngineMode::Pause;
    else if (mode == EngineMode::Pause) mode = EngineMode::Play;
}
