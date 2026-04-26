//#include "EngineKernel.h"
//#include "Audio/AudioWorldSystem.h"
//#include "Graphics.h"
//#include "Layer/GameLayer.h"
//#include "Layer/EditorLayer.h"
//#include <imgui.h>
//#include "Icon/IconFontManager.h"
//#include "Asset/ThumbnailGenerator.h"
//#include "RenderPass/DrawObjectsPass.h"
//#include "RenderPass/ExtractVisibleInstancesPass.h"
//#include "RenderPass/BuildInstanceBufferPass.h"
//#include "RenderPass/BuildIndirectCommandPass.h"
//#include "RenderPass/ComputeCullingPass.h"
//#include "RenderPass/ShadowPass.h"
//#include <RenderPass\SkyboxPass.h>
//#include <RenderPass\GBufferPass.h>
//#include <RenderPass\ForwardTransparentPass.h>
//#include <RenderPass\EffectMeshPass.h>
//#include <RenderPass\EffectParticlePass.h>
//#include <RenderPass\DeferredLightingPass.h>
//#include <RenderPass\GTAOPass.h>
//#include "RenderPass/SSGIPass.h"
//#include "Render/GlobalRootSignature.h"
//#include <RenderPass\VolumetricFogPass.h>
//#include <RenderPass\SSRPass.h>
//#include "RenderPass/FinalBlitPass.h"
//#include <Material\MaterialPreviewStudio.h>
//#include "RHI/IResourceFactory.h"
//#include "ImGuiRenderer.h"
//#include "RHI/DX11/DX11Texture.h"
//#include "RHI/DX12/DX12CommandList.h"
//#include "RHI/DX12/DX12Texture.h"
//#include "Console/Logger.h"
//#include "Input/SDLInputBackend.h"
//#include "Model/Model.h"
//#include "PlayerEditor/PlayerModelPreviewStudio.h"
//#include <wrl/client.h>
//#include <cfloat>
//#include <DirectXCollision.h>
//#include <algorithm>
//#include <cstring>
//#include <fstream>
//#include "Gameplay/TimelineShakeSystem.h"
//
//namespace {
//    constexpr bool kEnableDx12RuntimeDiagnostics = false;
//    constexpr uint32_t kDiagnosticStartFrame = 1;
//    constexpr uint32_t kMaxDiagnosticFrames = 8;
//    constexpr const char* kPhase4DiagPath = "C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Saved/Logs/phase4_diag.txt";
//
//    struct TextureSnapshotState {
//        Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
//        UINT64 bufferSize = 0;
//        UINT width = 0;
//        UINT height = 0;
//        UINT rowPitch = 0;
//        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
//        bool pending = false;
//        uint32_t captureCount = 0;
//        std::vector<float> prevRgb;
//        std::vector<float> latestRgb;
//    };
//
//    bool CopyTextureResource(ICommandList* commandList, ITexture* source, ITexture* destination)
//    {
//        if (!commandList || !source || !destination) {
//            return false;
//        }
//
//        if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
//            auto* dx12Source = dynamic_cast<DX12Texture*>(source);
//            auto* dx12Destination = dynamic_cast<DX12Texture*>(destination);
//            auto* dx12CommandList = dynamic_cast<DX12CommandList*>(commandList);
//            if (!dx12Source || !dx12Destination || !dx12CommandList) {
//                return false;
//            }
//
//            commandList->TransitionBarrier(source, ResourceState::CopySource);
//            commandList->TransitionBarrier(destination, ResourceState::CopyDest);
//            dx12CommandList->FlushResourceBarriers();
//            dx12CommandList->GetNativeCommandList()->CopyResource(
//                dx12Destination->GetNativeResource(),
//                dx12Source->GetNativeResource());
//            return true;
//        }
//
//        auto* dx11Source = dynamic_cast<DX11Texture*>(source);
//        auto* dx11Destination = dynamic_cast<DX11Texture*>(destination);
//        ID3D11DeviceContext* deviceContext = commandList->GetNativeContext();
//        if (!dx11Source || !dx11Destination || !deviceContext) {
//            return false;
//        }
//
//        deviceContext->CopyResource(
//            dx11Destination->GetNativeResource(),
//            dx11Source->GetNativeResource());
//        return true;
//    }
//
//    float HalfToFloat(uint16_t value) {
//        const uint32_t sign = static_cast<uint32_t>(value & 0x8000u) << 16;
//        uint32_t exponent = (value >> 10) & 0x1Fu;
//        uint32_t mantissa = value & 0x03FFu;
//
//        if (exponent == 0) {
//            if (mantissa == 0) {
//                uint32_t bits = sign;
//                float result;
//                memcpy(&result, &bits, sizeof(result));
//                return result;
//            }
//
//            while ((mantissa & 0x0400u) == 0) {
//                mantissa <<= 1;
//                --exponent;
//            }
//            ++exponent;
//            mantissa &= ~0x0400u;
//        } else if (exponent == 31) {
//            uint32_t bits = sign | 0x7F800000u | (mantissa << 13);
//            float result;
//            memcpy(&result, &bits, sizeof(result));
//            return result;
//        }
//
//        exponent = exponent + (127 - 15);
//        uint32_t bits = sign | (exponent << 23) | (mantissa << 13);
//        float result;
//        memcpy(&result, &bits, sizeof(result));
//        return result;
//    }
//
//    void AppendPhase4Diag(const std::string& line)
//    {
//        static bool cleared = false;
//        const std::filesystem::path path(kPhase4DiagPath);
//        std::filesystem::create_directories(path.parent_path());
//        std::ofstream file(path, cleared ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc));
//        cleared = true;
//        if (file.is_open()) {
//            file << line << '\n';
//        }
//    }
//
//    TextureSnapshotState& GetDisplaySnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    TextureSnapshotState& GetBackBufferSnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    TextureSnapshotState& GetSceneSnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    TextureSnapshotState& GetSceneOpaqueSnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    TextureSnapshotState& GetGBuffer0SnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    TextureSnapshotState& GetGBuffer1SnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    TextureSnapshotState& GetGBuffer2SnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    TextureSnapshotState& GetGBufferDepthSnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    TextureSnapshotState& GetShadowMapSnapshotState() {
//        static TextureSnapshotState state;
//        return state;
//    }
//
//    void ReadSnapshotPixel(const TextureSnapshotState& state, const uint8_t* bytes, UINT x, UINT y,
//        double& r, double& g, double& b, double& a)
//    {
//        r = 0.0;
//        g = 0.0;
//        b = 0.0;
//        a = 1.0;
//
//        const uint8_t* row = bytes + static_cast<size_t>(y) * state.rowPitch;
//        if (state.format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
//            const auto* pixel = reinterpret_cast<const uint16_t*>(row + x * 8);
//            r = HalfToFloat(pixel[0]);
//            g = HalfToFloat(pixel[1]);
//            b = HalfToFloat(pixel[2]);
//            a = HalfToFloat(pixel[3]);
//            return;
//        }
//
//        if (state.format == DXGI_FORMAT_R32G32B32A32_FLOAT) {
//            const auto* pixel = reinterpret_cast<const float*>(row + x * 16);
//            r = pixel[0];
//            g = pixel[1];
//            b = pixel[2];
//            a = pixel[3];
//            return;
//        }
//
//        if (state.format == DXGI_FORMAT_D32_FLOAT ||
//            state.format == DXGI_FORMAT_R32_FLOAT ||
//            state.format == DXGI_FORMAT_R32_TYPELESS) {
//            const auto* pixel = reinterpret_cast<const float*>(row + x * 4);
//            r = pixel[0];
//            g = pixel[0];
//            b = pixel[0];
//            a = 1.0;
//            return;
//        }
//
//        const uint8_t* pixel = row + x * 4;
//        r = pixel[0];
//        g = pixel[1];
//        b = pixel[2];
//        a = pixel[3];
//    }
//
//    void WriteSnapshotBitmap(const char* label, const TextureSnapshotState& state, const uint8_t* bytes)
//    {
//        if (!label || !bytes || state.width == 0 || state.height == 0) {
//            return;
//        }
//
//        std::string filePath = "Saved/Logs/";
//        filePath += label;
//        filePath += ".bmp";
//
//        const uint32_t width = state.width;
//        const uint32_t height = state.height;
//        const uint32_t rowStride = (width * 3u + 3u) & ~3u;
//        const uint32_t pixelDataSize = rowStride * height;
//        const uint32_t fileSize = 54u + pixelDataSize;
//
//        std::vector<uint8_t> bmp(fileSize, 0);
//        bmp[0] = 'B';
//        bmp[1] = 'M';
//        memcpy(&bmp[2], &fileSize, sizeof(fileSize));
//        const uint32_t pixelOffset = 54u;
//        memcpy(&bmp[10], &pixelOffset, sizeof(pixelOffset));
//        const uint32_t dibSize = 40u;
//        memcpy(&bmp[14], &dibSize, sizeof(dibSize));
//        memcpy(&bmp[18], &width, sizeof(width));
//        memcpy(&bmp[22], &height, sizeof(height));
//        const uint16_t planes = 1u;
//        const uint16_t bpp = 24u;
//        memcpy(&bmp[26], &planes, sizeof(planes));
//        memcpy(&bmp[28], &bpp, sizeof(bpp));
//        memcpy(&bmp[34], &pixelDataSize, sizeof(pixelDataSize));
//
//        const bool isGBuffer1 = strstr(label, "GBuffer1") != nullptr;
//
//        for (uint32_t y = 0; y < height; ++y) {
//            uint8_t* dstRow = bmp.data() + pixelOffset + (height - 1u - y) * rowStride;
//            for (uint32_t x = 0; x < width; ++x) {
//                double r = 0.0, g = 0.0, b = 0.0, a = 1.0;
//                ReadSnapshotPixel(state, bytes, x, y, r, g, b, a);
//
//                if (isGBuffer1) {
//                    r = r * 0.5 + 0.5;
//                    g = g * 0.5 + 0.5;
//                    b = b * 0.5 + 0.5;
//                }
//
//                r = std::clamp(r, 0.0, 1.0);
//                g = std::clamp(g, 0.0, 1.0);
//                b = std::clamp(b, 0.0, 1.0);
//
//                dstRow[x * 3 + 0] = static_cast<uint8_t>(b * 255.0);
//                dstRow[x * 3 + 1] = static_cast<uint8_t>(g * 255.0);
//                dstRow[x * 3 + 2] = static_cast<uint8_t>(r * 255.0);
//            }
//        }
//
//        std::ofstream ofs(filePath, std::ios::binary);
//        if (!ofs) {
//            return;
//        }
//        ofs.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
//    }
//
//    void ScheduleTextureSnapshot(DX12CommandList* commandList, DX12Texture* texture, TextureSnapshotState& state) {
//        if (!commandList || !texture || state.pending || state.captureCount >= kMaxDiagnosticFrames) {
//            return;
//        }
//
//        auto* device = Graphics::Instance().GetDX12Device();
//        if (!device) {
//            return;
//        }
//
//        auto* nativeTexture = texture->GetNativeResource();
//        D3D12_RESOURCE_DESC desc = nativeTexture->GetDesc();
//
//        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
//        UINT numRows = 0;
//        UINT64 rowSizeInBytes = 0;
//        UINT64 totalBytes = 0;
//        device->GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);
//
//        if (!state.readbackBuffer || state.bufferSize < totalBytes) {
//            D3D12_HEAP_PROPERTIES heapProps = {};
//            heapProps.Type = D3D12_HEAP_TYPE_READBACK;
//
//            D3D12_RESOURCE_DESC bufferDesc = {};
//            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
//            bufferDesc.Width = totalBytes;
//            bufferDesc.Height = 1;
//            bufferDesc.DepthOrArraySize = 1;
//            bufferDesc.MipLevels = 1;
//            bufferDesc.SampleDesc.Count = 1;
//            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
//
//            state.readbackBuffer.Reset();
//            HRESULT hr = device->GetDevice()->CreateCommittedResource(
//                &heapProps,
//                D3D12_HEAP_FLAG_NONE,
//                &bufferDesc,
//                D3D12_RESOURCE_STATE_COPY_DEST,
//                nullptr,
//                IID_PPV_ARGS(&state.readbackBuffer));
//            if (FAILED(hr)) {
//                LOG_ERROR("[Snapshot] Failed to create readback buffer hr=0x%08X", static_cast<unsigned int>(hr));
//                return;
//            }
//            state.bufferSize = totalBytes;
//        }
//
//        state.width = static_cast<UINT>(desc.Width);
//        state.height = desc.Height;
//        state.rowPitch = footprint.Footprint.RowPitch;
//        state.format = desc.Format;
//
//        auto* nativeCommandList = commandList->GetNativeCommandList();
//        const ResourceState originalState = texture->GetCurrentState();
//        auto toD3D12State = [](ResourceState stateValue) -> D3D12_RESOURCE_STATES {
//            switch (stateValue) {
//            case ResourceState::Common:          return D3D12_RESOURCE_STATE_COMMON;
//            case ResourceState::RenderTarget:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
//            case ResourceState::DepthWrite:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
//            case ResourceState::DepthRead:       return D3D12_RESOURCE_STATE_DEPTH_READ;
//            case ResourceState::ShaderResource:  return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
//            case ResourceState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
//            case ResourceState::CopySource:      return D3D12_RESOURCE_STATE_COPY_SOURCE;
//            case ResourceState::CopyDest:        return D3D12_RESOURCE_STATE_COPY_DEST;
//            case ResourceState::Present:         return D3D12_RESOURCE_STATE_PRESENT;
//            default:                             return D3D12_RESOURCE_STATE_COMMON;
//            }
//        };
//        const D3D12_RESOURCE_STATES originalD3DState = toD3D12State(originalState);
//
//        if (originalD3DState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
//            D3D12_RESOURCE_BARRIER toCopy = {};
//            toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
//            toCopy.Transition.pResource = nativeTexture;
//            toCopy.Transition.StateBefore = originalD3DState;
//            toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
//            toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
//            nativeCommandList->ResourceBarrier(1, &toCopy);
//        }
//
//        D3D12_TEXTURE_COPY_LOCATION dst = {};
//        dst.pResource = state.readbackBuffer.Get();
//        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
//        dst.PlacedFootprint = footprint;
//
//        D3D12_TEXTURE_COPY_LOCATION src = {};
//        src.pResource = nativeTexture;
//        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
//        src.SubresourceIndex = 0;
//
//        nativeCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
//
//        if (originalD3DState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
//            D3D12_RESOURCE_BARRIER restore = {};
//            restore.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
//            restore.Transition.pResource = nativeTexture;
//            restore.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
//            restore.Transition.StateAfter = originalD3DState;
//            restore.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
//            nativeCommandList->ResourceBarrier(1, &restore);
//        }
//
//        state.pending = true;
//    }
//
//    void LogTextureSnapshot(const char* label, TextureSnapshotState& state) {
//        if (!state.pending || !state.readbackBuffer) {
//            return;
//        }
//
//        void* mapped = nullptr;
//        D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(state.bufferSize) };
//        HRESULT hr = state.readbackBuffer->Map(0, &readRange, &mapped);
//        if (FAILED(hr) || !mapped) {
//            LOG_ERROR("[%s] Failed to map readback buffer hr=0x%08X", label, static_cast<unsigned int>(hr));
//            state.pending = false;
//            ++state.captureCount;
//            return;
//        }
//
//        const auto* bytes = static_cast<const uint8_t*>(mapped);
//        double sumR = 0.0, sumG = 0.0, sumB = 0.0, sumA = 0.0;
//        uint64_t nonBlack = 0;
//        double centerR = 0.0, centerG = 0.0, centerB = 0.0, centerA = 0.0;
//        std::vector<float> currentRgb;
//        currentRgb.reserve(static_cast<size_t>(state.width) * static_cast<size_t>(state.height) * 3u);
//
//        for (UINT y = 0; y < state.height; ++y) {
//            for (UINT x = 0; x < state.width; ++x) {
//                double r = 0.0, g = 0.0, b = 0.0, a = 1.0;
//                ReadSnapshotPixel(state, bytes, x, y, r, g, b, a);
//
//                sumR += r;
//                sumG += g;
//                sumB += b;
//                sumA += a;
//                currentRgb.push_back(static_cast<float>(r));
//                currentRgb.push_back(static_cast<float>(g));
//                currentRgb.push_back(static_cast<float>(b));
//                if (r > 0.0001 || g > 0.0001 || b > 0.0001) {
//                    ++nonBlack;
//                }
//
//                if (x == state.width / 2 && y == state.height / 2) {
//                    centerR = r;
//                    centerG = g;
//                    centerB = b;
//                    centerA = a;
//                }
//            }
//        }
//
//        const double invPixelCount = (state.width && state.height) ? (1.0 / static_cast<double>(state.width * state.height)) : 0.0;
//        LOG_INFO("[%s] format=%d avgRGBA=(%.4f, %.4f, %.4f, %.4f) centerRGBA=(%.4f, %.4f, %.4f, %.4f) nonBlack=%llu/%u",
//            label,
//            static_cast<int>(state.format),
//            sumR * invPixelCount,
//            sumG * invPixelCount,
//            sumB * invPixelCount,
//            sumA * invPixelCount,
//            centerR, centerG, centerB, centerA,
//            static_cast<unsigned long long>(nonBlack),
//            state.width * state.height);
//        {
//            char buffer[512];
//            snprintf(buffer, sizeof(buffer),
//                "[%s] format=%d avgRGBA=(%.4f, %.4f, %.4f, %.4f) centerRGBA=(%.4f, %.4f, %.4f, %.4f) nonBlack=%llu/%u",
//                label,
//                static_cast<int>(state.format),
//                sumR * invPixelCount,
//                sumG * invPixelCount,
//                sumB * invPixelCount,
//                sumA * invPixelCount,
//                centerR, centerG, centerB, centerA,
//                static_cast<unsigned long long>(nonBlack),
//                state.width * state.height);
//            AppendPhase4Diag(buffer);
//        }
//
//        if (!state.prevRgb.empty() && state.prevRgb.size() == currentRgb.size()) {
//            double diffSum = 0.0;
//            double diffActiveSum = 0.0;
//            uint64_t activeCount = 0;
//            for (size_t i = 0; i < currentRgb.size(); i += 3) {
//                const double dr = std::abs(static_cast<double>(currentRgb[i + 0]) - static_cast<double>(state.prevRgb[i + 0]));
//                const double dg = std::abs(static_cast<double>(currentRgb[i + 1]) - static_cast<double>(state.prevRgb[i + 1]));
//                const double db = std::abs(static_cast<double>(currentRgb[i + 2]) - static_cast<double>(state.prevRgb[i + 2]));
//                const double diff = (dr + dg + db) / 3.0;
//                diffSum += diff;
//                if (currentRgb[i + 0] > 0.0001f || currentRgb[i + 1] > 0.0001f || currentRgb[i + 2] > 0.0001f ||
//                    state.prevRgb[i + 0] > 0.0001f || state.prevRgb[i + 1] > 0.0001f || state.prevRgb[i + 2] > 0.0001f) {
//                    diffActiveSum += diff;
//                    ++activeCount;
//                }
//            }
//
//            const double invCount = currentRgb.empty() ? 0.0 : (1.0 / (static_cast<double>(currentRgb.size()) / 3.0));
//            const double invActiveCount = activeCount ? (1.0 / static_cast<double>(activeCount)) : 0.0;
//            LOG_INFO("[%sDiff] frame=%u avgAbsRGB=%.6f activeAvgAbsRGB=%.6f activePixels=%llu",
//                label,
//                state.captureCount,
//                diffSum * invCount,
//                diffActiveSum * invActiveCount,
//                static_cast<unsigned long long>(activeCount));
//            {
//                char buffer[256];
//                snprintf(buffer, sizeof(buffer),
//                    "[%sDiff] frame=%u avgAbsRGB=%.6f activeAvgAbsRGB=%.6f activePixels=%llu",
//                    label,
//                    state.captureCount,
//                    diffSum * invCount,
//                    diffActiveSum * invActiveCount,
//                    static_cast<unsigned long long>(activeCount));
//                AppendPhase4Diag(buffer);
//            }
//        }
//
//        D3D12_RANGE writeRange = { 0, 0 };
//        state.readbackBuffer->Unmap(0, &writeRange);
//        state.pending = false;
//        state.latestRgb = currentRgb;
//        state.prevRgb = std::move(currentRgb);
//        ++state.captureCount;
//    }
//
//    void LogMaskedSnapshotDiff(const char* label, const TextureSnapshotState& maskState, const TextureSnapshotState& targetState)
//    {
//        if (!label) {
//            return;
//        }
//        if (maskState.latestRgb.empty() || targetState.latestRgb.empty() || targetState.prevRgb.empty()) {
//            return;
//        }
//
//        const size_t maskPixels = maskState.latestRgb.size() / 3u;
//        const size_t targetPixels = targetState.latestRgb.size() / 3u;
//        if (maskPixels == 0 || targetPixels == 0 || targetPixels != (targetState.prevRgb.size() / 3u)) {
//            return;
//        }
//
//        const UINT width = (maskState.width < targetState.width) ? maskState.width : targetState.width;
//        const UINT height = (maskState.height < targetState.height) ? maskState.height : targetState.height;
//        if (width == 0 || height == 0) {
//            return;
//        }
//
//        double diffSum = 0.0;
//        uint64_t count = 0;
//        for (UINT y = 0; y < height; ++y) {
//            const size_t maskRow = static_cast<size_t>(y) * maskState.width;
//            const size_t targetRow = static_cast<size_t>(y) * targetState.width;
//            for (UINT x = 0; x < width; ++x) {
//                const size_t maskIndex = (maskRow + x) * 3u;
//                if (maskIndex + 2 >= maskState.latestRgb.size()) {
//                    continue;
//                }
//                const float maskR = maskState.latestRgb[maskIndex + 0];
//                const float maskG = maskState.latestRgb[maskIndex + 1];
//                const float maskB = maskState.latestRgb[maskIndex + 2];
//                if (maskR <= 0.0001f && maskG <= 0.0001f && maskB <= 0.0001f) {
//                    continue;
//                }
//
//                const size_t targetIndex = (targetRow + x) * 3u;
//                if (targetIndex + 2 >= targetState.latestRgb.size() || targetIndex + 2 >= targetState.prevRgb.size()) {
//                    continue;
//                }
//
//                const double dr = std::abs(static_cast<double>(targetState.latestRgb[targetIndex + 0]) - static_cast<double>(targetState.prevRgb[targetIndex + 0]));
//                const double dg = std::abs(static_cast<double>(targetState.latestRgb[targetIndex + 1]) - static_cast<double>(targetState.prevRgb[targetIndex + 1]));
//                const double db = std::abs(static_cast<double>(targetState.latestRgb[targetIndex + 2]) - static_cast<double>(targetState.prevRgb[targetIndex + 2]));
//                diffSum += (dr + dg + db) / 3.0;
//                ++count;
//            }
//        }
//
//        if (count == 0) {
//            return;
//        }
//
//        LOG_INFO("[%s] frame=%u avgAbsRGB=%.6f pixels=%llu",
//            label,
//            targetState.captureCount - 1u,
//            diffSum / static_cast<double>(count),
//            static_cast<unsigned long long>(count));
//    }
//
//    void LogSceneOverGBufferMask(TextureSnapshotState& maskState, TextureSnapshotState& sceneState) {
//        static uint32_t loggedCount = 0;
//        if (loggedCount >= kMaxDiagnosticFrames || !maskState.readbackBuffer || !sceneState.readbackBuffer ||
//            maskState.captureCount == 0 || sceneState.captureCount == 0) {
//            return;
//        }
//
//        void* maskMapped = nullptr;
//        void* sceneMapped = nullptr;
//        D3D12_RANGE maskRange = { 0, static_cast<SIZE_T>(maskState.bufferSize) };
//        D3D12_RANGE sceneRange = { 0, static_cast<SIZE_T>(sceneState.bufferSize) };
//        if (FAILED(maskState.readbackBuffer->Map(0, &maskRange, &maskMapped)) || !maskMapped) {
//            return;
//        }
//        if (FAILED(sceneState.readbackBuffer->Map(0, &sceneRange, &sceneMapped)) || !sceneMapped) {
//            D3D12_RANGE writeRange = { 0, 0 };
//            maskState.readbackBuffer->Unmap(0, &writeRange);
//            return;
//        }
//
//        const auto* maskBytes = static_cast<const uint8_t*>(maskMapped);
//        const auto* sceneBytes = static_cast<const uint8_t*>(sceneMapped);
//        const UINT width = (maskState.width < sceneState.width) ? maskState.width : sceneState.width;
//        const UINT height = (maskState.height < sceneState.height) ? maskState.height : sceneState.height;
//
//        double sumMaskR = 0.0, sumMaskG = 0.0, sumMaskB = 0.0;
//        double sumSceneR = 0.0, sumSceneG = 0.0, sumSceneB = 0.0;
//        uint64_t count = 0;
//
//        for (UINT y = 0; y < height; ++y) {
//            for (UINT x = 0; x < width; ++x) {
//                double maskR, maskG, maskB, maskA;
//                ReadSnapshotPixel(maskState, maskBytes, x, y, maskR, maskG, maskB, maskA);
//                if (maskR <= 0.0001 && maskG <= 0.0001 && maskB <= 0.0001) {
//                    continue;
//                }
//
//                double sceneR, sceneG, sceneB, sceneA;
//                ReadSnapshotPixel(sceneState, sceneBytes, x, y, sceneR, sceneG, sceneB, sceneA);
//                sumMaskR += maskR;
//                sumMaskG += maskG;
//                sumMaskB += maskB;
//                sumSceneR += sceneR;
//                sumSceneG += sceneG;
//                sumSceneB += sceneB;
//                ++count;
//            }
//        }
//
//        D3D12_RANGE writeRange = { 0, 0 };
//        sceneState.readbackBuffer->Unmap(0, &writeRange);
//        maskState.readbackBuffer->Unmap(0, &writeRange);
//
//        if (count == 0) {
//            return;
//        }
//
//        const double invCount = 1.0 / static_cast<double>(count);
//        LOG_INFO("[SceneOverAlbedoMask] maskAvgRGB=(%.4f, %.4f, %.4f) sceneAvgRGB=(%.4f, %.4f, %.4f) count=%llu",
//            sumMaskR * invCount,
//            sumMaskG * invCount,
//            sumMaskB * invCount,
//            sumSceneR * invCount,
//            sumSceneG * invCount,
//            sumSceneB * invCount,
//            static_cast<unsigned long long>(count));
//        ++loggedCount;
//    }
//
//    void LogDepthOverGBufferMask(const TextureSnapshotState& maskState, const TextureSnapshotState& depthState)
//    {
//        static uint32_t loggedCount = 0;
//        if (loggedCount >= kMaxDiagnosticFrames || maskState.latestRgb.empty() || depthState.latestRgb.empty()) {
//            return;
//        }
//
//        const size_t maskPixelCount = maskState.latestRgb.size() / 3u;
//        const size_t depthPixelCount = depthState.latestRgb.size() / 3u;
//        const size_t pixelCount = (std::min)(maskPixelCount, depthPixelCount);
//        if (pixelCount == 0) {
//            return;
//        }
//
//        double minDepth = DBL_MAX;
//        double maxDepth = -DBL_MAX;
//        double sumDepth = 0.0;
//        uint64_t count = 0;
//
//        for (size_t i = 0; i < pixelCount; ++i) {
//            const size_t base = i * 3u;
//            const float maskR = maskState.latestRgb[base + 0];
//            const float maskG = maskState.latestRgb[base + 1];
//            const float maskB = maskState.latestRgb[base + 2];
//            if (maskR <= 0.0001f && maskG <= 0.0001f && maskB <= 0.0001f) {
//                continue;
//            }
//
//            const double depth = static_cast<double>(depthState.latestRgb[base + 0]);
//            minDepth = (std::min)(minDepth, depth);
//            maxDepth = (std::max)(maxDepth, depth);
//            sumDepth += depth;
//            ++count;
//        }
//
//        if (count == 0) {
//            return;
//        }
//
//        LOG_INFO("[DepthOverAlbedoMask] avg=%.6f min=%.6f max=%.6f count=%llu",
//            sumDepth / static_cast<double>(count),
//            minDepth,
//            maxDepth,
//            static_cast<unsigned long long>(count));
//        ++loggedCount;
//    }
//
//    void LogGBufferStatsOverAlbedoMask(
//        const TextureSnapshotState& maskState,
//        const TextureSnapshotState& gbuffer1State,
//        const TextureSnapshotState& depthState)
//    {
//        static uint32_t loggedCount = 0;
//        if (loggedCount >= kMaxDiagnosticFrames ||
//            maskState.latestRgb.empty() ||
//            gbuffer1State.latestRgb.empty() ||
//            depthState.latestRgb.empty()) {
//            return;
//        }
//
//        const size_t pixelCount = (std::min)(
//            maskState.latestRgb.size(),
//            (std::min)(gbuffer1State.latestRgb.size(), depthState.latestRgb.size())) / 3u;
//        if (pixelCount == 0) {
//            return;
//        }
//
//        double minNormalLen = DBL_MAX;
//        double maxNormalLen = -DBL_MAX;
//        double sumNormalLen = 0.0;
//        double minDepth = DBL_MAX;
//        double maxDepth = -DBL_MAX;
//        double sumDepth = 0.0;
//        uint64_t count = 0;
//
//        for (size_t i = 0; i < pixelCount; ++i) {
//            const size_t base = i * 3u;
//            const float maskR = maskState.latestRgb[base + 0];
//            const float maskG = maskState.latestRgb[base + 1];
//            const float maskB = maskState.latestRgb[base + 2];
//            if (maskR <= 0.0001f && maskG <= 0.0001f && maskB <= 0.0001f) {
//                continue;
//            }
//
//            const float nx = gbuffer1State.latestRgb[base + 0];
//            const float ny = gbuffer1State.latestRgb[base + 1];
//            const float nz = gbuffer1State.latestRgb[base + 2];
//            const double normalLen = std::sqrt(
//                static_cast<double>(nx) * static_cast<double>(nx) +
//                static_cast<double>(ny) * static_cast<double>(ny) +
//                static_cast<double>(nz) * static_cast<double>(nz));
//            const double depthValue = static_cast<double>(depthState.latestRgb[base + 0]);
//
//            minNormalLen = (std::min)(minNormalLen, normalLen);
//            maxNormalLen = (std::max)(maxNormalLen, normalLen);
//            sumNormalLen += normalLen;
//            minDepth = (std::min)(minDepth, depthValue);
//            maxDepth = (std::max)(maxDepth, depthValue);
//            sumDepth += depthValue;
//            ++count;
//        }
//
//        if (count == 0) {
//            return;
//        }
//
//        LOG_INFO("[GBufferMaskStats] normalLen(avg=%.6f min=%.6f max=%.6f) depth(avg=%.6f min=%.6f max=%.6f) count=%llu",
//            sumNormalLen / static_cast<double>(count),
//            minNormalLen,
//            maxNormalLen,
//            sumDepth / static_cast<double>(count),
//            minDepth,
//            maxDepth,
//            static_cast<unsigned long long>(count));
//        ++loggedCount;
//    }
//
//    void LogSceneViewPresentationDiff(const EditorLayer* editorLayer, const TextureSnapshotState& sceneState, const TextureSnapshotState& backBufferState)
//    {
//        if (!editorLayer || sceneState.latestRgb.empty() || backBufferState.latestRgb.empty()) {
//            return;
//        }
//
//        const DirectX::XMFLOAT4 rect = editorLayer->GetSceneViewRect();
//        const UINT panelX = rect.x > 0.0f ? static_cast<UINT>(rect.x) : 0u;
//        const UINT panelY = rect.y > 0.0f ? static_cast<UINT>(rect.y) : 0u;
//        const UINT panelW = rect.z > 0.0f ? static_cast<UINT>(rect.z) : 0u;
//        const UINT panelH = rect.w > 0.0f ? static_cast<UINT>(rect.w) : 0u;
//        if (panelW == 0 || panelH == 0) {
//            return;
//        }
//        if (panelX >= backBufferState.width || panelY >= backBufferState.height) {
//            return;
//        }
//
//        const UINT compareW = (std::min)(panelW, backBufferState.width - panelX);
//        const UINT compareH = (std::min)(panelH, backBufferState.height - panelY);
//        if (compareW == 0 || compareH == 0) {
//            return;
//        }
//
//        double diffSum = 0.0;
//        uint64_t count = 0;
//        for (UINT y = 0; y < compareH; ++y) {
//            const double v = (compareH > 1) ? (static_cast<double>(y) / static_cast<double>(compareH - 1)) : 0.0;
//            const UINT sceneY = static_cast<UINT>(v * static_cast<double>((std::max)(1u, sceneState.height) - 1u));
//            for (UINT x = 0; x < compareW; ++x) {
//                const double u = (compareW > 1) ? (static_cast<double>(x) / static_cast<double>(compareW - 1)) : 0.0;
//                const UINT sceneX = static_cast<UINT>(u * static_cast<double>((std::max)(1u, sceneState.width) - 1u));
//
//                const size_t sceneIndex = (static_cast<size_t>(sceneY) * sceneState.width + sceneX) * 3u;
//                const size_t displayIndex = (static_cast<size_t>(panelY + y) * backBufferState.width + (panelX + x)) * 3u;
//                if (sceneIndex + 2 >= sceneState.latestRgb.size() || displayIndex + 2 >= backBufferState.latestRgb.size()) {
//                    continue;
//                }
//
//                const double dr = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 0]) - static_cast<double>(backBufferState.latestRgb[displayIndex + 0]) / 255.0);
//                const double dg = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 1]) - static_cast<double>(backBufferState.latestRgb[displayIndex + 1]) / 255.0);
//                const double db = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 2]) - static_cast<double>(backBufferState.latestRgb[displayIndex + 2]) / 255.0);
//                diffSum += (dr + dg + db) / 3.0;
//                ++count;
//            }
//        }
//
//        if (count == 0) {
//            return;
//        }
//
//        LOG_INFO("[SceneViewPresentationDiff] rect=(%u,%u,%u,%u) scene=%ux%u display=%ux%u avgAbsRGB=%.6f pixels=%llu",
//            panelX, panelY, compareW, compareH,
//            sceneState.width, sceneState.height,
//            backBufferState.width, backBufferState.height,
//            diffSum / static_cast<double>(count),
//            static_cast<unsigned long long>(count));
//    }
//
//    void LogSceneViewModelPresentationDiff(
//        const EditorLayer* editorLayer,
//        const TextureSnapshotState& maskState,
//        const TextureSnapshotState& sceneState,
//        const TextureSnapshotState& backBufferState)
//    {
//        if (!editorLayer || maskState.latestRgb.empty() || sceneState.latestRgb.empty() || backBufferState.latestRgb.empty()) {
//            return;
//        }
//
//        const DirectX::XMFLOAT4 rect = editorLayer->GetSceneViewRect();
//        const UINT panelX = rect.x > 0.0f ? static_cast<UINT>(rect.x) : 0u;
//        const UINT panelY = rect.y > 0.0f ? static_cast<UINT>(rect.y) : 0u;
//        const UINT panelW = rect.z > 0.0f ? static_cast<UINT>(rect.z) : 0u;
//        const UINT panelH = rect.w > 0.0f ? static_cast<UINT>(rect.w) : 0u;
//        if (panelW == 0 || panelH == 0 || panelX >= backBufferState.width || panelY >= backBufferState.height) {
//            return;
//        }
//
//        const UINT compareW = (std::min)(panelW, backBufferState.width - panelX);
//        const UINT compareH = (std::min)(panelH, backBufferState.height - panelY);
//        if (compareW == 0 || compareH == 0 || sceneState.width == 0 || sceneState.height == 0) {
//            return;
//        }
//
//        double diffSum = 0.0;
//        uint64_t count = 0;
//        for (UINT y = 0; y < compareH; ++y) {
//            const double v = (compareH > 1) ? (static_cast<double>(y) / static_cast<double>(compareH - 1)) : 0.0;
//            const UINT sceneY = static_cast<UINT>(v * static_cast<double>((std::max)(1u, sceneState.height) - 1u));
//            for (UINT x = 0; x < compareW; ++x) {
//                const double u = (compareW > 1) ? (static_cast<double>(x) / static_cast<double>(compareW - 1)) : 0.0;
//                const UINT sceneX = static_cast<UINT>(u * static_cast<double>((std::max)(1u, sceneState.width) - 1u));
//
//                const size_t maskIndex = (static_cast<size_t>(sceneY) * maskState.width + sceneX) * 3u;
//                const size_t sceneIndex = (static_cast<size_t>(sceneY) * sceneState.width + sceneX) * 3u;
//                const size_t backIndex = (static_cast<size_t>(panelY + y) * backBufferState.width + (panelX + x)) * 3u;
//                if (maskIndex + 2 >= maskState.latestRgb.size() ||
//                    sceneIndex + 2 >= sceneState.latestRgb.size() ||
//                    backIndex + 2 >= backBufferState.latestRgb.size()) {
//                    continue;
//                }
//
//                const float maskR = maskState.latestRgb[maskIndex + 0];
//                const float maskG = maskState.latestRgb[maskIndex + 1];
//                const float maskB = maskState.latestRgb[maskIndex + 2];
//                if (maskR <= 0.0001f && maskG <= 0.0001f && maskB <= 0.0001f) {
//                    continue;
//                }
//
//                const double dr = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 0]) - static_cast<double>(backBufferState.latestRgb[backIndex + 0]) / 255.0);
//                const double dg = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 1]) - static_cast<double>(backBufferState.latestRgb[backIndex + 1]) / 255.0);
//                const double db = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 2]) - static_cast<double>(backBufferState.latestRgb[backIndex + 2]) / 255.0);
//                diffSum += (dr + dg + db) / 3.0;
//                ++count;
//            }
//        }
//
//        if (count == 0) {
//            return;
//        }
//
//        LOG_INFO("[SceneViewModelPresentationDiff] rect=(%u,%u,%u,%u) avgAbsRGB=%.6f pixels=%llu",
//            panelX, panelY, compareW, compareH,
//            diffSum / static_cast<double>(count),
//            static_cast<unsigned long long>(count));
//    }
//}
//
//EngineKernel::EngineKernel() = default;
//
//EngineKernel::~EngineKernel() = default;
//
//EngineKernel& EngineKernel::Instance()
//{
//    static EngineKernel instance;
//    return instance;
//}
//
//void EngineKernel::RenderPlayerPreviewOffscreen()
//{
//    if (!m_editorLayer) {
//        return;
//    }
//    m_editorLayer->SetPlayerPreviewTexture(nullptr);
//
//    auto& previewStudio = PlayerModelPreviewStudio::Instance();
//    if (!previewStudio.IsReady()) {
//        return;
//    }
//
//    const Model* previewModel = m_editorLayer->GetPlayerEditorPanel().GetPreviewModel();
//    if (!previewModel) {
//        return;
//    }
//    const DirectX::XMFLOAT2 renderSize = m_editorLayer->GetPlayerPreviewRenderSize();
//    const float width = (std::max)(renderSize.x, 1.0f);
//    const float height = (std::max)(renderSize.y, 1.0f);
//    previewStudio.RenderPreview(
//        previewModel,
//        m_editorLayer->GetPlayerPreviewCameraPosition(),
//        m_editorLayer->GetPlayerPreviewCameraTarget(),
//        width / height,
//        m_editorLayer->GetPlayerPreviewCameraFovY(),
//        m_editorLayer->GetPlayerPreviewNearZ(),
//        m_editorLayer->GetPlayerPreviewFarZ(),
//        m_editorLayer->GetPlayerPreviewClearColor(),
//        m_editorLayer->GetPlayerEditorPanel().GetPreviewModelScale());
//    m_editorLayer->SetPlayerPreviewTexture(previewStudio.GetPreviewTexture());
//}
//
//void EngineKernel::Initialize()
//{
//    time = EngineTime();
//    mode = EngineMode::Editor;
//
//    m_renderPipeline = std::make_unique<RenderPipeline>();
//
//    const bool isDX12 = (Graphics::Instance().GetAPI() == GraphicsAPI::DX12);
//    auto* factory = Graphics::Instance().GetResourceFactory();
//
//    m_renderPipeline->AddPass(std::make_shared<ExtractVisibleInstancesPass>());
//    m_renderPipeline->AddPass(std::make_shared<BuildInstanceBufferPass>());
//    m_renderPipeline->AddPass(std::make_shared<BuildIndirectCommandPass>());
//    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
//        m_renderPipeline->AddPass(std::make_shared<ComputeCullingPass>());
//    }
//    m_renderPipeline->AddPass(std::make_shared<ShadowPass>());
//    m_renderPipeline->AddPass(std::make_shared<GBufferPass>());
//    m_renderPipeline->AddPass(std::make_shared<GTAOPass>(factory));
//    m_renderPipeline->AddPass(std::make_shared<SSGIPass>(factory));
//    m_renderPipeline->AddPass(std::make_shared<VolumetricFogPass>(factory));
//    m_renderPipeline->AddPass(std::make_shared<SSRPass>(factory));
//    m_renderPipeline->AddPass(std::make_shared<DeferredLightingPass>(factory));
//    m_renderPipeline->AddPass(std::make_shared<SkyboxPass>());
//    m_renderPipeline->AddPass(std::make_shared<ForwardTransparentPass>());
//    m_renderPipeline->AddPass(std::make_shared<EffectMeshPass>());
//    m_renderPipeline->AddPass(std::make_shared<EffectParticlePass>());
//    if (isDX12) {
//        m_renderPipeline->AddPass(std::make_shared<FinalBlitPass>(factory));
//    }
//
//    m_gameLayer = std::make_unique<GameLayer>();
//    m_gameLayer->Initialize();
//
//    m_audioWorld = std::make_unique<AudioWorldSystem>();
//    if (!m_audioWorld->Initialize()) {
//        LOG_ERROR("[EngineKernel] Failed to initialize AudioWorldSystem.");
//    }
//
//    std::vector<IconFontManager::SizeConfig> configs = {
//        { IconFontSize::Mini,   14.0f },
//        { IconFontSize::Small,  14.0f },
//        { IconFontSize::Medium, 18.0f },
//        { IconFontSize::Large,  24.0f },
//        { IconFontSize::Extra,  64.0f }
//    };
//    IconFontManager::Instance().Setup(configs);
//    m_sharedOffscreen = std::make_unique<OffscreenRenderer>();
//    if (m_sharedOffscreen->Initialize()) {
//        ThumbnailGenerator::Instance().Initialize(m_sharedOffscreen.get());
//        MaterialPreviewStudio::Instance().Initialize(m_sharedOffscreen.get());
//        PlayerModelPreviewStudio::Instance().Initialize(m_sharedOffscreen.get());
//    } else {
//        LOG_ERROR("[EngineKernel] Failed to initialize shared OffscreenRenderer.");
//        ThumbnailGenerator::Instance().Initialize(nullptr);
//        MaterialPreviewStudio::Instance().Initialize(nullptr);
//        PlayerModelPreviewStudio::Instance().Initialize(nullptr);
//    }
//
//    // SDL3 input backend
//    m_inputBackend = std::make_unique<SDLInputBackend>();
//    {
//        HWND hwnd = Graphics::Instance().GetWindowHandle();
//        if (!m_inputBackend->Initialize(hwnd)) {
//            LOG_ERROR("[EngineKernel] Failed to initialize SDL input backend.");
//        }
//    }
//
//    LOG_INFO("[EngineKernel] Initialize API=%s", isDX12 ? "DX12" : "DX11");
//
//    if (isDX12) {
//        m_editorLayer = std::make_unique<EditorLayer>(m_gameLayer.get());
//        m_editorLayer->Initialize();
//        return;
//    }
//
//    ID3D11Device* dx11Dev = Graphics::Instance().GetDevice();
//
//    m_probeBaker = std::make_unique<ReflectionProbeBaker>(dx11Dev);
//    GlobalRootSignature::Instance().Initialize(dx11Dev);
//
//    m_editorLayer = std::make_unique<EditorLayer>(m_gameLayer.get());
//    m_editorLayer->Initialize();
//}
//
//void EngineKernel::Finalize()
//{
//    if (m_inputBackend) {
//        m_inputBackend->Shutdown();
//        m_inputBackend.reset();
//    }
//    if (m_audioWorld) {
//        m_audioWorld->Finalize();
//        m_audioWorld.reset();
//    }
//    if (m_editorLayer) m_editorLayer->Finalize();
//    if (m_gameLayer) m_gameLayer->Finalize();
//}
//
//void EngineKernel::PollInput()
//{
//    if (m_inputBackend) {
//        m_inputBackend->BeginFrame();
//        m_inputQueue.Clear();
//        m_inputBackend->PollEvents(m_inputQueue);
//        m_inputBackend->EndFrame();
//    }
//}
//
//void EngineKernel::Update(float rawDt)
//{
//    time.unscaledDt = rawDt;
//    time.frameCount++;
//
//    if (m_editorLayer) m_editorLayer->Update(time);
//
//    const bool stepThisFrame = m_stepFrameRequested;
//    if (mode == EngineMode::Play || stepThisFrame)
//        time.dt = rawDt * time.timeScale;
//    else
//        time.dt = 0.0f;
//
//    if (m_gameLayer) m_gameLayer->Update(time);
//    if (m_audioWorld && m_gameLayer) {
//        m_audioWorld->Update(m_gameLayer->GetRegistry(), mode);
//    }
//
//
//    if (m_editorLayer) {
//        const bool usePlayerEditorShake =
//            m_gameLayer &&
//            m_editorLayer->IsPlayerWorkspaceActive();
//
//        if (usePlayerEditorShake) {
//            m_editorLayer->SetPlayerEditorCameraShakeOffset(
//                TimelineShakeSystem::GetShakeOffset());
//        }
//        else {
//            m_editorLayer->ClearPlayerEditorCameraShakeOffset();
//        }
//    }
//
//
//    time.totalTime += time.dt;
//    if (stepThisFrame) {
//        m_stepFrameRequested = false;
//        mode = EngineMode::Pause;
//    }
//}
//
//void EngineKernel::Render()
//{
//    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
//        if (auto* dx12Device = Graphics::Instance().GetDX12Device()) {
//            dx12Device->ProcessDeferredFrees();
//        }
//    }
//    if (m_sharedOffscreen) {
//        ImGuiRenderer::ProcessDeferredUnregisters(m_sharedOffscreen->GetCompletedFenceValue());
//    }
//
//    // Priority dispatch: MaterialPreview (active editing) > Thumbnails (background)
//    // 1 job per frame on shared OffscreenRenderer to avoid GPU contention
//    static int s_thumbnailSkipCounter = 0;
//    const bool usePlayerPreviewAsPrimary = false;
//    const bool usePlayerWorkspaceScene = m_editorLayer && m_editorLayer->IsPlayerWorkspaceActive();
//    const bool wantsPlayerPreview = false;
//    if (m_sharedOffscreen && m_sharedOffscreen->IsGpuIdle() && !wantsPlayerPreview) {
//        if (MaterialPreviewStudio::Instance().IsDirty())
//            MaterialPreviewStudio::Instance().PumpPreview();
//        else if (ThumbnailGenerator::Instance().HasPending() && ++s_thumbnailSkipCounter >= 2) {
//            s_thumbnailSkipCounter = 0;
//            ThumbnailGenerator::Instance().PumpOne();
//        }
//    }
//
//    Registry& reg = m_gameLayer ? m_gameLayer->GetRegistry() : m_emptyRegistry;
//    RenderContext rc = m_renderPipeline->BeginFrame(reg);
//    m_renderQueue.Clear();
//
//    if (m_gameLayer) {
//        m_gameLayer->Render(rc, m_renderQueue);
//    }
//
//    // ReflectionProbeBaker is DX11-only; skip in DX12 mode to avoid crash
//    if (m_probeBaker && m_gameLayer && Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
//        m_probeBaker->BakeAllDirtyProbes(m_gameLayer->GetRegistry(), m_renderQueue, rc);
//    }
//    std::vector<RenderPipeline::RenderViewContext> views;
//    if (m_editorLayer) {
//        m_editorLayer->SetPlayerPreviewTexture(nullptr);
//        m_editorLayer->SetEffectPreviewTexture(nullptr);
//        const bool useEffectPreviewAsPrimary = !usePlayerWorkspaceScene && !usePlayerPreviewAsPrimary && m_editorLayer->ShouldRenderEffectPreview();
//        if (!m_editorLayer->HasEditorCameraUserOverride() && !m_editorLayer->HasEditorCameraAutoFramed()) {
//            DirectX::BoundingBox mergedBounds{};
//            bool hasMergedBounds = false;
//            for (const auto& packet : m_renderQueue.opaquePackets) {
//                if (!packet.modelResource) {
//                    continue;
//                }
//                DirectX::BoundingBox worldBounds{};
//                const auto& localBounds = packet.modelResource->GetLocalBounds();
//                DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&packet.worldMatrix);
//                localBounds.Transform(worldBounds, world);
//                if (!hasMergedBounds) {
//                    mergedBounds = worldBounds;
//                    hasMergedBounds = true;
//                } else {
//                    DirectX::BoundingBox::CreateMerged(mergedBounds, mergedBounds, worldBounds);
//                }
//            }
//
//            if (hasMergedBounds) {
//                DirectX::XMFLOAT3 cameraDir = rc.cameraDirection;
//                const float dirLengthSq =
//                    cameraDir.x * cameraDir.x +
//                    cameraDir.y * cameraDir.y +
//                    cameraDir.z * cameraDir.z;
//                if (dirLengthSq < 0.0001f) {
//                    cameraDir = { 0.0f, 0.0f, 1.0f };
//                } else {
//                    const float invDirLength = 1.0f / std::sqrt(dirLengthSq);
//                    cameraDir.x *= invDirLength;
//                    cameraDir.y *= invDirLength;
//                    cameraDir.z *= invDirLength;
//                }
//
//                const float radius = (std::max)(
//                    (std::max)(mergedBounds.Extents.x, mergedBounds.Extents.y),
//                    (std::max)(mergedBounds.Extents.z, 1.0f));
//                const float safeFov = (std::max)(m_editorLayer->GetEditorCameraFovY(), 0.2f);
//                const float fitDistance = radius / std::tan(safeFov * 0.5f);
//                DirectX::XMFLOAT3 target = mergedBounds.Center;
//                DirectX::XMFLOAT3 position = {
//                    target.x - cameraDir.x * (fitDistance * 1.35f),
//                    target.y - cameraDir.y * (fitDistance * 1.35f) + radius * 0.35f,
//                    target.z - cameraDir.z * (fitDistance * 1.35f)
//                };
//                m_editorLayer->SetEditorCameraLookAt(position, target);
//            } else {
//                DirectX::XMFLOAT3 target = {
//                    rc.cameraPosition.x + rc.cameraDirection.x,
//                    rc.cameraPosition.y + rc.cameraDirection.y,
//                    rc.cameraPosition.z + rc.cameraDirection.z
//                };
//                m_editorLayer->SetEditorCameraLookAt(rc.cameraPosition, target);
//            }
//        }
//        const DirectX::XMFLOAT2 sceneViewSize = usePlayerWorkspaceScene
//            ? m_editorLayer->GetPlayerPreviewRenderSize()
//            : (usePlayerPreviewAsPrimary
//            ? m_editorLayer->GetPlayerPreviewRenderSize()
//            : (useEffectPreviewAsPrimary
//                ? m_editorLayer->GetEffectPreviewRenderSize()
//                : m_editorLayer->GetSceneViewSize()));
//        const uint32_t panelWidth = static_cast<uint32_t>((std::max)(sceneViewSize.x, 0.0f));
//        const uint32_t panelHeight = static_cast<uint32_t>((std::max)(sceneViewSize.y, 0.0f));
//        auto primaryView = m_renderPipeline->BuildPrimaryViewContext(rc, panelWidth, panelHeight);
//        auto& state = primaryView.state;
//        if (usePlayerPreviewAsPrimary) {
//            const uint32_t previewWidth = (std::max)(panelWidth, 64u);
//            const uint32_t previewHeight = (std::max)(panelHeight, 64u);
//            state.historyKey = 0x50450001ull;
//            state.panelWidth = previewWidth;
//            state.panelHeight = previewHeight;
//            state.renderWidth = previewWidth;
//            state.renderHeight = previewHeight;
//            state.displayWidth = previewWidth;
//            state.displayHeight = previewHeight;
//            state.viewport = RhiViewport(0.0f, 0.0f, static_cast<float>(previewWidth), static_cast<float>(previewHeight));
//            state.cameraPosition = m_editorLayer->GetPlayerPreviewCameraPosition();
//            state.cameraDirection = m_editorLayer->GetPlayerPreviewCameraDirection();
//            state.fovY = m_editorLayer->GetPlayerPreviewCameraFovY();
//            state.nearZ = m_editorLayer->GetPlayerPreviewNearZ();
//            state.farZ = m_editorLayer->GetPlayerPreviewFarZ();
//            state.aspect = static_cast<float>(previewWidth) / static_cast<float>(previewHeight);
//            state.enableComputeCulling = false;
//            state.enableAsyncCompute = false;
//            state.enableGTAO = false;
//            state.enableSSGI = false;
//            state.enableVolumetricFog = false;
//            state.enableSSR = false;
//            state.enableSkybox = m_editorLayer->ShouldPlayerPreviewUseSkybox();
//            state.clearColor = m_editorLayer->GetPlayerPreviewClearColor();
//            rc.bloomData = {};
//            rc.colorFilterData = {};
//            rc.dofData = {};
//            rc.motionBlurData = {};
//            {
//                using namespace DirectX;
//                const XMFLOAT3 previewTarget = m_editorLayer->GetPlayerPreviewCameraTarget();
//                const XMVECTOR eye = XMLoadFloat3(&state.cameraPosition);
//                const XMVECTOR target = XMLoadFloat3(&previewTarget);
//                const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
//                const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
//                const XMMATRIX projection = XMMatrixPerspectiveFovLH(
//                    state.fovY,
//                    state.aspect,
//                    state.nearZ,
//                    state.farZ);
//                XMStoreFloat4x4(&state.viewMatrix, view);
//                XMStoreFloat4x4(&state.projectionMatrix, projection);
//                XMStoreFloat4x4(&state.viewProjectionUnjittered, view * projection);
//            }
//        } else if (useEffectPreviewAsPrimary) {
//            const uint32_t previewWidth = (std::max)(panelWidth, 64u);
//            const uint32_t previewHeight = (std::max)(panelHeight, 64u);
//            state.historyKey = 0xEFFE0001ull;
//            state.panelWidth = previewWidth;
//            state.panelHeight = previewHeight;
//            state.renderWidth = previewWidth;
//            state.renderHeight = previewHeight;
//            state.displayWidth = previewWidth;
//            state.displayHeight = previewHeight;
//            state.viewport = RhiViewport(0.0f, 0.0f, static_cast<float>(previewWidth), static_cast<float>(previewHeight));
//            state.cameraPosition = m_editorLayer->GetEffectPreviewCameraPosition();
//            state.cameraDirection = m_editorLayer->GetEffectPreviewCameraDirection();
//            state.fovY = m_editorLayer->GetEffectPreviewCameraFovY();
//            state.nearZ = m_editorLayer->GetEffectPreviewNearZ();
//            state.farZ = m_editorLayer->GetEffectPreviewFarZ();
//            state.aspect = static_cast<float>(previewWidth) / static_cast<float>(previewHeight);
//            state.enableAsyncCompute = false;
//            state.enableGTAO = false;
//            state.enableSSGI = false;
//            state.enableVolumetricFog = false;
//            state.enableSSR = false;
//            state.enableDeferredLighting = false;
//            state.enableSkybox = m_editorLayer->ShouldEffectPreviewUseSkybox();
//            state.clearColor = m_editorLayer->GetEffectPreviewClearColor();
//            {
//                using namespace DirectX;
//                const XMFLOAT3 previewTarget = m_editorLayer->GetEffectPreviewCameraTarget();
//                const XMVECTOR eye = XMLoadFloat3(&state.cameraPosition);
//                const XMVECTOR target = XMLoadFloat3(&previewTarget);
//                const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
//                const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
//                const XMMATRIX projection = XMMatrixPerspectiveFovLH(
//                    state.fovY,
//                    state.aspect,
//                    state.nearZ,
//                    state.farZ);
//                XMStoreFloat4x4(&state.viewMatrix, view);
//                XMStoreFloat4x4(&state.projectionMatrix, projection);
//                XMStoreFloat4x4(&state.viewProjectionUnjittered, view * projection);
//            }
//        } else {
//            state.viewMatrix = m_editorLayer->GetEditorViewMatrix();
//            state.cameraPosition = m_editorLayer->GetEditorCameraPosition();
//            state.cameraDirection = m_editorLayer->GetEditorCameraDirection();
//            state.fovY = m_editorLayer->GetEditorCameraFovY();
//            state.aspect = (state.renderHeight > 0)
//                ? (static_cast<float>(state.renderWidth) / static_cast<float>(state.renderHeight))
//                : state.aspect;
//            state.projectionMatrix = m_editorLayer->BuildEditorProjectionMatrix(state.aspect);
//            {
//                using namespace DirectX;
//                const XMMATRIX view = XMLoadFloat4x4(&state.viewMatrix);
//                const XMMATRIX projection = XMLoadFloat4x4(&state.projectionMatrix);
//                XMStoreFloat4x4(&state.viewProjectionUnjittered, view * projection);
//            }
//        }
//        state.prevViewProjectionMatrix = state.viewProjectionUnjittered;
//        state.jitterOffset = { 0.0f, 0.0f };
//        state.prevJitterOffset = { 0.0f, 0.0f };
//        views.push_back(std::move(primaryView));
//    } else {
//        views.push_back(m_renderPipeline->BuildPrimaryViewContext(rc));
//    }
//    m_renderPipeline->ExecuteViews(m_renderQueue, rc, views);
//
//    auto renderPrimaryViewGizmos = [&](ITexture* colorTarget, ITexture* depthTarget) -> bool {
//        if (views.empty()) {
//            return false;
//        }
//
//        auto& primaryView = views.front();
//        Gizmos* gizmos = Graphics::Instance().GetGizmos();
//        if (!gizmos || !colorTarget || !depthTarget) {
//            return false;
//        }
//
//        RenderContext gizmoRc = rc;
//        gizmoRc.mainRenderTarget = colorTarget;
//        gizmoRc.sceneColorTexture = colorTarget;
//        gizmoRc.mainDepthStencil = depthTarget;
//        gizmoRc.sceneDepthTexture = depthTarget;
//        gizmoRc.mainViewport = primaryView.state.viewport;
//        gizmoRc.renderWidth = primaryView.state.renderWidth;
//        gizmoRc.renderHeight = primaryView.state.renderHeight;
//        gizmoRc.displayWidth = primaryView.state.displayWidth;
//        gizmoRc.displayHeight = primaryView.state.displayHeight;
//        gizmoRc.panelWidth = primaryView.state.panelWidth;
//        gizmoRc.panelHeight = primaryView.state.panelHeight;
//        gizmoRc.viewMatrix = primaryView.state.viewMatrix;
//        gizmoRc.projectionMatrix = primaryView.state.projectionMatrix;
//        gizmoRc.viewProjectionUnjittered = primaryView.state.viewProjectionUnjittered;
//        gizmoRc.prevViewProjectionMatrix = primaryView.state.prevViewProjectionMatrix;
//        gizmoRc.cameraPosition = primaryView.state.cameraPosition;
//        gizmoRc.cameraDirection = primaryView.state.cameraDirection;
//        gizmoRc.fovY = primaryView.state.fovY;
//        gizmoRc.aspect = primaryView.state.aspect;
//        gizmoRc.nearZ = primaryView.state.nearZ;
//        gizmoRc.farZ = primaryView.state.farZ;
//        gizmoRc.jitterOffset = primaryView.state.jitterOffset;
//        gizmoRc.prevJitterOffset = primaryView.state.prevJitterOffset;
//
//        rc.commandList->TransitionBarrier(colorTarget, ResourceState::RenderTarget);
//        rc.commandList->TransitionBarrier(depthTarget, ResourceState::DepthRead);
//        rc.commandList->SetRenderTarget(colorTarget, depthTarget);
//        rc.commandList->SetViewport(gizmoRc.mainViewport);
//        gizmos->Render(gizmoRc);
//        rc.commandList->TransitionBarrier(colorTarget, ResourceState::ShaderResource);
//        rc.commandList->TransitionBarrier(depthTarget, ResourceState::ShaderResource);
//        return true;
//    };
//
//    const bool deferPrimaryViewGizmos = m_editorLayer && m_editorLayer->ShouldRenderSceneGrid3D();
//    if (!deferPrimaryViewGizmos && !views.empty()) {
//        auto& primaryView = views.front();
//        ITexture* gizmoColorTarget = primaryView.sceneViewTexture ? primaryView.sceneViewTexture : rc.sceneColorTexture;
//        ITexture* gizmoDepthTarget = primaryView.sceneDepthTexture ? primaryView.sceneDepthTexture : rc.sceneDepthTexture;
//        renderPrimaryViewGizmos(gizmoColorTarget, gizmoDepthTarget);
//    }
//
//    ITexture* editorScenePreviewTexture = nullptr;
//    if (m_editorLayer) {
//        auto& primaryView = views.front();
//        ITexture* sceneViewTexture = primaryView.sceneViewTexture ? primaryView.sceneViewTexture : rc.sceneColorTexture;
//        ITexture* sceneDepthTexture = primaryView.sceneDepthTexture ? primaryView.sceneDepthTexture : rc.sceneDepthTexture;
//        ITexture* gameViewTexture = primaryView.sceneViewTexture ? primaryView.sceneViewTexture :
//            (primaryView.displayTexture ? primaryView.displayTexture : rc.sceneColorTexture);
//        bool renderedPrimaryViewGizmos = false;
//
//        if (m_editorLayer->ShouldRenderSceneGrid3D() && sceneViewTexture && sceneDepthTexture) {
//            FrameBuffer* editorSceneFrameBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::EditorScene);
//            ITexture* editorSceneColor = editorSceneFrameBuffer ? editorSceneFrameBuffer->GetColorTexture(0) : nullptr;
//            if (editorSceneColor &&
//                editorSceneColor->GetWidth() == sceneViewTexture->GetWidth() &&
//                editorSceneColor->GetHeight() == sceneViewTexture->GetHeight() &&
//                CopyTextureResource(rc.commandList, sceneViewTexture, editorSceneColor)) {
//                RenderContext gridRc = rc;
//                gridRc.mainRenderTarget = editorSceneColor;
//                gridRc.sceneColorTexture = editorSceneColor;
//                gridRc.mainDepthStencil = sceneDepthTexture;
//                gridRc.sceneDepthTexture = sceneDepthTexture;
//                gridRc.mainViewport = RhiViewport(
//                    0.0f,
//                    0.0f,
//                    static_cast<float>(editorSceneColor->GetWidth()),
//                    static_cast<float>(editorSceneColor->GetHeight()));
//                gridRc.renderWidth = editorSceneColor->GetWidth();
//                gridRc.renderHeight = editorSceneColor->GetHeight();
//                gridRc.viewMatrix = primaryView.state.viewMatrix;
//                gridRc.projectionMatrix = primaryView.state.projectionMatrix;
//                gridRc.viewProjectionUnjittered = primaryView.state.viewProjectionUnjittered;
//                gridRc.prevViewProjectionMatrix = primaryView.state.prevViewProjectionMatrix;
//                gridRc.cameraPosition = primaryView.state.cameraPosition;
//                gridRc.cameraDirection = primaryView.state.cameraDirection;
//                gridRc.fovY = primaryView.state.fovY;
//                gridRc.aspect = primaryView.state.aspect;
//                gridRc.nearZ = primaryView.state.nearZ;
//                gridRc.farZ = primaryView.state.farZ;
//                gridRc.jitterOffset = primaryView.state.jitterOffset;
//                gridRc.prevJitterOffset = primaryView.state.prevJitterOffset;
//
//                rc.commandList->TransitionBarrier(editorSceneColor, ResourceState::RenderTarget);
//                rc.commandList->TransitionBarrier(sceneDepthTexture, ResourceState::DepthRead);
//                rc.commandList->SetRenderTarget(editorSceneColor, sceneDepthTexture);
//                rc.commandList->SetViewport(gridRc.mainViewport);
//                GridRenderSystem::EditorGridSettings gridSettings{};
//                gridSettings.cellSize = m_editorLayer->GetSceneGridCellSize();
//                gridSettings.halfLineCount = m_editorLayer->GetSceneGridHalfLineCount();
//                m_editorGridRenderSystem.RenderEditorGrid(gridRc, gridSettings);
//                // Composite debug shapes after the grid so both overlays stay stable.
//                renderedPrimaryViewGizmos = renderPrimaryViewGizmos(editorSceneColor, sceneDepthTexture);
//                rc.commandList->TransitionBarrier(editorSceneColor, ResourceState::ShaderResource);
//                rc.commandList->TransitionBarrier(sceneViewTexture, ResourceState::ShaderResource);
//                rc.commandList->TransitionBarrier(sceneDepthTexture, ResourceState::ShaderResource);
//                editorScenePreviewTexture = editorSceneColor;
//                sceneViewTexture = editorSceneColor;
//            }
//        }
//
//        if (deferPrimaryViewGizmos && !renderedPrimaryViewGizmos) {
//            renderPrimaryViewGizmos(sceneViewTexture, sceneDepthTexture);
//        }
//
//        m_editorLayer->SetSceneViewTexture(sceneViewTexture);
//        m_editorLayer->SetGameViewTexture(gameViewTexture);
//        m_editorLayer->SetGBufferDebugTextures(
//            primaryView.debugGBuffer0 ? primaryView.debugGBuffer0 : rc.debugGBuffer0,
//            primaryView.debugGBuffer1 ? primaryView.debugGBuffer1 : rc.debugGBuffer1,
//            primaryView.debugGBuffer2 ? primaryView.debugGBuffer2 : rc.debugGBuffer2,
//            nullptr,
//            primaryView.debugDepth ? primaryView.debugDepth : rc.debugGBufferDepth);
//
//        if (m_editorLayer->IsPlayerWorkspaceActive()) {
//            m_editorLayer->SetPlayerPreviewTexture(sceneViewTexture);
//        } else if (m_editorLayer->ShouldRenderPlayerPreview()) {
//            RenderPlayerPreviewOffscreen();
//        } else if (m_editorLayer->ShouldRenderEffectPreview()) {
//            ITexture* effectPreviewTexture = primaryView.sceneViewTexture
//                ? primaryView.sceneViewTexture
//                : primaryView.displayTexture;
//            m_editorLayer->SetEffectPreviewTexture(effectPreviewTexture);
//        }
//    }
//
//    static uint64_t s_perfLogFrame = 0;
//    if ((s_perfLogFrame++ % 120ull) == 0ull) {
//        LOG_INFO(
//            "[DX12Perf] sceneUpload=%.3fms fg(setup=%.3f compile=%.3f execute=%.3f) submit=%.3fms "
//            "extract=%.3fms visible=%.3fms instance=%.3fms indirect=%.3fms async(submit=%.3f gpu=%.3f wait=%u async=%u fallback=%u) "
//            "opaque=%u transparent=%u batches=%u matGroups=%u visibleBatches=%u visibleInstances=%u gpuCandidates=%u/%u hit=%.2f avgVisible=%.2f "
//            "preparedDraw=%u preparedSkinned=%u gpuGroups=%u gpuReduce=%.2f matResolves=%u maxBatch=%u "
//            "packetGrow=(%u,%u) batchGrow=%u sort=%.3fms visibleScratchGrow=%u preparedGrow=(inst:%u,batch:%u) "
//            "indirectGrow=(gpu:%u,scratch:%u,args:%u,meta:%u) split=(nonSkin:%u,skin:%u packets:%u/%u)",
//            rc.prepMetrics.sceneUploadMs,
//            rc.prepMetrics.frameGraphSetupMs,
//            rc.prepMetrics.frameGraphCompileMs,
//            rc.prepMetrics.frameGraphExecuteMs,
//            rc.prepMetrics.submitFrameMs,
//            m_renderQueue.metrics.meshExtractMs,
//            rc.prepMetrics.visibleExtractMs,
//            rc.prepMetrics.instanceBuildMs,
//            rc.prepMetrics.indirectBuildMs,
//            rc.prepMetrics.asyncComputeSubmitMs,
//            rc.prepMetrics.asyncComputeGpuMs,
//            rc.prepMetrics.asyncComputeWaitCount,
//            rc.prepMetrics.asyncComputeDispatchCount,
//            rc.prepMetrics.asyncComputeFallbackCount,
//            m_renderQueue.metrics.opaquePacketCount,
//            m_renderQueue.metrics.transparentPacketCount,
//            m_renderQueue.metrics.opaqueBatchCount,
//            m_renderQueue.metrics.materialGroupCount,
//            rc.prepMetrics.visibleBatchCount,
//            rc.prepMetrics.visibleInstanceCount,
//            rc.prepMetrics.gpuDrivenCandidateBatchCount,
//            rc.prepMetrics.gpuDrivenCandidateInstanceCount,
//            rc.prepMetrics.visibleInstanceHitRate,
//            rc.prepMetrics.averageInstancesPerVisibleBatch,
//            rc.prepMetrics.preparedIndirectCount,
//            rc.prepMetrics.preparedSkinnedCount,
//            rc.prepMetrics.gpuDrivenDispatchGroupCount,
//            rc.prepMetrics.gpuDrivenDispatchReduction,
//            m_renderQueue.metrics.materialResolveCount,
//            m_renderQueue.metrics.maxInstancesPerBatch,
//            m_renderQueue.metrics.opaquePacketVectorGrowths,
//            m_renderQueue.metrics.transparentPacketVectorGrowths,
//            m_renderQueue.metrics.opaqueBatchVectorGrowths,
//            m_renderQueue.metrics.batchSortMs,
//            rc.prepMetrics.visibleScratchVectorGrowths,
//            rc.prepMetrics.preparedInstanceVectorGrowths,
//            rc.prepMetrics.preparedBatchVectorGrowths,
//            rc.prepMetrics.indirectBufferReallocs,
//            rc.prepMetrics.indirectScratchVectorGrowths,
//            rc.prepMetrics.drawArgsVectorGrowths,
//            rc.prepMetrics.metadataVectorGrowths,
//            rc.prepMetrics.nonSkinnedCommandCount,
//            rc.prepMetrics.skinnedCommandCount,
//            m_renderQueue.metrics.nonSkinnedOpaquePacketCount,
//            m_renderQueue.metrics.skinnedOpaquePacketCount);
//    }
//
//    m_renderPipeline->EndFrame(rc);
//
//    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
//        auto transitionTextureForUi = [&](ITexture* texture) {
//            if (texture) {
//                rc.commandList->TransitionBarrier(texture, ResourceState::ShaderResource);
//            }
//        };
//        transitionTextureForUi(rc.sceneColorTexture);
//        transitionTextureForUi(rc.sceneDepthTexture);
//        transitionTextureForUi(rc.debugGBuffer0);
//        transitionTextureForUi(rc.debugGBuffer1);
//        transitionTextureForUi(rc.debugGBuffer2);
//        transitionTextureForUi(rc.debugGBufferDepth);
//        transitionTextureForUi(editorScenePreviewTexture);
//    }
//
//    if (m_editorLayer) m_editorLayer->RenderUI();
//
//    TextureSnapshotState& displaySnapshot = GetDisplaySnapshotState();
//    TextureSnapshotState& backBufferSnapshot = GetBackBufferSnapshotState();
//    TextureSnapshotState& sceneSnapshot = GetSceneSnapshotState();
//    TextureSnapshotState& sceneOpaqueSnapshot = GetSceneOpaqueSnapshotState();
//    TextureSnapshotState& gbuffer0Snapshot = GetGBuffer0SnapshotState();
//    TextureSnapshotState& gbuffer1Snapshot = GetGBuffer1SnapshotState();
//    TextureSnapshotState& gbuffer2Snapshot = GetGBuffer2SnapshotState();
//    TextureSnapshotState& gbufferDepthSnapshot = GetGBufferDepthSnapshotState();
//    TextureSnapshotState& shadowMapSnapshot = GetShadowMapSnapshotState();
//    const bool shouldCaptureDisplay = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && s_perfLogFrame >= kDiagnosticStartFrame
//        && displaySnapshot.captureCount < kMaxDiagnosticFrames;
//    const bool shouldCaptureBackBuffer = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && s_perfLogFrame >= kDiagnosticStartFrame
//        && backBufferSnapshot.captureCount < kMaxDiagnosticFrames;
//    const bool shouldCaptureScene = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && s_perfLogFrame >= kDiagnosticStartFrame
//        && sceneSnapshot.captureCount < kMaxDiagnosticFrames;
//    const bool hasOpaqueGeometry = !m_renderQueue.opaquePackets.empty() || !m_renderQueue.opaqueInstanceBatches.empty();
//    const bool shouldCaptureSceneOpaque = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && s_perfLogFrame >= kDiagnosticStartFrame
//        && sceneOpaqueSnapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry;
//    const bool shouldCaptureGBuffer0 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && s_perfLogFrame >= kDiagnosticStartFrame
//        && gbuffer0Snapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry && rc.debugGBuffer0;
//    const bool shouldCaptureGBuffer1 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && s_perfLogFrame >= kDiagnosticStartFrame
//        && gbuffer1Snapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry && rc.debugGBuffer1;
//    const bool shouldCaptureGBuffer2 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && s_perfLogFrame >= kDiagnosticStartFrame
//        && gbuffer2Snapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry && rc.debugGBuffer2;
//    const bool shouldCaptureGBufferDepth = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && s_perfLogFrame >= kDiagnosticStartFrame
//        && gbufferDepthSnapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry && rc.debugGBufferDepth;
//    const bool shouldCaptureShadowMap = Graphics::Instance().GetAPI() == GraphicsAPI::DX12
//        && shadowMapSnapshot.captureCount < 4u
//        && rc.shadowMap
//        && rc.shadowMap->GetTexture();
//
//    if (kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
//        static bool s_loggedGBufferCaptureState = false;
//        if (!s_loggedGBufferCaptureState) {
//            LOG_INFO("[GBufferCaptureState] hasOpaque=%d dbg0=%p dbg1=%p dbg2=%p dbgDepth=%p capture=(%d,%d,%d,%d)",
//                hasOpaqueGeometry ? 1 : 0,
//                rc.debugGBuffer0,
//                rc.debugGBuffer1,
//                rc.debugGBuffer2,
//                rc.debugGBufferDepth,
//                shouldCaptureGBuffer0 ? 1 : 0,
//                shouldCaptureGBuffer1 ? 1 : 0,
//                shouldCaptureGBuffer2 ? 1 : 0,
//                shouldCaptureGBufferDepth ? 1 : 0);
//            s_loggedGBufferCaptureState = true;
//        }
//    }
//
//    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
//        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);
//        dx12Cmd->FlushResourceBarriers();
//        ImGuiRenderer::RenderDX12(dx12Cmd->GetNativeCommandList());
//        dx12Cmd->RestoreDescriptorHeap();  // ImGui がヒープを切り替えるため復元
//        if (shouldCaptureBackBuffer) {
//            auto* backBufferTexture = dynamic_cast<DX12Texture*>(Graphics::Instance().GetBackBufferTexture());
//            ScheduleTextureSnapshot(dx12Cmd, backBufferTexture, backBufferSnapshot);
//        }
//        if (shouldCaptureDisplay) {
//            auto* displayTexture = dynamic_cast<DX12Texture*>(rc.sceneColorTexture);
//            ScheduleTextureSnapshot(dx12Cmd, displayTexture, displaySnapshot);
//        }
//        if (shouldCaptureScene) {
//            auto* sceneTexture = dynamic_cast<DX12Texture*>(rc.sceneColorTexture);
//            ScheduleTextureSnapshot(dx12Cmd, sceneTexture, sceneSnapshot);
//        }
//        if (shouldCaptureSceneOpaque) {
//            auto* sceneTexture = dynamic_cast<DX12Texture*>(rc.sceneColorTexture);
//            ScheduleTextureSnapshot(dx12Cmd, sceneTexture, sceneOpaqueSnapshot);
//        }
//        if (shouldCaptureGBuffer0) {
//            auto* gbuffer0 = dynamic_cast<DX12Texture*>(rc.debugGBuffer0);
//            ScheduleTextureSnapshot(dx12Cmd, gbuffer0, gbuffer0Snapshot);
//        }
//        if (shouldCaptureGBuffer1) {
//            auto* gbuffer1 = dynamic_cast<DX12Texture*>(rc.debugGBuffer1);
//            ScheduleTextureSnapshot(dx12Cmd, gbuffer1, gbuffer1Snapshot);
//        }
//        if (shouldCaptureGBuffer2) {
//            auto* gbuffer2 = dynamic_cast<DX12Texture*>(rc.debugGBuffer2);
//            ScheduleTextureSnapshot(dx12Cmd, gbuffer2, gbuffer2Snapshot);
//        }
//        if (shouldCaptureGBufferDepth) {
//            auto* gbufferDepth = dynamic_cast<DX12Texture*>(rc.debugGBufferDepth);
//            ScheduleTextureSnapshot(dx12Cmd, gbufferDepth, gbufferDepthSnapshot);
//        }
//        if (shouldCaptureShadowMap) {
//            auto* shadowTexture = dynamic_cast<DX12Texture*>(rc.shadowMap->GetTexture());
//            ScheduleTextureSnapshot(dx12Cmd, shadowTexture, shadowMapSnapshot);
//        }
//    }
//
//    m_renderPipeline->SubmitFrame(rc);
//
//    if (m_editorLayer) {
//        m_editorLayer->RenderDetachedWindows();
//    }
//
//    // D3D12 デバッグメッセージをログに出力（初回フレームのみ）
//    if (kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
//        Graphics::Instance().GetDX12Device()->FlushDebugMessages();
//    }
//
//    if (shouldCaptureDisplay || shouldCaptureBackBuffer || shouldCaptureScene || shouldCaptureSceneOpaque || shouldCaptureGBuffer0 || shouldCaptureGBuffer1 || shouldCaptureGBuffer2 || shouldCaptureGBufferDepth || shouldCaptureShadowMap) {
//        Graphics::Instance().GetDX12Device()->WaitForGPU();
//        LogTextureSnapshot("DisplaySnapshot", displaySnapshot);
//        LogTextureSnapshot("BackBufferSnapshot", backBufferSnapshot);
//        LogTextureSnapshot("SceneSnapshot", sceneSnapshot);
//        LogTextureSnapshot("SceneOpaqueSnapshot", sceneOpaqueSnapshot);
//        LogTextureSnapshot("GBuffer0Snapshot", gbuffer0Snapshot);
//        LogTextureSnapshot("GBuffer1Snapshot", gbuffer1Snapshot);
//        LogTextureSnapshot("GBuffer2Snapshot", gbuffer2Snapshot);
//        LogTextureSnapshot("GBufferDepthSnapshot", gbufferDepthSnapshot);
//      /*  LogTextureSnapshot("ShadowMapSnapshot", shadowMapSnapshot);*/
//        LogSceneOverGBufferMask(gbuffer0Snapshot, sceneOpaqueSnapshot);
//        LogDepthOverGBufferMask(gbuffer0Snapshot, gbufferDepthSnapshot);
//        LogGBufferStatsOverAlbedoMask(gbuffer0Snapshot, gbuffer1Snapshot, gbufferDepthSnapshot);
//        LogMaskedSnapshotDiff("SceneOverAlbedoMaskDiff", gbuffer0Snapshot, sceneSnapshot);
//        LogSceneViewPresentationDiff(m_editorLayer.get(), sceneSnapshot, backBufferSnapshot);
//        LogSceneViewModelPresentationDiff(m_editorLayer.get(), gbuffer0Snapshot, sceneSnapshot, backBufferSnapshot);
//    }
//}
//
//void EngineKernel::Play()
//{
//    if (mode == EngineMode::Editor) {
//        mode = EngineMode::Play;
//        if (m_editorLayer && m_gameLayer) {
//            m_editorLayer->GetInputBridge().OnPlayStarted(m_gameLayer->GetRegistry());
//        }
//    }
//    else if (mode == EngineMode::Pause) {
//        mode = EngineMode::Play;
//    }
//    m_stepFrameRequested = false;
//}
//
//void EngineKernel::Stop()
//{
//    if (mode == EngineMode::Play || mode == EngineMode::Pause) {
//        mode = EngineMode::Editor;
//        if (m_editorLayer && m_gameLayer) {
//            m_editorLayer->GetInputBridge().OnPlayStopped(m_gameLayer->GetRegistry());
//        }
//    }
//    m_stepFrameRequested = false;
//}
//
//void EngineKernel::Pause()
//{
//    if (mode == EngineMode::Play) mode = EngineMode::Pause;
//    else if (mode == EngineMode::Pause) mode = EngineMode::Play;
//    m_stepFrameRequested = false;
//}
//
//void EngineKernel::Step()
//{
//    if (mode == EngineMode::Pause) {
//        m_stepFrameRequested = true;
//    }
//}
//
//void EngineKernel::ResetRenderStateForSceneChange()
//{
//    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
//        if (auto* dx12 = Graphics::Instance().GetDX12Device()) {
//            dx12->WaitForGPU();
//        }
//    }
//
//    if (m_audioWorld) {
//        m_audioWorld->ResetForSceneChange();
//    }
//
//    if (m_renderPipeline) {
//        m_renderPipeline->ResetForSceneChange();
//    }
//
//    m_renderQueue.Clear();
//}
#include "EngineKernel.h"
#include "Audio/AudioWorldSystem.h"
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
#include <RenderPass\SkyboxPass.h>
#include <RenderPass\GBufferPass.h>
#include <RenderPass\ForwardTransparentPass.h>
#include <RenderPass\EffectMeshPass.h>
#include <RenderPass\EffectParticlePass.h>
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
#include "RHI/DX11/DX11Texture.h"
#include "RHI/DX12/DX12CommandList.h"
#include "RHI/DX12/DX12Texture.h"
#include "Console/Logger.h"
#include "Input/SDLInputBackend.h"
#include "Model/Model.h"
#include "PlayerEditor/PlayerModelPreviewStudio.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include <wrl/client.h>
#include <cfloat>
#include <DirectXCollision.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include "Gameplay/TimelineShakeSystem.h"
#include "GameLoop/GameLoopSystem.h"
#include "GameLoop/SceneTransitionSystem.h"
#include "GameLoop/UIButtonClickSystem.h"
#include "GameLoop/UIButtonClickEventQueue.h"
#include "Component/CameraComponent.h"
#include "Component/Camera2DComponent.h"
#include "Component/TransformComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Input/InputUserComponent.h"
#include "Input/InputBindingComponent.h"
#include "Input/InputContextComponent.h"
#include "Input/InputActionMapComponent.h"
#include "Input/ResolvedInputStateComponent.h"
#include "Input/InputResolveSystem.h"
#include "System/Query.h"

namespace {
    // DX12 実行時診断の有効フラグ。
    constexpr bool kEnableDx12RuntimeDiagnostics = false;

    // 診断を開始するフレーム番号。
    constexpr uint32_t kDiagnosticStartFrame = 1;

    // 診断で最大何フレーム分キャプチャするか。
    constexpr uint32_t kMaxDiagnosticFrames = 8;

    // 診断ログの出力先。
    constexpr const char* kPhase4DiagPath = "C:/Users/yuito/Documents/MyEngine_Workspace/game_dx12_upload/Saved/Logs/phase4_diag.txt";

    // GPU テクスチャ内容の readback 用状態。
    struct TextureSnapshotState {
        Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
        UINT64 bufferSize = 0;
        UINT width = 0;
        UINT height = 0;
        UINT rowPitch = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool pending = false;
        uint32_t captureCount = 0;
        std::vector<float> prevRgb;
        std::vector<float> latestRgb;
    };

    // source テクスチャを destination へコピーする。
    // DX12 / DX11 の API 差を吸収する共通関数。
    bool CopyTextureResource(ICommandList* commandList, ITexture* source, ITexture* destination)
    {
        if (!commandList || !source || !destination) {
            return false;
        }

        // DX12 の場合は ResourceState 遷移込みで CopyResource する。
        if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
            auto* dx12Source = dynamic_cast<DX12Texture*>(source);
            auto* dx12Destination = dynamic_cast<DX12Texture*>(destination);
            auto* dx12CommandList = dynamic_cast<DX12CommandList*>(commandList);
            if (!dx12Source || !dx12Destination || !dx12CommandList) {
                return false;
            }

            commandList->TransitionBarrier(source, ResourceState::CopySource);
            commandList->TransitionBarrier(destination, ResourceState::CopyDest);
            dx12CommandList->FlushResourceBarriers();
            dx12CommandList->GetNativeCommandList()->CopyResource(
                dx12Destination->GetNativeResource(),
                dx12Source->GetNativeResource());
            return true;
        }

        // DX11 の場合は CopyResource を直接呼ぶ。
        auto* dx11Source = dynamic_cast<DX11Texture*>(source);
        auto* dx11Destination = dynamic_cast<DX11Texture*>(destination);
        ID3D11DeviceContext* deviceContext = commandList->GetNativeContext();
        if (!dx11Source || !dx11Destination || !deviceContext) {
            return false;
        }

        deviceContext->CopyResource(
            dx11Destination->GetNativeResource(),
            dx11Source->GetNativeResource());
        return true;
    }

    // half float を float へ変換する。
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
        }
        else if (exponent == 31) {
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

    // Phase4 用診断ファイルへ 1 行追記する。
    void AppendPhase4Diag(const std::string& line)
    {
        static bool cleared = false;
        const std::filesystem::path path(kPhase4DiagPath);
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, cleared ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc));
        cleared = true;
        if (file.is_open()) {
            file << line << '\n';
        }
    }

    // 各診断対象ごとの static snapshot state を返す。
    TextureSnapshotState& GetDisplaySnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    TextureSnapshotState& GetBackBufferSnapshotState() {
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

    TextureSnapshotState& GetGBufferDepthSnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    TextureSnapshotState& GetShadowMapSnapshotState() {
        static TextureSnapshotState state;
        return state;
    }

    // readback 済みテクスチャから 1 ピクセル分の RGBA を読み取る。
    // format ごとに読み方を切り替える。
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

        if (state.format == DXGI_FORMAT_D32_FLOAT ||
            state.format == DXGI_FORMAT_R32_FLOAT ||
            state.format == DXGI_FORMAT_R32_TYPELESS) {
            const auto* pixel = reinterpret_cast<const float*>(row + x * 4);
            r = pixel[0];
            g = pixel[0];
            b = pixel[0];
            a = 1.0;
            return;
        }

        const uint8_t* pixel = row + x * 4;
        r = pixel[0];
        g = pixel[1];
        b = pixel[2];
        a = pixel[3];
    }

    // snapshot 内容を簡易 BMP として保存する。
    void WriteSnapshotBitmap(const char* label, const TextureSnapshotState& state, const uint8_t* bytes)
    {
        if (!label || !bytes || state.width == 0 || state.height == 0) {
            return;
        }

        std::string filePath = "Saved/Logs/";
        filePath += label;
        filePath += ".bmp";

        const uint32_t width = state.width;
        const uint32_t height = state.height;
        const uint32_t rowStride = (width * 3u + 3u) & ~3u;
        const uint32_t pixelDataSize = rowStride * height;
        const uint32_t fileSize = 54u + pixelDataSize;

        std::vector<uint8_t> bmp(fileSize, 0);
        bmp[0] = 'B';
        bmp[1] = 'M';
        memcpy(&bmp[2], &fileSize, sizeof(fileSize));
        const uint32_t pixelOffset = 54u;
        memcpy(&bmp[10], &pixelOffset, sizeof(pixelOffset));
        const uint32_t dibSize = 40u;
        memcpy(&bmp[14], &dibSize, sizeof(dibSize));
        memcpy(&bmp[18], &width, sizeof(width));
        memcpy(&bmp[22], &height, sizeof(height));
        const uint16_t planes = 1u;
        const uint16_t bpp = 24u;
        memcpy(&bmp[26], &planes, sizeof(planes));
        memcpy(&bmp[28], &bpp, sizeof(bpp));
        memcpy(&bmp[34], &pixelDataSize, sizeof(pixelDataSize));

        const bool isGBuffer1 = strstr(label, "GBuffer1") != nullptr;

        for (uint32_t y = 0; y < height; ++y) {
            uint8_t* dstRow = bmp.data() + pixelOffset + (height - 1u - y) * rowStride;
            for (uint32_t x = 0; x < width; ++x) {
                double r = 0.0, g = 0.0, b = 0.0, a = 1.0;
                ReadSnapshotPixel(state, bytes, x, y, r, g, b, a);

                // GBuffer1 は法線っぽい値なので可視化しやすいよう 0.5 バイアス。
                if (isGBuffer1) {
                    r = r * 0.5 + 0.5;
                    g = g * 0.5 + 0.5;
                    b = b * 0.5 + 0.5;
                }

                r = std::clamp(r, 0.0, 1.0);
                g = std::clamp(g, 0.0, 1.0);
                b = std::clamp(b, 0.0, 1.0);

                dstRow[x * 3 + 0] = static_cast<uint8_t>(b * 255.0);
                dstRow[x * 3 + 1] = static_cast<uint8_t>(g * 255.0);
                dstRow[x * 3 + 2] = static_cast<uint8_t>(r * 255.0);
            }
        }

        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs) {
            return;
        }
        ofs.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
    }

    // DX12 テクスチャの内容を readback buffer へコピーするよう予約する。
    void ScheduleTextureSnapshot(DX12CommandList* commandList, DX12Texture* texture, TextureSnapshotState& state) {
        if (!commandList || !texture || state.pending || state.captureCount >= kMaxDiagnosticFrames) {
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

        // readback buffer が無い、または足りないなら作り直す。
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
        const ResourceState originalState = texture->GetCurrentState();

        // Engine 独自 ResourceState を D3D12 state へ変換する。
        auto toD3D12State = [](ResourceState stateValue) -> D3D12_RESOURCE_STATES {
            switch (stateValue) {
            case ResourceState::Common:          return D3D12_RESOURCE_STATE_COMMON;
            case ResourceState::RenderTarget:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
            case ResourceState::DepthWrite:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            case ResourceState::DepthRead:       return D3D12_RESOURCE_STATE_DEPTH_READ;
            case ResourceState::ShaderResource:  return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            case ResourceState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case ResourceState::CopySource:      return D3D12_RESOURCE_STATE_COPY_SOURCE;
            case ResourceState::CopyDest:        return D3D12_RESOURCE_STATE_COPY_DEST;
            case ResourceState::Present:         return D3D12_RESOURCE_STATE_PRESENT;
            default:                             return D3D12_RESOURCE_STATE_COMMON;
            }
            };
        const D3D12_RESOURCE_STATES originalD3DState = toD3D12State(originalState);

        // CopySource でなければ一時的に遷移する。
        if (originalD3DState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
            D3D12_RESOURCE_BARRIER toCopy = {};
            toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            toCopy.Transition.pResource = nativeTexture;
            toCopy.Transition.StateBefore = originalD3DState;
            toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            nativeCommandList->ResourceBarrier(1, &toCopy);
        }

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = state.readbackBuffer.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = nativeTexture;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        nativeCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        // 元 state に戻す。
        if (originalD3DState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
            D3D12_RESOURCE_BARRIER restore = {};
            restore.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            restore.Transition.pResource = nativeTexture;
            restore.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            restore.Transition.StateAfter = originalD3DState;
            restore.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            nativeCommandList->ResourceBarrier(1, &restore);
        }

        state.pending = true;
    }

    // 予約済み snapshot を map して統計情報をログへ出す。
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
            ++state.captureCount;
            return;
        }

        const auto* bytes = static_cast<const uint8_t*>(mapped);
        double sumR = 0.0, sumG = 0.0, sumB = 0.0, sumA = 0.0;
        uint64_t nonBlack = 0;
        double centerR = 0.0, centerG = 0.0, centerB = 0.0, centerA = 0.0;
        std::vector<float> currentRgb;
        currentRgb.reserve(static_cast<size_t>(state.width) * static_cast<size_t>(state.height) * 3u);

        for (UINT y = 0; y < state.height; ++y) {
            for (UINT x = 0; x < state.width; ++x) {
                double r = 0.0, g = 0.0, b = 0.0, a = 1.0;
                ReadSnapshotPixel(state, bytes, x, y, r, g, b, a);

                sumR += r;
                sumG += g;
                sumB += b;
                sumA += a;
                currentRgb.push_back(static_cast<float>(r));
                currentRgb.push_back(static_cast<float>(g));
                currentRgb.push_back(static_cast<float>(b));

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

        {
            char buffer[512];
            snprintf(buffer, sizeof(buffer),
                "[%s] format=%d avgRGBA=(%.4f, %.4f, %.4f, %.4f) centerRGBA=(%.4f, %.4f, %.4f, %.4f) nonBlack=%llu/%u",
                label,
                static_cast<int>(state.format),
                sumR * invPixelCount,
                sumG * invPixelCount,
                sumB * invPixelCount,
                sumA * invPixelCount,
                centerR, centerG, centerB, centerA,
                static_cast<unsigned long long>(nonBlack),
                state.width * state.height);
            AppendPhase4Diag(buffer);
        }

        // 前回との差分もログに出す。
        if (!state.prevRgb.empty() && state.prevRgb.size() == currentRgb.size()) {
            double diffSum = 0.0;
            double diffActiveSum = 0.0;
            uint64_t activeCount = 0;
            for (size_t i = 0; i < currentRgb.size(); i += 3) {
                const double dr = std::abs(static_cast<double>(currentRgb[i + 0]) - static_cast<double>(state.prevRgb[i + 0]));
                const double dg = std::abs(static_cast<double>(currentRgb[i + 1]) - static_cast<double>(state.prevRgb[i + 1]));
                const double db = std::abs(static_cast<double>(currentRgb[i + 2]) - static_cast<double>(state.prevRgb[i + 2]));
                const double diff = (dr + dg + db) / 3.0;
                diffSum += diff;

                if (currentRgb[i + 0] > 0.0001f || currentRgb[i + 1] > 0.0001f || currentRgb[i + 2] > 0.0001f ||
                    state.prevRgb[i + 0] > 0.0001f || state.prevRgb[i + 1] > 0.0001f || state.prevRgb[i + 2] > 0.0001f) {
                    diffActiveSum += diff;
                    ++activeCount;
                }
            }

            const double invCount = currentRgb.empty() ? 0.0 : (1.0 / (static_cast<double>(currentRgb.size()) / 3.0));
            const double invActiveCount = activeCount ? (1.0 / static_cast<double>(activeCount)) : 0.0;
            LOG_INFO("[%sDiff] frame=%u avgAbsRGB=%.6f activeAvgAbsRGB=%.6f activePixels=%llu",
                label,
                state.captureCount,
                diffSum * invCount,
                diffActiveSum * invActiveCount,
                static_cast<unsigned long long>(activeCount));

            {
                char buffer[256];
                snprintf(buffer, sizeof(buffer),
                    "[%sDiff] frame=%u avgAbsRGB=%.6f activeAvgAbsRGB=%.6f activePixels=%llu",
                    label,
                    state.captureCount,
                    diffSum * invCount,
                    diffActiveSum * invActiveCount,
                    static_cast<unsigned long long>(activeCount));
                AppendPhase4Diag(buffer);
            }
        }

        D3D12_RANGE writeRange = { 0, 0 };
        state.readbackBuffer->Unmap(0, &writeRange);
        state.pending = false;
        state.latestRgb = currentRgb;
        state.prevRgb = std::move(currentRgb);
        ++state.captureCount;
    }

    // mask が立っている画素だけ対象に target の前フレーム差分を測る。
    void LogMaskedSnapshotDiff(const char* label, const TextureSnapshotState& maskState, const TextureSnapshotState& targetState)
    {
        if (!label) {
            return;
        }
        if (maskState.latestRgb.empty() || targetState.latestRgb.empty() || targetState.prevRgb.empty()) {
            return;
        }

        const size_t maskPixels = maskState.latestRgb.size() / 3u;
        const size_t targetPixels = targetState.latestRgb.size() / 3u;
        if (maskPixels == 0 || targetPixels == 0 || targetPixels != (targetState.prevRgb.size() / 3u)) {
            return;
        }

        const UINT width = (maskState.width < targetState.width) ? maskState.width : targetState.width;
        const UINT height = (maskState.height < targetState.height) ? maskState.height : targetState.height;
        if (width == 0 || height == 0) {
            return;
        }

        double diffSum = 0.0;
        uint64_t count = 0;
        for (UINT y = 0; y < height; ++y) {
            const size_t maskRow = static_cast<size_t>(y) * maskState.width;
            const size_t targetRow = static_cast<size_t>(y) * targetState.width;
            for (UINT x = 0; x < width; ++x) {
                const size_t maskIndex = (maskRow + x) * 3u;
                if (maskIndex + 2 >= maskState.latestRgb.size()) {
                    continue;
                }

                const float maskR = maskState.latestRgb[maskIndex + 0];
                const float maskG = maskState.latestRgb[maskIndex + 1];
                const float maskB = maskState.latestRgb[maskIndex + 2];
                if (maskR <= 0.0001f && maskG <= 0.0001f && maskB <= 0.0001f) {
                    continue;
                }

                const size_t targetIndex = (targetRow + x) * 3u;
                if (targetIndex + 2 >= targetState.latestRgb.size() || targetIndex + 2 >= targetState.prevRgb.size()) {
                    continue;
                }

                const double dr = std::abs(static_cast<double>(targetState.latestRgb[targetIndex + 0]) - static_cast<double>(targetState.prevRgb[targetIndex + 0]));
                const double dg = std::abs(static_cast<double>(targetState.latestRgb[targetIndex + 1]) - static_cast<double>(targetState.prevRgb[targetIndex + 1]));
                const double db = std::abs(static_cast<double>(targetState.latestRgb[targetIndex + 2]) - static_cast<double>(targetState.prevRgb[targetIndex + 2]));
                diffSum += (dr + dg + db) / 3.0;
                ++count;
            }
        }

        if (count == 0) {
            return;
        }

        LOG_INFO("[%s] frame=%u avgAbsRGB=%.6f pixels=%llu",
            label,
            targetState.captureCount - 1u,
            diffSum / static_cast<double>(count),
            static_cast<unsigned long long>(count));
    }

    // GBuffer0 をマスクとして SceneOpaque の平均色を集計する。
    void LogSceneOverGBufferMask(TextureSnapshotState& maskState, TextureSnapshotState& sceneState) {
        static uint32_t loggedCount = 0;
        if (loggedCount >= kMaxDiagnosticFrames || !maskState.readbackBuffer || !sceneState.readbackBuffer ||
            maskState.captureCount == 0 || sceneState.captureCount == 0) {
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
        ++loggedCount;
    }

    // GBuffer0 の mask 部分に対応する depth の統計を出す。
    void LogDepthOverGBufferMask(const TextureSnapshotState& maskState, const TextureSnapshotState& depthState)
    {
        static uint32_t loggedCount = 0;
        if (loggedCount >= kMaxDiagnosticFrames || maskState.latestRgb.empty() || depthState.latestRgb.empty()) {
            return;
        }

        const size_t maskPixelCount = maskState.latestRgb.size() / 3u;
        const size_t depthPixelCount = depthState.latestRgb.size() / 3u;
        const size_t pixelCount = (std::min)(maskPixelCount, depthPixelCount);
        if (pixelCount == 0) {
            return;
        }

        double minDepth = DBL_MAX;
        double maxDepth = -DBL_MAX;
        double sumDepth = 0.0;
        uint64_t count = 0;

        for (size_t i = 0; i < pixelCount; ++i) {
            const size_t base = i * 3u;
            const float maskR = maskState.latestRgb[base + 0];
            const float maskG = maskState.latestRgb[base + 1];
            const float maskB = maskState.latestRgb[base + 2];
            if (maskR <= 0.0001f && maskG <= 0.0001f && maskB <= 0.0001f) {
                continue;
            }

            const double depth = static_cast<double>(depthState.latestRgb[base + 0]);
            minDepth = (std::min)(minDepth, depth);
            maxDepth = (std::max)(maxDepth, depth);
            sumDepth += depth;
            ++count;
        }

        if (count == 0) {
            return;
        }

        LOG_INFO("[DepthOverAlbedoMask] avg=%.6f min=%.6f max=%.6f count=%llu",
            sumDepth / static_cast<double>(count),
            minDepth,
            maxDepth,
            static_cast<unsigned long long>(count));
        ++loggedCount;
    }

    // GBuffer1 の法線長と Depth の統計を、GBuffer0 の mask 範囲で集計する。
    void LogGBufferStatsOverAlbedoMask(
        const TextureSnapshotState& maskState,
        const TextureSnapshotState& gbuffer1State,
        const TextureSnapshotState& depthState)
    {
        static uint32_t loggedCount = 0;
        if (loggedCount >= kMaxDiagnosticFrames ||
            maskState.latestRgb.empty() ||
            gbuffer1State.latestRgb.empty() ||
            depthState.latestRgb.empty()) {
            return;
        }

        const size_t pixelCount = (std::min)(
            maskState.latestRgb.size(),
            (std::min)(gbuffer1State.latestRgb.size(), depthState.latestRgb.size())) / 3u;
        if (pixelCount == 0) {
            return;
        }

        double minNormalLen = DBL_MAX;
        double maxNormalLen = -DBL_MAX;
        double sumNormalLen = 0.0;
        double minDepth = DBL_MAX;
        double maxDepth = -DBL_MAX;
        double sumDepth = 0.0;
        uint64_t count = 0;

        for (size_t i = 0; i < pixelCount; ++i) {
            const size_t base = i * 3u;
            const float maskR = maskState.latestRgb[base + 0];
            const float maskG = maskState.latestRgb[base + 1];
            const float maskB = maskState.latestRgb[base + 2];
            if (maskR <= 0.0001f && maskG <= 0.0001f && maskB <= 0.0001f) {
                continue;
            }

            const float nx = gbuffer1State.latestRgb[base + 0];
            const float ny = gbuffer1State.latestRgb[base + 1];
            const float nz = gbuffer1State.latestRgb[base + 2];
            const double normalLen = std::sqrt(
                static_cast<double>(nx) * static_cast<double>(nx) +
                static_cast<double>(ny) * static_cast<double>(ny) +
                static_cast<double>(nz) * static_cast<double>(nz));
            const double depthValue = static_cast<double>(depthState.latestRgb[base + 0]);

            minNormalLen = (std::min)(minNormalLen, normalLen);
            maxNormalLen = (std::max)(maxNormalLen, normalLen);
            sumNormalLen += normalLen;
            minDepth = (std::min)(minDepth, depthValue);
            maxDepth = (std::max)(maxDepth, depthValue);
            sumDepth += depthValue;
            ++count;
        }

        if (count == 0) {
            return;
        }

        LOG_INFO("[GBufferMaskStats] normalLen(avg=%.6f min=%.6f max=%.6f) depth(avg=%.6f min=%.6f max=%.6f) count=%llu",
            sumNormalLen / static_cast<double>(count),
            minNormalLen,
            maxNormalLen,
            sumDepth / static_cast<double>(count),
            minDepth,
            maxDepth,
            static_cast<unsigned long long>(count));
        ++loggedCount;
    }

    // SceneView に表示された見た目と scene texture の差分を調べる。
    void LogSceneViewPresentationDiff(const EditorLayer* editorLayer, const TextureSnapshotState& sceneState, const TextureSnapshotState& backBufferState)
    {
        if (!editorLayer || sceneState.latestRgb.empty() || backBufferState.latestRgb.empty()) {
            return;
        }

        const DirectX::XMFLOAT4 rect = editorLayer->GetSceneViewRect();
        const UINT panelX = rect.x > 0.0f ? static_cast<UINT>(rect.x) : 0u;
        const UINT panelY = rect.y > 0.0f ? static_cast<UINT>(rect.y) : 0u;
        const UINT panelW = rect.z > 0.0f ? static_cast<UINT>(rect.z) : 0u;
        const UINT panelH = rect.w > 0.0f ? static_cast<UINT>(rect.w) : 0u;
        if (panelW == 0 || panelH == 0) {
            return;
        }
        if (panelX >= backBufferState.width || panelY >= backBufferState.height) {
            return;
        }

        const UINT compareW = (std::min)(panelW, backBufferState.width - panelX);
        const UINT compareH = (std::min)(panelH, backBufferState.height - panelY);
        if (compareW == 0 || compareH == 0) {
            return;
        }

        double diffSum = 0.0;
        uint64_t count = 0;
        for (UINT y = 0; y < compareH; ++y) {
            const double v = (compareH > 1) ? (static_cast<double>(y) / static_cast<double>(compareH - 1)) : 0.0;
            const UINT sceneY = static_cast<UINT>(v * static_cast<double>((std::max)(1u, sceneState.height) - 1u));
            for (UINT x = 0; x < compareW; ++x) {
                const double u = (compareW > 1) ? (static_cast<double>(x) / static_cast<double>(compareW - 1)) : 0.0;
                const UINT sceneX = static_cast<UINT>(u * static_cast<double>((std::max)(1u, sceneState.width) - 1u));

                const size_t sceneIndex = (static_cast<size_t>(sceneY) * sceneState.width + sceneX) * 3u;
                const size_t displayIndex = (static_cast<size_t>(panelY + y) * backBufferState.width + (panelX + x)) * 3u;
                if (sceneIndex + 2 >= sceneState.latestRgb.size() || displayIndex + 2 >= backBufferState.latestRgb.size()) {
                    continue;
                }

                const double dr = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 0]) - static_cast<double>(backBufferState.latestRgb[displayIndex + 0]) / 255.0);
                const double dg = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 1]) - static_cast<double>(backBufferState.latestRgb[displayIndex + 1]) / 255.0);
                const double db = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 2]) - static_cast<double>(backBufferState.latestRgb[displayIndex + 2]) / 255.0);
                diffSum += (dr + dg + db) / 3.0;
                ++count;
            }
        }

        if (count == 0) {
            return;
        }

        LOG_INFO("[SceneViewPresentationDiff] rect=(%u,%u,%u,%u) scene=%ux%u display=%ux%u avgAbsRGB=%.6f pixels=%llu",
            panelX, panelY, compareW, compareH,
            sceneState.width, sceneState.height,
            backBufferState.width, backBufferState.height,
            diffSum / static_cast<double>(count),
            static_cast<unsigned long long>(count));
    }

    // モデルが映っている部分だけに限定して SceneView 表示差分を測る。
    void LogSceneViewModelPresentationDiff(
        const EditorLayer* editorLayer,
        const TextureSnapshotState& maskState,
        const TextureSnapshotState& sceneState,
        const TextureSnapshotState& backBufferState)
    {
        if (!editorLayer || maskState.latestRgb.empty() || sceneState.latestRgb.empty() || backBufferState.latestRgb.empty()) {
            return;
        }

        const DirectX::XMFLOAT4 rect = editorLayer->GetSceneViewRect();
        const UINT panelX = rect.x > 0.0f ? static_cast<UINT>(rect.x) : 0u;
        const UINT panelY = rect.y > 0.0f ? static_cast<UINT>(rect.y) : 0u;
        const UINT panelW = rect.z > 0.0f ? static_cast<UINT>(rect.z) : 0u;
        const UINT panelH = rect.w > 0.0f ? static_cast<UINT>(rect.w) : 0u;
        if (panelW == 0 || panelH == 0 || panelX >= backBufferState.width || panelY >= backBufferState.height) {
            return;
        }

        const UINT compareW = (std::min)(panelW, backBufferState.width - panelX);
        const UINT compareH = (std::min)(panelH, backBufferState.height - panelY);
        if (compareW == 0 || compareH == 0 || sceneState.width == 0 || sceneState.height == 0) {
            return;
        }

        double diffSum = 0.0;
        uint64_t count = 0;
        for (UINT y = 0; y < compareH; ++y) {
            const double v = (compareH > 1) ? (static_cast<double>(y) / static_cast<double>(compareH - 1)) : 0.0;
            const UINT sceneY = static_cast<UINT>(v * static_cast<double>((std::max)(1u, sceneState.height) - 1u));
            for (UINT x = 0; x < compareW; ++x) {
                const double u = (compareW > 1) ? (static_cast<double>(x) / static_cast<double>(compareW - 1)) : 0.0;
                const UINT sceneX = static_cast<UINT>(u * static_cast<double>((std::max)(1u, sceneState.width) - 1u));

                const size_t maskIndex = (static_cast<size_t>(sceneY) * maskState.width + sceneX) * 3u;
                const size_t sceneIndex = (static_cast<size_t>(sceneY) * sceneState.width + sceneX) * 3u;
                const size_t backIndex = (static_cast<size_t>(panelY + y) * backBufferState.width + (panelX + x)) * 3u;
                if (maskIndex + 2 >= maskState.latestRgb.size() ||
                    sceneIndex + 2 >= sceneState.latestRgb.size() ||
                    backIndex + 2 >= backBufferState.latestRgb.size()) {
                    continue;
                }

                const float maskR = maskState.latestRgb[maskIndex + 0];
                const float maskG = maskState.latestRgb[maskIndex + 1];
                const float maskB = maskState.latestRgb[maskIndex + 2];
                if (maskR <= 0.0001f && maskG <= 0.0001f && maskB <= 0.0001f) {
                    continue;
                }

                const double dr = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 0]) - static_cast<double>(backBufferState.latestRgb[backIndex + 0]) / 255.0);
                const double dg = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 1]) - static_cast<double>(backBufferState.latestRgb[backIndex + 1]) / 255.0);
                const double db = std::abs(static_cast<double>(sceneState.latestRgb[sceneIndex + 2]) - static_cast<double>(backBufferState.latestRgb[backIndex + 2]) / 255.0);
                diffSum += (dr + dg + db) / 3.0;
                ++count;
            }
        }

        if (count == 0) {
            return;
        }

        LOG_INFO("[SceneViewModelPresentationDiff] rect=(%u,%u,%u,%u) avgAbsRGB=%.6f pixels=%llu",
            panelX, panelY, compareW, compareH,
            diffSum / static_cast<double>(count),
            static_cast<unsigned long long>(count));
    }
}

// デフォルト構築。
EngineKernel::EngineKernel() = default;

// デフォルト破棄。
EngineKernel::~EngineKernel() = default;

// singleton インスタンスを返す。
EngineKernel& EngineKernel::Instance()
{
    static EngineKernel instance;
    return instance;
}

// PlayerEditor 用のプレビューを offscreen 描画する。
void EngineKernel::RenderPlayerPreviewOffscreen()
{
    if (!m_editorLayer) {
        return;
    }
    m_editorLayer->SetPlayerPreviewTexture(nullptr);

    auto& previewStudio = PlayerModelPreviewStudio::Instance();
    if (!previewStudio.IsReady()) {
        return;
    }

    const Model* previewModel = m_editorLayer->GetPlayerEditorPanel().GetPreviewModel();
    if (!previewModel) {
        return;
    }

    const DirectX::XMFLOAT2 renderSize = m_editorLayer->GetPlayerPreviewRenderSize();
    const float width = (std::max)(renderSize.x, 1.0f);
    const float height = (std::max)(renderSize.y, 1.0f);

    previewStudio.RenderPreview(
        previewModel,
        m_editorLayer->GetPlayerPreviewCameraPosition(),
        m_editorLayer->GetPlayerPreviewCameraTarget(),
        width / height,
        m_editorLayer->GetPlayerPreviewCameraFovY(),
        m_editorLayer->GetPlayerPreviewNearZ(),
        m_editorLayer->GetPlayerPreviewFarZ(),
        m_editorLayer->GetPlayerPreviewClearColor(),
        m_editorLayer->GetPlayerEditorPanel().GetPreviewModelScale());

    m_editorLayer->SetPlayerPreviewTexture(previewStudio.GetPreviewTexture());
}

// エンジン全体を初期化する。
void EngineKernel::Initialize()
{
    time = EngineTime();
    mode = EngineMode::Editor;

    m_renderPipeline = std::make_unique<RenderPipeline>();

    const bool isDX12 = (Graphics::Instance().GetAPI() == GraphicsAPI::DX12);
    auto* factory = Graphics::Instance().GetResourceFactory();

    // RenderPipeline に必要な pass を登録する。
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
    m_renderPipeline->AddPass(std::make_shared<EffectMeshPass>());
    m_renderPipeline->AddPass(std::make_shared<EffectParticlePass>());
    if (isDX12) {
        m_renderPipeline->AddPass(std::make_shared<FinalBlitPass>(factory));
    }

    // GameLayer 初期化。
    m_gameLayer = std::make_unique<GameLayer>();
    m_gameLayer->Initialize();

    // AudioWorld 初期化。
    m_audioWorld = std::make_unique<AudioWorldSystem>();
    if (!m_audioWorld->Initialize()) {
        LOG_ERROR("[EngineKernel] Failed to initialize AudioWorldSystem.");
    }

    // アイコンフォント初期化。
    std::vector<IconFontManager::SizeConfig> configs = {
        { IconFontSize::Mini,   14.0f },
        { IconFontSize::Small,  14.0f },
        { IconFontSize::Medium, 18.0f },
        { IconFontSize::Large,  24.0f },
        { IconFontSize::Extra,  64.0f }
    };
    IconFontManager::Instance().Setup(configs);

    // 共有 OffscreenRenderer 初期化。
    m_sharedOffscreen = std::make_unique<OffscreenRenderer>();
    if (m_sharedOffscreen->Initialize()) {
        ThumbnailGenerator::Instance().Initialize(m_sharedOffscreen.get());
        MaterialPreviewStudio::Instance().Initialize(m_sharedOffscreen.get());
        PlayerModelPreviewStudio::Instance().Initialize(m_sharedOffscreen.get());
    }
    else {
        LOG_ERROR("[EngineKernel] Failed to initialize shared OffscreenRenderer.");
        ThumbnailGenerator::Instance().Initialize(nullptr);
        MaterialPreviewStudio::Instance().Initialize(nullptr);
        PlayerModelPreviewStudio::Instance().Initialize(nullptr);
    }

    // SDL3 input backend 初期化。
    m_inputBackend = std::make_unique<SDLInputBackend>();
    {
        HWND hwnd = Graphics::Instance().GetWindowHandle();
        if (!m_inputBackend->Initialize(hwnd)) {
            LOG_ERROR("[EngineKernel] Failed to initialize SDL input backend.");
        }
    }

    LOG_INFO("[EngineKernel] Initialize API=%s", isDX12 ? "DX12" : "DX11");

    // GameLoop default asset を読み込む (なければ default を生成する)。
    {
        const std::filesystem::path gameLoopPath = "Data/GameLoop/Main.gameloop";
        if (!m_gameLoopAsset.LoadFromFile(gameLoopPath)) {
            LOG_WARN("[GameLoop] %s not found, using default loop", gameLoopPath.string().c_str());
            m_gameLoopAsset = GameLoopAsset::CreateDefault();
        }
    }

    // GameLoop persistent input owner entity を生成する (scene 跨ぎで保持される)。
    if (!m_gameLoopInputOwnerInitialized) {
        EntityID owner = m_gameLoopRegistry.CreateEntity();
        m_gameLoopRegistry.AddComponent(owner, NameComponent{ "GameLoopInputOwner" });

        InputUserComponent userComp{};
        userComp.userId = 0;
        userComp.isPrimary = true;
        m_gameLoopRegistry.AddComponent(owner, userComp);

        m_gameLoopRegistry.AddComponent(owner, InputBindingComponent{});

        InputContextComponent ctx{};
        ctx.priority = InputContextPriority::RuntimeUI;
        ctx.enabled = true;
        m_gameLoopRegistry.AddComponent(owner, ctx);

        InputActionMapComponent mapComp{};
        mapComp.asset.name = "GameLoop";
        // Index 0 = Confirm.
        {
            ActionBinding b;
            b.actionName = "Confirm";
            b.scancode = 40;        // SDL Enter
            b.gamepadButton = 0;    // SDL Gamepad A
            b.trigger = ActionTriggerType::Pressed;
            mapComp.asset.actions.push_back(b);
        }
        // Index 1 = Cancel.
        {
            ActionBinding b;
            b.actionName = "Cancel";
            b.scancode = 41;        // SDL Escape
            b.gamepadButton = 1;    // SDL Gamepad B
            b.trigger = ActionTriggerType::Pressed;
            mapComp.asset.actions.push_back(b);
        }
        // Index 2 = Retry.
        {
            ActionBinding b;
            b.actionName = "Retry";
            b.scancode = 21;        // SDL R
            b.gamepadButton = 2;    // SDL Gamepad X
            b.trigger = ActionTriggerType::Pressed;
            mapComp.asset.actions.push_back(b);
        }
        m_gameLoopRegistry.AddComponent(owner, mapComp);
        m_gameLoopRegistry.AddComponent(owner, ResolvedInputStateComponent{});

        m_gameLoopInputOwnerInitialized = true;
    }

    // DX12 では EditorLayer を作って終了。
    if (isDX12) {
        m_editorLayer = std::make_unique<EditorLayer>(m_gameLayer.get());
        m_editorLayer->Initialize();
        return;
    }

    // DX11 固有初期化。
    ID3D11Device* dx11Dev = Graphics::Instance().GetDevice();

    m_probeBaker = std::make_unique<ReflectionProbeBaker>(dx11Dev);
    GlobalRootSignature::Instance().Initialize(dx11Dev);

    m_editorLayer = std::make_unique<EditorLayer>(m_gameLayer.get());
    m_editorLayer->Initialize();
}

// 全サブシステムを終了処理する。
void EngineKernel::Finalize()
{
    if (m_inputBackend) {
        m_inputBackend->Shutdown();
        m_inputBackend.reset();
    }

    if (m_audioWorld) {
        m_audioWorld->Finalize();
        m_audioWorld.reset();
    }

    if (m_editorLayer) m_editorLayer->Finalize();
    if (m_gameLayer) m_gameLayer->Finalize();
}

// 入力を 1 フレームぶん収集する。
void EngineKernel::PollInput()
{
    if (m_inputBackend) {
        m_inputBackend->BeginFrame();
        m_inputQueue.Clear();
        m_inputBackend->PollEvents(m_inputQueue);
        m_inputBackend->EndFrame();
    }
}

// エンジン全体の更新。
void EngineKernel::Update(float rawDt)
{
    time.unscaledDt = rawDt;
    time.frameCount++;

    // EditorLayer 更新。
    if (m_editorLayer) m_editorLayer->Update(time);

    const bool stepThisFrame = m_stepFrameRequested;

    // Play 中か Step 中だけ dt を進める。
    if (mode == EngineMode::Play || stepThisFrame)
        time.dt = rawDt * time.timeScale;
    else
        time.dt = 0.0f;

    if (m_gameLayer) m_gameLayer->Update(time);

    // ---- GameLoop pipeline (frame 末尾扱い)。
    if (m_gameLayer) {
        Registry& gameRegistry = m_gameLayer->GetRegistry();

        // GameLoop 用 persistent input owner も resolve する。
        InputResolveSystem::Update(m_gameLoopRegistry, m_inputQueue, time.unscaledDt);

        // 2D Button click を main camera 行列で hit test する。
        // 編集中は EditorLayer の Game View rect (前 frame 分) を使う。
        DirectX::XMFLOAT4 gameViewRect{ 0.0f, 0.0f, 0.0f, 0.0f };
        if (m_editorLayer) {
            gameViewRect = m_editorLayer->GetGameViewRect();
            if (gameViewRect.z <= 1.0f || gameViewRect.w <= 1.0f) {
                const DirectX::XMFLOAT2 sz = m_editorLayer->GetGameViewSize();
                gameViewRect = { 0.0f, 0.0f, sz.x, sz.y };
            }
        }
        DirectX::XMFLOAT4X4 cameraView{};
        DirectX::XMFLOAT4X4 cameraProj{};
        bool hasCamera = false;
        {
            Query<CameraMainTagComponent, CameraMatricesComponent> q(gameRegistry);
            q.ForEach([&](CameraMainTagComponent&, CameraMatricesComponent& mats) {
                if (!hasCamera) {
                    cameraView = mats.view;
                    cameraProj = mats.projection;
                    hasCamera = true;
                }
            });
        }
        if (hasCamera) {
            UIButtonClickSystem::Update(
                gameRegistry,
                m_uiButtonClickQueue,
                m_inputQueue,
                gameViewRect,
                cameraView,
                cameraProj);
        }

        GameLoopSystem::Update(
            m_gameLoopAsset,
            m_gameLoopRuntime,
            gameRegistry,
            m_gameLoopRegistry,
            m_uiButtonClickQueue,
            time.dt);

        // frame 末尾: 実際の scene load を消化する。
        SceneTransitionSystem::UpdateEndOfFrame(m_gameLoopRuntime, gameRegistry);

        // 1 frame 分の click event を破棄。
        m_uiButtonClickQueue.Clear();
    }

    // AudioWorld 更新。
    if (m_audioWorld && m_gameLayer) {
        m_audioWorld->Update(m_gameLayer->GetRegistry(), mode);
    }

    // PlayerEditor がアクティブな時だけ TimelineShake を editor camera 側へ転写する。
    if (m_editorLayer) {
        const bool usePlayerEditorShake =
            m_gameLayer &&
            m_editorLayer->IsPlayerWorkspaceActive();

        if (usePlayerEditorShake) {
            m_editorLayer->SetPlayerEditorCameraShakeOffset(
                TimelineShakeSystem::GetShakeOffset());
        }
        else {
            m_editorLayer->ClearPlayerEditorCameraShakeOffset();
        }
    }

    time.totalTime += time.dt;

    // Step 実行後は自動で Pause へ戻す。
    if (stepThisFrame) {
        m_stepFrameRequested = false;
        mode = EngineMode::Pause;
    }
}

// 描画本体。
void EngineKernel::Render()
{
    // DX12 の deferred free を処理する。
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        if (auto* dx12Device = Graphics::Instance().GetDX12Device()) {
            dx12Device->ProcessDeferredFrees();
        }
    }

    // shared offscreen に紐づく deferred unregister を処理する。
    if (m_sharedOffscreen) {
        ImGuiRenderer::ProcessDeferredUnregisters(m_sharedOffscreen->GetCompletedFenceValue());
    }

    // shared offscreen は 1 フレーム 1 ジョブだけ進める。
    // 優先度は MaterialPreview > Thumbnail。
    static int s_thumbnailSkipCounter = 0;
    const bool usePlayerPreviewAsPrimary = false;
    const bool usePlayerWorkspaceScene = m_editorLayer && m_editorLayer->IsPlayerWorkspaceActive();
    const bool wantsPlayerPreview = false;
    if (m_sharedOffscreen && m_sharedOffscreen->IsGpuIdle() && !wantsPlayerPreview) {
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

    // ゲーム側抽出。
    if (m_gameLayer) {
        m_gameLayer->Render(rc, m_renderQueue);
    }

    // ReflectionProbeBaker は DX11 専用。
    if (m_probeBaker && m_gameLayer && Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
        m_probeBaker->BakeAllDirtyProbes(m_gameLayer->GetRegistry(), m_renderQueue, rc);
    }

    std::vector<RenderPipeline::RenderViewContext> views;

    if (m_editorLayer) {
        m_editorLayer->SetPlayerPreviewTexture(nullptr);
        m_editorLayer->SetEffectPreviewTexture(nullptr);

        const bool useEffectPreviewAsPrimary = !usePlayerWorkspaceScene && !usePlayerPreviewAsPrimary && m_editorLayer->ShouldRenderEffectPreview();

        // Editor camera にユーザー override も auto frame も無いなら、
        // opaque packet 群からバウンディングボックスを作って自動で見やすい位置へ寄せる。
        if (!m_editorLayer->HasEditorCameraUserOverride() && !m_editorLayer->HasEditorCameraAutoFramed()) {
            DirectX::BoundingBox mergedBounds{};
            bool hasMergedBounds = false;

            for (const auto& packet : m_renderQueue.opaquePackets) {
                if (!packet.modelResource) {
                    continue;
                }

                DirectX::BoundingBox worldBounds{};
                const auto& localBounds = packet.modelResource->GetLocalBounds();
                DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&packet.worldMatrix);
                localBounds.Transform(worldBounds, world);

                if (!hasMergedBounds) {
                    mergedBounds = worldBounds;
                    hasMergedBounds = true;
                }
                else {
                    DirectX::BoundingBox::CreateMerged(mergedBounds, mergedBounds, worldBounds);
                }
            }

            if (hasMergedBounds) {
                DirectX::XMFLOAT3 cameraDir = rc.cameraDirection;
                const float dirLengthSq =
                    cameraDir.x * cameraDir.x +
                    cameraDir.y * cameraDir.y +
                    cameraDir.z * cameraDir.z;

                if (dirLengthSq < 0.0001f) {
                    cameraDir = { 0.0f, 0.0f, 1.0f };
                }
                else {
                    const float invDirLength = 1.0f / std::sqrt(dirLengthSq);
                    cameraDir.x *= invDirLength;
                    cameraDir.y *= invDirLength;
                    cameraDir.z *= invDirLength;
                }

                const float radius = (std::max)(
                    (std::max)(mergedBounds.Extents.x, mergedBounds.Extents.y),
                    (std::max)(mergedBounds.Extents.z, 1.0f));
                const float safeFov = (std::max)(m_editorLayer->GetEditorCameraFovY(), 0.2f);
                const float fitDistance = radius / std::tan(safeFov * 0.5f);

                DirectX::XMFLOAT3 target = mergedBounds.Center;
                DirectX::XMFLOAT3 position = {
                    target.x - cameraDir.x * (fitDistance * 1.35f),
                    target.y - cameraDir.y * (fitDistance * 1.35f) + radius * 0.35f,
                    target.z - cameraDir.z * (fitDistance * 1.35f)
                };
                m_editorLayer->SetEditorCameraLookAt(position, target);
            }
            else {
                DirectX::XMFLOAT3 target = {
                    rc.cameraPosition.x + rc.cameraDirection.x,
                    rc.cameraPosition.y + rc.cameraDirection.y,
                    rc.cameraPosition.z + rc.cameraDirection.z
                };
                m_editorLayer->SetEditorCameraLookAt(rc.cameraPosition, target);
            }
        }

        // SceneView / PlayerPreview / EffectPreview のどれを primary view にするか決める。
        const DirectX::XMFLOAT2 sceneViewSize = usePlayerWorkspaceScene
            ? m_editorLayer->GetPlayerPreviewRenderSize()
            : (usePlayerPreviewAsPrimary
                ? m_editorLayer->GetPlayerPreviewRenderSize()
                : (useEffectPreviewAsPrimary
                    ? m_editorLayer->GetEffectPreviewRenderSize()
                    : m_editorLayer->GetSceneViewSize()));

        const uint32_t panelWidth = static_cast<uint32_t>((std::max)(sceneViewSize.x, 0.0f));
        const uint32_t panelHeight = static_cast<uint32_t>((std::max)(sceneViewSize.y, 0.0f));

        auto primaryView = m_renderPipeline->BuildPrimaryViewContext(rc, panelWidth, panelHeight);
        auto& state = primaryView.state;

        // PlayerPreview を primary に使う場合。
        if (usePlayerPreviewAsPrimary) {
            const uint32_t previewWidth = (std::max)(panelWidth, 64u);
            const uint32_t previewHeight = (std::max)(panelHeight, 64u);
            state.historyKey = 0x50450001ull;
            state.panelWidth = previewWidth;
            state.panelHeight = previewHeight;
            state.renderWidth = previewWidth;
            state.renderHeight = previewHeight;
            state.displayWidth = previewWidth;
            state.displayHeight = previewHeight;
            state.viewport = RhiViewport(0.0f, 0.0f, static_cast<float>(previewWidth), static_cast<float>(previewHeight));
            state.cameraPosition = m_editorLayer->GetPlayerPreviewCameraPosition();
            state.cameraDirection = m_editorLayer->GetPlayerPreviewCameraDirection();
            state.fovY = m_editorLayer->GetPlayerPreviewCameraFovY();
            state.nearZ = m_editorLayer->GetPlayerPreviewNearZ();
            state.farZ = m_editorLayer->GetPlayerPreviewFarZ();
            state.aspect = static_cast<float>(previewWidth) / static_cast<float>(previewHeight);
            state.enableComputeCulling = false;
            state.enableAsyncCompute = false;
            state.enableGTAO = false;
            state.enableSSGI = false;
            state.enableVolumetricFog = false;
            state.enableSSR = false;
            state.enableSkybox = m_editorLayer->ShouldPlayerPreviewUseSkybox();
            state.clearColor = m_editorLayer->GetPlayerPreviewClearColor();
            rc.bloomData = {};
            rc.colorFilterData = {};
            rc.dofData = {};
            rc.motionBlurData = {};

            {
                using namespace DirectX;
                const XMFLOAT3 previewTarget = m_editorLayer->GetPlayerPreviewCameraTarget();
                const XMVECTOR eye = XMLoadFloat3(&state.cameraPosition);
                const XMVECTOR target = XMLoadFloat3(&previewTarget);
                const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
                const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
                const XMMATRIX projection = XMMatrixPerspectiveFovLH(
                    state.fovY,
                    state.aspect,
                    state.nearZ,
                    state.farZ);
                XMStoreFloat4x4(&state.viewMatrix, view);
                XMStoreFloat4x4(&state.projectionMatrix, projection);
                XMStoreFloat4x4(&state.viewProjectionUnjittered, view * projection);
            }
        }
        // EffectPreview を primary に使う場合。
        else if (useEffectPreviewAsPrimary) {
            const uint32_t previewWidth = (std::max)(panelWidth, 64u);
            const uint32_t previewHeight = (std::max)(panelHeight, 64u);
            state.historyKey = 0xEFFE0001ull;
            state.panelWidth = previewWidth;
            state.panelHeight = previewHeight;
            state.renderWidth = previewWidth;
            state.renderHeight = previewHeight;
            state.displayWidth = previewWidth;
            state.displayHeight = previewHeight;
            state.viewport = RhiViewport(0.0f, 0.0f, static_cast<float>(previewWidth), static_cast<float>(previewHeight));
            state.cameraPosition = m_editorLayer->GetEffectPreviewCameraPosition();
            state.cameraDirection = m_editorLayer->GetEffectPreviewCameraDirection();
            state.fovY = m_editorLayer->GetEffectPreviewCameraFovY();
            state.nearZ = m_editorLayer->GetEffectPreviewNearZ();
            state.farZ = m_editorLayer->GetEffectPreviewFarZ();
            state.aspect = static_cast<float>(previewWidth) / static_cast<float>(previewHeight);
            state.enableAsyncCompute = false;
            state.enableGTAO = false;
            state.enableSSGI = false;
            state.enableVolumetricFog = false;
            state.enableSSR = false;
            state.enableDeferredLighting = false;
            state.enableSkybox = m_editorLayer->ShouldEffectPreviewUseSkybox();
            state.clearColor = m_editorLayer->GetEffectPreviewClearColor();

            {
                using namespace DirectX;
                const XMFLOAT3 previewTarget = m_editorLayer->GetEffectPreviewCameraTarget();
                const XMVECTOR eye = XMLoadFloat3(&state.cameraPosition);
                const XMVECTOR target = XMLoadFloat3(&previewTarget);
                const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
                const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
                const XMMATRIX projection = XMMatrixPerspectiveFovLH(
                    state.fovY,
                    state.aspect,
                    state.nearZ,
                    state.farZ);
                XMStoreFloat4x4(&state.viewMatrix, view);
                XMStoreFloat4x4(&state.projectionMatrix, projection);
                XMStoreFloat4x4(&state.viewProjectionUnjittered, view * projection);
            }
        }
        // 通常の editor scene view。
        else {
            state.viewMatrix = m_editorLayer->GetEditorViewMatrix();
            state.cameraPosition = m_editorLayer->GetEditorCameraPosition();
            state.cameraDirection = m_editorLayer->GetEditorCameraDirection();
            state.fovY = m_editorLayer->GetEditorCameraFovY();
            state.aspect = (state.renderHeight > 0)
                ? (static_cast<float>(state.renderWidth) / static_cast<float>(state.renderHeight))
                : state.aspect;
            state.projectionMatrix = m_editorLayer->BuildEditorProjectionMatrix(state.aspect);

            {
                using namespace DirectX;
                const XMMATRIX view = XMLoadFloat4x4(&state.viewMatrix);
                const XMMATRIX projection = XMLoadFloat4x4(&state.projectionMatrix);
                XMStoreFloat4x4(&state.viewProjectionUnjittered, view * projection);
            }
        }

        state.prevViewProjectionMatrix = state.viewProjectionUnjittered;
        state.jitterOffset = { 0.0f, 0.0f };
        state.prevJitterOffset = { 0.0f, 0.0f };

        views.push_back(std::move(primaryView));
    }
    else {
        views.push_back(m_renderPipeline->BuildPrimaryViewContext(rc));
    }

    // 各 view を実行する。
    m_renderPipeline->ExecuteViews(m_renderQueue, rc, views);

    // primary view の color/depth を使って gizmo を重ね描きするラムダ。
    auto renderPrimaryViewGizmos = [&](ITexture* colorTarget, ITexture* depthTarget) -> bool {
        if (views.empty()) {
            return false;
        }

        auto& primaryView = views.front();
        Gizmos* gizmos = Graphics::Instance().GetGizmos();
        if (!gizmos || !colorTarget || !depthTarget) {
            return false;
        }

        RenderContext gizmoRc = rc;
        gizmoRc.mainRenderTarget = colorTarget;
        gizmoRc.sceneColorTexture = colorTarget;
        gizmoRc.mainDepthStencil = depthTarget;
        gizmoRc.sceneDepthTexture = depthTarget;
        gizmoRc.mainViewport = primaryView.state.viewport;
        gizmoRc.renderWidth = primaryView.state.renderWidth;
        gizmoRc.renderHeight = primaryView.state.renderHeight;
        gizmoRc.displayWidth = primaryView.state.displayWidth;
        gizmoRc.displayHeight = primaryView.state.displayHeight;
        gizmoRc.panelWidth = primaryView.state.panelWidth;
        gizmoRc.panelHeight = primaryView.state.panelHeight;
        gizmoRc.viewMatrix = primaryView.state.viewMatrix;
        gizmoRc.projectionMatrix = primaryView.state.projectionMatrix;
        gizmoRc.viewProjectionUnjittered = primaryView.state.viewProjectionUnjittered;
        gizmoRc.prevViewProjectionMatrix = primaryView.state.prevViewProjectionMatrix;
        gizmoRc.cameraPosition = primaryView.state.cameraPosition;
        gizmoRc.cameraDirection = primaryView.state.cameraDirection;
        gizmoRc.fovY = primaryView.state.fovY;
        gizmoRc.aspect = primaryView.state.aspect;
        gizmoRc.nearZ = primaryView.state.nearZ;
        gizmoRc.farZ = primaryView.state.farZ;
        gizmoRc.jitterOffset = primaryView.state.jitterOffset;
        gizmoRc.prevJitterOffset = primaryView.state.prevJitterOffset;

        rc.commandList->TransitionBarrier(colorTarget, ResourceState::RenderTarget);
        rc.commandList->TransitionBarrier(depthTarget, ResourceState::DepthRead);
        rc.commandList->SetRenderTarget(colorTarget, depthTarget);
        rc.commandList->SetViewport(gizmoRc.mainViewport);
        gizmos->Render(gizmoRc);
        rc.commandList->TransitionBarrier(colorTarget, ResourceState::ShaderResource);
        rc.commandList->TransitionBarrier(depthTarget, ResourceState::ShaderResource);
        return true;
        };

    const bool deferPrimaryViewGizmos = m_editorLayer && m_editorLayer->ShouldRenderSceneGrid3D();

    // Grid 後に描きたいケースでなければここで gizmo を描く。
    if (!deferPrimaryViewGizmos && !views.empty()) {
        auto& primaryView = views.front();
        ITexture* gizmoColorTarget = primaryView.sceneViewTexture ? primaryView.sceneViewTexture : rc.sceneColorTexture;
        ITexture* gizmoDepthTarget = primaryView.sceneDepthTexture ? primaryView.sceneDepthTexture : rc.sceneDepthTexture;
        renderPrimaryViewGizmos(gizmoColorTarget, gizmoDepthTarget);
    }

    ITexture* editorScenePreviewTexture = nullptr;

    // Editor 用テクスチャの差し替えや grid 合成。
    if (m_editorLayer) {
        auto& primaryView = views.front();
        ITexture* sceneViewTexture = primaryView.sceneViewTexture ? primaryView.sceneViewTexture : rc.sceneColorTexture;
        ITexture* sceneDepthTexture = primaryView.sceneDepthTexture ? primaryView.sceneDepthTexture : rc.sceneDepthTexture;
        ITexture* gameViewTexture = primaryView.sceneViewTexture ? primaryView.sceneViewTexture :
            (primaryView.displayTexture ? primaryView.displayTexture : rc.sceneColorTexture);
        bool renderedPrimaryViewGizmos = false;

        // 3D グリッドを Scene 用専用フレームバッファへ合成する。
        if (m_editorLayer->ShouldRenderSceneGrid3D() && sceneViewTexture && sceneDepthTexture) {
            FrameBuffer* editorSceneFrameBuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::EditorScene);
            ITexture* editorSceneColor = editorSceneFrameBuffer ? editorSceneFrameBuffer->GetColorTexture(0) : nullptr;

            if (editorSceneColor &&
                editorSceneColor->GetWidth() == sceneViewTexture->GetWidth() &&
                editorSceneColor->GetHeight() == sceneViewTexture->GetHeight() &&
                CopyTextureResource(rc.commandList, sceneViewTexture, editorSceneColor)) {

                RenderContext gridRc = rc;
                gridRc.mainRenderTarget = editorSceneColor;
                gridRc.sceneColorTexture = editorSceneColor;
                gridRc.mainDepthStencil = sceneDepthTexture;
                gridRc.sceneDepthTexture = sceneDepthTexture;
                gridRc.mainViewport = RhiViewport(
                    0.0f,
                    0.0f,
                    static_cast<float>(editorSceneColor->GetWidth()),
                    static_cast<float>(editorSceneColor->GetHeight()));
                gridRc.renderWidth = editorSceneColor->GetWidth();
                gridRc.renderHeight = editorSceneColor->GetHeight();
                gridRc.viewMatrix = primaryView.state.viewMatrix;
                gridRc.projectionMatrix = primaryView.state.projectionMatrix;
                gridRc.viewProjectionUnjittered = primaryView.state.viewProjectionUnjittered;
                gridRc.prevViewProjectionMatrix = primaryView.state.prevViewProjectionMatrix;
                gridRc.cameraPosition = primaryView.state.cameraPosition;
                gridRc.cameraDirection = primaryView.state.cameraDirection;
                gridRc.fovY = primaryView.state.fovY;
                gridRc.aspect = primaryView.state.aspect;
                gridRc.nearZ = primaryView.state.nearZ;
                gridRc.farZ = primaryView.state.farZ;
                gridRc.jitterOffset = primaryView.state.jitterOffset;
                gridRc.prevJitterOffset = primaryView.state.prevJitterOffset;

                rc.commandList->TransitionBarrier(editorSceneColor, ResourceState::RenderTarget);
                rc.commandList->TransitionBarrier(sceneDepthTexture, ResourceState::DepthRead);
                rc.commandList->SetRenderTarget(editorSceneColor, sceneDepthTexture);
                rc.commandList->SetViewport(gridRc.mainViewport);

                GridRenderSystem::EditorGridSettings gridSettings{};
                gridSettings.cellSize = m_editorLayer->GetSceneGridCellSize();
                gridSettings.halfLineCount = m_editorLayer->GetSceneGridHalfLineCount();
                m_editorGridRenderSystem.RenderEditorGrid(gridRc, gridSettings);

                // Grid の後に gizmo も重ねる。
                renderedPrimaryViewGizmos = renderPrimaryViewGizmos(editorSceneColor, sceneDepthTexture);

                rc.commandList->TransitionBarrier(editorSceneColor, ResourceState::ShaderResource);
                rc.commandList->TransitionBarrier(sceneViewTexture, ResourceState::ShaderResource);
                rc.commandList->TransitionBarrier(sceneDepthTexture, ResourceState::ShaderResource);

                editorScenePreviewTexture = editorSceneColor;
                sceneViewTexture = editorSceneColor;
            }
        }

        // Grid 後描画指定ならここで gizmo を描く。
        if (deferPrimaryViewGizmos && !renderedPrimaryViewGizmos) {
            renderPrimaryViewGizmos(sceneViewTexture, sceneDepthTexture);
        }

        // EditorLayer へ各プレビュー用テクスチャを渡す。
        m_editorLayer->SetSceneViewTexture(sceneViewTexture);
        m_editorLayer->SetGameViewTexture(gameViewTexture);
        m_editorLayer->SetGBufferDebugTextures(
            primaryView.debugGBuffer0 ? primaryView.debugGBuffer0 : rc.debugGBuffer0,
            primaryView.debugGBuffer1 ? primaryView.debugGBuffer1 : rc.debugGBuffer1,
            primaryView.debugGBuffer2 ? primaryView.debugGBuffer2 : rc.debugGBuffer2,
            nullptr,
            primaryView.debugDepth ? primaryView.debugDepth : rc.debugGBufferDepth);

        if (m_editorLayer->IsPlayerWorkspaceActive()) {
            m_editorLayer->SetPlayerPreviewTexture(sceneViewTexture);
        }
        else if (m_editorLayer->ShouldRenderPlayerPreview()) {
            RenderPlayerPreviewOffscreen();
        }
        else if (m_editorLayer->ShouldRenderEffectPreview()) {
            ITexture* effectPreviewTexture = primaryView.sceneViewTexture
                ? primaryView.sceneViewTexture
                : primaryView.displayTexture;
            m_editorLayer->SetEffectPreviewTexture(effectPreviewTexture);
        }
    }

    // 定期的に DX12 パフォーマンス統計をログ出力する。
    static uint64_t s_perfLogFrame = 0;
    if ((s_perfLogFrame++ % 120ull) == 0ull) {
        LOG_INFO(
            "[DX12Perf] sceneUpload=%.3fms fg(setup=%.3f compile=%.3f execute=%.3f) submit=%.3fms "
            "extract=%.3fms visible=%.3fms instance=%.3fms indirect=%.3fms async(submit=%.3f gpu=%.3f wait=%u async=%u fallback=%u) "
            "opaque=%u transparent=%u batches=%u matGroups=%u visibleBatches=%u visibleInstances=%u gpuCandidates=%u/%u hit=%.2f avgVisible=%.2f "
            "preparedDraw=%u preparedSkinned=%u gpuGroups=%u gpuReduce=%.2f matResolves=%u maxBatch=%u "
            "packetGrow=(%u,%u) batchGrow=%u sort=%.3fms visibleScratchGrow=%u preparedGrow=(inst:%u,batch:%u) "
            "indirectGrow=(gpu:%u,scratch:%u,args:%u,meta:%u) split=(nonSkin:%u,skin:%u packets:%u/%u)",
            rc.prepMetrics.sceneUploadMs,
            rc.prepMetrics.frameGraphSetupMs,
            rc.prepMetrics.frameGraphCompileMs,
            rc.prepMetrics.frameGraphExecuteMs,
            rc.prepMetrics.submitFrameMs,
            m_renderQueue.metrics.meshExtractMs,
            rc.prepMetrics.visibleExtractMs,
            rc.prepMetrics.instanceBuildMs,
            rc.prepMetrics.indirectBuildMs,
            rc.prepMetrics.asyncComputeSubmitMs,
            rc.prepMetrics.asyncComputeGpuMs,
            rc.prepMetrics.asyncComputeWaitCount,
            rc.prepMetrics.asyncComputeDispatchCount,
            rc.prepMetrics.asyncComputeFallbackCount,
            m_renderQueue.metrics.opaquePacketCount,
            m_renderQueue.metrics.transparentPacketCount,
            m_renderQueue.metrics.opaqueBatchCount,
            m_renderQueue.metrics.materialGroupCount,
            rc.prepMetrics.visibleBatchCount,
            rc.prepMetrics.visibleInstanceCount,
            rc.prepMetrics.gpuDrivenCandidateBatchCount,
            rc.prepMetrics.gpuDrivenCandidateInstanceCount,
            rc.prepMetrics.visibleInstanceHitRate,
            rc.prepMetrics.averageInstancesPerVisibleBatch,
            rc.prepMetrics.preparedIndirectCount,
            rc.prepMetrics.preparedSkinnedCount,
            rc.prepMetrics.gpuDrivenDispatchGroupCount,
            rc.prepMetrics.gpuDrivenDispatchReduction,
            m_renderQueue.metrics.materialResolveCount,
            m_renderQueue.metrics.maxInstancesPerBatch,
            m_renderQueue.metrics.opaquePacketVectorGrowths,
            m_renderQueue.metrics.transparentPacketVectorGrowths,
            m_renderQueue.metrics.opaqueBatchVectorGrowths,
            m_renderQueue.metrics.batchSortMs,
            rc.prepMetrics.visibleScratchVectorGrowths,
            rc.prepMetrics.preparedInstanceVectorGrowths,
            rc.prepMetrics.preparedBatchVectorGrowths,
            rc.prepMetrics.indirectBufferReallocs,
            rc.prepMetrics.indirectScratchVectorGrowths,
            rc.prepMetrics.drawArgsVectorGrowths,
            rc.prepMetrics.metadataVectorGrowths,
            rc.prepMetrics.nonSkinnedCommandCount,
            rc.prepMetrics.skinnedCommandCount,
            m_renderQueue.metrics.nonSkinnedOpaquePacketCount,
            m_renderQueue.metrics.skinnedOpaquePacketCount);
    }

    m_renderPipeline->EndFrame(rc);

    // UI 表示用に必要なテクスチャは ShaderResource へ戻す。
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto transitionTextureForUi = [&](ITexture* texture) {
            if (texture) {
                rc.commandList->TransitionBarrier(texture, ResourceState::ShaderResource);
            }
            };
        transitionTextureForUi(rc.sceneColorTexture);
        transitionTextureForUi(rc.sceneDepthTexture);
        transitionTextureForUi(rc.debugGBuffer0);
        transitionTextureForUi(rc.debugGBuffer1);
        transitionTextureForUi(rc.debugGBuffer2);
        transitionTextureForUi(rc.debugGBufferDepth);
        transitionTextureForUi(editorScenePreviewTexture);
    }

    // Editor UI 描画。
    if (m_editorLayer) m_editorLayer->RenderUI();

    // 各 snapshot state 参照を取る。
    TextureSnapshotState& displaySnapshot = GetDisplaySnapshotState();
    TextureSnapshotState& backBufferSnapshot = GetBackBufferSnapshotState();
    TextureSnapshotState& sceneSnapshot = GetSceneSnapshotState();
    TextureSnapshotState& sceneOpaqueSnapshot = GetSceneOpaqueSnapshotState();
    TextureSnapshotState& gbuffer0Snapshot = GetGBuffer0SnapshotState();
    TextureSnapshotState& gbuffer1Snapshot = GetGBuffer1SnapshotState();
    TextureSnapshotState& gbuffer2Snapshot = GetGBuffer2SnapshotState();
    TextureSnapshotState& gbufferDepthSnapshot = GetGBufferDepthSnapshotState();
    TextureSnapshotState& shadowMapSnapshot = GetShadowMapSnapshotState();

    const bool shouldCaptureDisplay = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && s_perfLogFrame >= kDiagnosticStartFrame
        && displaySnapshot.captureCount < kMaxDiagnosticFrames;

    const bool shouldCaptureBackBuffer = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && s_perfLogFrame >= kDiagnosticStartFrame
        && backBufferSnapshot.captureCount < kMaxDiagnosticFrames;

    const bool shouldCaptureScene = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && s_perfLogFrame >= kDiagnosticStartFrame
        && sceneSnapshot.captureCount < kMaxDiagnosticFrames;

    const bool hasOpaqueGeometry = !m_renderQueue.opaquePackets.empty() || !m_renderQueue.opaqueInstanceBatches.empty();

    const bool shouldCaptureSceneOpaque = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && s_perfLogFrame >= kDiagnosticStartFrame
        && sceneOpaqueSnapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry;

    const bool shouldCaptureGBuffer0 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && s_perfLogFrame >= kDiagnosticStartFrame
        && gbuffer0Snapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry && rc.debugGBuffer0;

    const bool shouldCaptureGBuffer1 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && s_perfLogFrame >= kDiagnosticStartFrame
        && gbuffer1Snapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry && rc.debugGBuffer1;

    const bool shouldCaptureGBuffer2 = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && s_perfLogFrame >= kDiagnosticStartFrame
        && gbuffer2Snapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry && rc.debugGBuffer2;

    const bool shouldCaptureGBufferDepth = kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && s_perfLogFrame >= kDiagnosticStartFrame
        && gbufferDepthSnapshot.captureCount < kMaxDiagnosticFrames && hasOpaqueGeometry && rc.debugGBufferDepth;

    const bool shouldCaptureShadowMap = Graphics::Instance().GetAPI() == GraphicsAPI::DX12
        && shadowMapSnapshot.captureCount < 4u
        && rc.shadowMap
        && rc.shadowMap->GetTexture();

    // GBuffer キャプチャ状態を一度だけログする。
    if (kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        static bool s_loggedGBufferCaptureState = false;
        if (!s_loggedGBufferCaptureState) {
            LOG_INFO("[GBufferCaptureState] hasOpaque=%d dbg0=%p dbg1=%p dbg2=%p dbgDepth=%p capture=(%d,%d,%d,%d)",
                hasOpaqueGeometry ? 1 : 0,
                rc.debugGBuffer0,
                rc.debugGBuffer1,
                rc.debugGBuffer2,
                rc.debugGBufferDepth,
                shouldCaptureGBuffer0 ? 1 : 0,
                shouldCaptureGBuffer1 ? 1 : 0,
                shouldCaptureGBuffer2 ? 1 : 0,
                shouldCaptureGBufferDepth ? 1 : 0);
            s_loggedGBufferCaptureState = true;
        }
    }

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        auto* dx12Cmd = static_cast<DX12CommandList*>(rc.commandList);

        dx12Cmd->FlushResourceBarriers();

        // ImGui を DX12 コマンドリストへ描画。
        ImGuiRenderer::RenderDX12(dx12Cmd->GetNativeCommandList());

        // ImGui が descriptor heap を触るので復元する。
        dx12Cmd->RestoreDescriptorHeap();

        // 必要なら snapshot を予約する。
        if (shouldCaptureBackBuffer) {
            auto* backBufferTexture = dynamic_cast<DX12Texture*>(Graphics::Instance().GetBackBufferTexture());
            ScheduleTextureSnapshot(dx12Cmd, backBufferTexture, backBufferSnapshot);
        }
        if (shouldCaptureDisplay) {
            auto* displayTexture = dynamic_cast<DX12Texture*>(rc.sceneColorTexture);
            ScheduleTextureSnapshot(dx12Cmd, displayTexture, displaySnapshot);
        }
        if (shouldCaptureScene) {
            auto* sceneTexture = dynamic_cast<DX12Texture*>(rc.sceneColorTexture);
            ScheduleTextureSnapshot(dx12Cmd, sceneTexture, sceneSnapshot);
        }
        if (shouldCaptureSceneOpaque) {
            auto* sceneTexture = dynamic_cast<DX12Texture*>(rc.sceneColorTexture);
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
        if (shouldCaptureGBufferDepth) {
            auto* gbufferDepth = dynamic_cast<DX12Texture*>(rc.debugGBufferDepth);
            ScheduleTextureSnapshot(dx12Cmd, gbufferDepth, gbufferDepthSnapshot);
        }
        if (shouldCaptureShadowMap) {
            auto* shadowTexture = dynamic_cast<DX12Texture*>(rc.shadowMap->GetTexture());
            ScheduleTextureSnapshot(dx12Cmd, shadowTexture, shadowMapSnapshot);
        }
    }

    // フレーム送信。
    m_renderPipeline->SubmitFrame(rc);

    // 分離ウィンドウ描画。
    if (m_editorLayer) {
        m_editorLayer->RenderDetachedWindows();
    }

    // DX12 debug messages を必要なら flush。
    if (kEnableDx12RuntimeDiagnostics && Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        Graphics::Instance().GetDX12Device()->FlushDebugMessages();
    }

    // snapshot を取得したフレームだけ GPU 待機してログ出力する。
    if (shouldCaptureDisplay || shouldCaptureBackBuffer || shouldCaptureScene || shouldCaptureSceneOpaque || shouldCaptureGBuffer0 || shouldCaptureGBuffer1 || shouldCaptureGBuffer2 || shouldCaptureGBufferDepth || shouldCaptureShadowMap) {
        Graphics::Instance().GetDX12Device()->WaitForGPU();
        LogTextureSnapshot("DisplaySnapshot", displaySnapshot);
        LogTextureSnapshot("BackBufferSnapshot", backBufferSnapshot);
        LogTextureSnapshot("SceneSnapshot", sceneSnapshot);
        LogTextureSnapshot("SceneOpaqueSnapshot", sceneOpaqueSnapshot);
        LogTextureSnapshot("GBuffer0Snapshot", gbuffer0Snapshot);
        LogTextureSnapshot("GBuffer1Snapshot", gbuffer1Snapshot);
        LogTextureSnapshot("GBuffer2Snapshot", gbuffer2Snapshot);
        LogTextureSnapshot("GBufferDepthSnapshot", gbufferDepthSnapshot);
        /* LogTextureSnapshot("ShadowMapSnapshot", shadowMapSnapshot); */

        LogSceneOverGBufferMask(gbuffer0Snapshot, sceneOpaqueSnapshot);
        LogDepthOverGBufferMask(gbuffer0Snapshot, gbufferDepthSnapshot);
        LogGBufferStatsOverAlbedoMask(gbuffer0Snapshot, gbuffer1Snapshot, gbufferDepthSnapshot);
        LogMaskedSnapshotDiff("SceneOverAlbedoMaskDiff", gbuffer0Snapshot, sceneSnapshot);
        LogSceneViewPresentationDiff(m_editorLayer.get(), sceneSnapshot, backBufferSnapshot);
        LogSceneViewModelPresentationDiff(m_editorLayer.get(), gbuffer0Snapshot, sceneSnapshot, backBufferSnapshot);
    }
}

// Editor -> Play へ遷移する。
void EngineKernel::Play()
{
    if (mode == EngineMode::Editor) {
        if (m_gameLayer) {
            PlayerRuntimeSetup::EnsureAllPlayerRuntimeComponents(m_gameLayer->GetRegistry(), true);
        }
        mode = EngineMode::Play;
        if (m_editorLayer && m_gameLayer) {
            m_editorLayer->GetInputBridge().OnPlayStarted(m_gameLayer->GetRegistry());

            // Editor で開いていた scene path を退避する (Stop 時に復元)。
            m_savedEditorScenePath = m_editorLayer->GetCurrentScenePath();
        }

        // GameLoop を起動する。startNode の scene を frame 末尾で load する。
        GameLoopRuntime& rt = m_gameLoopRuntime;
        rt.Reset();
        rt.isActive = true;

        if (!m_gameLoopAsset.nodes.empty()) {
            const GameLoopNode* startNode = m_gameLoopAsset.FindNode(m_gameLoopAsset.startNodeId);
            if (startNode) {
                rt.pendingNodeId            = startNode->id;
                rt.pendingScenePath         = startNode->scenePath;
                rt.sceneTransitionRequested = true;
                rt.forceReload              = true;
            }
            else {
                LOG_ERROR("[GameLoop] startNodeId %u not found in asset", m_gameLoopAsset.startNodeId);
                rt.isActive = false;
            }
        }
        else {
            LOG_WARN("[GameLoop] asset has no nodes; GameLoop disabled");
            rt.isActive = false;
        }
    }
    else if (mode == EngineMode::Pause) {
        mode = EngineMode::Play;
    }

    m_stepFrameRequested = false;
}

// Play / Pause -> Editor へ戻る。
void EngineKernel::Stop()
{
    if (mode == EngineMode::Play || mode == EngineMode::Pause) {
        mode = EngineMode::Editor;
        if (m_editorLayer && m_gameLayer) {
            m_editorLayer->GetInputBridge().OnPlayStopped(m_gameLayer->GetRegistry());
        }

        // GameLoop runtime をリセットする。
        m_gameLoopRuntime.Reset();
        m_uiButtonClickQueue.Clear();

        // Editor scene を復元する (Play 開始時の scene へ戻る)。
        if (m_editorLayer && !m_savedEditorScenePath.empty()) {
            m_editorLayer->LoadSceneFromPath(m_savedEditorScenePath);
        }
        m_savedEditorScenePath.clear();
    }

    m_stepFrameRequested = false;
}

// Play と Pause をトグルする。
void EngineKernel::Pause()
{
    if (mode == EngineMode::Play) mode = EngineMode::Pause;
    else if (mode == EngineMode::Pause) mode = EngineMode::Play;

    m_stepFrameRequested = false;
}

// Pause 中に 1 フレームだけ進める要求を出す。
void EngineKernel::Step()
{
    if (mode == EngineMode::Pause) {
        m_stepFrameRequested = true;
    }
}

// シーン切り替え時に render/audio 状態を安全にリセットする。
void EngineKernel::ResetRenderStateForSceneChange()
{
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        if (auto* dx12 = Graphics::Instance().GetDX12Device()) {
            dx12->WaitForGPU();
        }
    }

    if (m_audioWorld) {
        m_audioWorld->ResetForSceneChange();
    }

    if (m_renderPipeline) {
        m_renderPipeline->ResetForSceneChange();
    }

    m_renderQueue.Clear();
}
