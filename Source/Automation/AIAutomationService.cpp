#include "Automation/AIAutomationService.h"
#include "Automation/WebSocketServer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <memory>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <nlohmann/json.hpp>
#include <windows.h>

#include "Archetype/Archetype.h"
#include "Asset/PrefabSystem.h"
#include "Asset/AssetManager.h"
#include "Camera/Camera2DUtils.h"
#include "Component/HierarchyComponent.h"
#include "Component/EffectAssetComponent.h"
#include "Component/EffectPlaybackComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/EffectSpawnRequestComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Console/Logger.h"
#include "Duplicate/DuplicateSystem.h"
#include "Engine/EditorSelection.h"
#include "Engine/EngineKernel.h"
#include "EffectRuntime/EffectCompiler.h"
#include "EffectRuntime/EffectGraphAsset.h"
#include "EffectRuntime/EffectGraphSerializer.h"
#include "EffectEditor/EffectEditorPanelInternal.h"
#include "Generated/ComponentMeta.generated.h"
#include "Hierarchy/HierarchySystem.h"
#include "Layer/EditorLayer.h"
#include "Layer/GameLayer.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "Render/Graphics.h"
#include "RHI/DX11/DX11Texture.h"
#include "System/PathResolver.h"
#include "System/ResourceManager.h"
#include "Material/MaterialAsset.h"
#include "Terrain/TerrainAssetIO.h"
#include "Terrain/TerrainComponent.h"
#include "Terrain/TerrainGpuPipeline.h"
#include "Vegetation/GrassComponent.h"
#include "System/UndoSystem.h"
#include "Undo/ComponentUndoAction.h"
#include "Undo/EntitySnapshot.h"
#include "Undo/EntityUndoActions.h"
#include "PlayerEditor/PlayerEditorSession.h"
#include "Gameplay/CoinGameSystem.h"
#include "GameLoop/GameLoopAsset.h"
#include "Gameplay/CoinTagComponent.h"
#include "Component/NodeSocket.h"
#include "Component/ColliderComponent.h"
#include "Input/InputActionMapAsset.h"
#include "Component/EnvironmentComponent.h"
#include "Component/PostEffectComponent.h"
#include "UIEditor/UIEditorState.h"
#include "Sequencer/CinematicSequenceAsset.h"
#include "Asset/ModelAssetSerializer.h"

namespace
{
    using json = nlohmann::json;

    constexpr int kProtocolVersion = 1;

    enum class PathAccess
    {
        ReadAsset,
        WriteAsset,
        ReadScene,
        WriteScene,
        AutomationFile
    };

    std::string MakeTimestampSuffix()
    {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return std::to_string(millis);
    }

    std::string EntityToString(EntityID entity)
    {
        if (Entity::IsNull(entity)) {
            return "null";
        }
        return std::to_string(entity);
    }

    EntityID EntityFromJson(const json& value)
    {
        if (value.is_null()) {
            return Entity::NULL_ID;
        }
        if (value.is_number_unsigned()) {
            return value.get<EntityID>();
        }
        if (value.is_number_integer()) {
            const auto signedValue = value.get<int64_t>();
            return signedValue < 0 ? Entity::NULL_ID : static_cast<EntityID>(signedValue);
        }
        if (value.is_string()) {
            const std::string text = value.get<std::string>();
            if (text.empty() || text == "null") {
                return Entity::NULL_ID;
            }
            try {
                return static_cast<EntityID>(std::stoull(text));
            }
            catch (...) {
                return Entity::NULL_ID;
            }
        }
        return Entity::NULL_ID;
    }

    json Float2ToJson(const DirectX::XMFLOAT2& value)
    {
        return json::array({ value.x, value.y });
    }

    json Float3ToJson(const DirectX::XMFLOAT3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    json Float4ToJson(const DirectX::XMFLOAT4& value)
    {
        return json::array({ value.x, value.y, value.z, value.w });
    }

    bool ReadFloat3(const json& in, DirectX::XMFLOAT3& out)
    {
        if (!in.is_array() || in.size() < 3) {
            return false;
        }
        out.x = in[0].get<float>();
        out.y = in[1].get<float>();
        out.z = in[2].get<float>();
        return true;
    }

    bool ReadFloat4(const json& in, DirectX::XMFLOAT4& out)
    {
        if (!in.is_array() || in.size() < 4) {
            return false;
        }
        out.x = in[0].get<float>();
        out.y = in[1].get<float>();
        out.z = in[2].get<float>();
        out.w = in[3].get<float>();
        return true;
    }

    bool ReadFloat2(const json& in, DirectX::XMFLOAT2& out)
    {
        if (!in.is_array() || in.size() < 2) {
            return false;
        }
        out.x = in[0].get<float>();
        out.y = in[1].get<float>();
        return true;
    }

    json MakeError(const std::string& code, const std::string& message, json details = nullptr)
    {
        json error;
        error["code"] = code;
        error["message"] = message;
        if (!details.is_null()) {
            error["details"] = std::move(details);
        }
        return error;
    }

    bool IsPathUnder(const std::filesystem::path& path, const std::filesystem::path& root)
    {
        auto pIt = path.begin();
        auto rIt = root.begin();
        for (; rIt != root.end(); ++rIt, ++pIt) {
            if (pIt == path.end() || *pIt != *rIt) {
                return false;
            }
        }
        return true;
    }

    std::string ToGenericProjectPath(const std::filesystem::path& absolutePath)
    {
        std::filesystem::path root(PathResolver::GetRootPath());
        if (root.empty()) {
            PathResolver::Initialize();
            root = PathResolver::GetRootPath();
        }

        std::error_code ec;
        const auto relative = std::filesystem::relative(absolutePath, root, ec);
        if (ec) {
            return absolutePath.generic_string();
        }
        return relative.generic_string();
    }

    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    std::string AssetTypeToString(AssetType type)
    {
        switch (type) {
        case AssetType::Folder: return "Folder";
        case AssetType::Model: return "Model";
        case AssetType::Texture: return "Texture";
        case AssetType::Font: return "Font";
        case AssetType::Prefab: return "Prefab";
        case AssetType::Script: return "Script";
        case AssetType::Audio: return "Audio";
        case AssetType::Material: return "Material";
        default: return "Unknown";
        }
    }

    AssetType GuessAssetTypeFromPath(const std::filesystem::path& path)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec)) {
            return AssetType::Folder;
        }

        const std::string ext = ToLowerCopy(path.extension().string());
        if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") return AssetType::Model;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga" || ext == ".bmp") return AssetType::Texture;
        if (ext == ".ttf" || ext == ".otf") return AssetType::Font;
        if (ext == ".prefab") return AssetType::Prefab;
        if (ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".cs") return AssetType::Script;
        if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return AssetType::Audio;
        if (ext == ".material" || ext == ".mat") return AssetType::Material;
        if (ext == ".terrain") return AssetType::Unknown;
        return AssetType::Unknown;
    }

    json AssetEntryToJson(const std::filesystem::path& path)
    {
        std::error_code ec;
        const AssetType type = GuessAssetTypeFromPath(path);
        json out = {
            { "name", path.filename().string() },
            { "path", ToGenericProjectPath(path) },
            { "type", AssetTypeToString(type) },
            { "extension", path.extension().string() },
            { "isDirectory", std::filesystem::is_directory(path, ec) }
        };
        if (!out["isDirectory"].get<bool>()) {
            out["size"] = std::filesystem::file_size(path, ec);
            if (ec) {
                out["size"] = nullptr;
            }
        }
        return out;
    }

    std::filesystem::path ResolveProjectPath(const std::string& input, PathAccess access, bool mustExist)
    {
        if (input.empty()) {
            throw MakeError("missing_param", "path is required.");
        }

        if (PathResolver::GetRootPath().empty()) {
            PathResolver::Initialize();
        }

        std::filesystem::path root(PathResolver::GetRootPath());
        std::filesystem::path candidate(input);
        if (!candidate.is_absolute()) {
            candidate = root / candidate;
        }

        candidate = candidate.lexically_normal();
        root = root.lexically_normal();

        std::vector<std::filesystem::path> allowedRoots;
        switch (access) {
        case PathAccess::ReadAsset:
        case PathAccess::WriteAsset:
            allowedRoots = { root / "Data" };
            break;
        case PathAccess::ReadScene:
        case PathAccess::WriteScene:
            allowedRoots = { root / "Data" / "Scene", root / "Saved" / "AI" };
            break;
        case PathAccess::AutomationFile:
            allowedRoots = { root / "Saved" / "AI", root / "Saved" / "Logs" };
            break;
        }

        bool allowed = false;
        for (const auto& allowedRoot : allowedRoots) {
            if (IsPathUnder(candidate, allowedRoot.lexically_normal())) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            throw MakeError("path_not_allowed", "Path is outside the allowed project roots.", {
                { "path", input },
                { "resolvedPath", candidate.string() }
            });
        }

        if (mustExist) {
            std::error_code ec;
            if (!std::filesystem::exists(candidate, ec)) {
                throw MakeError("file_not_found", "Path does not exist.", {
                    { "path", input },
                    { "resolvedPath", candidate.string() }
                });
            }
        }

        return candidate;
    }

    std::string SanitizeFileStem(std::string value)
    {
        if (value.empty()) {
            value = "command";
        }
        for (char& c : value) {
            const bool ok =
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_';
            if (!ok) {
                c = '_';
            }
        }
        return value;
    }

    struct ImageBuffer
    {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> bgra;
    };

    int ClampInt(int value, int low, int high)
    {
        if (value < low) {
            return low;
        }
        if (value > high) {
            return high;
        }
        return value;
    }

    bool IsRGBAFormat(DXGI_FORMAT format)
    {
        return format == DXGI_FORMAT_R8G8B8A8_UNORM ||
            format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    bool IsBGRAFormat(DXGI_FORMAT format)
    {
        return format == DXGI_FORMAT_B8G8R8A8_UNORM ||
            format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    }

    void CopyMappedTextureToBGRA(ImageBuffer& out,
                                 const uint8_t* source,
                                 size_t rowPitch,
                                 int width,
                                 int height,
                                 DXGI_FORMAT format)
    {
        out.width = width;
        out.height = height;
        out.bgra.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);

        const bool rgba = IsRGBAFormat(format);
        for (int y = 0; y < height; ++y) {
            const uint8_t* srcRow = source + static_cast<size_t>(y) * rowPitch;
            for (int x = 0; x < width; ++x) {
                const size_t src = static_cast<size_t>(x) * 4u;
                const size_t dst = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                if (rgba) {
                    out.bgra[dst + 0] = srcRow[src + 2];
                    out.bgra[dst + 1] = srcRow[src + 1];
                    out.bgra[dst + 2] = srcRow[src + 0];
                    out.bgra[dst + 3] = srcRow[src + 3];
                }
                else {
                    out.bgra[dst + 0] = srcRow[src + 0];
                    out.bgra[dst + 1] = srcRow[src + 1];
                    out.bgra[dst + 2] = srcRow[src + 2];
                    out.bgra[dst + 3] = srcRow[src + 3];
                }
            }
        }
    }

    bool CaptureBackBufferDX11(ImageBuffer& out)
    {
        auto& graphics = Graphics::Instance();
        ID3D11Device* device = graphics.GetDevice();
        ID3D11DeviceContext* context = graphics.GetDeviceContext();
        auto* texture = dynamic_cast<DX11Texture*>(graphics.GetBackBufferTexture());
        if (!device || !context || !texture || !texture->GetNativeResource()) {
            return false;
        }

        D3D11_TEXTURE2D_DESC desc{};
        texture->GetNativeResource()->GetDesc(&desc);
        if (desc.Width == 0 || desc.Height == 0 || (!IsRGBAFormat(desc.Format) && !IsBGRAFormat(desc.Format))) {
            return false;
        }

        D3D11_TEXTURE2D_DESC stagingDesc = desc;
        stagingDesc.BindFlags = 0;
        stagingDesc.MiscFlags = 0;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        if (FAILED(device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf()))) {
            return false;
        }

        context->CopyResource(staging.Get(), texture->GetNativeResource());

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
            return false;
        }

        CopyMappedTextureToBGRA(
            out,
            static_cast<const uint8_t*>(mapped.pData),
            mapped.RowPitch,
            static_cast<int>(desc.Width),
            static_cast<int>(desc.Height),
            desc.Format);

        context->Unmap(staging.Get(), 0);
        return true;
    }

    bool CaptureBackBufferDX12(ImageBuffer& out)
    {
        DX12Device* dx12 = Graphics::Instance().GetDX12Device();
        if (!dx12 || !dx12->GetDevice() || !dx12->GetCommandQueue()) {
            return false;
        }

        ID3D12Device* device = dx12->GetDevice();
        ID3D12Resource* source = dx12->GetCurrentBackBuffer();
        if (!source) {
            return false;
        }

        const D3D12_RESOURCE_DESC desc = source->GetDesc();
        if (desc.Width == 0 || desc.Height == 0 || (!IsRGBAFormat(desc.Format) && !IsBGRAFormat(desc.Format))) {
            return false;
        }

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT numRows = 0;
        UINT64 rowSize = 0;
        UINT64 totalBytes = 0;
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSize, &totalBytes);
        if (totalBytes == 0) {
            return false;
        }

        D3D12_HEAP_PROPERTIES readbackHeap{};
        readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
        readbackHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        readbackHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        readbackHeap.CreationNodeMask = 1;
        readbackHeap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufferDesc{};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment = 0;
        bufferDesc.Width = totalBytes;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> readback;
        if (FAILED(device->CreateCommittedResource(
            &readbackHeap,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(readback.GetAddressOf())))) {
            return false;
        }

        // Wait for any in-flight GPU work to complete before issuing the capture
        // barrier. This prevents a validation error when the back buffer is still
        // in D3D12_RESOURCE_STATE_RENDER_TARGET from the previous frame.
        dx12->WaitForGPU();

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.GetAddressOf())))) {
            return false;
        }
        if (FAILED(device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(),
            nullptr,
            IID_PPV_ARGS(commandList.GetAddressOf())))) {
            return false;
        }

        D3D12_RESOURCE_BARRIER toCopy{};
        toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopy.Transition.pResource = source;
        toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        commandList->ResourceBarrier(1, &toCopy);

        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = readback.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = source;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;
        commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER toPresent = toCopy;
        toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &toPresent);

        if (FAILED(commandList->Close())) {
            return false;
        }

        ID3D12CommandList* lists[] = { commandList.Get() };
        dx12->GetCommandQueue()->ExecuteCommandLists(1, lists);

        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) {
            return false;
        }

        HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!eventHandle) {
            return false;
        }

        constexpr uint64_t kFenceValue = 1;
        if (FAILED(dx12->GetCommandQueue()->Signal(fence.Get(), kFenceValue))) {
            CloseHandle(eventHandle);
            return false;
        }
        if (fence->GetCompletedValue() < kFenceValue) {
            if (FAILED(fence->SetEventOnCompletion(kFenceValue, eventHandle))) {
                CloseHandle(eventHandle);
                return false;
            }
            WaitForSingleObject(eventHandle, INFINITE);
        }
        CloseHandle(eventHandle);

        void* mapped = nullptr;
        D3D12_RANGE readRange{ 0, static_cast<SIZE_T>(totalBytes) };
        if (FAILED(readback->Map(0, &readRange, &mapped)) || !mapped) {
            return false;
        }

        const uint8_t* base = static_cast<const uint8_t*>(mapped) + footprint.Offset;
        CopyMappedTextureToBGRA(
            out,
            base,
            footprint.Footprint.RowPitch,
            static_cast<int>(desc.Width),
            static_cast<int>(desc.Height),
            desc.Format);

        D3D12_RANGE writeRange{ 0, 0 };
        readback->Unmap(0, &writeRange);
        return true;
    }

    bool CaptureBackBuffer(ImageBuffer& out)
    {
        auto& graphics = Graphics::Instance();
        if (graphics.GetAPI() == GraphicsAPI::DX12 && CaptureBackBufferDX12(out)) {
            return true;
        }
        if (graphics.GetAPI() == GraphicsAPI::DX11 && CaptureBackBufferDX11(out)) {
            return true;
        }
        return false;
    }

    bool CaptureClientArea(HWND hwnd, ImageBuffer& out)
    {
        if (!hwnd) {
            return false;
        }

        RECT clientRect{};
        if (!GetClientRect(hwnd, &clientRect)) {
            return false;
        }

        const int width = clientRect.right - clientRect.left;
        const int height = clientRect.bottom - clientRect.top;
        if (width <= 0 || height <= 0) {
            return false;
        }

        HDC windowDc = GetDC(hwnd);
        if (!windowDc) {
            return false;
        }

        HDC memoryDc = CreateCompatibleDC(windowDc);
        if (!memoryDc) {
            ReleaseDC(hwnd, windowDc);
            return false;
        }

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = width;
        bitmapInfo.bmiHeader.biHeight = -height;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(windowDc, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap || !pixels) {
            DeleteDC(memoryDc);
            ReleaseDC(hwnd, windowDc);
            return false;
        }

        HGDIOBJ oldObject = SelectObject(memoryDc, bitmap);
        const BOOL copied = BitBlt(memoryDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);

        out.width = width;
        out.height = height;
        out.bgra.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
        if (copied) {
            std::memcpy(out.bgra.data(), pixels, out.bgra.size());
        }

        SelectObject(memoryDc, oldObject);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
        ReleaseDC(hwnd, windowDc);

        return copied == TRUE;
    }

    ImageBuffer CropImage(const ImageBuffer& source, const DirectX::XMFLOAT4& rect)
    {
        const int left = ClampInt(static_cast<int>(rect.x), 0, source.width);
        const int top = ClampInt(static_cast<int>(rect.y), 0, source.height);
        const int right = ClampInt(static_cast<int>(rect.x + rect.z), 0, source.width);
        const int bottom = ClampInt(static_cast<int>(rect.y + rect.w), 0, source.height);

        ImageBuffer out;
        out.width = right > left ? right - left : 0;
        out.height = bottom > top ? bottom - top : 0;
        if (out.width <= 0 || out.height <= 0) {
            return out;
        }

        out.bgra.resize(static_cast<size_t>(out.width) * static_cast<size_t>(out.height) * 4u);
        for (int y = 0; y < out.height; ++y) {
            const size_t srcOffset =
                (static_cast<size_t>(top + y) * static_cast<size_t>(source.width) + static_cast<size_t>(left)) * 4u;
            const size_t dstOffset = static_cast<size_t>(y) * static_cast<size_t>(out.width) * 4u;
            std::memcpy(out.bgra.data() + dstOffset, source.bgra.data() + srcOffset, static_cast<size_t>(out.width) * 4u);
        }
        return out;
    }

    void WriteBmp24(const std::filesystem::path& path, const ImageBuffer& image)
    {
        if (image.width <= 0 || image.height <= 0 || image.bgra.empty()) {
            throw MakeError("capture_failed", "Captured image is empty.", { { "path", path.string() } });
        }

        const uint32_t rowStride = static_cast<uint32_t>((image.width * 3 + 3) & ~3);
        const uint32_t pixelBytes = rowStride * static_cast<uint32_t>(image.height);
        const uint32_t fileSize = 14u + 40u + pixelBytes;

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            throw MakeError("file_write_failed", "Failed to open screenshot output.", { { "path", path.string() } });
        }

        auto writeU16 = [&](uint16_t value) {
            ofs.put(static_cast<char>(value & 0xff));
            ofs.put(static_cast<char>((value >> 8) & 0xff));
        };
        auto writeU32 = [&](uint32_t value) {
            ofs.put(static_cast<char>(value & 0xff));
            ofs.put(static_cast<char>((value >> 8) & 0xff));
            ofs.put(static_cast<char>((value >> 16) & 0xff));
            ofs.put(static_cast<char>((value >> 24) & 0xff));
        };

        writeU16(0x4d42);
        writeU32(fileSize);
        writeU16(0);
        writeU16(0);
        writeU32(54);

        writeU32(40);
        writeU32(static_cast<uint32_t>(image.width));
        writeU32(static_cast<uint32_t>(image.height));
        writeU16(1);
        writeU16(24);
        writeU32(0);
        writeU32(pixelBytes);
        writeU32(0);
        writeU32(0);
        writeU32(0);
        writeU32(0);

        std::vector<uint8_t> row(rowStride, 0);
        for (int y = image.height - 1; y >= 0; --y) {
            std::fill(row.begin(), row.end(), 0);
            for (int x = 0; x < image.width; ++x) {
                const size_t src = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4u;
                const size_t dst = static_cast<size_t>(x) * 3u;
                row[dst + 0] = image.bgra[src + 0];
                row[dst + 1] = image.bgra[src + 1];
                row[dst + 2] = image.bgra[src + 2];
            }
            ofs.write(reinterpret_cast<const char*>(row.data()), row.size());
        }
    }

    // ---- PNG writer (no external dependencies, store-block deflate) ----

    uint32_t Crc32Update(uint32_t crc, const uint8_t* data, size_t len)
    {
        static const auto kTable = []() {
            std::array<uint32_t, 256> t{};
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
                }
                t[i] = c;
            }
            return t;
        }();
        crc = ~crc;
        for (size_t i = 0; i < len; ++i) {
            crc = kTable[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
        }
        return ~crc;
    }

    void WritePng(const std::filesystem::path& path, const ImageBuffer& image)
    {
        if (image.width <= 0 || image.height <= 0 || image.bgra.empty()) {
            throw MakeError("capture_failed", "Captured image is empty.", { { "path", path.string() } });
        }

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            throw MakeError("file_write_failed", "Failed to open PNG output.", { { "path", path.string() } });
        }

        const int W = image.width;
        const int H = image.height;

        // Helpers
        auto writeBytes = [&](const uint8_t* p, size_t n) {
            ofs.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n));
        };
        auto writeU8 = [&](uint8_t v) { ofs.put(static_cast<char>(v)); };
        auto writeU32BE = [&](uint32_t v) {
            uint8_t b[4] = {
                static_cast<uint8_t>(v >> 24),
                static_cast<uint8_t>(v >> 16),
                static_cast<uint8_t>(v >> 8),
                static_cast<uint8_t>(v)
            };
            writeBytes(b, 4);
        };

        // PNG chunk helper: writes length + type + data + CRC
        auto writeChunk = [&](const char type[4], const std::vector<uint8_t>& data) {
            writeU32BE(static_cast<uint32_t>(data.size()));
            writeBytes(reinterpret_cast<const uint8_t*>(type), 4);
            if (!data.empty()) writeBytes(data.data(), data.size());
            uint32_t crc = Crc32Update(0, reinterpret_cast<const uint8_t*>(type), 4);
            if (!data.empty()) crc = Crc32Update(crc, data.data(), data.size());
            writeU32BE(crc);
        };

        // PNG signature
        const uint8_t kSig[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
        writeBytes(kSig, 8);

        // IHDR
        {
            std::vector<uint8_t> ihdr(13);
            auto w32 = [&](int off, uint32_t v) {
                ihdr[off + 0] = static_cast<uint8_t>(v >> 24);
                ihdr[off + 1] = static_cast<uint8_t>(v >> 16);
                ihdr[off + 2] = static_cast<uint8_t>(v >> 8);
                ihdr[off + 3] = static_cast<uint8_t>(v);
            };
            w32(0, static_cast<uint32_t>(W));
            w32(4, static_cast<uint32_t>(H));
            ihdr[8]  = 8;   // bit depth
            ihdr[9]  = 2;   // color type: RGB
            ihdr[10] = 0;   // compression: deflate
            ihdr[11] = 0;   // filter: adaptive
            ihdr[12] = 0;   // interlace: none
            writeChunk("IHDR", ihdr);
        }

        // IDAT: non-compressed deflate store blocks
        // Each row: filter byte 0x00 followed by W*3 RGB bytes (converted from BGRA)
        const size_t rowBytes = static_cast<size_t>(W) * 3u;
        const size_t filteredRowBytes = 1u + rowBytes;
        const size_t rawDataSize = static_cast<size_t>(H) * filteredRowBytes;

        // Build raw (uncompressed) image data
        std::vector<uint8_t> raw;
        raw.reserve(rawDataSize);
        for (int y = 0; y < H; ++y) {
            raw.push_back(0x00); // filter type None
            for (int x = 0; x < W; ++x) {
                const size_t s = (static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)) * 4u;
                raw.push_back(image.bgra[s + 2]); // R
                raw.push_back(image.bgra[s + 1]); // G
                raw.push_back(image.bgra[s + 0]); // B
            }
        }

        // Wrap in zlib/deflate stored blocks (non-compressed)
        // zlib header: CMF=0x78 (deflate, 32KB window), FLG computed for FCHECK
        {
            std::vector<uint8_t> zlib;
            zlib.push_back(0x78); // CMF
            // FLG: no dict, level 0; must satisfy (CMF*256+FLG) % 31 == 0
            // 0x78*256 = 30720; 30720 % 31 = 30720 - 991*31 = 30720 - 30721... recalc:
            // 991*31=30721, so need FLG = 1 so 30721%31=0. Actually 30720+FLG divisible by 31.
            // 30720 mod 31: 30720/31=990 rem 30, so FLG=1 -> 30721/31=991 exactly.
            zlib.push_back(0x01); // FLG

            // Adler-32 accumulators
            uint32_t s1 = 1, s2 = 0;
            constexpr uint32_t MOD_ADLER = 65521;

            size_t offset = 0;
            const size_t totalRaw = raw.size();
            constexpr size_t kMaxBlock = 65535;

            while (offset < totalRaw || totalRaw == 0) {
                const size_t blockSize = (kMaxBlock < totalRaw - offset) ? kMaxBlock : (totalRaw - offset);
                const bool bfinal = (offset + blockSize >= totalRaw);

                // BFINAL | BTYPE=00 (stored)
                zlib.push_back(static_cast<uint8_t>(bfinal ? 0x01 : 0x00));
                // LEN
                const uint16_t len16 = static_cast<uint16_t>(blockSize);
                zlib.push_back(static_cast<uint8_t>(len16 & 0xff));
                zlib.push_back(static_cast<uint8_t>(len16 >> 8));
                // NLEN
                const uint16_t nlen16 = ~len16;
                zlib.push_back(static_cast<uint8_t>(nlen16 & 0xff));
                zlib.push_back(static_cast<uint8_t>(nlen16 >> 8));

                for (size_t i = 0; i < blockSize; ++i) {
                    const uint8_t b = raw[offset + i];
                    zlib.push_back(b);
                    s1 = (s1 + b) % MOD_ADLER;
                    s2 = (s2 + s1) % MOD_ADLER;
                }

                offset += blockSize;
                if (bfinal) break;
                if (totalRaw == 0) break;
            }

            // Adler-32 checksum (big-endian)
            const uint32_t adler = (s2 << 16) | s1;
            zlib.push_back(static_cast<uint8_t>(adler >> 24));
            zlib.push_back(static_cast<uint8_t>(adler >> 16));
            zlib.push_back(static_cast<uint8_t>(adler >> 8));
            zlib.push_back(static_cast<uint8_t>(adler));

            writeChunk("IDAT", zlib);
        }

        // IEND
        writeChunk("IEND", {});
    }

    // ---- Base64 encode ----

    std::string Base64Encode(const uint8_t* data, size_t len)
    {
        static constexpr char kTable[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string out;
        out.reserve(((len + 2u) / 3u) * 4u);
        for (size_t i = 0; i < len; i += 3u) {
            const uint32_t b0 = data[i];
            const uint32_t b1 = (i + 1u < len) ? data[i + 1u] : 0u;
            const uint32_t b2 = (i + 2u < len) ? data[i + 2u] : 0u;
            const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

            out.push_back(kTable[(triple >> 18) & 0x3fu]);
            out.push_back(kTable[(triple >> 12) & 0x3fu]);
            out.push_back((i + 1u < len) ? kTable[(triple >> 6) & 0x3fu] : '=');
            out.push_back((i + 2u < len) ? kTable[triple & 0x3fu] : '=');
        }
        return out;
    }

    std::string JsonStringValue(const json& object, const char* key, std::string fallback = {})
    {
        if (!object.is_object()) {
            return fallback;
        }

        const auto it = object.find(key);
        if (it == object.end() || it->is_null()) {
            return fallback;
        }
        if (it->is_string()) {
            return it->get<std::string>();
        }
        if (it->is_number_integer()) {
            return std::to_string(it->get<int64_t>());
        }
        if (it->is_number_unsigned()) {
            return std::to_string(it->get<uint64_t>());
        }
        return fallback;
    }

    json MakeResult(const json& command, bool ok, json result, json error)
    {
        json out;
        out["version"] = kProtocolVersion;
        out["id"] = JsonStringValue(command, "id");
        out["command"] = JsonStringValue(command, "command");
        out["ok"] = ok;
        out["result"] = ok ? std::move(result) : nullptr;
        out["error"] = ok ? nullptr : std::move(error);
        return out;
    }

    const char* ModeToString(EngineMode mode)
    {
        switch (mode) {
        case EngineMode::Editor: return "Editor";
        case EngineMode::Play: return "Play";
        case EngineMode::Pause: return "Pause";
        default: return "Unknown";
        }
    }

    const char* SceneViewModeToString(EditorLayer::SceneViewMode mode)
    {
        return mode == EditorLayer::SceneViewMode::Mode2D ? "2D" : "3D";
    }

    template<typename T>
    void AppendComponentNameIfPresent(const Signature& signature, json& components)
    {
        const ComponentTypeID typeId = TypeManager::GetComponentTypeID<T>();
        if (typeId < MAX_COMPONENTS && signature.test(typeId)) {
            components.push_back(std::string(ComponentMeta<T>::Name));
        }
    }

    json ComponentNamesFromSignature(const Signature& signature)
    {
        json components = json::array();
        std::apply(
            [&](auto... component) {
                (AppendComponentNameIfPresent<std::decay_t<decltype(component)>>(signature, components), ...);
            },
            AllComponentTypes{});
        return components;
    }

    template<typename T>
    bool ComponentNameEquals(const std::string& componentName)
    {
        return componentName == ComponentMeta<T>::Name;
    }

    template<typename T>
    constexpr bool IsXMFLOAT2 = std::is_same_v<std::decay_t<T>, DirectX::XMFLOAT2>;

    template<typename T>
    constexpr bool IsXMFLOAT3 = std::is_same_v<std::decay_t<T>, DirectX::XMFLOAT3>;

    template<typename T>
    constexpr bool IsXMFLOAT4 = std::is_same_v<std::decay_t<T>, DirectX::XMFLOAT4>;

    template<typename T>
    constexpr bool IsEditableFieldType()
    {
        using U = std::decay_t<T>;
        return std::is_same_v<U, std::string> ||
            std::is_same_v<U, bool> ||
            std::is_integral_v<U> ||
            std::is_floating_point_v<U> ||
            std::is_enum_v<U> ||
            IsXMFLOAT2<U> ||
            IsXMFLOAT3<U> ||
            IsXMFLOAT4<U>;
    }

    template<typename T>
    std::string FieldTypeName()
    {
        using U = std::decay_t<T>;
        if constexpr (std::is_same_v<U, std::string>) {
            return "string";
        }
        else if constexpr (std::is_same_v<U, bool>) {
            return "bool";
        }
        else if constexpr (std::is_enum_v<U>) {
            return "enum";
        }
        else if constexpr (std::is_integral_v<U>) {
            return std::is_signed_v<U> ? "int" : "uint";
        }
        else if constexpr (std::is_floating_point_v<U>) {
            return "float";
        }
        else if constexpr (IsXMFLOAT2<U>) {
            return "float2";
        }
        else if constexpr (IsXMFLOAT3<U>) {
            return "float3";
        }
        else if constexpr (IsXMFLOAT4<U>) {
            return "float4";
        }
        else {
            return "unsupported";
        }
    }

    template<typename T>
    json FieldValueToJson(const T& value)
    {
        using U = std::decay_t<T>;
        if constexpr (std::is_same_v<U, std::string>) {
            return value;
        }
        else if constexpr (std::is_same_v<U, bool>) {
            return value;
        }
        else if constexpr (std::is_enum_v<U>) {
            return static_cast<std::underlying_type_t<U>>(value);
        }
        else if constexpr (std::is_integral_v<U> || std::is_floating_point_v<U>) {
            return value;
        }
        else if constexpr (IsXMFLOAT2<U>) {
            return Float2ToJson(value);
        }
        else if constexpr (IsXMFLOAT3<U>) {
            return Float3ToJson(value);
        }
        else if constexpr (IsXMFLOAT4<U>) {
            return Float4ToJson(value);
        }
        else {
            return nullptr;
        }
    }

    template<typename T>
    bool ReadFieldValue(const json& in, T& out)
    {
        using U = std::decay_t<T>;
        try {
            if constexpr (std::is_same_v<U, std::string>) {
                if (!in.is_string()) {
                    return false;
                }
                out = in.get<std::string>();
                return true;
            }
            else if constexpr (std::is_same_v<U, bool>) {
                if (!in.is_boolean()) {
                    return false;
                }
                out = in.get<bool>();
                return true;
            }
            else if constexpr (std::is_enum_v<U>) {
                if (!in.is_number_integer()) {
                    return false;
                }
                out = static_cast<U>(in.get<std::underlying_type_t<U>>());
                return true;
            }
            else if constexpr (std::is_integral_v<U>) {
                if (!in.is_number_integer() && !in.is_number_unsigned()) {
                    return false;
                }
                out = in.get<U>();
                return true;
            }
            else if constexpr (std::is_floating_point_v<U>) {
                if (!in.is_number()) {
                    return false;
                }
                out = in.get<U>();
                return true;
            }
            else if constexpr (IsXMFLOAT2<U>) {
                if (!in.is_array() || in.size() < 2) {
                    return false;
                }
                out.x = in[0].get<float>();
                out.y = in[1].get<float>();
                return true;
            }
            else if constexpr (IsXMFLOAT3<U>) {
                return ReadFloat3(in, out);
            }
            else if constexpr (IsXMFLOAT4<U>) {
                return ReadFloat4(in, out);
            }
            else {
                return false;
            }
        }
        catch (...) {
            return false;
        }
    }

    template<typename Component, typename FieldInfo>
    json ComponentFieldSchema(const FieldInfo& field)
    {
        using FieldType = std::decay_t<decltype(std::declval<Component>().*field.ptr)>;
        return {
            { "name", std::string(field.name) },
            { "type", FieldTypeName<FieldType>() },
            { "editable", IsEditableFieldType<FieldType>() }
        };
    }

    template<typename Component, typename FieldInfo>
    void AppendComponentFieldValue(const Component& component, const FieldInfo& field, json& fields)
    {
        fields[std::string(field.name)] = FieldValueToJson(component.*field.ptr);
    }

    template<typename Component, typename FieldInfo>
    bool TryApplyComponentField(Component& component, const FieldInfo& field, const json& fields, json& applied, json& rejected)
    {
        const std::string name(field.name);
        if (!fields.contains(name)) {
            return false;
        }

        using FieldType = std::decay_t<decltype(component.*field.ptr)>;
        if constexpr (!IsEditableFieldType<FieldType>()) {
            rejected.push_back({
                { "field", name },
                { "reason", "unsupported_type" },
                { "type", FieldTypeName<FieldType>() }
            });
            return true;
        }
        else {
            FieldType next = component.*field.ptr;
            if (!ReadFieldValue(fields[name], next)) {
                rejected.push_back({
                    { "field", name },
                    { "reason", "invalid_value" },
                    { "type", FieldTypeName<FieldType>() }
                });
                return true;
            }
            component.*field.ptr = next;
            applied.push_back(name);
            return true;
        }
    }

    template<typename T>
    void SetDirtyIfPossible(T& component)
    {
        if constexpr (std::is_same_v<T, TransformComponent>) {
            component.isDirty = true;
        }
    }

    void MarkEntityEdited(Registry& registry, EntityID entity)
    {
        HierarchySystem::MarkDirtyRecursive(entity, registry);
        PrefabSystem::MarkPrefabOverride(entity, registry);
    }

    EntityID GetEntityParent(Registry& registry, EntityID entity)
    {
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            return hierarchy->parent;
        }
        if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
            return transform->parent == 0 ? Entity::NULL_ID : transform->parent;
        }
        return Entity::NULL_ID;
    }

    bool BuildEntityFocusBounds(Registry& registry, EntityID root, DirectX::XMFLOAT3& outCenter, float& outRadius)
    {
        bool hasBounds = false;
        DirectX::BoundingBox merged{};

        std::vector<EntityID> entities;
        EntitySnapshot::CollectHierarchy(root, registry, entities);
        if (entities.empty() && registry.IsAlive(root)) {
            entities.push_back(root);
        }

        for (EntityID entity : entities) {
            if (auto* mesh = registry.GetComponent<MeshComponent>(entity)) {
                if (mesh->model && mesh->isVisible) {
                    const DirectX::BoundingBox& box = mesh->model->GetWorldBounds();
                    if (!hasBounds) {
                        merged = box;
                        hasBounds = true;
                    }
                    else {
                        DirectX::BoundingBox::CreateMerged(merged, merged, box);
                    }
                    continue;
                }
            }

            if (!hasBounds) {
                if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                    merged.Center = transform->worldPosition;
                    merged.Extents = { 0.5f, 0.5f, 0.5f };
                    hasBounds = true;
                }
            }
        }

        if (!hasBounds) {
            return false;
        }

        outCenter = merged.Center;
        outRadius = std::sqrt(
            merged.Extents.x * merged.Extents.x +
            merged.Extents.y * merged.Extents.y +
            merged.Extents.z * merged.Extents.z);
        return true;
    }

    float ComputeFocusDistanceForRadius(float radius, float fovY)
    {
        const float safeRadius = (std::max)(radius, 0.5f);
        const float halfFov = (std::max)(fovY * 0.5f, 0.1f);
        return (std::max)(2.0f, safeRadius / std::tan(halfFov));
    }

    bool BuildSceneViewRay(EditorLayer& editor,
                           const json& params,
                           DirectX::XMFLOAT3& outOrigin,
                           DirectX::XMFLOAT3& outDirection)
    {
        using namespace DirectX;
        const XMFLOAT4 rect = editor.GetSceneViewRect();
        if (rect.z <= 1.0f || rect.w <= 1.0f) {
            return false;
        }

        float screenX = rect.x + rect.z * 0.5f;
        float screenY = rect.y + rect.w * 0.5f;
        if (params.contains("screenPosition")) {
            const json& p = params["screenPosition"];
            if (!p.is_array() || p.size() < 2) {
                throw MakeError("invalid_param", "screenPosition must be [x, y].");
            }
            screenX = p[0].get<float>();
            screenY = p[1].get<float>();
        }
        else if (params.contains("normalizedPosition")) {
            const json& p = params["normalizedPosition"];
            if (!p.is_array() || p.size() < 2) {
                throw MakeError("invalid_param", "normalizedPosition must be [x, y].");
            }
            screenX = rect.x + p[0].get<float>() * rect.z;
            screenY = rect.y + p[1].get<float>() * rect.w;
        }

        const float localX = (screenX - rect.x) / rect.z;
        const float localY = (screenY - rect.y) / rect.w;
        if (localX < 0.0f || localX > 1.0f || localY < 0.0f || localY > 1.0f) {
            return false;
        }

        const float aspect = rect.z / rect.w;
        const XMFLOAT4X4 viewFloat = editor.GetEditorViewMatrix();
        const XMFLOAT4X4 projectionFloat = editor.BuildEditorProjectionMatrix(aspect);
        const XMMATRIX view = XMLoadFloat4x4(&viewFloat);
        const XMMATRIX projection = XMLoadFloat4x4(&projectionFloat);
        const XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, view * projection);

        const float ndcX = localX * 2.0f - 1.0f;
        const float ndcY = 1.0f - localY * 2.0f;
        const XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverseViewProjection);
        const XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inverseViewProjection);
        const XMVECTOR direction = XMVector3Normalize(farPoint - nearPoint);

        XMStoreFloat3(&outOrigin, nearPoint);
        XMStoreFloat3(&outDirection, direction);
        return true;
    }

    bool IntersectGroundPlane(const DirectX::XMFLOAT3& origin,
                              const DirectX::XMFLOAT3& direction,
                              DirectX::XMFLOAT3& outPoint,
                              float& outDistance)
    {
        const float denom = direction.y;
        if (std::fabs(denom) < 0.0001f) {
            return false;
        }
        const float t = -origin.y / denom;
        if (!std::isfinite(t) || t <= 0.0f) {
            return false;
        }
        outDistance = t;
        outPoint = {
            origin.x + direction.x * t,
            0.0f,
            origin.z + direction.z * t
        };
        return true;
    }

    json RaycastSceneObjects(Registry& registry,
                             const DirectX::XMFLOAT3& origin,
                             const DirectX::XMFLOAT3& direction,
                             float maxDistance)
    {
        EntityID bestEntity = Entity::NULL_ID;
        DirectX::XMFLOAT3 bestPoint{};
        float bestDistance = maxDistance;

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<MeshComponent>())) {
                continue;
            }

            auto* meshColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<MeshComponent>());
            const auto& entities = archetype->GetEntities();
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                EntityID entity = entities[i];
                auto* mesh = static_cast<MeshComponent*>(meshColumn->Get(i));
                if (!mesh || !mesh->model || !mesh->isVisible) {
                    continue;
                }

                RaycastHit hit;
                if (mesh->model->Raycast(origin, direction, hit) && hit.distance < bestDistance) {
                    bestDistance = hit.distance;
                    bestEntity = entity;
                    bestPoint = hit.point;
                    continue;
                }

                float boundsDistance = maxDistance;
                const DirectX::XMVECTOR rayOrigin = DirectX::XMLoadFloat3(&origin);
                const DirectX::XMVECTOR rayDirection = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction));
                if (mesh->model->GetWorldBounds().Intersects(rayOrigin, rayDirection, boundsDistance) &&
                    boundsDistance < bestDistance) {
                    bestDistance = boundsDistance;
                    bestEntity = entity;
                    bestPoint = {
                        origin.x + direction.x * boundsDistance,
                        origin.y + direction.y * boundsDistance,
                        origin.z + direction.z * boundsDistance
                    };
                }
            }
        }

        if (Entity::IsNull(bestEntity)) {
            return nullptr;
        }

        return {
            { "entity", EntityToString(bestEntity) },
            { "distance", bestDistance },
            { "position", Float3ToJson(bestPoint) }
        };
    }

    json EntitySummary(Registry& registry, EntityID entity, const Signature& signature)
    {
        json out;
        out["entity"] = EntityToString(entity);
        out["index"] = Entity::GetIndex(entity);
        out["generation"] = Entity::GetGeneration(entity);
        out["alive"] = registry.IsAlive(entity);
        out["name"] = "Entity " + std::to_string(Entity::GetIndex(entity));
        out["parent"] = nullptr;
        out["children"] = json::array();
        out["components"] = ComponentNamesFromSignature(signature);

        if (auto* name = registry.GetComponent<NameComponent>(entity)) {
            out["name"] = name->name;
        }

        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            if (!Entity::IsNull(hierarchy->parent)) {
                out["parent"] = EntityToString(hierarchy->parent);
            }

            EntityID child = hierarchy->firstChild;
            while (!Entity::IsNull(child)) {
                out["children"].push_back(EntityToString(child));
                auto* childHierarchy = registry.GetComponent<HierarchyComponent>(child);
                child = childHierarchy ? childHierarchy->nextSibling : Entity::NULL_ID;
            }
            out["active"] = hierarchy->isActive;
        }

        return out;
    }

    std::optional<Signature> FindEntitySignature(Registry& registry, EntityID entity)
    {
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& entities = archetype->GetEntities();
            if (std::find(entities.begin(), entities.end(), entity) != entities.end()) {
                return archetype->GetSignature();
            }
        }
        return std::nullopt;
    }

    json TransformToJson(const TransformComponent& transform)
    {
        json out;
        out["localPosition"] = Float3ToJson(transform.localPosition);
        out["localRotation"] = Float4ToJson(transform.localRotation);
        out["localScale"] = Float3ToJson(transform.localScale);
        out["worldPosition"] = Float3ToJson(transform.worldPosition);
        out["worldRotation"] = Float4ToJson(transform.worldRotation);
        out["worldScale"] = Float3ToJson(transform.worldScale);
        return out;
    }

    bool ProjectToSceneView(const EditorLayer& editor, const DirectX::XMFLOAT3& worldPosition, json& outScreen)
    {
        const DirectX::XMFLOAT4 rect = editor.GetSceneViewRect();
        if (rect.z <= 1.0f || rect.w <= 1.0f) {
            return false;
        }

        const float aspect = rect.z / rect.w;
        const DirectX::XMFLOAT4X4 viewFloat = editor.GetEditorViewMatrix();
        const DirectX::XMFLOAT4X4 projectionFloat = editor.BuildEditorProjectionMatrix(aspect);
        const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&viewFloat);
        const DirectX::XMMATRIX projection = DirectX::XMLoadFloat4x4(&projectionFloat);
        const DirectX::XMVECTOR world =
            DirectX::XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
        const DirectX::XMVECTOR clip = DirectX::XMVector4Transform(world, view * projection);

        DirectX::XMFLOAT4 clipFloat{};
        DirectX::XMStoreFloat4(&clipFloat, clip);
        if (clipFloat.w <= 0.0001f) {
            return false;
        }

        const float ndcX = clipFloat.x / clipFloat.w;
        const float ndcY = clipFloat.y / clipFloat.w;
        const bool visible = ndcX >= -1.0f && ndcX <= 1.0f && ndcY >= -1.0f && ndcY <= 1.0f;
        const float screenX = rect.x + (ndcX * 0.5f + 0.5f) * rect.z;
        const float screenY = rect.y + (-ndcY * 0.5f + 0.5f) * rect.w;

        outScreen = {
            { "x", screenX },
            { "y", screenY },
            { "ndc", json::array({ ndcX, ndcY }) },
            { "visibleInSceneView", visible }
        };
        return true;
    }

    bool ProjectWorldToRect(const DirectX::XMFLOAT4& rect,
                            const DirectX::XMFLOAT4X4& viewFloat,
                            const DirectX::XMFLOAT4X4& projectionFloat,
                            const DirectX::XMFLOAT3& worldPosition,
                            json& outScreen)
    {
        if (rect.z <= 1.0f || rect.w <= 1.0f) {
            return false;
        }

        const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&viewFloat);
        const DirectX::XMMATRIX projection = DirectX::XMLoadFloat4x4(&projectionFloat);
        const DirectX::XMVECTOR world =
            DirectX::XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
        const DirectX::XMVECTOR clip = DirectX::XMVector4Transform(world, view * projection);

        DirectX::XMFLOAT4 clipFloat{};
        DirectX::XMStoreFloat4(&clipFloat, clip);
        if (clipFloat.w <= 0.0001f) {
            return false;
        }

        const float ndcX = clipFloat.x / clipFloat.w;
        const float ndcY = clipFloat.y / clipFloat.w;
        const bool visible = ndcX >= -1.0f && ndcX <= 1.0f && ndcY >= -1.0f && ndcY <= 1.0f;
        outScreen = {
            { "x", rect.x + (ndcX * 0.5f + 0.5f) * rect.z },
            { "y", rect.y + (-ndcY * 0.5f + 0.5f) * rect.w },
            { "ndc", json::array({ ndcX, ndcY }) },
            { "visible", visible }
        };
        return true;
    }

    bool ProjectBoundsToRect(const DirectX::XMFLOAT4& rect,
                             const DirectX::XMFLOAT4X4& viewFloat,
                             const DirectX::XMFLOAT4X4& projectionFloat,
                             const DirectX::XMFLOAT3& center,
                             float radius,
                             json& outBounds)
    {
        const float r = (std::max)(radius, 0.05f);
        const std::array<DirectX::XMFLOAT3, 8> points = {
            DirectX::XMFLOAT3{ center.x - r, center.y - r, center.z - r },
            DirectX::XMFLOAT3{ center.x - r, center.y - r, center.z + r },
            DirectX::XMFLOAT3{ center.x - r, center.y + r, center.z - r },
            DirectX::XMFLOAT3{ center.x - r, center.y + r, center.z + r },
            DirectX::XMFLOAT3{ center.x + r, center.y - r, center.z - r },
            DirectX::XMFLOAT3{ center.x + r, center.y - r, center.z + r },
            DirectX::XMFLOAT3{ center.x + r, center.y + r, center.z - r },
            DirectX::XMFLOAT3{ center.x + r, center.y + r, center.z + r }
        };

        bool hasPoint = false;
        bool allProjected = true;
        bool allVisible = true;
        float minX = (std::numeric_limits<float>::max)();
        float minY = (std::numeric_limits<float>::max)();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (const DirectX::XMFLOAT3& point : points) {
            json screen;
            if (!ProjectWorldToRect(rect, viewFloat, projectionFloat, point, screen)) {
                allProjected = false;
                allVisible = false;
                continue;
            }
            hasPoint = true;
            const float x = screen.value("x", 0.0f);
            const float y = screen.value("y", 0.0f);
            minX = (std::min)(minX, x);
            minY = (std::min)(minY, y);
            maxX = (std::max)(maxX, x);
            maxY = (std::max)(maxY, y);
            allVisible = allVisible && screen.value("visible", false);
        }

        if (!hasPoint) {
            return false;
        }

        const float leftMargin = minX - rect.x;
        const float topMargin = minY - rect.y;
        const float rightMargin = (rect.x + rect.z) - maxX;
        const float bottomMargin = (rect.y + rect.w) - maxY;
        const float minMargin = (std::min)((std::min)(leftMargin, rightMargin), (std::min)(topMargin, bottomMargin));
        const float fillX = rect.z > 0.0f ? (maxX - minX) / rect.z : 0.0f;
        const float fillY = rect.w > 0.0f ? (maxY - minY) / rect.w : 0.0f;
        outBounds = {
            { "min", json::array({ minX, minY }) },
            { "max", json::array({ maxX, maxY }) },
            { "size", json::array({ maxX - minX, maxY - minY }) },
            { "margins", {
                { "left", leftMargin },
                { "top", topMargin },
                { "right", rightMargin },
                { "bottom", bottomMargin },
                { "min", minMargin }
            } },
            { "fill", json::array({ fillX, fillY }) },
            { "maxFill", (std::max)(fillX, fillY) },
            { "allProjected", allProjected },
            { "fullyVisible", allProjected && allVisible && minMargin >= 0.0f }
        };
        return true;
    }

    template<typename T>
    json ComponentToJson(const T& component);

    template<typename T>
    void AppendComponentDataIfPresent(Registry& registry, EntityID entity, json& components)
    {
        if (const T* component = registry.GetComponent<T>(entity)) {
            components[std::string(ComponentMeta<T>::Name)] = ComponentToJson(*component);
        }
    }

    json BuildAllComponentData(Registry& registry, EntityID entity)
    {
        json components = json::object();
        std::apply(
            [&](auto... component) {
                (AppendComponentDataIfPresent<std::decay_t<decltype(component)>>(registry, entity, components), ...);
            },
            AllComponentTypes{});
        return components;
    }

    json EntityObservationRecord(Registry& registry, EntityID entity)
    {
        const auto signature = FindEntitySignature(registry, entity);
        json out = EntitySummary(registry, entity, signature.value_or(Signature{}));
        out["componentData"] = BuildAllComponentData(registry, entity);
        return out;
    }

    bool SignatureHasComponentName(const Signature& signature, const std::string& componentName)
    {
        bool matched = false;
        std::apply(
            [&](auto... component) {
                ((ComponentNameEquals<std::decay_t<decltype(component)>>(componentName)
                    ? (matched = signature.test(TypeManager::GetComponentTypeID<std::decay_t<decltype(component)>>()), true)
                    : false), ...);
            },
            AllComponentTypes{});
        return matched;
    }

    std::vector<std::string> JsonStringList(const json& params, const char* key)
    {
        std::vector<std::string> values;
        if (!params.contains(key)) {
            return values;
        }
        const json& in = params[key];
        if (in.is_string()) {
            values.push_back(in.get<std::string>());
            return values;
        }
        if (!in.is_array()) {
            throw MakeError("invalid_param", std::string(key) + " must be a string or string array.");
        }
        for (const json& item : in) {
            if (!item.is_string()) {
                throw MakeError("invalid_param", std::string(key) + " must contain only strings.");
            }
            values.push_back(item.get<std::string>());
        }
        return values;
    }

    bool EntityMatchesNameFilter(Registry& registry, EntityID entity, const std::string& filter)
    {
        if (filter.empty()) {
            return true;
        }
        std::string name = "Entity " + std::to_string(Entity::GetIndex(entity));
        if (auto* nameComponent = registry.GetComponent<NameComponent>(entity)) {
            name = nameComponent->name;
        }
        return ToLowerCopy(name).find(ToLowerCopy(filter)) != std::string::npos;
    }

    bool EntityMatchesQuery(Registry& registry,
                            EntityID entity,
                            const Signature& signature,
                            const std::vector<std::string>& hasComponents,
                            const std::vector<std::string>& missingComponents,
                            const std::string& nameContains,
                            bool activeOnly,
                            bool rootsOnly)
    {
        if (!EntityMatchesNameFilter(registry, entity, nameContains)) {
            return false;
        }
        for (const std::string& component : hasComponents) {
            if (!SignatureHasComponentName(signature, component)) {
                return false;
            }
        }
        for (const std::string& component : missingComponents) {
            if (SignatureHasComponentName(signature, component)) {
                return false;
            }
        }
        if (activeOnly) {
            if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
                if (!hierarchy->isActive) {
                    return false;
                }
            }
        }
        if (rootsOnly && !Entity::IsNull(GetEntityParent(registry, entity))) {
            return false;
        }
        return true;
    }

    std::unordered_map<std::string, json> BuildECSObservationSnapshot(Registry& registry)
    {
        std::unordered_map<std::string, json> snapshot;
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                if (registry.IsAlive(entity)) {
                    snapshot.emplace(EntityToString(entity), EntityObservationRecord(registry, entity));
                }
            }
        }
        return snapshot;
    }

    json BuildRelationshipNode(Registry& registry, EntityID entity, bool includeComponents, int depth, int maxDepth)
    {
        const auto signature = FindEntitySignature(registry, entity);
        json node = EntitySummary(registry, entity, signature.value_or(Signature{}));
        if (!includeComponents) {
            node.erase("components");
        }

        node["children"] = json::array();
        if (depth >= maxDepth) {
            node["truncated"] = true;
            return node;
        }

        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            EntityID child = hierarchy->firstChild;
            while (!Entity::IsNull(child)) {
                if (registry.IsAlive(child)) {
                    node["children"].push_back(BuildRelationshipNode(registry, child, includeComponents, depth + 1, maxDepth));
                }
                auto* childHierarchy = registry.GetComponent<HierarchyComponent>(child);
                child = childHierarchy ? childHierarchy->nextSibling : Entity::NULL_ID;
            }
        }
        return node;
    }

    json BuildReferenceSummary(Registry& registry, EntityID entity)
    {
        json refs = json::object();
        if (auto* mesh = registry.GetComponent<MeshComponent>(entity)) {
            refs["mesh"] = {
                { "modelFilePath", mesh->modelFilePath },
                { "hasModel", mesh->model != nullptr },
                { "isVisible", mesh->isVisible }
            };
        }
        if (auto* material = registry.GetComponent<MaterialComponent>(entity)) {
            refs["material"] = ComponentToJson(*material);
        }
        if (auto* prefab = registry.GetComponent<PrefabInstanceComponent>(entity)) {
            refs["prefab"] = ComponentToJson(*prefab);
        }
        if (auto* effect = registry.GetComponent<EffectAssetComponent>(entity)) {
            refs["effect"] = ComponentToJson(*effect);
        }
        if (auto* light = registry.GetComponent<LightComponent>(entity)) {
            refs["light"] = ComponentToJson(*light);
        }
        return refs;
    }

    json BuildVisualState(EngineKernel& kernel)
    {
        json out;
        auto* editor = kernel.GetEditorLayer();
        Registry* registry = kernel.GetGameRegistry();

        out["available"] = editor != nullptr;
        if (!editor) {
            return out;
        }

        const auto sceneRect = editor->GetSceneViewRect();
        const auto gameRect = editor->GetGameViewRect();
        out["sceneViewRect"] = json::array({ sceneRect.x, sceneRect.y, sceneRect.z, sceneRect.w });
        out["gameViewRect"] = json::array({ gameRect.x, gameRect.y, gameRect.z, gameRect.w });
        out["sceneViewMode"] = SceneViewModeToString(editor->GetSceneViewMode());
        out["editorCamera"] = {
            { "position", Float3ToJson(editor->GetEditorCameraPosition()) },
            { "direction", Float3ToJson(editor->GetEditorCameraDirection()) },
            { "fovY", editor->GetEditorCameraFovY() }
        };

        out["selectedVisuals"] = json::array();
        if (!registry) {
            return out;
        }

        for (EntityID entity : EditorSelection::Instance().GetSelectedEntities()) {
            if (Entity::IsNull(entity) || !registry->IsAlive(entity)) {
                continue;
            }

            json item;
            item["entity"] = EntityToString(entity);
            item["name"] = "Entity " + std::to_string(Entity::GetIndex(entity));
            if (auto* name = registry->GetComponent<NameComponent>(entity)) {
                item["name"] = name->name;
            }

            if (auto* transform = registry->GetComponent<TransformComponent>(entity)) {
                const DirectX::XMFLOAT3 worldPosition = transform->worldPosition;
                item["worldPosition"] = Float3ToJson(worldPosition);
                item["localPosition"] = Float3ToJson(transform->localPosition);

                json screen;
                if (ProjectToSceneView(*editor, worldPosition, screen)) {
                    item["sceneScreenPosition"] = std::move(screen);
                }
                else {
                    item["sceneScreenPosition"] = nullptr;
                }
            }

            if (auto* mesh = registry->GetComponent<MeshComponent>(entity)) {
                item["mesh"] = {
                    { "modelFilePath", mesh->modelFilePath },
                    { "isVisible", mesh->isVisible },
                    { "hasModel", mesh->model != nullptr }
                };
            }

            out["selectedVisuals"].push_back(std::move(item));
        }

        return out;
    }

    json EntityDetail(Registry& registry, EntityID entity)
    {
        const auto signature = FindEntitySignature(registry, entity);
        json out = EntitySummary(registry, entity, signature.value_or(Signature{}));
        json components = json::object();

        if (auto* name = registry.GetComponent<NameComponent>(entity)) {
            components["NameComponent"] = { { "name", name->name } };
        }
        if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
            components["TransformComponent"] = TransformToJson(*transform);
        }
        if (auto* mesh = registry.GetComponent<MeshComponent>(entity)) {
            components["MeshComponent"] = {
                { "modelFilePath", mesh->modelFilePath },
                { "isVisible", mesh->isVisible },
                { "castShadow", mesh->castShadow },
                { "isDebugModel", mesh->isDebugModel },
                { "hasModel", mesh->model != nullptr }
            };
        }
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            components["HierarchyComponent"] = {
                { "parent", Entity::IsNull(hierarchy->parent) ? json(nullptr) : json(EntityToString(hierarchy->parent)) },
                { "firstChild", Entity::IsNull(hierarchy->firstChild) ? json(nullptr) : json(EntityToString(hierarchy->firstChild)) },
                { "prevSibling", Entity::IsNull(hierarchy->prevSibling) ? json(nullptr) : json(EntityToString(hierarchy->prevSibling)) },
                { "nextSibling", Entity::IsNull(hierarchy->nextSibling) ? json(nullptr) : json(EntityToString(hierarchy->nextSibling)) },
                { "isActive", hierarchy->isActive }
            };
        }

        out["componentData"] = std::move(components);
        return out;
    }

    template<typename T>
    json BuildComponentSchema()
    {
        json fields = json::array();
        std::apply(
            [&](auto... field) {
                (fields.push_back(ComponentFieldSchema<T>(field)), ...);
            },
            ComponentMeta<T>::Fields);

        return {
            { "component", std::string(ComponentMeta<T>::Name) },
            { "typeId", TypeManager::GetComponentTypeID<T>() },
            { "fields", std::move(fields) }
        };
    }

    template<typename T>
    json ComponentToJson(const T& component)
    {
        json fields = json::object();
        std::apply(
            [&](auto... field) {
                (AppendComponentFieldValue(component, field, fields), ...);
            },
            ComponentMeta<T>::Fields);
        return fields;
    }

    template<typename T>
    bool ComponentHasField(const std::string& name)
    {
        bool found = false;
        std::apply(
            [&](auto... field) {
                ((found = found || name == std::string(field.name)), ...);
            },
            ComponentMeta<T>::Fields);
        return found;
    }

    template<typename T>
    bool ApplyJsonFieldsToComponent(T& component, const json& fields, json& applied, json& rejected)
    {
        if (!fields.is_object()) {
            rejected.push_back({ { "field", nullptr }, { "reason", "fields_must_be_object" } });
            return false;
        }

        std::apply(
            [&](auto... field) {
                (TryApplyComponentField(component, field, fields, applied, rejected), ...);
            },
            ComponentMeta<T>::Fields);

        for (auto it = fields.begin(); it != fields.end(); ++it) {
            if (!ComponentHasField<T>(it.key())) {
                rejected.push_back({
                    { "field", it.key() },
                    { "reason", "unknown_field" }
                });
            }
        }

        return rejected.empty();
    }

    json HandleGetComponentSchema(const json& params)
    {
        const std::string componentName = params.value("component", std::string{});
        if (componentName.empty()) {
            json schemas = json::array();
            std::apply(
                [&](auto... component) {
                    (schemas.push_back(BuildComponentSchema<std::decay_t<decltype(component)>>()), ...);
                },
                AllComponentTypes{});
            return { { "components", std::move(schemas) } };
        }

        json schema;
        bool found = false;
        std::apply(
            [&](auto... component) {
                ((ComponentNameEquals<std::decay_t<decltype(component)>>(componentName)
                    ? (schema = BuildComponentSchema<std::decay_t<decltype(component)>>(), found = true)
                    : false), ...);
            },
            AllComponentTypes{});

        if (!found) {
            throw MakeError("component_not_found", "Component type was not found.", { { "component", componentName } });
        }

        return { { "schema", std::move(schema) } };
    }

    json HandleGetComponent(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        const std::string componentName = params.value("component", std::string{});
        if (componentName.empty()) {
            throw MakeError("missing_param", "component is required.");
        }

        json componentJson;
        bool foundType = false;
        bool hasComponent = false;
        std::apply(
            [&](auto... component) {
                ((ComponentNameEquals<std::decay_t<decltype(component)>>(componentName)
                    ? (foundType = true,
                       hasComponent = registry.GetComponent<std::decay_t<decltype(component)>>(entity) != nullptr,
                       componentJson = hasComponent
                            ? ComponentToJson(*registry.GetComponent<std::decay_t<decltype(component)>>(entity))
                            : json(nullptr),
                       true)
                    : false), ...);
            },
            AllComponentTypes{});

        if (!foundType) {
            throw MakeError("component_not_found", "Component type was not found.", { { "component", componentName } });
        }
        if (!hasComponent) {
            throw MakeError("component_not_found", "Entity does not have the requested component.", {
                { "entity", EntityToString(entity) },
                { "component", componentName }
            });
        }

        return {
            { "entity", EntityToString(entity) },
            { "component", componentName },
            { "fields", std::move(componentJson) }
        };
    }

    json HandleAddComponent(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        const std::string componentName = params.value("component", std::string{});
        if (componentName.empty()) {
            throw MakeError("missing_param", "component is required.");
        }
        const json fields = params.value("fields", json::object());

        json result;
        bool foundType = false;
        std::apply(
            [&](auto... component) {
                ((ComponentNameEquals<std::decay_t<decltype(component)>>(componentName)
                    ? ([&]() {
                        using Component = std::decay_t<decltype(component)>;
                        foundType = true;
                        if (registry.GetComponent<Component>(entity)) {
                            throw MakeError("operation_not_allowed", "Entity already has the requested component.", {
                                { "entity", EntityToString(entity) },
                                { "component", componentName }
                            });
                        }

                        Component next{};
                        json applied = json::array();
                        json rejected = json::array();
                        if (!ApplyJsonFieldsToComponent(next, fields, applied, rejected)) {
                            throw MakeError("component_field_not_supported", "One or more component fields could not be applied.", {
                                { "component", componentName },
                                { "rejected", rejected }
                            });
                        }
                        SetDirtyIfPossible(next);

                        if (params.value("recordUndo", true)) {
                            auto action = std::make_unique<OptionalComponentUndoAction<Component>>(
                                entity,
                                std::nullopt,
                                next,
                                "AI Add Component");
                            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
                        }
                        else {
                            registry.AddComponent<Component>(entity, next);
                        }

                        MarkEntityEdited(registry, entity);
                        result = {
                            { "entity", EntityToString(entity) },
                            { "component", componentName },
                            { "appliedFields", applied },
                            { "fields", ComponentToJson(next) }
                        };
                        return true;
                    }())
                    : false), ...);
            },
            AllComponentTypes{});

        if (!foundType) {
            throw MakeError("component_not_found", "Component type was not found.", { { "component", componentName } });
        }
        return result;
    }

    json HandleRemoveComponent(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        const std::string componentName = params.value("component", std::string{});
        if (componentName.empty()) {
            throw MakeError("missing_param", "component is required.");
        }

        bool foundType = false;
        bool removed = false;
        std::apply(
            [&](auto... component) {
                ((ComponentNameEquals<std::decay_t<decltype(component)>>(componentName)
                    ? ([&]() {
                        using Component = std::decay_t<decltype(component)>;
                        foundType = true;
                        Component* current = registry.GetComponent<Component>(entity);
                        if (!current) {
                            throw MakeError("component_not_found", "Entity does not have the requested component.", {
                                { "entity", EntityToString(entity) },
                                { "component", componentName }
                            });
                        }

                        if (params.value("recordUndo", true)) {
                            auto action = std::make_unique<OptionalComponentUndoAction<Component>>(
                                entity,
                                *current,
                                std::nullopt,
                                "AI Remove Component");
                            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
                        }
                        else {
                            registry.RemoveComponent<Component>(entity);
                        }
                        MarkEntityEdited(registry, entity);
                        removed = true;
                        return true;
                    }())
                    : false), ...);
            },
            AllComponentTypes{});

        if (!foundType) {
            throw MakeError("component_not_found", "Component type was not found.", { { "component", componentName } });
        }
        return {
            { "entity", EntityToString(entity) },
            { "component", componentName },
            { "removed", removed }
        };
    }

    json HandleSetComponentFields(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        const std::string componentName = params.value("component", std::string{});
        if (componentName.empty()) {
            throw MakeError("missing_param", "component is required.");
        }
        if (!params.contains("fields")) {
            throw MakeError("missing_param", "fields is required.");
        }
        const json& fields = params["fields"];

        json result;
        bool foundType = false;
        std::apply(
            [&](auto... component) {
                ((ComponentNameEquals<std::decay_t<decltype(component)>>(componentName)
                    ? ([&]() {
                        using Component = std::decay_t<decltype(component)>;
                        foundType = true;
                        Component* current = registry.GetComponent<Component>(entity);
                        if (!current) {
                            throw MakeError("component_not_found", "Entity does not have the requested component.", {
                                { "entity", EntityToString(entity) },
                                { "component", componentName }
                            });
                        }

                        Component before = *current;
                        Component after = before;
                        json applied = json::array();
                        json rejected = json::array();
                        if (!ApplyJsonFieldsToComponent(after, fields, applied, rejected)) {
                            throw MakeError("component_field_not_supported", "One or more component fields could not be applied.", {
                                { "component", componentName },
                                { "rejected", rejected }
                            });
                        }
                        SetDirtyIfPossible(after);

                        if (params.value("recordUndo", true)) {
                            auto action = std::make_unique<ComponentUndoAction<Component>>(entity, before, after);
                            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
                        }
                        else {
                            *current = after;
                        }

                        MarkEntityEdited(registry, entity);
                        result = {
                            { "entity", EntityToString(entity) },
                            { "component", componentName },
                            { "appliedFields", applied },
                            { "fields", ComponentToJson(after) }
                        };
                        return true;
                    }())
                    : false), ...);
            },
            AllComponentTypes{});

        if (!foundType) {
            throw MakeError("component_not_found", "Component type was not found.", { { "component", componentName } });
        }
        return result;
    }

    EntitySnapshot::Snapshot BuildEntitySnapshot(const std::string& name,
                                                 const TransformComponent& transform,
                                                 const std::optional<MeshComponent>& mesh)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;
        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name.empty() ? "New Entity" : name };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        if (mesh.has_value()) {
            std::get<std::optional<MeshComponent>>(node.components) = *mesh;
        }

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    EntityID CreateEntityFromSnapshot(Registry& registry,
                                      EntitySnapshot::Snapshot snapshot,
                                      EntityID parent,
                                      bool recordUndo,
                                      const char* actionName)
    {
        if (recordUndo) {
            auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parent, actionName);
            auto* actionPtr = action.get();
            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
            return actionPtr->GetLiveRoot();
        }

        for (auto& node : snapshot.nodes) {
            if (node.localID == snapshot.rootLocalID) {
                node.externalParent = parent;
                break;
            }
        }
        return EntitySnapshot::RestoreSubtree(snapshot, registry).root;
    }

    json HandlePing()
    {
        return { { "message", "pong" } };
    }

    json HandleGetEngineState(EngineKernel& kernel)
    {
        json out;
        out["mode"] = ModeToString(kernel.GetMode());
        out["frameCount"] = kernel.GetTime().frameCount;
        out["timeScale"] = kernel.GetTime().timeScale;
        out["selectedEntities"] = json::array();
        for (EntityID entity : EditorSelection::Instance().GetSelectedEntities()) {
            out["selectedEntities"].push_back(EntityToString(entity));
        }
        const EntityID primary = EditorSelection::Instance().GetPrimaryEntity();
        out["primarySelectedEntity"] = Entity::IsNull(primary) || primary == 0 ? json(nullptr) : json(EntityToString(primary));

        if (auto* editor = kernel.GetEditorLayer()) {
            out["currentScenePath"] = editor->GetCurrentScenePath();
            out["sceneViewMode"] = SceneViewModeToString(editor->GetSceneViewMode());
            out["effectEditorActive"] = editor->IsEffectEditorWorkspaceActive();
            out["effectEditorDocumentPath"] = editor->GetEffectEditorDocumentPath();
            out["effectPreviewEntity"] = Entity::IsNull(editor->GetEffectPreviewEntity())
                ? json(nullptr)
                : json(EntityToString(editor->GetEffectPreviewEntity()));
            const auto sceneRect = editor->GetSceneViewRect();
            const auto gameRect = editor->GetGameViewRect();
            out["sceneViewRect"] = json::array({ sceneRect.x, sceneRect.y, sceneRect.z, sceneRect.w });
            out["gameViewRect"] = json::array({ gameRect.x, gameRect.y, gameRect.z, gameRect.w });
            out["editorCamera"] = {
                { "position", Float3ToJson(editor->GetEditorCameraPosition()) },
                { "direction", Float3ToJson(editor->GetEditorCameraDirection()) },
                { "fovY", editor->GetEditorCameraFovY() }
            };
        }
        else {
            out["currentScenePath"] = nullptr;
            out["effectEditorActive"] = false;
            out["effectEditorDocumentPath"] = nullptr;
            out["effectPreviewEntity"] = nullptr;
        }

        out["visualState"] = BuildVisualState(kernel);
        return out;
    }

    json HandleGetVisualState(EngineKernel& kernel)
    {
        return BuildVisualState(kernel);
    }

    json HandleECSQuery(Registry& registry, const json& params)
    {
        const std::vector<std::string> hasComponents = JsonStringList(params, "hasComponents");
        const std::vector<std::string> missingComponents = JsonStringList(params, "missingComponents");
        const std::string nameContains = params.value("nameContains", std::string{});
        const bool activeOnly = params.value("activeOnly", false);
        const bool rootsOnly = params.value("rootsOnly", false);
        const bool includeDetails = params.value("includeDetails", false);
        const int limit = (std::max)(0, params.value("limit", 0));

        json matches = json::array();
        int totalMatched = 0;
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const Signature signature = archetype->GetSignature();
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                if (!registry.IsAlive(entity)) {
                    continue;
                }
                if (!EntityMatchesQuery(registry, entity, signature, hasComponents, missingComponents, nameContains, activeOnly, rootsOnly)) {
                    continue;
                }

                ++totalMatched;
                if (limit > 0 && static_cast<int>(matches.size()) >= limit) {
                    continue;
                }
                matches.push_back(includeDetails
                    ? EntityObservationRecord(registry, entity)
                    : EntitySummary(registry, entity, signature));
            }
        }

        return {
            { "entities", std::move(matches) },
            { "count", totalMatched },
            { "truncated", limit > 0 && totalMatched > limit }
        };
    }

    json HandleECSHierarchy(Registry& registry, const json& params)
    {
        const bool includeComponents = params.value("includeComponents", true);
        const bool includeReferences = params.value("includeReferences", true);
        const int maxDepth = (std::max)(0, params.value("maxDepth", 64));

        json roots = json::array();
        json references = json::object();
        const EntityID requestedRoot = EntityFromJson(params.value("root", json(nullptr)));

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                if (!registry.IsAlive(entity)) {
                    continue;
                }
                if (!Entity::IsNull(requestedRoot)) {
                    if (entity == requestedRoot) {
                        roots.push_back(BuildRelationshipNode(registry, entity, includeComponents, 0, maxDepth));
                    }
                }
                else if (Entity::IsNull(GetEntityParent(registry, entity))) {
                    roots.push_back(BuildRelationshipNode(registry, entity, includeComponents, 0, maxDepth));
                }

                if (includeReferences) {
                    json refs = BuildReferenceSummary(registry, entity);
                    if (!refs.empty()) {
                        references[EntityToString(entity)] = std::move(refs);
                    }
                }
            }
        }

        if (!Entity::IsNull(requestedRoot) && roots.empty()) {
            throw MakeError("entity_not_found", "Root entity is not alive.", { { "root", params.value("root", json(nullptr)) } });
        }

        return {
            { "roots", std::move(roots) },
            { "references", std::move(references) }
        };
    }

    json HandleECSDiff(Registry& registry, const json& params)
    {
        static std::unordered_map<std::string, json> previousSnapshot;
        static uint64_t revision = 0;

        const bool reset = params.value("reset", false);
        const bool includeBeforeAfter = params.value("includeBeforeAfter", true);
        std::unordered_map<std::string, json> current = BuildECSObservationSnapshot(registry);

        json added = json::array();
        json removed = json::array();
        json changed = json::array();

        if (!reset && !previousSnapshot.empty()) {
            for (const auto& [entity, currentRecord] : current) {
                auto it = previousSnapshot.find(entity);
                if (it == previousSnapshot.end()) {
                    added.push_back(currentRecord);
                }
                else if (it->second != currentRecord) {
                    json item = {
                        { "entity", entity },
                        { "name", currentRecord.value("name", std::string{}) }
                    };
                    if (includeBeforeAfter) {
                        item["before"] = it->second;
                        item["after"] = currentRecord;
                    }
                    changed.push_back(std::move(item));
                }
            }

            for (const auto& [entity, previousRecord] : previousSnapshot) {
                if (current.find(entity) == current.end()) {
                    removed.push_back(previousRecord);
                }
            }
        }

        previousSnapshot = std::move(current);
        ++revision;

        return {
            { "revision", revision },
            { "baselineReset", reset || revision == 1 },
            { "added", std::move(added) },
            { "removed", std::move(removed) },
            { "changed", std::move(changed) }
        };
    }

    std::string EntityDisplayName(Registry& registry, EntityID entity);
    bool IsGameplayActor(Registry& registry, EntityID entity);
    json HandleGameplayGetState(EngineKernel& kernel, Registry& registry, const json& params);
    json HandleCaptureScreenshot(EngineKernel& kernel, const json& params, const std::filesystem::path& defaultPath);
    json HandleRecoveryGetState(EngineKernel& kernel, const json& params);
    json HandleRecoveryRestore(EngineKernel& kernel);
    json HandleRecoveryDismiss(EngineKernel& kernel);
    json AnalyzeImageBuffer(const ImageBuffer& image);
    ImageBuffer CaptureAutomationTargetImage(EngineKernel& kernel, const std::string& target);

    json HandleVisualVerifyEntity(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        json out;
        out["entity"] = EntityToString(entity);
        out["name"] = "Entity " + std::to_string(Entity::GetIndex(entity));
        if (auto* name = registry.GetComponent<NameComponent>(entity)) {
            out["name"] = name->name;
        }
        out["selected"] = EditorSelection::Instance().IsEntitySelected(entity);
        out["sceneViewMode"] = SceneViewModeToString(editor->GetSceneViewMode());

        DirectX::XMFLOAT3 center{};
        float radius = 0.0f;
        const bool hasBounds = BuildEntityFocusBounds(registry, entity, center, radius);
        out["hasBounds"] = hasBounds;
        if (hasBounds) {
            out["boundsCenter"] = Float3ToJson(center);
            out["boundsRadius"] = radius;
            json screen;
            if (ProjectToSceneView(*editor, center, screen)) {
                out["sceneView"] = std::move(screen);
            }
            else {
                out["sceneView"] = nullptr;
            }
            const DirectX::XMFLOAT4 rect = editor->GetSceneViewRect();
            if (rect.z > 1.0f && rect.w > 1.0f) {
                const float aspect = rect.z / rect.w;
                const DirectX::XMFLOAT4X4 view = editor->GetEditorViewMatrix();
                const DirectX::XMFLOAT4X4 projection = editor->BuildEditorProjectionMatrix(aspect);
                json projectedBounds;
                if (ProjectBoundsToRect(rect, view, projection, center, radius, projectedBounds)) {
                    out["sceneViewBounds"] = std::move(projectedBounds);
                }
            }
        }
        else if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
            out["worldPosition"] = Float3ToJson(transform->worldPosition);
            json screen;
            if (ProjectToSceneView(*editor, transform->worldPosition, screen)) {
                out["sceneView"] = std::move(screen);
            }
            else {
                out["sceneView"] = nullptr;
            }
        }
        else {
            out["sceneView"] = nullptr;
        }

        if (auto* mesh = registry.GetComponent<MeshComponent>(entity)) {
            out["renderable"] = {
                { "meshComponent", true },
                { "isVisible", mesh->isVisible },
                { "hasModel", mesh->model != nullptr },
                { "modelFilePath", mesh->modelFilePath }
            };
        }
        else {
            out["renderable"] = {
                { "meshComponent", false }
            };
        }
        out["references"] = BuildReferenceSummary(registry, entity);
        return out;
    }

    bool TryBuildGameViewProjection(EngineKernel& kernel,
                                    Registry& registry,
                                    DirectX::XMFLOAT4X4& outView,
                                    DirectX::XMFLOAT4X4& outProjection,
                                    EntityID& outCameraEntity,
                                    std::string& outCameraKind)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            return false;
        }

        const DirectX::XMFLOAT4 gameRect = editor->GetGameViewRect();
        if (editor->GetSceneViewMode() == EditorLayer::SceneViewMode::Mode2D) {
            if (Camera2DUtils::TryBuildActiveViewProjection(registry, gameRect, outView, outProjection)) {
                const Camera2DUtils::ActiveCamera2D active = Camera2DUtils::FindActiveCamera2D(registry);
                outCameraEntity = active.entity;
                outCameraKind = "Camera2D";
                return true;
            }
            if (editor->TryBuildGameView2DPreviewViewProjection(outView, outProjection)) {
                outCameraEntity = Entity::NULL_ID;
                outCameraKind = "SceneView2DFallback";
                return true;
            }
            return false;
        }

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<CameraMainTagComponent>()) ||
                !signature.test(TypeManager::GetComponentTypeID<CameraMatricesComponent>())) {
                continue;
            }
            auto* matricesColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraMatricesComponent>());
            const auto& entities = archetype->GetEntities();
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                auto* matrices = static_cast<CameraMatricesComponent*>(matricesColumn->Get(i));
                if (!matrices) {
                    continue;
                }
                outView = matrices->view;
                outProjection = matrices->projection;
                outCameraEntity = entities[i];
                outCameraKind = "Camera3D";
                return true;
            }
        }

        return false;
    }

    json HandleVisualVerifyEntityGameView(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        DirectX::XMFLOAT3 point{};
        float radius = 0.0f;
        bool hasPoint = BuildEntityFocusBounds(registry, entity, point, radius);
        if (!hasPoint) {
            if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                point = transform->worldPosition;
                hasPoint = true;
            }
        }
        if (!hasPoint) {
            throw MakeError("operation_not_allowed", "Entity has no transform or focus bounds.", { { "entity", EntityToString(entity) } });
        }

        DirectX::XMFLOAT4X4 view{};
        DirectX::XMFLOAT4X4 projection{};
        EntityID cameraEntity = Entity::NULL_ID;
        std::string cameraKind;
        if (!TryBuildGameViewProjection(kernel, registry, view, projection, cameraEntity, cameraKind)) {
            throw MakeError("camera_not_found", "Could not build Game View projection.");
        }

        json screen;
        const DirectX::XMFLOAT4 gameRect = editor->GetGameViewRect();
        const bool projected = ProjectWorldToRect(gameRect, view, projection, point, screen);
        const bool visible = projected && screen.value("visible", false);
        json projectedBounds = nullptr;
        if (hasPoint) {
            json bounds;
            if (ProjectBoundsToRect(gameRect, view, projection, point, radius, bounds)) {
                projectedBounds = std::move(bounds);
            }
        }
        return {
            { "entity", EntityToString(entity) },
            { "name", EntityDisplayName(registry, entity) },
            { "point", Float3ToJson(point) },
            { "boundsRadius", radius },
            { "gameViewRect", json::array({ gameRect.x, gameRect.y, gameRect.z, gameRect.w }) },
            { "camera", {
                { "kind", cameraKind },
                { "entity", Entity::IsNull(cameraEntity) ? json(nullptr) : json(EntityToString(cameraEntity)) }
            } },
            { "gameView", projected ? std::move(screen) : json(nullptr) },
            { "gameViewBounds", std::move(projectedBounds) },
            { "visibleInGameView", visible }
        };
    }

    std::vector<EntityID> ResolveEntityListParam(Registry& registry, const json& params, bool defaultToAllGameplay = false)
    {
        std::vector<EntityID> entities;
        if (params.contains("entities")) {
            const json& in = params["entities"];
            if (!in.is_array()) {
                throw MakeError("invalid_param", "entities must be an array.");
            }
            for (const json& item : in) {
                const EntityID entity = EntityFromJson(item);
                if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
                    throw MakeError("entity_not_found", "Entity in entities is not alive.", { { "entity", item } });
                }
                entities.push_back(entity);
            }
        }
        else if (params.contains("entity")) {
            const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
            if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
                throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
            }
            entities.push_back(entity);
        }
        else if (defaultToAllGameplay) {
            for (Archetype* archetype : registry.GetAllArchetypes()) {
                const auto& archetypeEntities = archetype->GetEntities();
                for (EntityID entity : archetypeEntities) {
                    if (registry.IsAlive(entity) && IsGameplayActor(registry, entity)) {
                        entities.push_back(entity);
                    }
                }
            }
        }

        if (entities.empty()) {
            for (EntityID selected : EditorSelection::Instance().GetSelectedEntities()) {
                if (!Entity::IsNull(selected) && registry.IsAlive(selected)) {
                    entities.push_back(selected);
                }
            }
        }
        return entities;
    }

    bool ComputeEntityListBounds(Registry& registry,
                                 const std::vector<EntityID>& entities,
                                 DirectX::XMFLOAT3& outCenter,
                                 float& outRadius)
    {
        bool hasAny = false;
        DirectX::BoundingSphere merged{};
        for (EntityID entity : entities) {
            DirectX::XMFLOAT3 center{};
            float radius = 0.0f;
            if (!BuildEntityFocusBounds(registry, entity, center, radius)) {
                if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                    center = transform->worldPosition;
                    radius = 1.0f;
                }
                else {
                    continue;
                }
            }

            DirectX::BoundingSphere sphere(center, (std::max)(radius, 0.5f));
            if (!hasAny) {
                merged = sphere;
                hasAny = true;
            }
            else {
                DirectX::BoundingSphere::CreateMerged(merged, merged, sphere);
            }
        }

        if (!hasAny) {
            return false;
        }
        outCenter = merged.Center;
        outRadius = merged.Radius;
        return true;
    }

    json FrameEntitiesResult(const std::vector<EntityID>& entities,
                             const DirectX::XMFLOAT3& center,
                             float radius,
                             const DirectX::XMFLOAT3& cameraPosition)
    {
        json entityIds = json::array();
        for (EntityID entity : entities) {
            entityIds.push_back(EntityToString(entity));
        }
        return {
            { "entities", std::move(entityIds) },
            { "center", Float3ToJson(center) },
            { "radius", radius },
            { "cameraPosition", Float3ToJson(cameraPosition) }
        };
    }

    json HandleSceneViewFrameEntities(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        std::vector<EntityID> entities = ResolveEntityListParam(registry, params, true);
        if (entities.empty()) {
            throw MakeError("entity_not_found", "No entities were available to frame.");
        }

        DirectX::XMFLOAT3 center{};
        float radius = 1.0f;
        if (!ComputeEntityListBounds(registry, entities, center, radius)) {
            throw MakeError("operation_not_allowed", "Could not compute bounds for requested entities.");
        }

        const float yawDegrees = params.value("yawDegrees", 35.0f);
        const float pitchDegrees = params.value("pitchDegrees", 28.0f);
        const float padding = params.value("padding", 2.4f);
        const float distance = params.value("distance", ComputeFocusDistanceForRadius(radius * padding, editor->GetEditorCameraFovY()));
        const float yaw = DirectX::XMConvertToRadians(yawDegrees);
        const float pitch = DirectX::XMConvertToRadians(pitchDegrees);
        const DirectX::XMFLOAT3 direction = {
            std::sin(yaw) * std::cos(pitch),
            -std::sin(pitch),
            std::cos(yaw) * std::cos(pitch)
        };
        const DirectX::XMFLOAT3 cameraPosition = {
            center.x - direction.x * distance,
            center.y - direction.y * distance,
            center.z - direction.z * distance
        };

        editor->SetSceneViewMode(EditorLayer::SceneViewMode::Mode3D);
        editor->SetEditorCameraLookAt(cameraPosition, center);
        json result = FrameEntitiesResult(entities, center, radius, cameraPosition);
        result["view"] = "scene_view";
        return result;
    }

    json HandleSceneViewFrameAll(EngineKernel& kernel, Registry& registry, const json& params)
    {
        json frameParams = params;
        json entities = json::array();
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& archetypeEntities = archetype->GetEntities();
            for (EntityID entity : archetypeEntities) {
                if (!registry.IsAlive(entity)) {
                    continue;
                }
                if (registry.GetComponent<TransformComponent>(entity) ||
                    registry.GetComponent<MeshComponent>(entity) ||
                    registry.GetComponent<TerrainComponent>(entity)) {
                    entities.push_back(EntityToString(entity));
                }
            }
        }
        frameParams["entities"] = std::move(entities);
        return HandleSceneViewFrameEntities(kernel, registry, frameParams);
    }

    EntityID FindMainCamera3D(Registry& registry)
    {
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<CameraMainTagComponent>()) ||
                !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
                continue;
            }
            const auto& entities = archetype->GetEntities();
            if (!entities.empty()) {
                return entities.front();
            }
        }
        return Entity::NULL_ID;
    }

    void SetTransformLookAtWorld(Registry& registry,
                                 EntityID entity,
                                 TransformComponent& transform,
                                 const DirectX::XMFLOAT3& position,
                                 const DirectX::XMFLOAT3& target)
    {
        using namespace DirectX;
        XMMATRIX cameraWorld = XMMatrixInverse(
            nullptr,
            XMMatrixLookAtLH(XMLoadFloat3(&position), XMLoadFloat3(&target), XMVectorSet(0, 1, 0, 0)));

        XMVECTOR scale{};
        XMVECTOR rotation{};
        XMVECTOR translation{};
        if (!XMMatrixDecompose(&scale, &rotation, &translation, cameraWorld)) {
            throw MakeError("operation_not_allowed", "Failed to compute camera transform.");
        }

        if (!Entity::IsNull(GetEntityParent(registry, entity))) {
            if (auto* parentTransform = registry.GetComponent<TransformComponent>(GetEntityParent(registry, entity))) {
                const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
                cameraWorld = cameraWorld * XMMatrixInverse(nullptr, parentWorld);
                if (!XMMatrixDecompose(&scale, &rotation, &translation, cameraWorld)) {
                    throw MakeError("operation_not_allowed", "Failed to compute local camera transform.");
                }
            }
        }

        XMStoreFloat3(&transform.localPosition, translation);
        XMStoreFloat4(&transform.localRotation, XMQuaternionNormalize(rotation));
        transform.isDirty = true;
        HierarchySystem::MarkDirtyRecursive(entity, registry);
        PrefabSystem::MarkPrefabOverride(entity, registry);
    }

    json HandleCameraFrameEntities(EngineKernel& kernel, Registry& registry, const json& params)
    {
        std::vector<EntityID> entities = ResolveEntityListParam(registry, params, true);
        if (entities.empty()) {
            throw MakeError("entity_not_found", "No entities were available to frame.");
        }
        DirectX::XMFLOAT3 center{};
        float radius = 1.0f;
        if (!ComputeEntityListBounds(registry, entities, center, radius)) {
            throw MakeError("operation_not_allowed", "Could not compute bounds for requested entities.");
        }

        EntityID cameraEntity = EntityFromJson(params.value("camera", json(nullptr)));
        if (Entity::IsNull(cameraEntity)) {
            cameraEntity = FindMainCamera3D(registry);
        }
        if (Entity::IsNull(cameraEntity) || !registry.IsAlive(cameraEntity)) {
            throw MakeError("camera_not_found", "No live main camera was found.");
        }
        auto* transform = registry.GetComponent<TransformComponent>(cameraEntity);
        if (!transform) {
            throw MakeError("component_not_found", "Camera has no TransformComponent.", { { "camera", EntityToString(cameraEntity) } });
        }

        const float yawDegrees = params.value("yawDegrees", 0.0f);
        const float pitchDegrees = params.value("pitchDegrees", 16.0f);
        const float padding = params.value("padding", 2.8f);
        const float fovY = registry.GetComponent<CameraLensComponent>(cameraEntity)
            ? registry.GetComponent<CameraLensComponent>(cameraEntity)->fovY
            : 0.785398f;
        const float distance = params.value("distance", ComputeFocusDistanceForRadius(radius * padding, fovY));
        const float yaw = DirectX::XMConvertToRadians(yawDegrees);
        const float pitch = DirectX::XMConvertToRadians(pitchDegrees);
        const DirectX::XMFLOAT3 direction = {
            std::sin(yaw) * std::cos(pitch),
            -std::sin(pitch),
            std::cos(yaw) * std::cos(pitch)
        };
        const DirectX::XMFLOAT3 cameraPosition = {
            center.x - direction.x * distance,
            center.y - direction.y * distance,
            center.z - direction.z * distance
        };

        SetTransformLookAtWorld(registry, cameraEntity, *transform, cameraPosition, center);
        json result = FrameEntitiesResult(entities, center, radius, cameraPosition);
        result["view"] = "game_view";
        result["camera"] = EntityToString(cameraEntity);
        return result;
    }

    json HandleVisualAssertEntitiesVisible(EngineKernel& kernel, Registry& registry, const json& params)
    {
        const std::string view = params.value("view", std::string("game_view"));
        const float minVisibleRatio = params.value("minVisibleRatio", 1.0f);
        const bool requireAll = params.value("requireAll", true);
        const bool defaultToAllGameplay = params.value("defaultToAllGameplay", true);
        const bool requireBoundsFullyVisible = params.value("requireBoundsFullyVisible", true);
        const float minMarginPixels = params.value("minMarginPixels", 8.0f);
        const float maxFillRatio = params.value("maxFillRatio", 0.88f);
        std::vector<EntityID> entities = ResolveEntityListParam(registry, params, defaultToAllGameplay);
        if (entities.empty()) {
            throw MakeError("entity_not_found", "No entities were available to assert visibility.");
        }

        int visibleCount = 0;
        json items = json::array();
        for (EntityID entity : entities) {
            json verifyParams = { { "entity", EntityToString(entity) } };
            json item = (view == "scene_view")
                ? HandleVisualVerifyEntity(kernel, registry, verifyParams)
                : HandleVisualVerifyEntityGameView(kernel, registry, verifyParams);
            const bool visible = view == "scene_view"
                ? (item.contains("sceneView") && !item["sceneView"].is_null() && item["sceneView"].value("visibleInSceneView", false))
                : item.value("visibleInGameView", false);
            const char* boundsKey = view == "scene_view" ? "sceneViewBounds" : "gameViewBounds";
            bool boundsOk = true;
            if (requireBoundsFullyVisible) {
                boundsOk = false;
                if (item.contains(boundsKey) && !item[boundsKey].is_null()) {
                    const json& bounds = item[boundsKey];
                    const float margin = bounds.contains("margins") ? bounds["margins"].value("min", -1.0f) : -1.0f;
                    const float fill = bounds.value("maxFill", 1.0f);
                    boundsOk = bounds.value("fullyVisible", false) &&
                        margin >= minMarginPixels &&
                        (maxFillRatio <= 0.0f || fill <= maxFillRatio);
                }
            }
            const bool reviewVisible = visible && boundsOk;
            item["visible"] = reviewVisible;
            item["centerVisible"] = visible;
            item["boundsRequirement"] = {
                { "enabled", requireBoundsFullyVisible },
                { "minMarginPixels", minMarginPixels },
                { "maxFillRatio", maxFillRatio },
                { "ok", boundsOk }
            };
            if (reviewVisible) {
                ++visibleCount;
            }
            items.push_back(std::move(item));
        }

        const float ratio = entities.empty() ? 0.0f : static_cast<float>(visibleCount) / static_cast<float>(entities.size());
        const bool ok = requireAll ? (visibleCount == static_cast<int>(entities.size())) : (ratio >= minVisibleRatio);
        return {
            { "ok", ok },
            { "view", view },
            { "visibleCount", visibleCount },
            { "total", entities.size() },
            { "visibleRatio", ratio },
            { "requiredRatio", minVisibleRatio },
            { "items", std::move(items) }
        };
    }

    json HandleVisualCaptureReviewSet(EngineKernel& kernel, Registry& registry, const json& params)
    {
        const std::string stem = params.value("stem", std::string("review"));
        const std::filesystem::path dir = params.value("dir", std::string("Saved/AI/screenshots/review"));
        const std::string format = params.value("format", std::string("bmp"));
        const std::vector<std::string> targets = params.contains("targets")
            ? JsonStringList(params, "targets")
            : std::vector<std::string>{ "scene_view", "game_view", "window" };

        if (params.value("frameSceneView", false)) {
            HandleSceneViewFrameEntities(kernel, registry, params);
        }
        if (params.value("frameGameCamera", false)) {
            HandleCameraFrameEntities(kernel, registry, params);
        }

        json screenshots = json::array();
        for (const std::string& target : targets) {
            const std::filesystem::path path = dir / (stem + "_" + target + "." + format);
            json captureParams = {
                { "target", target },
                { "path", path.generic_string() },
                { "format", format },
                { "inline", params.value("inline", false) }
            };
            json shot = HandleCaptureScreenshot(kernel, captureParams, path);
            if (params.value("includeMetrics", true)) {
                shot["metrics"] = AnalyzeImageBuffer(CaptureAutomationTargetImage(kernel, target));
            }
            screenshots.push_back(std::move(shot));
        }

        json assertions = nullptr;
        if (params.value("assertVisible", true)) {
            json assertParams = params;
            assertParams["view"] = params.value("assertView", std::string("game_view"));
            assertions = HandleVisualAssertEntitiesVisible(kernel, registry, assertParams);
        }

        return {
            { "screenshots", std::move(screenshots) },
            { "assertions", std::move(assertions) },
            { "engineState", HandleGetEngineState(kernel) },
            { "gameplay", HandleGameplayGetState(kernel, registry, json{ { "includeVisual", false }, { "includeInput", false }, { "includeDamageEvents", false } }) }
        };
    }

    const char* CharacterStateToString(CharacterState state)
    {
        switch (state) {
        case CharacterState::Locomotion: return "Locomotion";
        case CharacterState::Action: return "Action";
        case CharacterState::Dodge: return "Dodge";
        case CharacterState::Jump: return "Jump";
        case CharacterState::Damage: return "Damage";
        case CharacterState::Dead: return "Dead";
        default: return "Unknown";
        }
    }

    const char* BattlePhaseToString(BattleFlowComponent::Phase phase)
    {
        switch (phase) {
        case BattleFlowComponent::Phase::Idle: return "Idle";
        case BattleFlowComponent::Phase::Encounter: return "Encounter";
        case BattleFlowComponent::Phase::Combat: return "Combat";
        case BattleFlowComponent::Phase::Victory: return "Victory";
        case BattleFlowComponent::Phase::Defeat: return "Defeat";
        case BattleFlowComponent::Phase::Draw: return "Draw";
        default: return "Unknown";
        }
    }

    std::string EntityDisplayName(Registry& registry, EntityID entity)
    {
        if (auto* name = registry.GetComponent<NameComponent>(entity)) {
            return name->name;
        }
        return "Entity " + std::to_string(Entity::GetIndex(entity));
    }

    json AnimatorLayerToJson(const AnimatorComponent::LayerState& layer)
    {
        return {
            { "currentAnimIndex", layer.currentAnimIndex },
            { "currentTime", layer.currentTime },
            { "currentSpeed", layer.currentSpeed },
            { "isLoop", layer.isLoop },
            { "weight", layer.weight },
            { "isFullBody", layer.isFullBody },
            { "prevAnimIndex", layer.prevAnimIndex },
            { "prevAnimTime", layer.prevAnimTime },
            { "blendDuration", layer.blendDuration },
            { "blendTimer", layer.blendTimer },
            { "isBlending", layer.isBlending }
        };
    }

    json HitboxTrackingToJson(const HitboxTrackingComponent& hitbox)
    {
        json hitEntities = json::array();
        const int count = (std::min)(static_cast<int>(hitbox.hitEntityCount), 16);
        for (int i = 0; i < count; ++i) {
            if (!Entity::IsNull(hitbox.hitEntities[i])) {
                hitEntities.push_back(EntityToString(hitbox.hitEntities[i]));
            }
        }
        return {
            { "lastHitboxStart", hitbox.lastHitboxStart },
            { "hitEntityCount", hitbox.hitEntityCount },
            { "hitEntities", std::move(hitEntities) }
        };
    }

    json ResolvedInputToJson(const ResolvedInputStateComponent& input)
    {
        json actions = json::array();
        const int actionCount = (std::min)(static_cast<int>(input.actionCount), ResolvedInputStateComponent::MAX_ACTIONS);
        for (int i = 0; i < actionCount; ++i) {
            const auto& action = input.actions[i];
            actions.push_back({
                { "index", i },
                { "pressed", action.pressed },
                { "held", action.held },
                { "released", action.released },
                { "value", action.value },
                { "framesSincePressed", action.framesSincePressed },
                { "framesSinceReleased", action.framesSinceReleased }
            });
        }

        json axes = json::array();
        const int axisCount = (std::min)(static_cast<int>(input.axisCount), ResolvedInputStateComponent::MAX_AXES);
        for (int i = 0; i < axisCount; ++i) {
            axes.push_back({ { "index", i }, { "value", input.axes[i] } });
        }

        return {
            { "actions", std::move(actions) },
            { "axes", std::move(axes) },
            { "pointer", {
                { "x", input.pointerX },
                { "y", input.pointerY },
                { "deltaX", input.deltaX },
                { "deltaY", input.deltaY },
                { "scrollX", input.scrollX },
                { "scrollY", input.scrollY }
            } },
            { "lastDeviceType", static_cast<int>(input.lastDeviceType) }
        };
    }

    json StateMachineParamsToJson(const StateMachineParamsComponent& stateMachine)
    {
        json params = json::array();
        const int count = (std::min)(static_cast<int>(stateMachine.paramCount), StateMachineParamsComponent::MAX_PARAMS);
        for (int i = 0; i < count; ++i) {
            params.push_back({
                { "name", std::string(stateMachine.params[i].name) },
                { "value", stateMachine.params[i].value }
            });
        }
        return {
            { "currentStateId", stateMachine.currentStateId },
            { "stateTimer", stateMachine.stateTimer },
            { "animFinished", stateMachine.animFinished },
            { "params", std::move(params) }
        };
    }

    json DamageEventToJson(const DamageEventComponent::Event& event)
    {
        return {
            { "attacker", Entity::IsNull(event.attacker) ? json(nullptr) : json(EntityToString(event.attacker)) },
            { "victim", Entity::IsNull(event.victim) ? json(nullptr) : json(EntityToString(event.victim)) },
            { "amount", event.amount },
            { "hitPoint", Float3ToJson(event.hitPoint) },
            { "knockbackDir", Float3ToJson(event.knockbackDir) },
            { "knockbackPower", event.knockbackPower },
            { "hitStopSec", event.hitStopSec },
            { "reactionKind", event.reactionKind },
            { "hitVfxPath", event.hitVfxPath },
            { "hitSfxPath", event.hitSfxPath }
        };
    }

    bool IsGameplayActor(Registry& registry, EntityID entity)
    {
        return registry.GetComponent<PlayerTagComponent>(entity) ||
            registry.GetComponent<EnemyTagComponent>(entity) ||
            registry.GetComponent<HealthComponent>(entity) ||
            registry.GetComponent<ActionStateComponent>(entity) ||
            registry.GetComponent<LocomotionStateComponent>(entity) ||
            registry.GetComponent<TeamComponent>(entity) ||
            registry.GetComponent<StaminaComponent>(entity);
    }

    json GameplayActorToJson(EngineKernel& kernel, Registry& registry, EntityID entity, bool includeVisual)
    {
        json actor;
        actor["entity"] = EntityToString(entity);
        actor["name"] = EntityDisplayName(registry, entity);
        actor["role"] = "Actor";

        if (auto* player = registry.GetComponent<PlayerTagComponent>(entity)) {
            actor["role"] = "Player";
            actor["playerId"] = player->playerId;
        }
        if (auto* enemy = registry.GetComponent<EnemyTagComponent>(entity)) {
            actor["role"] = "Enemy";
            actor["enemyKindId"] = enemy->enemyKindId;
        }
        if (auto* team = registry.GetComponent<TeamComponent>(entity)) {
            actor["team"] = team->teamId;
        }
        if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
            actor["position"] = Float3ToJson(transform->worldPosition);
            actor["localPosition"] = Float3ToJson(transform->localPosition);
        }
        if (auto* health = registry.GetComponent<HealthComponent>(entity)) {
            actor["health"] = {
                { "current", health->health },
                { "max", health->maxHealth },
                { "ratio", health->maxHealth > 0 ? static_cast<float>(health->health) / static_cast<float>(health->maxHealth) : 0.0f },
                { "isDead", health->isDead },
                { "isInvincible", health->isInvincible },
                { "invincibleTimer", health->invincibleTimer },
                { "lastDamage", health->lastDamage }
            };
        }
        if (auto* stamina = registry.GetComponent<StaminaComponent>(entity)) {
            actor["stamina"] = {
                { "current", stamina->current },
                { "max", stamina->max },
                { "ratio", stamina->max > 0.0f ? stamina->current / stamina->max : 0.0f },
                { "recoveryTimer", stamina->recoveryTimer }
            };
        }
        if (auto* action = registry.GetComponent<ActionStateComponent>(entity)) {
            actor["action"] = {
                { "state", CharacterStateToString(action->state) },
                { "currentNodeIndex", action->currentNodeIndex },
                { "reservedNodeIndex", action->reservedNodeIndex },
                { "stateTimer", action->stateTimer },
                { "comboCount", action->comboCount },
                { "comboTimer", action->comboTimer }
            };
        }
        if (auto* locomotion = registry.GetComponent<LocomotionStateComponent>(entity)) {
            actor["locomotion"] = ComponentToJson(*locomotion);
        }
        if (auto* physics = registry.GetComponent<CharacterPhysicsComponent>(entity)) {
            actor["physics"] = ComponentToJson(*physics);
        }
        if (auto* animator = registry.GetComponent<AnimatorComponent>(entity)) {
            actor["animator"] = {
                { "baseLayer", AnimatorLayerToJson(animator->baseLayer) },
                { "actionLayer", AnimatorLayerToJson(animator->actionLayer) },
                { "enableRootMotion", animator->enableRootMotion },
                { "rootMotionDelta", Float3ToJson(animator->rootMotionDelta) },
                { "driverConnected", animator->driverConnected },
                { "driverOverrideAnimIndex", animator->driverOverrideAnimIndex },
                { "driverTime", animator->driverTime }
            };
        }
        if (auto* playback = registry.GetComponent<PlaybackComponent>(entity)) {
            actor["playback"] = ComponentToJson(*playback);
        }
        if (auto* timeline = registry.GetComponent<TimelineComponent>(entity)) {
            actor["timeline"] = ComponentToJson(*timeline);
        }
        if (auto* stateMachine = registry.GetComponent<StateMachineParamsComponent>(entity)) {
            actor["stateMachine"] = StateMachineParamsToJson(*stateMachine);
        }
        if (auto* lockOn = registry.GetComponent<LockOnTargetComponent>(entity)) {
            actor["lockOn"] = {
                { "currentTarget", Entity::IsNull(lockOn->currentTarget) ? json(nullptr) : json(EntityToString(lockOn->currentTarget)) },
                { "targetAlive", !Entity::IsNull(lockOn->currentTarget) && registry.IsAlive(lockOn->currentTarget) },
                { "maxRange", lockOn->maxRange },
                { "fovRadians", lockOn->fovRadians },
                { "sticky", lockOn->sticky }
            };
        }
        if (auto* hitbox = registry.GetComponent<HitboxTrackingComponent>(entity)) {
            actor["hitbox"] = HitboxTrackingToJson(*hitbox);
        }
        if (auto* input = registry.GetComponent<ResolvedInputStateComponent>(entity)) {
            actor["input"] = ResolvedInputToJson(*input);
        }
        if (includeVisual) {
            json verifyParams = { { "entity", EntityToString(entity) } };
            actor["visual"] = HandleVisualVerifyEntity(kernel, registry, verifyParams);
        }
        return actor;
    }

    json BuildBattleFlowJson(Registry& registry, EntityID entity, const BattleFlowComponent& flow)
    {
        return {
            { "entity", EntityToString(entity) },
            { "name", EntityDisplayName(registry, entity) },
            { "phase", BattlePhaseToString(flow.phase) },
            { "phaseTimer", flow.phaseTimer },
            { "playerEntity", Entity::IsNull(flow.playerEntity) ? json(nullptr) : json(EntityToString(flow.playerEntity)) },
            { "bossEntity", Entity::IsNull(flow.bossEntity) ? json(nullptr) : json(EntityToString(flow.bossEntity)) },
            { "arenaEntity", Entity::IsNull(flow.arenaEntity) ? json(nullptr) : json(EntityToString(flow.arenaEntity)) },
            { "encounterRadius", flow.encounterRadius },
            { "introDuration", flow.introDuration },
            { "battleId", flow.battleId },
            { "autoStartOnPlayerEnter", flow.autoStartOnPlayerEnter }
        };
    }

    json HandleGameplayGetState(EngineKernel& kernel, Registry& registry, const json& params)
    {
        const bool includeVisual = params.value("includeVisual", true);
        const bool includeInput = params.value("includeInput", true);
        const bool includeDamageEvents = params.value("includeDamageEvents", true);
        const int eventLimit = (std::max)(0, params.value("eventLimit", 32));

        json actors = json::array();
        json battleFlows = json::array();
        json battleRules = json::array();
        json projectiles = json::array();
        json damageEvents = json::array();

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                if (!registry.IsAlive(entity)) {
                    continue;
                }
                if (IsGameplayActor(registry, entity)) {
                    json actor = GameplayActorToJson(kernel, registry, entity, includeVisual);
                    if (!includeInput) {
                        actor.erase("input");
                    }
                    actors.push_back(std::move(actor));
                }
                if (auto* flow = registry.GetComponent<BattleFlowComponent>(entity)) {
                    battleFlows.push_back(BuildBattleFlowJson(registry, entity, *flow));
                }
                if (auto* rules = registry.GetComponent<BattleRulesComponent>(entity)) {
                    battleRules.push_back({
                        { "entity", EntityToString(entity) },
                        { "name", EntityDisplayName(registry, entity) },
                        { "fields", ComponentToJson(*rules) }
                    });
                }
                if (auto* projectile = registry.GetComponent<ProjectileComponent>(entity)) {
                    json item = {
                        { "entity", EntityToString(entity) },
                        { "name", EntityDisplayName(registry, entity) },
                        { "velocity", Float3ToJson(projectile->velocity) },
                        { "lifetime", projectile->lifetime },
                        { "damage", projectile->damage },
                        { "radius", projectile->radius },
                        { "owner", Entity::IsNull(projectile->owner) ? json(nullptr) : json(EntityToString(projectile->owner)) },
                        { "targetsPlayer", projectile->targetsPlayer }
                    };
                    if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                        item["position"] = Float3ToJson(transform->worldPosition);
                    }
                    projectiles.push_back(std::move(item));
                }
                if (includeDamageEvents) {
                    if (auto* queue = registry.GetComponent<DamageEventComponent>(entity)) {
                        for (const auto& event : queue->events) {
                            if (eventLimit > 0 && static_cast<int>(damageEvents.size()) >= eventLimit) {
                                break;
                            }
                            damageEvents.push_back(DamageEventToJson(event));
                        }
                    }
                }
            }
        }

        if (includeDamageEvents && (eventLimit == 0 || static_cast<int>(damageEvents.size()) < eventLimit)) {
            for (const auto& event : DamageEventRuntimeQueue::GetAll()) {
                if (eventLimit > 0 && static_cast<int>(damageEvents.size()) >= eventLimit) {
                    break;
                }
                damageEvents.push_back(DamageEventToJson(event));
            }
        }

        return {
            { "mode", ModeToString(kernel.GetMode()) },
            { "frameCount", kernel.GetTime().frameCount },
            { "timeScale", kernel.GetTime().timeScale },
            { "actors", std::move(actors) },
            { "battle", {
                { "flows", std::move(battleFlows) },
                { "rules", std::move(battleRules) }
            } },
            { "projectiles", std::move(projectiles) },
            { "damageEvents", std::move(damageEvents) }
        };
    }

    uint64_t InputTimestampNow()
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    }

    InputEvent MakeKeyInputEvent(InputEventType type, uint32_t scancode, uint32_t keycode = 0)
    {
        InputEvent event;
        event.type = type;
        event.timestamp = InputTimestampNow();
        event.key.scancode = scancode;
        event.key.keycode = keycode;
        event.key.repeat = false;
        return event;
    }

    InputEvent MakeMouseButtonInputEvent(InputEventType type, uint8_t button, float x = 0.0f, float y = 0.0f)
    {
        InputEvent event;
        event.type = type;
        event.timestamp = InputTimestampNow();
        event.mouseButton.button = button;
        event.mouseButton.x = x;
        event.mouseButton.y = y;
        return event;
    }

    InputEvent MakeGamepadButtonInputEvent(InputEventType type, uint8_t button, uint32_t deviceId = 0)
    {
        InputEvent event;
        event.type = type;
        event.timestamp = InputTimestampNow();
        event.deviceId = deviceId;
        event.gamepadButton.button = button;
        return event;
    }

    InputEvent MakeGamepadAxisInputEvent(uint8_t axis, float value, uint32_t deviceId = 0)
    {
        InputEvent event;
        event.type = InputEventType::GamepadAxis;
        event.timestamp = InputTimestampNow();
        event.deviceId = deviceId;
        event.gamepadAxis.axis = axis;
        event.gamepadAxis.value = (std::clamp)(value, -1.0f, 1.0f);
        return event;
    }

    InputEvent MakeMouseMoveInputEvent(float x, float y, float dx, float dy)
    {
        InputEvent event;
        event.type = InputEventType::MouseMove;
        event.timestamp = InputTimestampNow();
        event.mouseMove.x = x;
        event.mouseMove.y = y;
        event.mouseMove.dx = dx;
        event.mouseMove.dy = dy;
        return event;
    }

    struct PendingInjectedInput
    {
        uint64_t releaseFrame = 0;
        InputEvent event;
    };

    std::vector<PendingInjectedInput>& PendingInjectedInputs()
    {
        static std::vector<PendingInjectedInput> pending;
        return pending;
    }

    uint32_t& PendingStepFrameCount()
    {
        static uint32_t pending = 0;
        return pending;
    }

    void ProcessPendingInjectedInputs(EngineKernel& kernel)
    {
        auto& pending = PendingInjectedInputs();
        const uint64_t frame = kernel.GetTime().frameCount;
        size_t write = 0;
        for (size_t i = 0; i < pending.size(); ++i) {
            if (pending[i].releaseFrame <= frame) {
                kernel.InjectInputEvent(pending[i].event);
            }
            else {
                if (write != i) {
                    pending[write] = pending[i];
                }
                ++write;
            }
        }
        pending.resize(write);

        uint32_t& pendingSteps = PendingStepFrameCount();
        if (pendingSteps > 0) {
            if (kernel.GetMode() != EngineMode::Pause) {
                if (kernel.GetMode() == EngineMode::Editor) {
                    kernel.Play();
                }
                if (kernel.GetMode() == EngineMode::Play) {
                    kernel.Pause();
                }
            }
            kernel.Step();
            --pendingSteps;
        }
    }

    const InputActionMapComponent* FindInputActionMapForAutomation(Registry& registry, const json& params)
    {
        if (params.contains("entity")) {
            const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
            if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
                throw MakeError("entity_not_found", "Input target entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
            }
            const auto* map = registry.GetComponent<InputActionMapComponent>(entity);
            if (!map) {
                throw MakeError("component_not_found", "Input target does not have InputActionMapComponent.", { { "entity", EntityToString(entity) } });
            }
            return map;
        }

        const int requestedPlayerId = params.value("playerId", params.value("player", -1));
        const std::string targetName = params.value("targetName", std::string{});
        const InputActionMapComponent* fallback = nullptr;
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                if (!registry.IsAlive(entity)) {
                    continue;
                }
                const auto* map = registry.GetComponent<InputActionMapComponent>(entity);
                if (!map) {
                    continue;
                }
                if (!fallback) {
                    fallback = map;
                }
                if (requestedPlayerId >= 0) {
                    if (auto* player = registry.GetComponent<PlayerTagComponent>(entity)) {
                        if (player->playerId == requestedPlayerId) {
                            return map;
                        }
                    }
                }
                if (!targetName.empty() && EntityDisplayName(registry, entity) == targetName) {
                    return map;
                }
            }
        }
        if (!fallback) {
            throw MakeError("input_map_not_found", "No InputActionMapComponent was found.");
        }
        return fallback;
    }

    const ActionBinding* FindActionBindingForAutomation(Registry& registry, const json& params)
    {
        const std::string actionName = params.value("action", params.value("actionName", std::string{}));
        if (actionName.empty()) {
            return nullptr;
        }
        const InputActionMapComponent* map = FindInputActionMapForAutomation(registry, params);
        for (const ActionBinding& action : map->asset.actions) {
            if (action.actionName == actionName) {
                return &action;
            }
        }
        throw MakeError("input_action_not_found", "Action was not found in the target InputActionMapComponent.", { { "action", actionName } });
    }

    const AxisBinding* FindAxisBindingForAutomation(Registry& registry, const json& params)
    {
        const std::string axisName = params.value("axis", params.value("axisName", std::string{}));
        if (axisName.empty()) {
            return nullptr;
        }
        const InputActionMapComponent* map = FindInputActionMapForAutomation(registry, params);
        for (const AxisBinding& axis : map->asset.axes) {
            if (axis.axisName == axisName) {
                return &axis;
            }
        }
        throw MakeError("input_axis_not_found", "Axis was not found in the target InputActionMapComponent.", { { "axis", axisName } });
    }

    json InjectActionBindingEvent(EngineKernel& kernel, const ActionBinding& binding, bool pressed, uint32_t deviceId)
    {
        if (binding.scancode != 0) {
            kernel.InjectInputEvent(MakeKeyInputEvent(pressed ? InputEventType::KeyDown : InputEventType::KeyUp, binding.scancode));
            return { { "source", "keyboard" }, { "scancode", binding.scancode }, { "pressed", pressed } };
        }
        if (binding.mouseButton != 0) {
            kernel.InjectInputEvent(MakeMouseButtonInputEvent(pressed ? InputEventType::MouseButtonDown : InputEventType::MouseButtonUp, binding.mouseButton));
            return { { "source", "mouse" }, { "button", binding.mouseButton }, { "pressed", pressed } };
        }
        if (binding.gamepadButton != 0xFF) {
            kernel.InjectInputEvent(MakeGamepadButtonInputEvent(pressed ? InputEventType::GamepadButtonDown : InputEventType::GamepadButtonUp, binding.gamepadButton, deviceId));
            return { { "source", "gamepad_button" }, { "button", binding.gamepadButton }, { "pressed", pressed }, { "deviceId", deviceId } };
        }
        if (binding.gamepadAxis != 0xFF) {
            kernel.InjectInputEvent(MakeGamepadAxisInputEvent(binding.gamepadAxis, pressed ? binding.axisDirection : 0.0f, deviceId));
            return { { "source", "gamepad_axis" }, { "axis", binding.gamepadAxis }, { "value", pressed ? binding.axisDirection : 0.0f }, { "deviceId", deviceId } };
        }
        throw MakeError("input_binding_unassigned", "Action binding has no keyboard, mouse, or gamepad source.", { { "action", binding.actionName } });
    }

    json HandleGameInputPress(EngineKernel& kernel, Registry& registry, const json& params)
    {
        const uint32_t deviceId = params.value("deviceId", 0u);
        if (const ActionBinding* action = FindActionBindingForAutomation(registry, params)) {
            json result = InjectActionBindingEvent(kernel, *action, true, deviceId);
            result["action"] = action->actionName;
            return result;
        }
        if (params.contains("scancode")) {
            const uint32_t scancode = params["scancode"].get<uint32_t>();
            kernel.InjectInputEvent(MakeKeyInputEvent(InputEventType::KeyDown, scancode, params.value("keycode", 0u)));
            return { { "source", "keyboard" }, { "scancode", scancode }, { "pressed", true } };
        }
        if (params.contains("mouseButton")) {
            const uint8_t button = params["mouseButton"].get<uint8_t>();
            kernel.InjectInputEvent(MakeMouseButtonInputEvent(InputEventType::MouseButtonDown, button, params.value("x", 0.0f), params.value("y", 0.0f)));
            return { { "source", "mouse" }, { "button", button }, { "pressed", true } };
        }
        if (params.contains("gamepadButton")) {
            const uint8_t button = params["gamepadButton"].get<uint8_t>();
            kernel.InjectInputEvent(MakeGamepadButtonInputEvent(InputEventType::GamepadButtonDown, button, deviceId));
            return { { "source", "gamepad_button" }, { "button", button }, { "pressed", true }, { "deviceId", deviceId } };
        }
        throw MakeError("missing_param", "game.input.press requires action, scancode, mouseButton, or gamepadButton.");
    }

    json HandleGameInputRelease(EngineKernel& kernel, Registry& registry, const json& params)
    {
        const uint32_t deviceId = params.value("deviceId", 0u);
        if (const ActionBinding* action = FindActionBindingForAutomation(registry, params)) {
            json result = InjectActionBindingEvent(kernel, *action, false, deviceId);
            result["action"] = action->actionName;
            return result;
        }
        if (params.contains("scancode")) {
            const uint32_t scancode = params["scancode"].get<uint32_t>();
            kernel.InjectInputEvent(MakeKeyInputEvent(InputEventType::KeyUp, scancode, params.value("keycode", 0u)));
            return { { "source", "keyboard" }, { "scancode", scancode }, { "pressed", false } };
        }
        if (params.contains("mouseButton")) {
            const uint8_t button = params["mouseButton"].get<uint8_t>();
            kernel.InjectInputEvent(MakeMouseButtonInputEvent(InputEventType::MouseButtonUp, button, params.value("x", 0.0f), params.value("y", 0.0f)));
            return { { "source", "mouse" }, { "button", button }, { "pressed", false } };
        }
        if (params.contains("gamepadButton")) {
            const uint8_t button = params["gamepadButton"].get<uint8_t>();
            kernel.InjectInputEvent(MakeGamepadButtonInputEvent(InputEventType::GamepadButtonUp, button, deviceId));
            return { { "source", "gamepad_button" }, { "button", button }, { "pressed", false }, { "deviceId", deviceId } };
        }
        throw MakeError("missing_param", "game.input.release requires action, scancode, mouseButton, or gamepadButton.");
    }

    json HandleGameInputTap(EngineKernel& kernel, Registry& registry, const json& params)
    {
        const uint64_t releaseFrame = kernel.GetTime().frameCount + (std::max)(1, params.value("holdFrames", 1));
        const uint32_t deviceId = params.value("deviceId", 0u);
        json pressResult = HandleGameInputPress(kernel, registry, params);

        InputEvent releaseEvent;
        bool hasRelease = true;
        if (const ActionBinding* action = FindActionBindingForAutomation(registry, params)) {
            if (action->scancode != 0) {
                releaseEvent = MakeKeyInputEvent(InputEventType::KeyUp, action->scancode);
            }
            else if (action->mouseButton != 0) {
                releaseEvent = MakeMouseButtonInputEvent(InputEventType::MouseButtonUp, action->mouseButton);
            }
            else if (action->gamepadButton != 0xFF) {
                releaseEvent = MakeGamepadButtonInputEvent(InputEventType::GamepadButtonUp, action->gamepadButton, deviceId);
            }
            else if (action->gamepadAxis != 0xFF) {
                releaseEvent = MakeGamepadAxisInputEvent(action->gamepadAxis, 0.0f, deviceId);
            }
            else {
                hasRelease = false;
            }
        }
        else if (params.contains("scancode")) {
            releaseEvent = MakeKeyInputEvent(InputEventType::KeyUp, params["scancode"].get<uint32_t>(), params.value("keycode", 0u));
        }
        else if (params.contains("mouseButton")) {
            releaseEvent = MakeMouseButtonInputEvent(InputEventType::MouseButtonUp, params["mouseButton"].get<uint8_t>(), params.value("x", 0.0f), params.value("y", 0.0f));
        }
        else if (params.contains("gamepadButton")) {
            releaseEvent = MakeGamepadButtonInputEvent(InputEventType::GamepadButtonUp, params["gamepadButton"].get<uint8_t>(), deviceId);
        }
        else {
            hasRelease = false;
        }

        if (hasRelease) {
            PendingInjectedInputs().push_back({ releaseFrame, releaseEvent });
        }
        pressResult["tap"] = true;
        pressResult["releaseFrame"] = releaseFrame;
        return pressResult;
    }

    json HandleGameInputAxis(EngineKernel& kernel, Registry& registry, const json& params)
    {
        const float value = (std::clamp)(params.value("value", 0.0f), -1.0f, 1.0f);
        const uint32_t deviceId = params.value("deviceId", 0u);

        if (const AxisBinding* axis = FindAxisBindingForAutomation(registry, params)) {
            if (axis->gamepadAxis != 0xFF) {
                kernel.InjectInputEvent(MakeGamepadAxisInputEvent(axis->gamepadAxis, value, deviceId));
                return { { "source", "gamepad_axis" }, { "axisName", axis->axisName }, { "axis", axis->gamepadAxis }, { "value", value }, { "deviceId", deviceId } };
            }
            const uint32_t pressKey = value >= 0.0f ? axis->positiveKey : axis->negativeKey;
            const uint32_t releaseKey = value >= 0.0f ? axis->negativeKey : axis->positiveKey;
            if (releaseKey != 0) {
                kernel.InjectInputEvent(MakeKeyInputEvent(InputEventType::KeyUp, releaseKey));
            }
            if (pressKey != 0 && std::fabs(value) > 0.001f) {
                kernel.InjectInputEvent(MakeKeyInputEvent(InputEventType::KeyDown, pressKey));
            }
            else if (axis->positiveKey != 0) {
                kernel.InjectInputEvent(MakeKeyInputEvent(InputEventType::KeyUp, axis->positiveKey));
            }
            if (std::fabs(value) <= 0.001f && axis->negativeKey != 0) {
                kernel.InjectInputEvent(MakeKeyInputEvent(InputEventType::KeyUp, axis->negativeKey));
            }
            return { { "source", "keyboard_axis" }, { "axisName", axis->axisName }, { "value", value } };
        }

        if (params.contains("gamepadAxis")) {
            const uint8_t axis = params["gamepadAxis"].get<uint8_t>();
            kernel.InjectInputEvent(MakeGamepadAxisInputEvent(axis, value, deviceId));
            return { { "source", "gamepad_axis" }, { "axis", axis }, { "value", value }, { "deviceId", deviceId } };
        }
        throw MakeError("missing_param", "game.input.axis requires axis or gamepadAxis.");
    }

    json HandleGameInputMouseMove(EngineKernel& kernel, const json& params)
    {
        kernel.InjectInputEvent(MakeMouseMoveInputEvent(
            params.value("x", 0.0f),
            params.value("y", 0.0f),
            params.value("dx", 0.0f),
            params.value("dy", 0.0f)));
        return {
            { "x", params.value("x", 0.0f) },
            { "y", params.value("y", 0.0f) },
            { "dx", params.value("dx", 0.0f) },
            { "dy", params.value("dy", 0.0f) }
        };
    }

    json EngineModeResult(EngineKernel& kernel)
    {
        return {
            { "mode", ModeToString(kernel.GetMode()) },
            { "frameCount", kernel.GetTime().frameCount },
            { "timeScale", kernel.GetTime().timeScale },
            { "pendingStepFrames", PendingStepFrameCount() }
        };
    }

    json HandleGamePlay(EngineKernel& kernel)
    {
        if (kernel.GetMode() != EngineMode::Play) {
            kernel.Play();
        }
        return EngineModeResult(kernel);
    }

    json HandleGamePause(EngineKernel& kernel)
    {
        if (kernel.GetMode() == EngineMode::Play) {
            kernel.Pause();
        }
        else if (kernel.GetMode() == EngineMode::Editor) {
            kernel.Play();
            if (kernel.GetMode() == EngineMode::Play) {
                kernel.Pause();
            }
        }
        return EngineModeResult(kernel);
    }

    json HandleGameStop(EngineKernel& kernel)
    {
        PendingStepFrameCount() = 0;
        if (kernel.GetMode() != EngineMode::Editor) {
            kernel.Stop();
        }
        return EngineModeResult(kernel);
    }

    json HandleGameStepFrames(EngineKernel& kernel, const json& params)
    {
        const uint32_t frames = (std::max)(1, params.value("frames", 1));
        PendingStepFrameCount() += frames;
        if (kernel.GetMode() == EngineMode::Editor) {
            kernel.Play();
        }
        if (kernel.GetMode() == EngineMode::Play) {
            kernel.Pause();
        }
        return EngineModeResult(kernel);
    }

    json HandleGameSetTimeScale(EngineKernel& kernel, const json& params)
    {
        const float scale = (std::max)(0.0f, params.value("timeScale", params.value("scale", 1.0f)));
        kernel.SetTimeScale(scale);
        return EngineModeResult(kernel);
    }

    json FlowEventToJson(const FlowEvent& event, int index)
    {
        return {
            { "index", index },
            { "name", event.name },
            { "value", event.value }
        };
    }

    json HandleGameplayGetEvents(EngineKernel& kernel, const json& params)
    {
        const bool clear = params.value("clear", false);
        const bool includeFlow = params.value("includeFlow", true);
        const bool includeDamage = params.value("includeDamage", true);
        const int limit = (std::max)(0, params.value("limit", 64));
        json flow = json::array();
        json damage = json::array();

        if (includeFlow) {
            const auto& recent = kernel.GetFlowEventQueue().GetRecentEvents();
            const int start = (limit > 0 && static_cast<int>(recent.size()) > limit)
                ? static_cast<int>(recent.size()) - limit
                : 0;
            for (int i = start; i < static_cast<int>(recent.size()); ++i) {
                flow.push_back(FlowEventToJson(recent[static_cast<size_t>(i)], i));
            }
        }

        if (includeDamage) {
            const auto& recentDamage = DamageEventRuntimeQueue::GetRecent();
            const int start = (limit > 0 && static_cast<int>(recentDamage.size()) > limit)
                ? static_cast<int>(recentDamage.size()) - limit
                : 0;
            for (int i = start; i < static_cast<int>(recentDamage.size()); ++i) {
                damage.push_back(DamageEventToJson(recentDamage[static_cast<size_t>(i)]));
            }
        }

        if (clear) {
            DamageEventRuntimeQueue::ClearRecent();
        }

        return {
            { "frameCount", kernel.GetTime().frameCount },
            { "flowEvents", std::move(flow) },
            { "damageEvents", std::move(damage) }
        };
    }

    json HandleGameplayClearEvents()
    {
        DamageEventRuntimeQueue::ClearRecent();
        return { { "ok", true } };
    }

    json HandleListEntities(Registry& registry)
    {
        json entitiesJson = json::array();
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const Signature signature = archetype->GetSignature();
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                if (registry.IsAlive(entity)) {
                    entitiesJson.push_back(EntitySummary(registry, entity, signature));
                }
            }
        }
        return { { "entities", std::move(entitiesJson) } };
    }

    json HandleGetEntity(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }
        return { { "entity", EntityDetail(registry, entity) } };
    }

    json HandleSelectEntity(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }
        EditorSelection::Instance().SelectEntity(entity);
        return { { "selected", EntityToString(entity) } };
    }

    json HandleCreateEmpty(Registry& registry, const json& params)
    {
        TransformComponent transform{};
        if (params.contains("position")) {
            ReadFloat3(params["position"], transform.localPosition);
        }
        if (params.contains("rotation")) {
            ReadFloat4(params["rotation"], transform.localRotation);
        }
        if (params.contains("scale")) {
            ReadFloat3(params["scale"], transform.localScale);
        }
        transform.isDirty = true;

        const std::string name = params.value("name", std::string("Empty"));
        const EntityID parent = EntityFromJson(params.value("parent", json(nullptr)));
        const bool recordUndo = params.value("recordUndo", true);
        EntityID entity = CreateEntityFromSnapshot(
            registry,
            BuildEntitySnapshot(name, transform, std::nullopt),
            parent,
            recordUndo,
            "AI Create Empty");

        if (params.value("select", true) && !Entity::IsNull(entity)) {
            EditorSelection::Instance().SelectEntity(entity);
        }

        return { { "entity", EntityToString(entity) } };
    }

    json HandleCreateModelEntity(Registry& registry, const json& params)
    {
        TransformComponent transform{};
        if (params.contains("position")) {
            ReadFloat3(params["position"], transform.localPosition);
        }
        if (params.contains("rotation")) {
            ReadFloat4(params["rotation"], transform.localRotation);
        }
        if (params.contains("scale")) {
            ReadFloat3(params["scale"], transform.localScale);
        }
        transform.isDirty = true;

        MeshComponent mesh{};
        mesh.modelFilePath = params.value("modelFilePath", std::string{});
        if (mesh.modelFilePath.empty()) {
            throw MakeError("missing_param", "modelFilePath is required.");
        }
        ResolveProjectPath(mesh.modelFilePath, PathAccess::ReadAsset, true);
        mesh.model = ResourceManager::Instance().CreateModelInstance(mesh.modelFilePath);
        if (!mesh.model) {
            throw MakeError("asset_load_failed", "Failed to load model asset.", { { "modelFilePath", mesh.modelFilePath } });
        }
        mesh.isVisible = params.value("isVisible", true);
        mesh.castShadow = params.value("castShadow", true);

        const std::string name = params.value("name", std::string("Model Entity"));
        const EntityID parent = EntityFromJson(params.value("parent", json(nullptr)));
        const bool recordUndo = params.value("recordUndo", true);
        EntityID entity = CreateEntityFromSnapshot(
            registry,
            BuildEntitySnapshot(name, transform, mesh),
            parent,
            recordUndo,
            "AI Create Model Entity");

        if (params.value("select", true) && !Entity::IsNull(entity)) {
            EditorSelection::Instance().SelectEntity(entity);
        }

        return { { "entity", EntityToString(entity) } };
    }

    json HandleSetTransform(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        auto* transform = registry.GetComponent<TransformComponent>(entity);
        if (!transform) {
            throw MakeError("component_not_found", "TransformComponent was not found.", { { "entity", EntityToString(entity) } });
        }

        const TransformComponent before = *transform;
        TransformComponent after = before;
        if (params.contains("position")) {
            ReadFloat3(params["position"], after.localPosition);
        }
        if (params.contains("localPosition")) {
            ReadFloat3(params["localPosition"], after.localPosition);
        }
        if (params.contains("rotation")) {
            ReadFloat4(params["rotation"], after.localRotation);
        }
        if (params.contains("localRotation")) {
            ReadFloat4(params["localRotation"], after.localRotation);
        }
        if (params.contains("scale")) {
            ReadFloat3(params["scale"], after.localScale);
        }
        if (params.contains("localScale")) {
            ReadFloat3(params["localScale"], after.localScale);
        }
        after.isDirty = true;

        if (params.value("recordUndo", true)) {
            auto action = std::make_unique<ComponentUndoAction<TransformComponent>>(entity, before, after);
            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
        }
        else {
            *transform = after;
        }

        HierarchySystem::MarkDirtyRecursive(entity, registry);
        PrefabSystem::MarkPrefabOverride(entity, registry);
        return { { "entity", EntityToString(entity) }, { "transform", TransformToJson(after) } };
    }

    json HandleDeleteEntity(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }
        if (!PrefabSystem::CanDelete(entity, registry)) {
            throw MakeError("operation_not_allowed", "Prefab instance children cannot be deleted directly.");
        }

        if (params.value("recordUndo", true)) {
            auto snapshot = EntitySnapshot::CaptureSubtree(entity, registry);
            auto action = std::make_unique<DeleteEntityAction>(std::move(snapshot), entity);
            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
        }
        else {
            EntitySnapshot::DestroySubtree(entity, registry);
        }

        if (EditorSelection::Instance().IsEntitySelected(entity)) {
            EditorSelection::Instance().Clear();
        }

        return { { "deleted", EntityToString(entity) } };
    }

    json HandleDuplicateEntity(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }
        if (!PrefabSystem::CanDuplicate(entity, registry)) {
            throw MakeError("operation_not_allowed", "Entity cannot be duplicated because of prefab restrictions.", {
                { "entity", EntityToString(entity) }
            });
        }

        EntityID duplicate = Entity::NULL_ID;
        if (params.value("recordUndo", true)) {
            EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(entity, registry);
            if (snapshot.nodes.empty()) {
                throw MakeError("operation_not_allowed", "Entity subtree could not be captured.");
            }
            EntitySnapshot::AppendRootNameSuffix(snapshot, params.value("nameSuffix", std::string(" (Clone)")));
            const EntityID parent = GetEntityParent(registry, entity);
            auto action = std::make_unique<DuplicateEntityAction>(std::move(snapshot), parent);
            auto* actionPtr = action.get();
            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
            duplicate = actionPtr->GetLiveRoot();
        }
        else {
            duplicate = DuplicateSystem::Duplicate(entity, registry);
        }

        if (Entity::IsNull(duplicate)) {
            throw MakeError("operation_not_allowed", "Failed to duplicate entity.", { { "entity", EntityToString(entity) } });
        }

        if (params.value("select", true)) {
            EditorSelection::Instance().SelectEntity(duplicate);
        }
        return { { "source", EntityToString(entity) }, { "entity", EntityToString(duplicate) } };
    }

    json HandleReparentEntity(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        const EntityID newParent = EntityFromJson(params.value("parent", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }
        if (!Entity::IsNull(newParent) && !registry.IsAlive(newParent)) {
            throw MakeError("entity_not_found", "Parent entity is not alive.", { { "parent", params.value("parent", json(nullptr)) } });
        }
        if (HierarchySystem::WouldCreateCycle(entity, newParent, registry)) {
            throw MakeError("operation_not_allowed", "Reparent would create a hierarchy cycle.");
        }
        if (!PrefabSystem::CanReparent(entity, newParent, registry)) {
            throw MakeError("operation_not_allowed", "Entity cannot be reparented because of prefab restrictions.");
        }

        const EntityID oldParent = GetEntityParent(registry, entity);
        const bool keepWorld = params.value("keepWorldTransform", true);
        if (params.value("recordUndo", true)) {
            auto action = std::make_unique<ReparentEntityAction>(entity, newParent, oldParent, keepWorld);
            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
        }
        else {
            HierarchySystem::Reparent(entity, newParent, registry, keepWorld);
        }

        MarkEntityEdited(registry, entity);
        return {
            { "entity", EntityToString(entity) },
            { "oldParent", Entity::IsNull(oldParent) ? json(nullptr) : json(EntityToString(oldParent)) },
            { "parent", Entity::IsNull(newParent) ? json(nullptr) : json(EntityToString(newParent)) }
        };
    }

    json HandleInstantiatePrefab(Registry& registry, const json& params)
    {
        const std::string prefabPath = params.value("path", params.value("prefabPath", std::string{}));
        if (prefabPath.empty()) {
            throw MakeError("missing_param", "path is required.");
        }
        const std::filesystem::path safePath = ResolveProjectPath(prefabPath, PathAccess::ReadAsset, true);
        if (safePath.extension() != ".prefab") {
            throw MakeError("invalid_param", "Prefab path must use .prefab extension.", { { "path", prefabPath } });
        }

        const EntityID parent = EntityFromJson(params.value("parent", json(nullptr)));
        if (!Entity::IsNull(parent) && !registry.IsAlive(parent)) {
            throw MakeError("entity_not_found", "Parent entity is not alive.", { { "parent", params.value("parent", json(nullptr)) } });
        }
        if (!PrefabSystem::CanCreateChild(parent, registry)) {
            throw MakeError("operation_not_allowed", "Cannot create a prefab child under the requested parent.");
        }

        const EntityID entity = PrefabSystem::InstantiatePrefab(safePath, registry, parent);
        if (Entity::IsNull(entity)) {
            throw MakeError("prefab_instantiate_failed", "Failed to instantiate prefab.", { { "path", prefabPath } });
        }

        if (params.contains("position")) {
            if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                ReadFloat3(params["position"], transform->localPosition);
                transform->isDirty = true;
                HierarchySystem::MarkDirtyRecursive(entity, registry);
            }
        }

        if (params.value("select", true)) {
            EditorSelection::Instance().SelectEntity(entity);
        }
        return { { "entity", EntityToString(entity) }, { "path", ToGenericProjectPath(safePath) } };
    }

    json HandleFocusEntity(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        DirectX::XMFLOAT3 center{};
        float radius = 1.0f;
        if (!BuildEntityFocusBounds(registry, entity, center, radius)) {
            throw MakeError("operation_not_allowed", "Could not compute entity bounds.", { { "entity", EntityToString(entity) } });
        }

        const DirectX::XMFLOAT3 direction = editor->GetEditorCameraDirection();
        const float distance = params.value("distance", ComputeFocusDistanceForRadius(radius, editor->GetEditorCameraFovY()));
        const DirectX::XMFLOAT3 position = {
            center.x - direction.x * distance,
            center.y - direction.y * distance,
            center.z - direction.z * distance
        };
        editor->SetEditorCameraLookAt(position, center);

        if (params.value("select", true)) {
            EditorSelection::Instance().SelectEntity(entity);
        }
        return {
            { "entity", EntityToString(entity) },
            { "cameraPosition", Float3ToJson(position) },
            { "target", Float3ToJson(center) },
            { "radius", radius }
        };
    }

    json HandleFrameSelection(EngineKernel& kernel, Registry& registry, const json& params)
    {
        const EntityID primary = EditorSelection::Instance().GetPrimaryEntity();
        if (Entity::IsNull(primary) || !registry.IsAlive(primary)) {
            throw MakeError("entity_not_found", "No live primary selected entity.");
        }
        json focusParams = params;
        focusParams["entity"] = EntityToString(primary);
        return HandleFocusEntity(kernel, registry, focusParams);
    }

    json HandleRaycastSceneView(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        DirectX::XMFLOAT3 origin{};
        DirectX::XMFLOAT3 direction{};
        if (!BuildSceneViewRay(*editor, params, origin, direction)) {
            throw MakeError("invalid_param", "Could not build a ray from the requested Scene View position.");
        }

        const float maxDistance = params.value("maxDistance", 100000.0f);
        json objectHit = RaycastSceneObjects(registry, origin, direction, maxDistance);

        DirectX::XMFLOAT3 groundPoint{};
        float groundDistance = maxDistance;
        json groundHit = nullptr;
        if (params.value("includeGroundPlane", true) &&
            IntersectGroundPlane(origin, direction, groundPoint, groundDistance) &&
            groundDistance <= maxDistance) {
            groundHit = {
                { "distance", groundDistance },
                { "position", Float3ToJson(groundPoint) }
            };
        }

        return {
            { "origin", Float3ToJson(origin) },
            { "direction", Float3ToJson(direction) },
            { "objectHit", std::move(objectHit) },
            { "groundHit", std::move(groundHit) }
        };
    }

    json HandlePlaceAssetAtCursor(EngineKernel& kernel, Registry& registry, const json& params)
    {
        DirectX::XMFLOAT3 position{};
        if (params.contains("position")) {
            ReadFloat3(params["position"], position);
        }
        else {
            auto* editor = kernel.GetEditorLayer();
            if (!editor) {
                throw MakeError("operation_not_allowed", "EditorLayer is not available.");
            }

            DirectX::XMFLOAT3 origin{};
            DirectX::XMFLOAT3 direction{};
            if (!BuildSceneViewRay(*editor, params, origin, direction)) {
                throw MakeError("invalid_param", "Could not build a ray from the requested Scene View position.");
            }

            float groundDistance = 0.0f;
            if (!IntersectGroundPlane(origin, direction, position, groundDistance)) {
                throw MakeError("operation_not_allowed", "Scene View ray did not hit the ground plane.");
            }
        }

        const std::string assetPath = params.value("assetPath", params.value("path", std::string{}));
        if (assetPath.empty()) {
            throw MakeError("missing_param", "assetPath is required.");
        }

        const std::filesystem::path safePath = ResolveProjectPath(assetPath, PathAccess::ReadAsset, true);
        const std::string extension = safePath.extension().string();
        if (extension == ".prefab") {
            json prefabParams = {
                { "path", assetPath },
                { "position", Float3ToJson(position) },
                { "parent", params.value("parent", json(nullptr)) },
                { "select", params.value("select", true) }
            };
            return HandleInstantiatePrefab(registry, prefabParams);
        }

        json createParams = {
            { "name", params.value("name", safePath.stem().string()) },
            { "modelFilePath", assetPath },
            { "position", Float3ToJson(position) },
            { "parent", params.value("parent", json(nullptr)) },
            { "select", params.value("select", true) },
            { "recordUndo", params.value("recordUndo", true) }
        };
        if (params.contains("scale")) {
            createParams["scale"] = params["scale"];
        }
        if (params.contains("rotation")) {
            createParams["rotation"] = params["rotation"];
        }

        json result = HandleCreateModelEntity(registry, createParams);
        result["path"] = ToGenericProjectPath(safePath);
        result["position"] = Float3ToJson(position);
        return result;
    }

    json HandleAssetBrowserList(const json& params)
    {
        const std::string pathText = params.value("path", std::string("Data"));
        const std::filesystem::path dir = ResolveProjectPath(pathText, PathAccess::ReadAsset, true);
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) {
            throw MakeError("invalid_param", "path must be a directory.", { { "path", pathText } });
        }

        const std::string typeFilter = params.value("type", std::string{});
        json entries = json::array();
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) {
                break;
            }
            json item = AssetEntryToJson(entry.path());
            if (!typeFilter.empty() && item["type"].get<std::string>() != typeFilter) {
                continue;
            }
            entries.push_back(std::move(item));
        }
        return { { "path", ToGenericProjectPath(dir) }, { "entries", std::move(entries) } };
    }

    json HandleAssetBrowserSearch(const json& params)
    {
        const std::string query = ToLowerCopy(params.value("query", std::string{}));
        const std::string rootText = params.value("root", std::string("Data"));
        const std::string typeFilter = params.value("type", std::string{});
        const int limit = params.value("limit", 100);
        if (query.empty()) {
            throw MakeError("missing_param", "query is required.");
        }

        const std::filesystem::path root = ResolveProjectPath(rootText, PathAccess::ReadAsset, true);
        std::error_code ec;
        json results = json::array();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
            if (ec || static_cast<int>(results.size()) >= limit) {
                break;
            }
            const std::string name = ToLowerCopy(entry.path().filename().string());
            const std::string generic = ToLowerCopy(ToGenericProjectPath(entry.path()));
            if (name.find(query) == std::string::npos && generic.find(query) == std::string::npos) {
                continue;
            }
            json item = AssetEntryToJson(entry.path());
            if (!typeFilter.empty() && item["type"].get<std::string>() != typeFilter) {
                continue;
            }
            results.push_back(std::move(item));
        }
        return { { "query", query }, { "results", std::move(results) } };
    }

    json HandleAssetBrowserCreateFolder(const json& params)
    {
        const std::filesystem::path parent = ResolveProjectPath(params.value("parent", std::string("Data")), PathAccess::WriteAsset, true);
        const std::string name = params.value("name", std::string{});
        if (name.empty()) {
            throw MakeError("missing_param", "name is required.");
        }
        const std::filesystem::path target = (parent / name).lexically_normal();
        ResolveProjectPath(target.string(), PathAccess::WriteAsset, false);
        std::error_code ec;
        std::filesystem::create_directories(target, ec);
        if (ec) {
            throw MakeError("asset_operation_failed", "Failed to create folder.", { { "path", target.string() } });
        }
        return { { "path", ToGenericProjectPath(target) } };
    }

    json HandleAssetBrowserCopyMove(const json& params, bool move)
    {
        const std::filesystem::path source = ResolveProjectPath(params.value("source", std::string{}), PathAccess::ReadAsset, true);
        const std::filesystem::path destinationDir = ResolveProjectPath(params.value("destination", std::string{}), PathAccess::WriteAsset, true);
        std::error_code ec;
        if (!std::filesystem::is_directory(destinationDir, ec)) {
            throw MakeError("invalid_param", "destination must be a directory.");
        }

        const std::filesystem::path target = destinationDir / source.filename();
        if (move) {
            std::filesystem::rename(source, target, ec);
        }
        else if (std::filesystem::is_directory(source, ec)) {
            std::filesystem::copy(source, target, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
        }
        else {
            std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
        }
        if (ec) {
            throw MakeError("asset_operation_failed", move ? "Failed to move asset." : "Failed to copy asset.", {
                { "source", source.string() },
                { "destination", target.string() }
            });
        }
        return { { "path", ToGenericProjectPath(target) } };
    }

    json HandleAssetBrowserRename(const json& params)
    {
        const std::filesystem::path source = ResolveProjectPath(params.value("path", std::string{}), PathAccess::WriteAsset, true);
        const std::string newName = params.value("newName", std::string{});
        if (newName.empty()) {
            throw MakeError("missing_param", "newName is required.");
        }
        const std::filesystem::path target = source.parent_path() / newName;
        ResolveProjectPath(target.string(), PathAccess::WriteAsset, false);
        std::error_code ec;
        std::filesystem::rename(source, target, ec);
        if (ec) {
            throw MakeError("asset_operation_failed", "Failed to rename asset.", { { "path", source.string() } });
        }
        return { { "path", ToGenericProjectPath(target) } };
    }

    json HandleAssetBrowserDelete(const json& params)
    {
        const std::filesystem::path source = ResolveProjectPath(params.value("path", std::string{}), PathAccess::WriteAsset, true);
        const bool permanent = params.value("permanent", false);
        std::error_code ec;
        if (permanent) {
            if (std::filesystem::is_directory(source, ec)) {
                std::filesystem::remove_all(source, ec);
            }
            else {
                std::filesystem::remove(source, ec);
            }
            if (ec) {
                throw MakeError("asset_operation_failed", "Failed to delete asset.", { { "path", source.string() } });
            }
            return { { "deleted", ToGenericProjectPath(source) }, { "permanent", true } };
        }

        const std::filesystem::path trashRoot = ResolveProjectPath("Data/.ai_trash", PathAccess::WriteAsset, false);
        std::filesystem::create_directories(trashRoot, ec);
        const std::filesystem::path target = trashRoot / (SanitizeFileStem(source.stem().string()) + "_" + MakeTimestampSuffix() + source.extension().string());
        std::filesystem::rename(source, target, ec);
        if (ec) {
            throw MakeError("asset_operation_failed", "Failed to move asset to AI trash.", { { "path", source.string() } });
        }
        return { { "deleted", ToGenericProjectPath(source) }, { "trashPath", ToGenericProjectPath(target) }, { "permanent", false } };
    }

    json HandlePrefabSave(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }

        const std::string path = params.value("path", std::string{});
        std::filesystem::path savedPath;
        bool ok = false;
        if (path.empty()) {
            const std::filesystem::path dir = ResolveProjectPath(params.value("directory", std::string("Data/Prefabs")), PathAccess::WriteAsset, false);
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            ok = PrefabSystem::SaveEntityAsPrefab(entity, registry, dir, &savedPath);
        }
        else {
            savedPath = ResolveProjectPath(path, PathAccess::WriteAsset, false);
            if (savedPath.extension() != ".prefab") {
                throw MakeError("invalid_param", "Prefab path must use .prefab extension.", { { "path", path } });
            }
            std::error_code ec;
            std::filesystem::create_directories(savedPath.parent_path(), ec);
            ok = PrefabSystem::SaveEntityToPrefabPath(entity, registry, savedPath);
        }
        if (!ok) {
            throw MakeError("prefab_save_failed", "Failed to save prefab.");
        }
        return { { "path", ToGenericProjectPath(savedPath) } };
    }

    json HandlePrefabApply(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }
        if (!PrefabSystem::ApplyPrefab(entity, registry)) {
            throw MakeError("prefab_apply_failed", "Failed to apply prefab.", { { "entity", EntityToString(entity) } });
        }
        return { { "entity", EntityToString(entity) } };
    }

    json HandlePrefabUnpack(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }
        if (!PrefabSystem::UnpackPrefab(entity, registry)) {
            throw MakeError("prefab_unpack_failed", "Failed to unpack prefab.", { { "entity", EntityToString(entity) } });
        }
        return { { "entity", EntityToString(entity) } };
    }

    json MaterialToJson(const MaterialAsset& material)
    {
        return {
            { "baseColor", Float4ToJson(material.baseColor) },
            { "metallic", material.metallic },
            { "roughness", material.roughness },
            { "emissive", material.emissive },
            { "diffuseTexturePath", material.diffuseTexturePath },
            { "normalTexturePath", material.normalTexturePath },
            { "metallicRoughnessTexturePath", material.metallicRoughnessTexturePath },
            { "emissiveTexturePath", material.emissiveTexturePath },
            { "shaderId", material.shaderId },
            { "alphaMode", material.alphaMode },
            { "toonShadingMode", material.toonShadingMode },
            { "toonShadowTint", Float3ToJson(material.toonShadowTint) },
            { "toonShadowDeep", Float3ToJson(material.toonShadowDeep) },
            { "toonRimColor", Float3ToJson(material.toonRimColor) },
            { "toonOutlineEnabled", material.toonOutlineEnabled },
            { "toonOutlineColor", Float3ToJson(material.toonOutlineColor) }
        };
    }

    void ApplyMaterialFields(MaterialAsset& material, const json& fields)
    {
        if (!fields.is_object()) {
            throw MakeError("invalid_param", "fields must be an object.");
        }
        if (fields.contains("baseColor")) ReadFloat4(fields["baseColor"], material.baseColor);
        if (fields.contains("metallic")) material.metallic = fields["metallic"].get<float>();
        if (fields.contains("roughness")) material.roughness = fields["roughness"].get<float>();
        if (fields.contains("emissive")) material.emissive = fields["emissive"].get<float>();
        if (fields.contains("diffuseTexturePath")) material.diffuseTexturePath = fields["diffuseTexturePath"].get<std::string>();
        if (fields.contains("normalTexturePath")) material.normalTexturePath = fields["normalTexturePath"].get<std::string>();
        if (fields.contains("metallicRoughnessTexturePath")) material.metallicRoughnessTexturePath = fields["metallicRoughnessTexturePath"].get<std::string>();
        if (fields.contains("emissiveTexturePath")) material.emissiveTexturePath = fields["emissiveTexturePath"].get<std::string>();
        if (fields.contains("shaderId")) material.shaderId = fields["shaderId"].get<int>();
        if (fields.contains("alphaMode")) material.alphaMode = fields["alphaMode"].get<int>();
        if (fields.contains("toonShadingMode")) material.toonShadingMode = fields["toonShadingMode"].get<int>();
        if (fields.contains("toonShadowTint")) ReadFloat3(fields["toonShadowTint"], material.toonShadowTint);
        if (fields.contains("toonShadowDeep")) ReadFloat3(fields["toonShadowDeep"], material.toonShadowDeep);
        if (fields.contains("toonRimColor")) ReadFloat3(fields["toonRimColor"], material.toonRimColor);
        if (fields.contains("toonOutlineEnabled")) material.toonOutlineEnabled = fields["toonOutlineEnabled"].get<bool>();
        if (fields.contains("toonOutlineColor")) ReadFloat3(fields["toonOutlineColor"], material.toonOutlineColor);
    }

    json HandleMaterialCreate(const json& params)
    {
        const std::filesystem::path path = ResolveProjectPath(params.value("path", std::string{}), PathAccess::WriteAsset, false);
        if (path.extension() != ".material" && path.extension() != ".mat") {
            throw MakeError("invalid_param", "Material path must use .material or .mat extension.", { { "path", path.string() } });
        }
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        MaterialAsset material(path.string());
        if (params.contains("fields")) {
            ApplyMaterialFields(material, params["fields"]);
        }
        material.Save();
        return { { "path", ToGenericProjectPath(path) }, { "material", MaterialToJson(material) } };
    }

    json HandleMaterialGet(const json& params)
    {
        const std::filesystem::path path = ResolveProjectPath(params.value("path", std::string{}), PathAccess::ReadAsset, true);
        MaterialAsset material(path.string());
        return { { "path", ToGenericProjectPath(path) }, { "material", MaterialToJson(material) } };
    }

    json HandleMaterialSet(const json& params)
    {
        const std::filesystem::path path = ResolveProjectPath(params.value("path", std::string{}), PathAccess::WriteAsset, true);
        MaterialAsset material(path.string());
        ApplyMaterialFields(material, params.value("fields", json::object()));
        material.Save();
        return { { "path", ToGenericProjectPath(path) }, { "material", MaterialToJson(material) } };
    }

    json HandleMaterialAssign(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "Entity is not alive.", { { "entity", params.value("entity", json(nullptr)) } });
        }
        const std::string pathText = params.value("path", std::string{});
        const std::filesystem::path path = ResolveProjectPath(pathText, PathAccess::ReadAsset, true);
        MaterialComponent before{};
        if (auto* existing = registry.GetComponent<MaterialComponent>(entity)) {
            before = *existing;
        }
        MaterialComponent after = before;
        after.materialAssetPath = ToGenericProjectPath(path);
        after.materialAsset = ResourceManager::Instance().GetMaterial(after.materialAssetPath);
        if (params.value("recordUndo", true)) {
            auto action = std::make_unique<OptionalComponentUndoAction<MaterialComponent>>(
                entity,
                registry.GetComponent<MaterialComponent>(entity) ? std::optional<MaterialComponent>(before) : std::nullopt,
                after,
                "AI Assign Material");
            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
        }
        else {
            registry.AddComponent<MaterialComponent>(entity, after);
        }
        MarkEntityEdited(registry, entity);
        return { { "entity", EntityToString(entity) }, { "path", after.materialAssetPath } };
    }

    LightType LightTypeFromString(const std::string& value)
    {
        const std::string lower = ToLowerCopy(value);
        if (lower == "directional") return LightType::Directional;
        if (lower == "spot") return LightType::Spot;
        return LightType::Point;
    }

    std::string LightTypeToStringValue(LightType type)
    {
        switch (type) {
        case LightType::Directional: return "Directional";
        case LightType::Spot: return "Spot";
        default: return "Point";
        }
    }

    json HandleLightCreate(Registry& registry, const json& params)
    {
        json createParams = {
            { "name", params.value("name", std::string("Light")) },
            { "position", params.value("position", json::array({ 0.0f, 3.0f, 0.0f })) },
            { "select", params.value("select", true) },
            { "recordUndo", params.value("recordUndo", true) }
        };
        json result = HandleCreateEmpty(registry, createParams);
        const EntityID entity = EntityFromJson(result["entity"]);
        LightComponent light;
        light.type = LightTypeFromString(params.value("type", std::string("Point")));
        if (params.contains("color")) ReadFloat3(params["color"], light.color);
        light.intensity = params.value("intensity", light.intensity);
        light.range = params.value("range", light.range);
        light.castShadow = params.value("castShadow", light.castShadow);
        registry.AddComponent<LightComponent>(entity, light);
        MarkEntityEdited(registry, entity);
        result["light"] = {
            { "type", LightTypeToStringValue(light.type) },
            { "color", Float3ToJson(light.color) },
            { "intensity", light.intensity },
            { "range", light.range },
            { "castShadow", light.castShadow }
        };
        return result;
    }

    json HandleCameraCreate(Registry& registry, const json& params)
    {
        json createParams = {
            { "name", params.value("name", std::string("Camera")) },
            { "position", params.value("position", json::array({ 0.0f, 4.0f, -8.0f })) },
            { "select", params.value("select", true) },
            { "recordUndo", params.value("recordUndo", true) }
        };
        json result = HandleCreateEmpty(registry, createParams);
        const EntityID entity = EntityFromJson(result["entity"]);
        CameraLensComponent lens;
        lens.fovY = params.value("fovY", lens.fovY);
        lens.nearZ = params.value("nearZ", lens.nearZ);
        lens.farZ = params.value("farZ", lens.farZ);
        lens.aspect = params.value("aspect", lens.aspect);
        registry.AddComponent<CameraLensComponent>(entity, lens);
        if (params.value("main", false)) {
            registry.AddComponent<CameraMainTagComponent>(entity, CameraMainTagComponent{});
        }
        MarkEntityEdited(registry, entity);
        result["camera"] = { { "fovY", lens.fovY }, { "nearZ", lens.nearZ }, { "farZ", lens.farZ }, { "aspect", lens.aspect } };
        return result;
    }

    // ---- light.get / light.set / light.list ----

    json LightComponentToJson(const LightComponent& light)
    {
        return {
            { "type",       LightTypeToStringValue(light.type) },
            { "color",      Float3ToJson(light.color) },
            { "intensity",  light.intensity },
            { "range",      light.range },
            { "castShadow", light.castShadow }
        };
    }

    EntityID FindFirstEntityWithComponent(Registry& registry, ComponentTypeID typeId)
    {
        for (Archetype* arch : registry.GetAllArchetypes()) {
            if (!arch->GetSignature().test(typeId)) { continue; }
            for (EntityID e : arch->GetEntities()) {
                if (registry.IsAlive(e)) { return e; }
            }
        }
        return Entity::NULL_ID;
    }

    json HandleLightGet(Registry& registry, const json& params)
    {
        EntityID entity = Entity::NULL_ID;
        if (params.contains("entity")) {
            entity = EntityFromJson(params["entity"]);
            if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
                throw MakeError("entity_not_found", "Entity not found.", { { "entity", params["entity"] } });
            }
        }
        else {
            entity = FindFirstEntityWithComponent(registry, TypeManager::GetComponentTypeID<LightComponent>());
            if (Entity::IsNull(entity)) {
                throw MakeError("entity_not_found", "No entity with LightComponent found.");
            }
        }
        const auto* light = registry.GetComponent<LightComponent>(entity);
        if (!light) {
            throw MakeError("component_missing", "Entity has no LightComponent.", { { "entity", EntityToString(entity) } });
        }
        return { { "entity", EntityToString(entity) }, { "light", LightComponentToJson(*light) } };
    }

    json HandleLightSet(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "entity is required and must be a valid entity ID.");
        }
        auto* light = registry.GetComponent<LightComponent>(entity);
        if (!light) {
            throw MakeError("component_missing", "Entity has no LightComponent.", { { "entity", EntityToString(entity) } });
        }
        if (params.contains("type"))      { light->type      = LightTypeFromString(params["type"].get<std::string>()); }
        if (params.contains("color"))     { ReadFloat3(params["color"], light->color); }
        if (params.contains("intensity")) { light->intensity  = params["intensity"].get<float>(); }
        if (params.contains("range"))     { light->range      = params["range"].get<float>(); }
        if (params.contains("castShadow")){ light->castShadow = params["castShadow"].get<bool>(); }
        MarkEntityEdited(registry, entity);
        return { { "entity", EntityToString(entity) }, { "light", LightComponentToJson(*light) } };
    }

    json HandleLightList(Registry& registry)
    {
        json lights = json::array();
        const auto typeId = TypeManager::GetComponentTypeID<LightComponent>();
        for (Archetype* arch : registry.GetAllArchetypes()) {
            if (!arch->GetSignature().test(typeId)) { continue; }
            auto* col = arch->GetColumn(typeId);
            const auto& entities = arch->GetEntities();
            for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                if (!registry.IsAlive(entities[i])) { continue; }
                json entry = EntitySummary(registry, entities[i], arch->GetSignature());
                entry["light"] = LightComponentToJson(*static_cast<const LightComponent*>(col->Get(i)));
                lights.push_back(std::move(entry));
            }
        }
        return { { "lights", std::move(lights) } };
    }

    // ---- camera.get / camera.set / camera.list ----

    json CameraLensToJson(const CameraLensComponent& lens)
    {
        return {
            { "fovY",   lens.fovY },
            { "nearZ",  lens.nearZ },
            { "farZ",   lens.farZ },
            { "aspect", lens.aspect }
        };
    }

    json HandleCameraGet(Registry& registry, const json& params)
    {
        EntityID entity = Entity::NULL_ID;
        if (params.contains("entity")) {
            entity = EntityFromJson(params["entity"]);
            if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
                throw MakeError("entity_not_found", "Entity not found.", { { "entity", params["entity"] } });
            }
        }
        else {
            entity = FindFirstEntityWithComponent(registry, TypeManager::GetComponentTypeID<CameraLensComponent>());
            if (Entity::IsNull(entity)) {
                throw MakeError("entity_not_found", "No entity with CameraLensComponent found.");
            }
        }
        const auto* lens = registry.GetComponent<CameraLensComponent>(entity);
        if (!lens) {
            throw MakeError("component_missing", "Entity has no CameraLensComponent.", { { "entity", EntityToString(entity) } });
        }
        return {
            { "entity", EntityToString(entity) },
            { "isMain", registry.GetComponent<CameraMainTagComponent>(entity) != nullptr },
            { "camera", CameraLensToJson(*lens) }
        };
    }

    json HandleCameraSet(Registry& registry, const json& params)
    {
        const EntityID entity = EntityFromJson(params.value("entity", json(nullptr)));
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            throw MakeError("entity_not_found", "entity is required and must be a valid entity ID.");
        }
        auto* lens = registry.GetComponent<CameraLensComponent>(entity);
        if (!lens) {
            throw MakeError("component_missing", "Entity has no CameraLensComponent.", { { "entity", EntityToString(entity) } });
        }
        if (params.contains("fovY"))   { lens->fovY   = params["fovY"].get<float>(); }
        if (params.contains("nearZ"))  { lens->nearZ  = params["nearZ"].get<float>(); }
        if (params.contains("farZ"))   { lens->farZ   = params["farZ"].get<float>(); }
        if (params.contains("aspect")) { lens->aspect = params["aspect"].get<float>(); }
        if (params.contains("main")) {
            const bool wantMain = params["main"].get<bool>();
            const bool isMain   = registry.GetComponent<CameraMainTagComponent>(entity) != nullptr;
            if (wantMain && !isMain) {
                registry.AddComponent<CameraMainTagComponent>(entity, CameraMainTagComponent{});
            }
            else if (!wantMain && isMain) {
                registry.RemoveComponent<CameraMainTagComponent>(entity);
            }
        }
        MarkEntityEdited(registry, entity);
        return {
            { "entity", EntityToString(entity) },
            { "isMain", registry.GetComponent<CameraMainTagComponent>(entity) != nullptr },
            { "camera", CameraLensToJson(*lens) }
        };
    }

    json HandleCameraList(Registry& registry)
    {
        json cameras = json::array();
        const auto typeId = TypeManager::GetComponentTypeID<CameraLensComponent>();
        for (Archetype* arch : registry.GetAllArchetypes()) {
            if (!arch->GetSignature().test(typeId)) { continue; }
            auto* col = arch->GetColumn(typeId);
            const auto& entities = arch->GetEntities();
            for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                if (!registry.IsAlive(entities[i])) { continue; }
                json entry = EntitySummary(registry, entities[i], arch->GetSignature());
                entry["isMain"] = registry.GetComponent<CameraMainTagComponent>(entities[i]) != nullptr;
                entry["camera"] = CameraLensToJson(*static_cast<const CameraLensComponent*>(col->Get(i)));
                cameras.push_back(std::move(entry));
            }
        }
        return { { "cameras", std::move(cameras) } };
    }

    EntityID FindTerrainEntity(Registry& registry, const json& params)
    {
        if (params.contains("entity")) {
            const EntityID entity = EntityFromJson(params["entity"]);
            if (Entity::IsNull(entity) || !registry.IsAlive(entity) || !registry.GetComponent<TerrainComponent>(entity)) {
                throw MakeError("entity_not_found", "Terrain entity is not alive or has no TerrainComponent.", { { "entity", params["entity"] } });
            }
            return entity;
        }

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<TerrainComponent>())) {
                continue;
            }
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                if (registry.IsAlive(entity)) {
                    return entity;
                }
            }
        }
        throw MakeError("entity_not_found", "No Terrain entity was found.");
    }

    json TerrainSummary(Registry& registry, EntityID entity)
    {
        auto* terrain = registry.GetComponent<TerrainComponent>(entity);
        if (!terrain || !terrain->asset) {
            return nullptr;
        }
        const TerrainAsset& asset = *terrain->asset;
        return {
            { "entity", EntityToString(entity) },
            { "resolution", asset.resolution },
            { "worldSize", json::array({ asset.worldSizeX, asset.worldSizeZ }) },
            { "heightScale", asset.heightScale },
            { "chunkCount", json::array({ asset.chunkCountX, asset.chunkCountZ }) },
            { "layerCount", asset.layers.size() },
            { "needsRebuild", terrain->needsRebuild },
            { "needsSplatUpload", terrain->needsSplatUpload }
        };
    }

    json HandleTerrainCreate(Registry& registry, const json& params)
    {
        EntityID entity = registry.CreateEntity();
        registry.AddComponent(entity, NameComponent{ params.value("name", std::string("Terrain")) });
        TransformComponent transform{};
        if (params.contains("position")) ReadFloat3(params["position"], transform.localPosition);
        transform.isDirty = true;
        registry.AddComponent(entity, transform);
        registry.AddComponent(entity, HierarchyComponent{});

        TerrainComponent terrain;
        terrain.asset = std::make_shared<TerrainAsset>();
        TerrainAsset& asset = *terrain.asset;
        asset.resolution = params.value("resolution", asset.resolution);
        asset.worldSizeX = params.value("worldSizeX", asset.worldSizeX);
        asset.worldSizeZ = params.value("worldSizeZ", asset.worldSizeZ);
        asset.heightScale = params.value("heightScale", asset.heightScale);
        asset.chunkCountX = params.value("chunkCountX", asset.chunkCountX);
        asset.chunkCountZ = params.value("chunkCountZ", asset.chunkCountZ);
        asset.EnsureDefaultLayers();
        if (params.value("generateNoise", true)) {
            TerrainGpuPipeline::Instance().Run(asset, TerrainGpuPipeline::StageNoise | TerrainGpuPipeline::StageAutoSplat);
        }
        else {
            asset.Reset(params.value("height", 0.0f));
        }
        terrain.needsRebuild = true;
        registry.AddComponent(entity, terrain);

        if (params.value("addGrass", true)) {
            GrassComponent grass;
            grass.enabled = true;
            grass.needsRebuild = true;
            grass.EnsureDefaultLayers();
            registry.AddComponent(entity, grass);
        }
        if (params.value("select", true)) {
            EditorSelection::Instance().SelectEntity(entity);
        }
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainList(Registry& registry)
    {
        json terrains = json::array();
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<TerrainComponent>())) {
                continue;
            }
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                if (registry.IsAlive(entity)) {
                    terrains.push_back(TerrainSummary(registry, entity));
                }
            }
        }
        return { { "terrains", std::move(terrains) } };
    }

    TerrainBrush::Mode TerrainBrushModeFromString(const std::string& value)
    {
        const std::string lower = ToLowerCopy(value);
        if (lower == "lower") return TerrainBrush::Mode::Lower;
        if (lower == "smooth") return TerrainBrush::Mode::Smooth;
        if (lower == "flatten" || lower == "flat") return TerrainBrush::Mode::Flatten;
        if (lower == "paint") return TerrainBrush::Mode::Paint;
        return TerrainBrush::Mode::Raise;
    }

    void ApplyTerrainBrushToAsset(TerrainAsset& asset, TerrainComponent& terrain, const TerrainBrush& brush, float worldHitX, float worldHitZ)
    {
        if (asset.heightData.empty()) return;
        const float halfW = asset.worldSizeX * 0.5f;
        const float halfD = asset.worldSizeZ * 0.5f;
        const float normX = (worldHitX + halfW) / asset.worldSizeX;
        const float normZ = (worldHitZ + halfD) / asset.worldSizeZ;
        const int px = static_cast<int>(normX * (asset.resolution - 1));
        const int pz = static_cast<int>(normZ * (asset.resolution - 1));
        const float cellSize = (asset.worldSizeX + asset.worldSizeZ) * 0.5f / static_cast<float>((std::max)(asset.resolution - 1u, 1u));
        const int radiusPx = static_cast<int>(brush.radius / (std::max)(cellSize, 0.001f)) + 1;
        const size_t expectedSplatSize = static_cast<size_t>(asset.resolution) * static_cast<size_t>(asset.resolution) * 4u;
        if (brush.mode == TerrainBrush::Mode::Paint && asset.splatData.size() != expectedSplatSize) {
            asset.splatData.assign(expectedSplatSize, 0);
            for (uint32_t i = 0; i < asset.resolution * asset.resolution; ++i) asset.splatData[static_cast<size_t>(i) * 4u] = 255;
        }
        for (int dz = -radiusPx; dz <= radiusPx; ++dz) {
            for (int dx = -radiusPx; dx <= radiusPx; ++dx) {
                int ix = px + dx, iz = pz + dz;
                if (ix < 0 || iz < 0 || ix >= static_cast<int>(asset.resolution) || iz >= static_cast<int>(asset.resolution)) continue;
                float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz)) / static_cast<float>(radiusPx);
                if (dist > 1.0f) continue;
                float w = (1.0f - dist) * brush.strength;
                if (brush.falloff > 0.0f) w *= std::pow(1.0f - dist, brush.falloff);
                float& h = asset.heightData[static_cast<size_t>(iz) * asset.resolution + ix];
                switch (brush.mode) {
                case TerrainBrush::Mode::Raise: h = (std::min)(1.0f, h + w * 0.01f); break;
                case TerrainBrush::Mode::Lower: h = (std::max)(0.0f, h - w * 0.01f); break;
                case TerrainBrush::Mode::Flatten: h += (brush.targetHeight - h) * w; break;
                case TerrainBrush::Mode::Smooth: {
                    float sum = 0.0f; int count = 0;
                    for (int sz = -1; sz <= 1; ++sz) for (int sx = -1; sx <= 1; ++sx) {
                        const int nx = ClampInt(ix + sx, 0, static_cast<int>(asset.resolution) - 1);
                        const int nz = ClampInt(iz + sz, 0, static_cast<int>(asset.resolution) - 1);
                        sum += asset.heightData[static_cast<size_t>(nz) * asset.resolution + nx]; ++count;
                    }
                    h += ((count > 0 ? sum / static_cast<float>(count) : h) - h) * w;
                    break;
                }
                case TerrainBrush::Mode::Paint: {
                    const int layer = ClampInt(brush.layerIndex, 0, 2);
                    const size_t p = (static_cast<size_t>(iz) * asset.resolution + ix) * 4u;
                    float weights[3] = { asset.splatData[p] / 255.0f, asset.splatData[p + 1] / 255.0f, asset.splatData[p + 2] / 255.0f };
                    weights[layer] = (std::min)(1.0f, weights[layer] + w * 0.08f);
                    const float fade = 1.0f - w * 0.08f;
                    for (int i = 0; i < 3; ++i) if (i != layer) weights[i] *= fade;
                    const float sum = (std::max)(weights[0] + weights[1] + weights[2], 0.0001f);
                    asset.splatData[p] = static_cast<uint8_t>(ClampInt(static_cast<int>(weights[0] / sum * 255.0f), 0, 255));
                    asset.splatData[p + 1] = static_cast<uint8_t>(ClampInt(static_cast<int>(weights[1] / sum * 255.0f), 0, 255));
                    asset.splatData[p + 2] = static_cast<uint8_t>(ClampInt(static_cast<int>(weights[2] / sum * 255.0f), 0, 255));
                    asset.splatData[p + 3] = 0;
                    break;
                }}
            }
        }
        terrain.needsRebuild = brush.mode != TerrainBrush::Mode::Paint;
        terrain.needsSplatUpload = brush.mode == TerrainBrush::Mode::Paint;
    }

    json HandleTerrainApplyBrush(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* terrain = registry.GetComponent<TerrainComponent>(entity);
        TerrainBrush brush;
        brush.mode = TerrainBrushModeFromString(params.value("mode", std::string("raise")));
        brush.radius = params.value("radius", brush.radius);
        brush.strength = params.value("strength", brush.strength);
        brush.falloff = params.value("falloff", brush.falloff);
        brush.layerIndex = params.value("layerIndex", brush.layerIndex);
        brush.targetHeight = params.value("targetHeight", brush.targetHeight);
        DirectX::XMFLOAT3 position{};
        if (!params.contains("position") || !ReadFloat3(params["position"], position)) {
            throw MakeError("missing_param", "position [x,y,z] is required.");
        }
        ApplyTerrainBrushToAsset(*terrain->asset, *terrain, brush, position.x, position.z);
        MarkEntityEdited(registry, entity);
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainSaveLoad(Registry& registry, const json& params, bool save)
    {
        if (save) {
            const EntityID entity = FindTerrainEntity(registry, params);
            auto* terrain = registry.GetComponent<TerrainComponent>(entity);
            const std::filesystem::path path = ResolveProjectPath(params.value("path", std::string{}), PathAccess::WriteAsset, false);
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (!TerrainAssetIO::SaveToFile(*terrain->asset, path)) {
                throw MakeError("terrain_save_failed", "Failed to save terrain asset.", { { "path", path.string() } });
            }
            return { { "path", ToGenericProjectPath(path) }, { "terrain", TerrainSummary(registry, entity) } };
        }
        const std::filesystem::path path = ResolveProjectPath(params.value("path", std::string{}), PathAccess::ReadAsset, true);
        auto asset = std::make_shared<TerrainAsset>();
        if (!TerrainAssetIO::LoadFromFile(*asset, path)) {
            throw MakeError("terrain_load_failed", "Failed to load terrain asset.", { { "path", path.string() } });
        }
        EntityID entity = registry.CreateEntity();
        registry.AddComponent(entity, NameComponent{ params.value("name", path.stem().string()) });
        registry.AddComponent(entity, TransformComponent{});
        registry.AddComponent(entity, HierarchyComponent{});
        TerrainComponent terrain;
        terrain.asset = asset;
        terrain.needsRebuild = true;
        registry.AddComponent(entity, terrain);
        if (params.value("select", true)) EditorSelection::Instance().SelectEntity(entity);
        return { { "path", ToGenericProjectPath(path) }, { "terrain", TerrainSummary(registry, entity) } };
    }

    // ---- Terrain helpers ----

    json TerrainLayerToJson(const TerrainLayer& l)
    {
        return {
            { "albedoPath",     l.albedoPath },
            { "normalPath",     l.normalPath },
            { "roughnessPath",  l.roughnessPath },
            { "tileScale",      l.tileScale },
            { "blendSharpness", l.blendSharpness }
        };
    }

    json FoliageLayerToJson(const FoliageLayer& l)
    {
        return {
            { "enabled",           l.enabled },
            { "name",              l.name },
            { "meshPath",          l.meshPath },
            { "densityMultiplier", l.densityMultiplier },
            { "maxPerCell",        static_cast<int>(l.maxPerCell) },
            { "densityThreshold",  l.densityThreshold },
            { "splatChannel",      l.splatChannel },
            { "minAltitudeNorm",   l.minAltitudeNorm },
            { "maxAltitudeNorm",   l.maxAltitudeNorm },
            { "maxSlopeDegrees",   l.maxSlopeDegrees },
            { "sizeScale",         l.sizeScale },
            { "sizeVariance",      l.sizeVariance },
            { "colorBottom",       json::array({ l.colorBottom.x, l.colorBottom.y, l.colorBottom.z }) },
            { "colorTop",          json::array({ l.colorTop.x, l.colorTop.y, l.colorTop.z }) },
            { "useWind",           l.useWind },
            { "windStrength",      l.windStrength },
            { "windSpeed",         l.windSpeed },
            { "seed",              l.seed },
            { "lastInstanceCount", static_cast<int>(l.lastInstanceCount) }
        };
    }

    json TerrainDetailJson(Registry& registry, EntityID entity)
    {
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        if (!tc || !tc->asset) return nullptr;
        const TerrainAsset& a = *tc->asset;

        std::string entityName;
        if (auto* nc = registry.GetComponent<NameComponent>(entity)) entityName = nc->name;

        json layers = json::array();
        for (const auto& l : a.layers) layers.push_back(TerrainLayerToJson(l));

        return {
            { "entity",      EntityToString(entity) },
            { "name",        entityName },
            // Dimensions
            { "resolution",  a.resolution },
            { "worldSizeX",  a.worldSizeX },
            { "worldSizeZ",  a.worldSizeZ },
            { "heightScale", a.heightScale },
            { "chunkCountX", a.chunkCountX },
            { "chunkCountZ", a.chunkCountZ },
            // Noise
            { "noiseType",           a.noiseType },
            { "noiseFreq",           a.noiseFreq },
            { "octaves",             a.octaves },
            { "lacunarity",          a.lacunarity },
            { "gain",                a.gain },
            { "seed",                a.seed },
            { "domainWarpStrength",  a.domainWarpStrength },
            { "terraceSteps",        a.terraceSteps },
            // AutoSplat
            { "autoSplat", {
                { "rockAltitudeMin",  a.autoSplat.rockAltitudeMin },
                { "rockSlopeDegrees", a.autoSplat.rockSlopeDegrees },
                { "dirtMidAltitude",  a.autoSplat.dirtMidAltitude },
                { "dirtStrength",     a.autoSplat.dirtStrength }
            }},
            // Water
            { "water", {
                { "enabled",      a.water.enabled },
                { "seaLevel",     a.water.seaLevel },
                { "shallowColor", json::array({ a.water.shallowColor.x, a.water.shallowColor.y, a.water.shallowColor.z, a.water.shallowColor.w }) },
                { "deepColor",    json::array({ a.water.deepColor.x, a.water.deepColor.y, a.water.deepColor.z, a.water.deepColor.w }) },
                { "depthFade",    a.water.depthFade },
                { "waveSpeed",    a.water.waveSpeed },
                { "waveScale",    a.water.waveScale }
            }},
            // Erosion
            { "erosion", {
                { "iterations",             a.erosion.iterations },
                { "erosionRadius",          a.erosion.erosionRadius },
                { "maxDropletLifetime",     a.erosion.maxDropletLifetime },
                { "inertia",                a.erosion.inertia },
                { "sedimentCapacityFactor", a.erosion.sedimentCapacityFactor },
                { "minSedimentCapacity",    a.erosion.minSedimentCapacity },
                { "erodeSpeed",             a.erosion.erodeSpeed },
                { "depositSpeed",           a.erosion.depositSpeed },
                { "evaporateSpeed",         a.erosion.evaporateSpeed },
                { "gravity",                a.erosion.gravity },
                { "seed",                   a.erosion.seed }
            }},
            // PBR layers
            { "layers", std::move(layers) },
            // Status
            { "needsRebuild",     tc->needsRebuild },
            { "needsSplatUpload", tc->needsSplatUpload }
        };
    }

    // ---- Terrain open / brush ----

    json HandleTerrainOpen(EngineKernel& kernel, const json& params)
    {
        EditorLayer* editor = kernel.GetEditorLayer();
        if (!editor) throw MakeError("editor_unavailable", "EditorLayer is not available.");
        EntityID entity = Entity::NULL_ID;
        if (params.contains("entity")) {
            Registry* reg = kernel.GetGameRegistry();
            if (reg) {
                entity = EntityFromJson(params["entity"]);
                if (!Entity::IsNull(entity) && (!reg->IsAlive(entity) || !reg->GetComponent<TerrainComponent>(entity)))
                    entity = Entity::NULL_ID;
            }
        }
        editor->OpenTerrainEditorFromAutomation(entity);
        return { { "open", true } };
    }

    json HandleTerrainSetBrush(EngineKernel& kernel, const json& params)
    {
        EditorLayer* editor = kernel.GetEditorLayer();
        if (!editor) throw MakeError("editor_unavailable", "EditorLayer is not available.");
        TerrainBrush& brush = editor->GetTerrainEditorPanel().GetBrushMutable();
        bool activated = false;
        if (params.contains("mode"))         { brush.mode        = TerrainBrushModeFromString(params["mode"].get<std::string>()); activated = true; }
        if (params.contains("radius"))       { brush.radius      = params["radius"].get<float>();       activated = true; }
        if (params.contains("strength"))     { brush.strength    = params["strength"].get<float>();     activated = true; }
        if (params.contains("falloff"))      { brush.falloff     = params["falloff"].get<float>(); }
        if (params.contains("layerIndex"))   { brush.layerIndex  = params["layerIndex"].get<int>(); }
        if (params.contains("targetHeight")) { brush.targetHeight= params["targetHeight"].get<float>(); }
        if (params.value("enable", activated))
            editor->GetTerrainEditorPanel().SetSceneBrushEnabled(true);
        if (params.value("disable", false))
            editor->GetTerrainEditorPanel().SetSceneBrushEnabled(false);

        const char* modeNames[] = { "raise", "lower", "smooth", "flatten", "paint" };
        const int   modeIdx     = static_cast<int>(brush.mode);
        return { { "brush", {
            { "mode",        modeNames[ClampInt(modeIdx, 0, 4)] },
            { "radius",      brush.radius },
            { "strength",    brush.strength },
            { "falloff",     brush.falloff },
            { "layerIndex",  brush.layerIndex },
            { "targetHeight",brush.targetHeight },
            { "enabled",     editor->GetTerrainEditorPanel().IsSceneBrushEnabled() }
        }}};
    }

    // ---- Terrain asset operations ----

    json HandleTerrainGet(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        return { { "terrain", TerrainDetailJson(registry, entity) } };
    }

    json HandleTerrainSetDimensions(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        TerrainAsset& a = *tc->asset;
        bool changed = false;
        if (params.contains("worldSizeX"))  { a.worldSizeX  = params["worldSizeX"].get<float>();  changed = true; }
        if (params.contains("worldSizeZ"))  { a.worldSizeZ  = params["worldSizeZ"].get<float>();  changed = true; }
        if (params.contains("heightScale")) { a.heightScale = params["heightScale"].get<float>(); changed = true; }
        if (params.contains("resolution"))  { a.resolution  = static_cast<uint32_t>(std::clamp(params["resolution"].get<int>(), 64, 1024)); changed = true; }
        if (params.contains("chunkCountX")) { a.chunkCountX = static_cast<uint32_t>(params["chunkCountX"].get<int>()); changed = true; }
        if (params.contains("chunkCountZ")) { a.chunkCountZ = static_cast<uint32_t>(params["chunkCountZ"].get<int>()); changed = true; }
        if (changed) { tc->needsRebuild = true; MarkEntityEdited(registry, entity); }
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainSetNoise(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        TerrainAsset& a = *tc->asset;
        if (params.contains("noiseType"))          a.noiseType          = std::clamp(params["noiseType"].get<int>(), 0, 2);
        if (params.contains("noiseFreq"))          a.noiseFreq          = params["noiseFreq"].get<float>();
        if (params.contains("octaves"))            a.octaves            = std::clamp(params["octaves"].get<int>(), 1, 8);
        if (params.contains("lacunarity"))         a.lacunarity         = params["lacunarity"].get<float>();
        if (params.contains("gain"))               a.gain               = params["gain"].get<float>();
        if (params.contains("seed"))               a.seed               = params["seed"].get<int>();
        if (params.contains("domainWarpStrength")) a.domainWarpStrength = params["domainWarpStrength"].get<float>();
        if (params.contains("terraceSteps"))       a.terraceSteps       = params["terraceSteps"].get<float>();
        MarkEntityEdited(registry, entity);
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainSetErosion(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        auto& ep = tc->asset->erosion;
        if (params.contains("iterations"))             ep.iterations             = params["iterations"].get<int>();
        if (params.contains("erosionRadius"))          ep.erosionRadius          = params["erosionRadius"].get<int>();
        if (params.contains("maxDropletLifetime"))     ep.maxDropletLifetime     = params["maxDropletLifetime"].get<int>();
        if (params.contains("inertia"))                ep.inertia                = params["inertia"].get<float>();
        if (params.contains("sedimentCapacityFactor")) ep.sedimentCapacityFactor = params["sedimentCapacityFactor"].get<float>();
        if (params.contains("minSedimentCapacity"))    ep.minSedimentCapacity    = params["minSedimentCapacity"].get<float>();
        if (params.contains("erodeSpeed"))             ep.erodeSpeed             = params["erodeSpeed"].get<float>();
        if (params.contains("depositSpeed"))           ep.depositSpeed           = params["depositSpeed"].get<float>();
        if (params.contains("evaporateSpeed"))         ep.evaporateSpeed         = params["evaporateSpeed"].get<float>();
        if (params.contains("gravity"))                ep.gravity                = params["gravity"].get<float>();
        if (params.contains("seed"))                   ep.seed                   = params["seed"].get<int>();
        MarkEntityEdited(registry, entity);
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainRunErosion(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        tc->asset->RunHydraulicErosion(tc->asset->erosion);
        tc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainSetAutoSplat(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        TerrainAsset& a = *tc->asset;
        auto& as = a.autoSplat;
        if (params.contains("rockAltitudeMin"))  as.rockAltitudeMin  = params["rockAltitudeMin"].get<float>();
        if (params.contains("rockSlopeDegrees")) as.rockSlopeDegrees = params["rockSlopeDegrees"].get<float>();
        if (params.contains("dirtMidAltitude"))  as.dirtMidAltitude  = params["dirtMidAltitude"].get<float>();
        if (params.contains("dirtStrength"))     as.dirtStrength     = params["dirtStrength"].get<float>();
        if (params.value("regenerate", true)) {
            a.GenerateAutoSplat(as);
            tc->needsRebuild = true;
        }
        MarkEntityEdited(registry, entity);
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainSetWater(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        auto& w = tc->asset->water;
        if (params.contains("enabled"))   w.enabled   = params["enabled"].get<bool>();
        if (params.contains("seaLevel"))  w.seaLevel  = params["seaLevel"].get<float>();
        if (params.contains("depthFade")) w.depthFade = params["depthFade"].get<float>();
        if (params.contains("waveSpeed")) w.waveSpeed = params["waveSpeed"].get<float>();
        if (params.contains("waveScale")) w.waveScale = params["waveScale"].get<float>();
        if (params.contains("shallowColor") && params["shallowColor"].is_array() && params["shallowColor"].size() >= 4)
            w.shallowColor = { params["shallowColor"][0].get<float>(), params["shallowColor"][1].get<float>(), params["shallowColor"][2].get<float>(), params["shallowColor"][3].get<float>() };
        if (params.contains("deepColor") && params["deepColor"].is_array() && params["deepColor"].size() >= 4)
            w.deepColor = { params["deepColor"][0].get<float>(), params["deepColor"][1].get<float>(), params["deepColor"][2].get<float>(), params["deepColor"][3].get<float>() };
        tc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainFitWater(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        TerrainAsset& a = *tc->asset;
        a.water.enabled  = true;
        a.water.seaLevel = a.SuggestVisibleWaterLevel();
        a.GenerateAutoSplat(a.autoSplat);
        tc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "seaLevel", a.water.seaLevel }, { "terrain", TerrainSummary(registry, entity) } };
    }

    uint32_t TerrainStagesFromString(const std::string& value)
    {
        const std::string lower = ToLowerCopy(value);
        if (lower == "noise")                            return TerrainGpuPipeline::StageNoise;
        if (lower == "erosion" || lower == "erode")      return TerrainGpuPipeline::StageErode;
        if (lower == "splat"   || lower == "autosplat")  return TerrainGpuPipeline::StageAutoSplat;
        if (lower == "noise_splat")                      return TerrainGpuPipeline::StageNoise | TerrainGpuPipeline::StageAutoSplat;
        if (lower == "erosion_splat")                    return TerrainGpuPipeline::StageErode | TerrainGpuPipeline::StageAutoSplat;
        return TerrainGpuPipeline::StageAll;  // "all" or unknown
    }

    json HandleTerrainRegenerate(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        const uint32_t stages = TerrainStagesFromString(params.value("stages", std::string("all")));
        TerrainGpuPipeline::Instance().Run(*tc->asset, stages);
        tc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "terrain", TerrainSummary(registry, entity) } };
    }

    void ApplyTerrainPresetByName(TerrainAsset& a, const std::string& name)
    {
        const std::string lower = ToLowerCopy(name);
        if (lower == "flat_plains" || lower == "flatplains") {
            a.noiseType = 1; a.noiseFreq = 0.0025f; a.octaves = 3; a.lacunarity = 2.0f; a.gain = 0.42f;
            a.domainWarpStrength = 25.0f; a.terraceSteps = 0.0f; a.heightScale = 14.0f;
            a.autoSplat = { 0.92f, 60.0f, 0.55f, 0.40f }; a.water.enabled = false;
        } else if (lower == "rolling_hills" || lower == "rollinghills") {
            a.noiseType = 1; a.noiseFreq = 0.0035f; a.octaves = 4; a.lacunarity = 2.0f; a.gain = 0.5f;
            a.domainWarpStrength = 60.0f; a.terraceSteps = 0.0f; a.heightScale = 32.0f;
            a.autoSplat = { 0.78f, 38.0f, 0.5f, 0.45f };
        } else if (lower == "mountain_range" || lower == "mountainrange") {
            a.noiseType = 0; a.noiseFreq = 0.0045f; a.octaves = 5; a.lacunarity = 2.2f; a.gain = 0.5f;
            a.domainWarpStrength = 100.0f; a.terraceSteps = 0.0f; a.heightScale = 80.0f;
            a.autoSplat = { 0.55f, 28.0f, 0.42f, 0.40f }; a.erosion.iterations = 80000;
        } else if (lower == "plateau_mesa" || lower == "plateaumesa") {
            a.noiseType = 1; a.noiseFreq = 0.003f; a.octaves = 4; a.lacunarity = 2.0f; a.gain = 0.5f;
            a.domainWarpStrength = 50.0f; a.terraceSteps = 8.0f; a.heightScale = 55.0f;
            a.autoSplat = { 0.7f, 45.0f, 0.45f, 0.4f };
        } else if (lower == "lake_basin" || lower == "lakebasin") {
            a.noiseType = 1; a.noiseFreq = 0.0028f; a.octaves = 4; a.lacunarity = 2.0f; a.gain = 0.5f;
            a.domainWarpStrength = 70.0f; a.terraceSteps = 0.0f; a.heightScale = 30.0f;
            a.water.enabled = true; a.autoSplat = { 0.85f, 50.0f, 0.4f, 0.5f };
        } else if (lower == "rocky_wasteland" || lower == "rockywasteland") {
            a.noiseType = 2; a.noiseFreq = 0.006f; a.octaves = 4; a.lacunarity = 2.0f; a.gain = 0.5f;
            a.domainWarpStrength = 50.0f; a.terraceSteps = 0.0f; a.heightScale = 50.0f;
            a.autoSplat = { 0.35f, 22.0f, 0.35f, 0.35f }; a.water.enabled = false;
        } else if (lower == "archipelago") {
            a.noiseType = 1; a.noiseFreq = 0.0045f; a.octaves = 4; a.lacunarity = 2.0f; a.gain = 0.55f;
            a.domainWarpStrength = 90.0f; a.terraceSteps = 0.0f; a.heightScale = 35.0f;
            a.water.enabled = true; a.water.seaLevel = 2.0f; a.autoSplat = { 0.8f, 45.0f, 0.4f, 0.5f };
        } else if (lower == "volcanic_crater" || lower == "volcaniccrater") {
            a.noiseType = 0; a.noiseFreq = 0.005f; a.octaves = 5; a.lacunarity = 2.3f; a.gain = 0.55f;
            a.domainWarpStrength = 60.0f; a.terraceSteps = 0.0f; a.heightScale = 90.0f;
            a.autoSplat = { 0.45f, 25.0f, 0.4f, 0.5f }; a.erosion.iterations = 100000;
        }
        // unknown preset: no change
    }

    json HandleTerrainApplyPreset(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        const std::string presetName = params.value("preset", std::string("flat_plains"));
        ApplyTerrainPresetByName(*tc->asset, presetName);
        const uint32_t stages = TerrainStagesFromString(params.value("stages", std::string("all")));
        TerrainGpuPipeline::Instance().Run(*tc->asset, stages);
        tc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "preset", presetName }, { "terrain", TerrainSummary(registry, entity) } };
    }

    json HandleTerrainSetLayer(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* tc = registry.GetComponent<TerrainComponent>(entity);
        TerrainAsset& a = *tc->asset;
        a.EnsureDefaultLayers();
        const int idx = params.value("index", 0);
        if (idx < 0 || idx >= static_cast<int>(a.layers.size()))
            throw MakeError("invalid_index", "Layer index out of range.", { { "index", idx } });
        TerrainLayer& l = a.layers[idx];
        if (params.contains("albedoPath"))     l.albedoPath    = params["albedoPath"].get<std::string>();
        if (params.contains("normalPath"))     l.normalPath    = params["normalPath"].get<std::string>();
        if (params.contains("roughnessPath"))  l.roughnessPath = params["roughnessPath"].get<std::string>();
        if (params.contains("tileScale"))      l.tileScale     = params["tileScale"].get<float>();
        if (params.contains("blendSharpness")) l.blendSharpness= params["blendSharpness"].get<float>();
        tc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "index", idx }, { "layer", TerrainLayerToJson(l) } };
    }

    // ---- Terrain foliage ----

    json HandleTerrainGetFoliage(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* gc = registry.GetComponent<GrassComponent>(entity);
        if (!gc) return { { "foliage", nullptr } };
        json foliageLayers = json::array();
        for (const auto& l : gc->layers) foliageLayers.push_back(FoliageLayerToJson(l));
        return { { "foliage", {
            { "enabled",       gc->enabled },
            { "drawDistance",  gc->drawDistance },
            { "windDirection", json::array({ gc->windDirection.x, gc->windDirection.y, gc->windDirection.z }) },
            { "layers",        std::move(foliageLayers) }
        }}};
    }

    json HandleTerrainSetFoliage(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* gc = registry.GetComponent<GrassComponent>(entity);
        if (!gc) throw MakeError("no_foliage", "Terrain entity has no GrassComponent.", { { "entity", EntityToString(entity) } });
        if (params.contains("enabled"))      gc->enabled      = params["enabled"].get<bool>();
        if (params.contains("drawDistance")) gc->drawDistance = params["drawDistance"].get<float>();
        if (params.contains("windDirection") && params["windDirection"].is_array() && params["windDirection"].size() >= 3)
            gc->windDirection = { params["windDirection"][0].get<float>(), params["windDirection"][1].get<float>(), params["windDirection"][2].get<float>() };
        gc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "entity", EntityToString(entity) } };
    }

    json HandleTerrainAddFoliageLayer(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* gc = registry.GetComponent<GrassComponent>(entity);
        if (!gc) throw MakeError("no_foliage", "Terrain entity has no GrassComponent.", { { "entity", EntityToString(entity) } });
        FoliageLayer newLayer;
        if (params.contains("name"))     newLayer.name     = params["name"].get<std::string>();
        if (params.contains("meshPath")) newLayer.meshPath = params["meshPath"].get<std::string>();
        gc->layers.push_back(std::move(newLayer));
        gc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        const int idx = static_cast<int>(gc->layers.size()) - 1;
        return { { "index", idx }, { "layer", FoliageLayerToJson(gc->layers[idx]) } };
    }

    json HandleTerrainSetFoliageLayer(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* gc = registry.GetComponent<GrassComponent>(entity);
        if (!gc) throw MakeError("no_foliage", "Terrain entity has no GrassComponent.", { { "entity", EntityToString(entity) } });
        const int idx = params.value("index", 0);
        if (idx < 0 || idx >= static_cast<int>(gc->layers.size()))
            throw MakeError("invalid_index", "Foliage layer index out of range.", { { "index", idx } });
        FoliageLayer& l = gc->layers[idx];
        if (params.contains("enabled"))           l.enabled           = params["enabled"].get<bool>();
        if (params.contains("name"))              l.name              = params["name"].get<std::string>();
        if (params.contains("meshPath"))          l.meshPath          = params["meshPath"].get<std::string>();
        if (params.contains("densityMultiplier")) l.densityMultiplier = params["densityMultiplier"].get<float>();
        if (params.contains("maxPerCell"))        l.maxPerCell        = static_cast<uint32_t>(params["maxPerCell"].get<int>());
        if (params.contains("densityThreshold"))  l.densityThreshold  = params["densityThreshold"].get<float>();
        if (params.contains("splatChannel"))      l.splatChannel      = params["splatChannel"].get<int>();
        if (params.contains("minAltitudeNorm"))   l.minAltitudeNorm   = params["minAltitudeNorm"].get<float>();
        if (params.contains("maxAltitudeNorm"))   l.maxAltitudeNorm   = params["maxAltitudeNorm"].get<float>();
        if (params.contains("maxSlopeDegrees"))   l.maxSlopeDegrees   = params["maxSlopeDegrees"].get<float>();
        if (params.contains("sizeScale"))         l.sizeScale         = params["sizeScale"].get<float>();
        if (params.contains("sizeVariance"))      l.sizeVariance      = params["sizeVariance"].get<float>();
        if (params.contains("useWind"))           l.useWind           = params["useWind"].get<bool>();
        if (params.contains("windStrength"))      l.windStrength      = params["windStrength"].get<float>();
        if (params.contains("windSpeed"))         l.windSpeed         = params["windSpeed"].get<float>();
        if (params.contains("seed"))              l.seed              = params["seed"].get<int>();
        if (params.contains("colorBottom") && params["colorBottom"].is_array() && params["colorBottom"].size() >= 3)
            l.colorBottom = { params["colorBottom"][0].get<float>(), params["colorBottom"][1].get<float>(), params["colorBottom"][2].get<float>() };
        if (params.contains("colorTop") && params["colorTop"].is_array() && params["colorTop"].size() >= 3)
            l.colorTop = { params["colorTop"][0].get<float>(), params["colorTop"][1].get<float>(), params["colorTop"][2].get<float>() };
        gc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "index", idx }, { "layer", FoliageLayerToJson(l) } };
    }

    json HandleTerrainDeleteFoliageLayer(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* gc = registry.GetComponent<GrassComponent>(entity);
        if (!gc) throw MakeError("no_foliage", "Terrain entity has no GrassComponent.", { { "entity", EntityToString(entity) } });
        const int idx = params.value("index", 0);
        if (idx < 0 || idx >= static_cast<int>(gc->layers.size()))
            throw MakeError("invalid_index", "Foliage layer index out of range.", { { "index", idx } });
        gc->layers.erase(gc->layers.begin() + idx);
        gc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "layerCount", static_cast<int>(gc->layers.size()) } };
    }

    json HandleTerrainResetFoliage(Registry& registry, const json& params)
    {
        const EntityID entity = FindTerrainEntity(registry, params);
        auto* gc = registry.GetComponent<GrassComponent>(entity);
        if (!gc) throw MakeError("no_foliage", "Terrain entity has no GrassComponent.", { { "entity", EntityToString(entity) } });
        gc->layers.clear();
        gc->EnsureDefaultLayers();
        gc->needsRebuild = true;
        MarkEntityEdited(registry, entity);
        return { { "layerCount", static_cast<int>(gc->layers.size()) } };
    }

    std::string EffectNodeTypeToApiString(EffectGraphNodeType type)
    {
        switch (type) {
        case EffectGraphNodeType::Output: return "Output";
        case EffectGraphNodeType::Spawn: return "Spawn";
        case EffectGraphNodeType::Lifetime: return "Lifetime";
        case EffectGraphNodeType::MeshSource: return "MeshSource";
        case EffectGraphNodeType::MeshRenderer: return "MeshRenderer";
        case EffectGraphNodeType::ParticleEmitter: return "ParticleEmitter";
        case EffectGraphNodeType::SpriteRenderer: return "SpriteRenderer";
        case EffectGraphNodeType::Float: return "Float";
        case EffectGraphNodeType::Vec3: return "Vec3";
        case EffectGraphNodeType::Color: return "Color";
        default: return "Unknown";
        }
    }

    EffectGraphNodeType EffectNodeTypeFromString(const std::string& value)
    {
        const std::string lower = ToLowerCopy(value);
        if (lower == "output" || lower == "effectoutput" || lower == "effect output") return EffectGraphNodeType::Output;
        if (lower == "spawn") return EffectGraphNodeType::Spawn;
        if (lower == "lifetime") return EffectGraphNodeType::Lifetime;
        if (lower == "meshsource" || lower == "mesh source") return EffectGraphNodeType::MeshSource;
        if (lower == "meshrenderer" || lower == "mesh renderer") return EffectGraphNodeType::MeshRenderer;
        if (lower == "particleemitter" || lower == "particle emitter") return EffectGraphNodeType::ParticleEmitter;
        if (lower == "spriterenderer" || lower == "sprite renderer") return EffectGraphNodeType::SpriteRenderer;
        if (lower == "float") return EffectGraphNodeType::Float;
        if (lower == "vec3" || lower == "vector3") return EffectGraphNodeType::Vec3;
        if (lower == "color" || lower == "colour") return EffectGraphNodeType::Color;
        throw MakeError("invalid_param", "Unknown effect node type.", { { "type", value } });
    }

    std::string EffectValueTypeToStringValue(EffectValueType type)
    {
        switch (type) {
        case EffectValueType::Flow: return "Flow";
        case EffectValueType::Float: return "Float";
        case EffectValueType::Vec3: return "Vec3";
        case EffectValueType::Color: return "Color";
        case EffectValueType::Mesh: return "Mesh";
        case EffectValueType::Particle: return "Particle";
        default: return "Unknown";
        }
    }

    EffectValueType EffectValueTypeFromString(const std::string& value)
    {
        const std::string lower = ToLowerCopy(value);
        if (lower == "flow") return EffectValueType::Flow;
        if (lower == "float") return EffectValueType::Float;
        if (lower == "vec3" || lower == "vector3") return EffectValueType::Vec3;
        if (lower == "color" || lower == "colour") return EffectValueType::Color;
        if (lower == "mesh") return EffectValueType::Mesh;
        if (lower == "particle") return EffectValueType::Particle;
        throw MakeError("invalid_param", "Unknown effect value type.", { { "valueType", value } });
    }

    DirectX::XMFLOAT2 ReadFloat2OrDefault(const json& value, const DirectX::XMFLOAT2& fallback)
    {
        if (!value.is_array() || value.size() < 2) {
            return fallback;
        }
        return { value[0].get<float>(), value[1].get<float>() };
    }

    json EffectPinToJson(const EffectGraphPin& pin)
    {
        return {
            { "id", pin.id },
            { "nodeId", pin.nodeId },
            { "name", pin.name },
            { "kind", pin.kind == EffectPinKind::Input ? "Input" : "Output" },
            { "valueType", EffectValueTypeToStringValue(pin.valueType) }
        };
    }

    json EffectNodeToJson(const EffectGraphNode& node)
    {
        return {
            { "id", node.id },
            { "type", EffectNodeTypeToApiString(node.type) },
            { "title", node.title },
            { "position", Float2ToJson(node.position) },
            { "scalar", node.scalar },
            { "scalar2", node.scalar2 },
            { "vectorValue", Float4ToJson(node.vectorValue) },
            { "vectorValue2", Float4ToJson(node.vectorValue2) },
            { "vectorValue3", Float4ToJson(node.vectorValue3) },
            { "vectorValue4", Float4ToJson(node.vectorValue4) },
            { "vectorValue5", Float4ToJson(node.vectorValue5) },
            { "vectorValue6", Float4ToJson(node.vectorValue6) },
            { "vectorValue7", Float4ToJson(node.vectorValue7) },
            { "vectorValue8", Float4ToJson(node.vectorValue8) },
            { "vectorValue9", Float4ToJson(node.vectorValue9) },
            { "stringValue", node.stringValue },
            { "stringValue2", node.stringValue2 },
            { "stringValue3", node.stringValue3 },
            { "stringValue4", node.stringValue4 },
            { "stringValue5", node.stringValue5 },
            { "stringValue6", node.stringValue6 },
            { "intValue", node.intValue },
            { "intValue2", node.intValue2 },
            { "boolValue", node.boolValue }
        };
    }

    json EffectLinkToJson(const EffectGraphLink& link)
    {
        return {
            { "id", link.id },
            { "startPinId", link.startPinId },
            { "endPinId", link.endPinId }
        };
    }

    json EffectGraphSummaryToJson(const EffectGraphAsset& asset, const std::filesystem::path& path)
    {
        json nodes = json::array();
        for (const auto& node : asset.nodes) {
            nodes.push_back(EffectNodeToJson(node));
        }
        json pins = json::array();
        for (const auto& pin : asset.pins) {
            pins.push_back(EffectPinToJson(pin));
        }
        json links = json::array();
        for (const auto& link : asset.links) {
            links.push_back(EffectLinkToJson(link));
        }
        json parameters = json::array();
        for (const auto& parameter : asset.exposedParameters) {
            parameters.push_back({
                { "name", parameter.name },
                { "valueType", EffectValueTypeToStringValue(parameter.valueType) },
                { "defaultValue", Float4ToJson(parameter.defaultValue) }
            });
        }
        return {
            { "path", path.empty() ? json(nullptr) : json(ToGenericProjectPath(path)) },
            { "schemaVersion", asset.schemaVersion },
            { "graphId", asset.graphId },
            { "name", asset.name },
            { "previewDefaults", {
                { "duration", asset.previewDefaults.duration },
                { "seed", asset.previewDefaults.seed },
                { "previewMeshPath", asset.previewDefaults.previewMeshPath },
                { "previewMaterialPath", asset.previewDefaults.previewMaterialPath }
            } },
            { "referencedAssets", asset.referencedAssets },
            { "nodes", std::move(nodes) },
            { "pins", std::move(pins) },
            { "links", std::move(links) },
            { "exposedParameters", std::move(parameters) }
        };
    }

    std::filesystem::path ResolveEffectGraphPath(const json& params, PathAccess access, bool mustExist)
    {
        const std::filesystem::path path = ResolveProjectPath(params.value("path", std::string{}), access, mustExist);
        const std::string generic = path.generic_string();
        const std::string suffix = ".effectgraph.json";
        if (generic.size() < suffix.size() || generic.compare(generic.size() - suffix.size(), suffix.size(), suffix) != 0) {
            throw MakeError("invalid_param", "Effect graph path must end with .effectgraph.json.", { { "path", ToGenericProjectPath(path) } });
        }
        return path;
    }

    EffectGraphAsset LoadEffectGraphAssetFromParams(const json& params, std::filesystem::path& path, PathAccess access)
    {
        path = ResolveEffectGraphPath(params, access, true);
        EffectGraphAsset asset;
        if (!EffectGraphSerializer::Load(path.string(), asset)) {
            throw MakeError("effect_load_failed", "Failed to load effect graph asset.", { { "path", ToGenericProjectPath(path) } });
        }
        return asset;
    }

    void SaveEffectGraphAssetOrThrow(const std::filesystem::path& path, const EffectGraphAsset& asset)
    {
        if (!EffectGraphSerializer::Save(path.string(), asset)) {
            throw MakeError("effect_save_failed", "Failed to save effect graph asset.", { { "path", ToGenericProjectPath(path) } });
        }
    }

    void RevealEffectEditorChange(EngineKernel& kernel, const json& params, const std::filesystem::path& path)
    {
        if (!params.value("showWorkspace", true)) {
            return;
        }

        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        if (!editor->OpenEffectEditorFromAutomation(path)) {
            throw MakeError("effect_open_failed", "Failed to reflect the effect graph in Effect Editor.", {
                { "path", ToGenericProjectPath(path) }
            });
        }
    }

    uint32_t FindEffectPinId(const EffectGraphAsset& asset,
                             uint32_t nodeId,
                             EffectPinKind kind,
                             std::optional<EffectValueType> valueType,
                             const std::string& name)
    {
        for (const auto& pin : asset.pins) {
            if (pin.nodeId != nodeId || pin.kind != kind) {
                continue;
            }
            if (valueType.has_value() && pin.valueType != *valueType) {
                continue;
            }
            if (!name.empty() && ToLowerCopy(pin.name) != ToLowerCopy(name)) {
                continue;
            }
            return pin.id;
        }
        return 0;
    }

    bool CanCreateEffectLink(const EffectGraphAsset& asset, uint32_t startPinId, uint32_t endPinId, std::string& reason)
    {
        const EffectGraphPin* startPin = asset.FindPin(startPinId);
        const EffectGraphPin* endPin = asset.FindPin(endPinId);
        if (!startPin || !endPin) {
            reason = "Invalid pin";
            return false;
        }
        if (startPin->nodeId == endPin->nodeId) {
            reason = "Same node";
            return false;
        }
        if (startPin->kind != EffectPinKind::Output || endPin->kind != EffectPinKind::Input) {
            reason = "Output -> Input only";
            return false;
        }
        if (startPin->valueType != endPin->valueType) {
            reason = "Type mismatch";
            return false;
        }
        for (const auto& link : asset.links) {
            if (link.startPinId == startPinId && link.endPinId == endPinId) {
                reason = "Duplicate link";
                return false;
            }
            if (link.endPinId == endPinId) {
                reason = "Input already connected";
                return false;
            }
        }
        return true;
    }

    void ApplyEffectNodeFields(EffectGraphNode& node, const json& fields)
    {
        if (!fields.is_object()) {
            throw MakeError("invalid_param", "fields must be an object.");
        }
        if (fields.contains("title")) node.title = fields["title"].get<std::string>();
        if (fields.contains("position")) node.position = ReadFloat2OrDefault(fields["position"], node.position);
        if (fields.contains("scalar")) node.scalar = fields["scalar"].get<float>();
        if (fields.contains("scalar2")) node.scalar2 = fields["scalar2"].get<float>();
        if (fields.contains("vectorValue")) ReadFloat4(fields["vectorValue"], node.vectorValue);
        if (fields.contains("vectorValue2")) ReadFloat4(fields["vectorValue2"], node.vectorValue2);
        if (fields.contains("vectorValue3")) ReadFloat4(fields["vectorValue3"], node.vectorValue3);
        if (fields.contains("vectorValue4")) ReadFloat4(fields["vectorValue4"], node.vectorValue4);
        if (fields.contains("vectorValue5")) ReadFloat4(fields["vectorValue5"], node.vectorValue5);
        if (fields.contains("vectorValue6")) ReadFloat4(fields["vectorValue6"], node.vectorValue6);
        if (fields.contains("vectorValue7")) ReadFloat4(fields["vectorValue7"], node.vectorValue7);
        if (fields.contains("vectorValue8")) ReadFloat4(fields["vectorValue8"], node.vectorValue8);
        if (fields.contains("vectorValue9")) ReadFloat4(fields["vectorValue9"], node.vectorValue9);
        if (fields.contains("stringValue")) node.stringValue = fields["stringValue"].get<std::string>();
        if (fields.contains("stringValue2")) node.stringValue2 = fields["stringValue2"].get<std::string>();
        if (fields.contains("stringValue3")) node.stringValue3 = fields["stringValue3"].get<std::string>();
        if (fields.contains("stringValue4")) node.stringValue4 = fields["stringValue4"].get<std::string>();
        if (fields.contains("stringValue5")) node.stringValue5 = fields["stringValue5"].get<std::string>();
        if (fields.contains("stringValue6")) node.stringValue6 = fields["stringValue6"].get<std::string>();
        if (fields.contains("intValue")) node.intValue = fields["intValue"].get<int>();
        if (fields.contains("intValue2")) node.intValue2 = fields["intValue2"].get<int>();
        if (fields.contains("boolValue")) node.boolValue = fields["boolValue"].get<bool>();
    }

    void ApplyEffectPreviewDefaults(EffectGraphAsset& asset, const json& fields)
    {
        if (!fields.is_object()) {
            throw MakeError("invalid_param", "previewDefaults must be an object.");
        }
        if (fields.contains("duration")) asset.previewDefaults.duration = fields["duration"].get<float>();
        if (fields.contains("seed")) asset.previewDefaults.seed = fields["seed"].get<uint32_t>();
        if (fields.contains("previewMeshPath")) asset.previewDefaults.previewMeshPath = fields["previewMeshPath"].get<std::string>();
        if (fields.contains("previewMaterialPath")) asset.previewDefaults.previewMaterialPath = fields["previewMaterialPath"].get<std::string>();
    }

    json EffectCompileResultToJson(const CompiledEffectAsset& compiled)
    {
        return {
            { "valid", compiled.valid },
            { "sourceAssetPath", compiled.sourceAssetPath },
            { "graphId", compiled.graphId },
            { "name", compiled.name },
            { "duration", compiled.duration },
            { "errors", compiled.errors },
            { "warnings", compiled.warnings },
            { "executionPlan", {
                { "spawnNodeIds", compiled.executionPlan.spawnNodeIds },
                { "updateNodeIds", compiled.executionPlan.updateNodeIds },
                { "renderNodeIds", compiled.executionPlan.renderNodeIds }
            } },
            { "meshRenderer", {
                { "enabled", compiled.meshRenderer.enabled },
                { "meshAssetPath", compiled.meshRenderer.meshAssetPath },
                { "materialAssetPath", compiled.meshRenderer.materialAssetPath },
                { "tint", Float4ToJson(compiled.meshRenderer.tint) }
            } },
            { "particleRenderer", {
                { "enabled", compiled.particleRenderer.enabled },
                { "maxParticles", compiled.particleRenderer.maxParticles },
                { "spawnRate", compiled.particleRenderer.spawnRate },
                { "burstCount", compiled.particleRenderer.burstCount },
                { "particleLifetime", compiled.particleRenderer.particleLifetime }
            } },
            { "requiredAssetReferences", compiled.requiredAssetReferences }
        };
    }

    json EffectEditorStateToJson(EditorLayer& editor, Registry* registry, bool includeGraph);

    EffectGraphNode* EnsureEffectNodeForAutomation(EffectGraphAsset& asset, EffectGraphNodeType type)
    {
        for (auto& node : asset.nodes) {
            if (node.type == type) {
                return &node;
            }
        }
        return &AddEffectGraphNode(asset, type, { 120.0f + static_cast<float>(asset.nodes.size()) * 48.0f, 120.0f });
    }

    EffectGraphNode* FindEffectNodeForAutomation(EffectGraphAsset& asset, EffectGraphNodeType type)
    {
        for (auto& node : asset.nodes) {
            if (node.type == type) {
                return &node;
            }
        }
        return nullptr;
    }

    void ApplyEffectSemanticParamsToAsset(EffectGraphAsset& asset, const json& fields)
    {
        if (!fields.is_object()) {
            throw MakeError("invalid_param", "semantic fields must be an object.");
        }

        auto* lifetime = EnsureEffectNodeForAutomation(asset, EffectGraphNodeType::Lifetime);
        auto* emitter = EnsureEffectNodeForAutomation(asset, EffectGraphNodeType::ParticleEmitter);
        auto* color = EnsureEffectNodeForAutomation(asset, EffectGraphNodeType::Color);
        auto* sprite = EnsureEffectNodeForAutomation(asset, EffectGraphNodeType::SpriteRenderer);

        if (fields.contains("name")) {
            asset.name = fields["name"].get<std::string>();
        }
        if (fields.contains("duration")) {
            const float duration = fields["duration"].get<float>();
            asset.previewDefaults.duration = duration;
            lifetime->scalar = duration;
        }
        if (fields.contains("seed")) {
            asset.previewDefaults.seed = fields["seed"].get<uint32_t>();
        }
        if (fields.contains("spawnRate")) emitter->scalar = fields["spawnRate"].get<float>();
        if (fields.contains("burstCount")) emitter->scalar2 = static_cast<float>(fields["burstCount"].get<uint32_t>());
        if (fields.contains("maxParticles")) emitter->intValue = fields["maxParticles"].get<int>();
        if (fields.contains("particleLifetime")) emitter->vectorValue.x = fields["particleLifetime"].get<float>();
        if (fields.contains("startSize")) emitter->vectorValue.y = fields["startSize"].get<float>();
        if (fields.contains("endSize")) emitter->vectorValue.z = fields["endSize"].get<float>();
        if (fields.contains("speed")) emitter->vectorValue.w = fields["speed"].get<float>();
        if (fields.contains("acceleration") && fields["acceleration"].is_array()) {
            DirectX::XMFLOAT3 v{};
            if (ReadFloat3(fields["acceleration"], v)) {
                emitter->vectorValue2.x = v.x;
                emitter->vectorValue2.y = v.y;
                emitter->vectorValue2.z = v.z;
            }
        }
        if (fields.contains("drag")) emitter->vectorValue2.w = fields["drag"].get<float>();
        if (fields.contains("shape")) {
            const std::string shape = ToLowerCopy(fields["shape"].get<std::string>());
            if (shape == "box") emitter->intValue2 = static_cast<int>(EffectSpawnShapeType::Box);
            else if (shape == "cone") emitter->intValue2 = static_cast<int>(EffectSpawnShapeType::Cone);
            else if (shape == "circle") emitter->intValue2 = static_cast<int>(EffectSpawnShapeType::Circle);
            else if (shape == "line") emitter->intValue2 = static_cast<int>(EffectSpawnShapeType::Line);
            else emitter->intValue2 = static_cast<int>(EffectSpawnShapeType::Sphere);
        }
        if (fields.contains("shapeParams") && fields["shapeParams"].is_array()) {
            DirectX::XMFLOAT3 v{};
            if (ReadFloat3(fields["shapeParams"], v)) {
                emitter->vectorValue3.x = v.x;
                emitter->vectorValue3.y = v.y;
                emitter->vectorValue3.z = v.z;
            }
        }
        if (fields.contains("spinRate")) emitter->vectorValue3.w = fields["spinRate"].get<float>();
        if (fields.contains("curlNoiseStrength")) emitter->vectorValue4.x = fields["curlNoiseStrength"].get<float>();
        if (fields.contains("curlNoiseScale")) emitter->vectorValue4.y = fields["curlNoiseScale"].get<float>();
        if (fields.contains("curlNoiseScroll")) emitter->vectorValue4.z = fields["curlNoiseScroll"].get<float>();
        if (fields.contains("vortexStrength")) emitter->vectorValue4.w = fields["vortexStrength"].get<float>();

        if (fields.contains("startColor")) {
            ReadFloat4(fields["startColor"], color->vectorValue);
            sprite->vectorValue = color->vectorValue;
        }
        if (fields.contains("endColor")) {
            ReadFloat4(fields["endColor"], color->vectorValue2);
        }
        if (fields.contains("texture")) {
            sprite->stringValue = fields["texture"].get<std::string>();
        }
        if (fields.contains("drawMode")) {
            const std::string drawMode = ToLowerCopy(fields["drawMode"].get<std::string>());
            if (drawMode == "ribbon") sprite->intValue = static_cast<int>(EffectParticleDrawMode::Ribbon);
            else if (drawMode == "mesh") sprite->intValue = static_cast<int>(EffectParticleDrawMode::Mesh);
            else sprite->intValue = static_cast<int>(EffectParticleDrawMode::Billboard);
        }
        if (fields.contains("ribbonWidth")) sprite->vectorValue2.x = fields["ribbonWidth"].get<float>();
        if (fields.contains("ribbonStretch")) sprite->vectorValue2.y = fields["ribbonStretch"].get<float>();
        if (fields.contains("alphaScale")) sprite->vectorValue2.z = fields["alphaScale"].get<float>();
        if (fields.contains("flipbookFps")) sprite->vectorValue2.w = fields["flipbookFps"].get<float>();
        if (fields.contains("sizeCurveBias")) sprite->vectorValue3.x = fields["sizeCurveBias"].get<float>();
        if (fields.contains("alphaCurveBias")) sprite->vectorValue3.y = fields["alphaCurveBias"].get<float>();
        if (fields.contains("subUvColumns")) sprite->vectorValue3.z = static_cast<float>(fields["subUvColumns"].get<int>());
        if (fields.contains("subUvRows")) sprite->vectorValue3.w = static_cast<float>(fields["subUvRows"].get<int>());

        EffectEditorInternal::EnsureGuiAuthoringLinks(asset);

        // SpriteRenderer (パーティクル) チェーンが確立された後は、
        // デフォルトグラフ由来の MeshRenderer / MeshSource ノードは不要になる。
        // 残留させると "Mesh Renderer has an unconnected flow input." 警告が出続けるため除去する。
        {
            std::vector<uint32_t> removeNodeIds;
            for (const auto& node : asset.nodes) {
                if (node.type == EffectGraphNodeType::MeshRenderer ||
                    node.type == EffectGraphNodeType::MeshSource) {
                    removeNodeIds.push_back(node.id);
                }
            }
            if (!removeNodeIds.empty()) {
                auto isRemoved = [&](uint32_t id) {
                    return std::find(removeNodeIds.begin(), removeNodeIds.end(), id) != removeNodeIds.end();
                };
                asset.nodes.erase(std::remove_if(asset.nodes.begin(), asset.nodes.end(),
                    [&](const EffectGraphNode& n) { return isRemoved(n.id); }), asset.nodes.end());
                asset.pins.erase(std::remove_if(asset.pins.begin(), asset.pins.end(),
                    [&](const EffectGraphPin& p) { return isRemoved(p.nodeId); }), asset.pins.end());
            }
        }

        EffectEditorInternal::SanitizeGraphAsset(asset);
    }

    void ApplyNamedEffectPreset(EffectGraphAsset& asset, const std::string& preset)
    {
        const std::string p = ToLowerCopy(preset);
        if (p == "smoke" || p == "smoke_plume") {
            ApplyEffectSemanticParamsToAsset(asset, {
                { "name", "Smoke Plume" }, { "duration", 6.0f }, { "spawnRate", 24000.0f },
                { "particleLifetime", 4.2f }, { "startSize", 0.18f }, { "endSize", 1.55f },
                { "speed", 0.95f }, { "shape", "sphere" }, { "shapeParams", json::array({ 0.65f, 0.65f, 0.65f }) },
                { "acceleration", json::array({ 0.0f, 0.18f, 0.0f }) }, { "drag", 0.12f },
                { "curlNoiseStrength", 0.75f }, { "curlNoiseScale", 0.10f }, { "curlNoiseScroll", 0.07f }, { "vortexStrength", 0.28f },
                { "startColor", json::array({ 0.42f, 0.43f, 0.45f, 0.82f }) },
                { "endColor", json::array({ 0.10f, 0.10f, 0.10f, 0.0f }) },
                { "texture", "Data/Effect/particle/smoke_03.png" }
            });
            return;
        }
        if (p == "slash" || p == "ribbon" || p == "ribbon_trail") {
            ApplyEffectSemanticParamsToAsset(asset, {
                { "name", "Ribbon Trail" }, { "duration", 2.8f }, { "spawnRate", 3000.0f },
                { "particleLifetime", 1.20f }, { "startSize", 0.08f }, { "endSize", 0.025f },
                { "speed", 1.9f }, { "shape", "line" }, { "shapeParams", json::array({ 0.40f, 0.0f, 0.0f }) },
                { "spinRate", 3.0f }, { "drawMode", "ribbon" }, { "ribbonWidth", 0.12f }, { "ribbonStretch", 1.75f },
                { "curlNoiseStrength", 0.16f }, { "curlNoiseScale", 0.22f }, { "curlNoiseScroll", 0.24f }, { "vortexStrength", 2.10f },
                { "startColor", json::array({ 0.30f, 0.95f, 1.00f, 0.95f }) },
                { "endColor", json::array({ 0.08f, 0.28f, 1.00f, 0.0f }) },
                { "texture", "Data/Effect/particle/trace_03.png" }
            });
            return;
        }
        if (p == "magic" || p == "magic_burst") {
            ApplyEffectSemanticParamsToAsset(asset, {
                { "name", "Magic Burst" }, { "duration", 2.1f }, { "spawnRate", 90000.0f },
                { "particleLifetime", 1.10f }, { "startSize", 0.14f }, { "endSize", 0.028f },
                { "speed", 7.8f }, { "shape", "sphere" }, { "shapeParams", json::array({ 0.22f, 0.22f, 0.22f }) },
                { "acceleration", json::array({ 0.0f, -0.55f, 0.0f }) }, { "spinRate", 22.0f },
                { "curlNoiseStrength", 0.55f }, { "curlNoiseScale", 0.26f }, { "curlNoiseScroll", 0.40f }, { "vortexStrength", 1.65f },
                { "startColor", json::array({ 0.95f, 0.45f, 1.00f, 1.0f }) },
                { "endColor", json::array({ 0.35f, 0.15f, 1.00f, 0.0f }) },
                { "texture", "Data/Effect/particle/magic_03.png" }
            });
            return;
        }
        ApplyEffectSemanticParamsToAsset(asset, {
            { "name", "Spark Fountain" }, { "duration", 3.2f }, { "spawnRate", 70000.0f },
            { "particleLifetime", 1.45f }, { "startSize", 0.08f }, { "endSize", 0.025f },
            { "speed", 4.2f }, { "shape", "sphere" }, { "shapeParams", json::array({ 0.26f, 0.26f, 0.26f }) },
            { "acceleration", json::array({ 0.0f, -2.2f, 0.0f }) }, { "drag", 0.02f }, { "spinRate", 13.0f },
            { "curlNoiseStrength", 0.30f }, { "curlNoiseScale", 0.35f }, { "curlNoiseScroll", 0.30f },
            { "startColor", json::array({ 0.65f, 0.95f, 1.00f, 1.0f }) },
            { "endColor", json::array({ 0.15f, 0.75f, 1.00f, 0.0f }) },
            { "texture", "Data/Effect/particle/spark_03.png" }
        });
    }

    json HandleEffectListNodeTypes()
    {
        json types = json::array();
        for (EffectGraphNodeType type : {
            EffectGraphNodeType::Output,
            EffectGraphNodeType::Spawn,
            EffectGraphNodeType::Lifetime,
            EffectGraphNodeType::MeshSource,
            EffectGraphNodeType::MeshRenderer,
            EffectGraphNodeType::ParticleEmitter,
            EffectGraphNodeType::SpriteRenderer,
            EffectGraphNodeType::Float,
            EffectGraphNodeType::Vec3,
            EffectGraphNodeType::Color }) {
            types.push_back({
                { "type", EffectNodeTypeToApiString(type) },
                { "label", EffectGraphNodeTypeToString(type) }
            });
        }
        return { { "nodeTypes", std::move(types) } };
    }

    json HandleEffectCreateAsset(EngineKernel& kernel, const json& params)
    {
        const std::filesystem::path path = ResolveEffectGraphPath(params, PathAccess::WriteAsset, false);
        EffectGraphAsset asset = CreateDefaultEffectGraphAsset();
        asset.name = params.value("name", asset.name);
        asset.graphId = params.value("graphId", asset.graphId);
        if (params.contains("previewDefaults")) {
            ApplyEffectPreviewDefaults(asset, params["previewDefaults"]);
        }
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "asset", EffectGraphSummaryToJson(asset, path) } };
    }

    json HandleEffectGetAsset(const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::ReadAsset);
        return { { "asset", EffectGraphSummaryToJson(asset, path) } };
    }

    json HandleEffectSetAsset(EngineKernel& kernel, const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::WriteAsset);
        if (params.contains("name")) asset.name = params["name"].get<std::string>();
        if (params.contains("graphId")) asset.graphId = params["graphId"].get<std::string>();
        if (params.contains("previewDefaults")) ApplyEffectPreviewDefaults(asset, params["previewDefaults"]);
        if (params.contains("referencedAssets") && params["referencedAssets"].is_array()) {
            asset.referencedAssets = params["referencedAssets"].get<std::vector<std::string>>();
        }
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "asset", EffectGraphSummaryToJson(asset, path) } };
    }

    json HandleEffectApplyPreset(EngineKernel& kernel, const json& params)
    {
        std::filesystem::path path = ResolveEffectGraphPath(params, PathAccess::WriteAsset, false);
        EffectGraphAsset asset;
        if (std::filesystem::exists(path)) {
            if (!EffectGraphSerializer::Load(path.string(), asset)) {
                throw MakeError("effect_load_failed", "Failed to load effect graph asset.", { { "path", ToGenericProjectPath(path) } });
            }
        }
        else {
            asset = CreateDefaultEffectGraphAsset();
        }

        ApplyNamedEffectPreset(asset, params.value("preset", std::string("spark")));
        if (params.contains("semantic")) {
            ApplyEffectSemanticParamsToAsset(asset, params["semantic"]);
        }
        if (params.contains("name")) asset.name = params["name"].get<std::string>();
        if (params.contains("graphId")) asset.graphId = params["graphId"].get<std::string>();
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "asset", EffectGraphSummaryToJson(asset, path) } };
    }

    json HandleEffectSetSemanticParams(EngineKernel& kernel, const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::WriteAsset);
        const json fields = params.contains("semantic") ? params["semantic"] : params.value("fields", json::object());
        ApplyEffectSemanticParamsToAsset(asset, fields);
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "asset", EffectGraphSummaryToJson(asset, path) } };
    }

    json HandleEffectSetPreviewView(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        DirectX::XMFLOAT3 target = editor->GetEffectEditorPanel().GetPreviewCameraTarget();
        if (params.contains("target")) {
            ReadFloat3(params["target"], target);
        }
        const float yaw = params.value("yaw", 0.85f);
        const float pitch = params.value("pitch", -0.18f);
        const float distance = params.value("distance", 4.5f);
        const float fovY = params.value("fovY", editor->GetEffectEditorPanel().GetPreviewCameraFovY());
        editor->GetEffectEditorPanel().SetPreviewCameraAutomation(target, yaw, pitch, distance, fovY);
        if (params.contains("clearColor") || params.contains("useSkybox")) {
            DirectX::XMFLOAT4 clear = editor->GetEffectEditorPanel().GetPreviewClearColor();
            if (params.contains("clearColor")) {
                ReadFloat4(params["clearColor"], clear);
            }
            editor->GetEffectEditorPanel().SetPreviewEnvironmentAutomation(clear, params.value("useSkybox", editor->GetEffectEditorPanel().ShouldPreviewUseSkybox()));
        }
        return { { "state", EffectEditorStateToJson(*editor, kernel.GetGameRegistry(), false) } };
    }

    json HandleEffectAddNode(EngineKernel& kernel, const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::WriteAsset);
        const EffectGraphNodeType type = EffectNodeTypeFromString(params.value("type", std::string{}));
        const DirectX::XMFLOAT2 position = ReadFloat2OrDefault(params.value("position", json::array({ 0.0f, 0.0f })), { 0.0f, 0.0f });
        EffectGraphNode& node = AddEffectGraphNode(asset, type, position);
        if (params.contains("fields")) {
            ApplyEffectNodeFields(node, params["fields"]);
        }
        const json nodeJson = EffectNodeToJson(node);
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "path", ToGenericProjectPath(path) }, { "node", nodeJson } };
    }

    json HandleEffectSetNode(EngineKernel& kernel, const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::WriteAsset);
        const uint32_t nodeId = params.value("nodeId", 0u);
        EffectGraphNode* node = asset.FindNode(nodeId);
        if (!node) {
            throw MakeError("node_not_found", "Effect graph node was not found.", { { "nodeId", nodeId } });
        }
        ApplyEffectNodeFields(*node, params.value("fields", json::object()));
        const json nodeJson = EffectNodeToJson(*node);
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "path", ToGenericProjectPath(path) }, { "node", nodeJson } };
    }

    json HandleEffectDeleteNode(EngineKernel& kernel, const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::WriteAsset);
        const uint32_t nodeId = params.value("nodeId", 0u);
        if (!asset.FindNode(nodeId)) {
            throw MakeError("node_not_found", "Effect graph node was not found.", { { "nodeId", nodeId } });
        }
        std::vector<uint32_t> pinsToRemove;
        for (const auto& pin : asset.pins) {
            if (pin.nodeId == nodeId) {
                pinsToRemove.push_back(pin.id);
            }
        }
        asset.links.erase(
            std::remove_if(
                asset.links.begin(),
                asset.links.end(),
                [&](const EffectGraphLink& link) {
                    return std::find(pinsToRemove.begin(), pinsToRemove.end(), link.startPinId) != pinsToRemove.end() ||
                        std::find(pinsToRemove.begin(), pinsToRemove.end(), link.endPinId) != pinsToRemove.end();
                }),
            asset.links.end());
        asset.pins.erase(
            std::remove_if(asset.pins.begin(), asset.pins.end(), [nodeId](const EffectGraphPin& pin) { return pin.nodeId == nodeId; }),
            asset.pins.end());
        asset.nodes.erase(
            std::remove_if(asset.nodes.begin(), asset.nodes.end(), [nodeId](const EffectGraphNode& node) { return node.id == nodeId; }),
            asset.nodes.end());
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "asset", EffectGraphSummaryToJson(asset, path) } };
    }

    json HandleEffectConnect(EngineKernel& kernel, const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::WriteAsset);
        uint32_t startPinId = params.value("startPinId", 0u);
        uint32_t endPinId = params.value("endPinId", 0u);
        if (startPinId == 0 || endPinId == 0) {
            const uint32_t fromNodeId = params.value("fromNodeId", 0u);
            const uint32_t toNodeId = params.value("toNodeId", 0u);
            if (!asset.FindNode(fromNodeId) || !asset.FindNode(toNodeId)) {
                throw MakeError("node_not_found", "fromNodeId and toNodeId must refer to existing nodes.", {
                    { "fromNodeId", fromNodeId },
                    { "toNodeId", toNodeId }
                });
            }
            std::optional<EffectValueType> valueType;
            if (params.contains("valueType")) {
                valueType = EffectValueTypeFromString(params["valueType"].get<std::string>());
            }
            startPinId = FindEffectPinId(asset, fromNodeId, EffectPinKind::Output, valueType, params.value("fromPin", std::string{}));
            endPinId = FindEffectPinId(asset, toNodeId, EffectPinKind::Input, valueType, params.value("toPin", std::string{}));
        }
        std::string reason;
        if (!CanCreateEffectLink(asset, startPinId, endPinId, reason)) {
            throw MakeError("effect_link_invalid", "Cannot create effect graph link.", {
                { "reason", reason },
                { "startPinId", startPinId },
                { "endPinId", endPinId }
            });
        }
        EffectGraphLink link;
        link.id = asset.nextLinkId++;
        link.startPinId = startPinId;
        link.endPinId = endPinId;
        asset.links.push_back(link);
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "path", ToGenericProjectPath(path) }, { "link", EffectLinkToJson(link) } };
    }

    json HandleEffectDisconnect(EngineKernel& kernel, const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::WriteAsset);
        const uint32_t linkId = params.value("linkId", 0u);
        const size_t before = asset.links.size();
        asset.links.erase(
            std::remove_if(
                asset.links.begin(),
                asset.links.end(),
                [&](const EffectGraphLink& link) {
                    if (linkId != 0) {
                        return link.id == linkId;
                    }
                    return link.startPinId == params.value("startPinId", 0u) &&
                        link.endPinId == params.value("endPinId", 0u);
                }),
            asset.links.end());
        if (asset.links.size() == before) {
            throw MakeError("link_not_found", "Effect graph link was not found.");
        }
        SaveEffectGraphAssetOrThrow(path, asset);
        RevealEffectEditorChange(kernel, params, path);
        return { { "asset", EffectGraphSummaryToJson(asset, path) } };
    }

    json HandleEffectCompile(const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::ReadAsset);
        auto compiled = EffectCompiler::Compile(asset, ToGenericProjectPath(path));
        if (!compiled) {
            throw MakeError("effect_compile_failed", "Effect compiler returned no result.", { { "path", ToGenericProjectPath(path) } });
        }
        return { { "compile", EffectCompileResultToJson(*compiled) } };
    }

    json HandleEffectOpenWorkspace(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        std::filesystem::path path;
        if (params.contains("path") && !params.value("path", std::string{}).empty()) {
            path = ResolveEffectGraphPath(params, PathAccess::ReadAsset, true);
        }
        if (!editor->OpenEffectEditorFromAutomation(path)) {
            throw MakeError("effect_open_failed", "Failed to open effect graph in Effect Editor.", {
                { "path", path.empty() ? json(nullptr) : json(ToGenericProjectPath(path)) }
            });
        }
        return {
            { "effectEditorActive", editor->IsEffectEditorWorkspaceActive() },
            { "documentPath", editor->GetEffectEditorDocumentPath() }
        };
    }

    json HandleEffectTimelinePlay(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        std::filesystem::path path;
        if (params.contains("path") && !params.value("path", std::string{}).empty()) {
            path = ResolveEffectGraphPath(params, PathAccess::ReadAsset, true);
        }
        const float startTime = params.value("startTime", 0.0f);
        const bool paused = params.value("paused", false);
        if (!editor->PlayEffectTimelineFromAutomation(path, startTime, paused)) {
            throw MakeError("effect_timeline_play_failed", "Failed to play the Effect Editor timeline.", {
                { "path", path.empty() ? json(nullptr) : json(ToGenericProjectPath(path)) },
                { "startTime", startTime },
                { "paused", paused }
            });
        }
        return {
            { "effectEditorActive", editor->IsEffectEditorWorkspaceActive() },
            { "documentPath", editor->GetEffectEditorDocumentPath() },
            { "previewEntity", Entity::IsNull(editor->GetEffectPreviewEntity()) ? json(nullptr) : json(EntityToString(editor->GetEffectPreviewEntity())) },
            { "state", paused ? "paused" : "playing" },
            { "startTime", startTime }
        };
    }

    json HandleEffectTimelineStop(EngineKernel& kernel)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        if (!editor->StopEffectTimelineFromAutomation()) {
            throw MakeError("effect_timeline_stop_failed", "Failed to stop the Effect Editor timeline.");
        }
        return {
            { "effectEditorActive", editor->IsEffectEditorWorkspaceActive() },
            { "documentPath", editor->GetEffectEditorDocumentPath() },
            { "previewEntity", nullptr },
            { "state", "stopped" }
        };
    }

    json EffectPreviewPlaybackToJson(Registry* registry, EntityID previewEntity)
    {
        if (!registry || Entity::IsNull(previewEntity) || !registry->IsAlive(previewEntity)) {
            return nullptr;
        }
        auto* playback = registry->GetComponent<EffectPlaybackComponent>(previewEntity);
        auto* asset = registry->GetComponent<EffectAssetComponent>(previewEntity);
        json out = {
            { "entity", EntityToString(previewEntity) },
            { "hasPlayback", playback != nullptr },
            { "hasAsset", asset != nullptr }
        };
        if (asset) {
            out["assetPath"] = asset->assetPath;
            out["autoPlay"] = asset->autoPlay;
            out["loop"] = asset->loop;
        }
        if (playback) {
            out["isPlaying"] = playback->isPlaying;
            out["isPaused"] = playback->isPaused;
            out["currentTime"] = playback->currentTime;
            out["duration"] = playback->duration;
            out["seed"] = playback->seed;
            out["runtimeInstanceId"] = playback->runtimeInstanceId;
            out["stopRequested"] = playback->stopRequested;
        }
        return out;
    }

    json AnalyzeImageBuffer(const ImageBuffer& image)
    {
        if (image.width <= 0 || image.height <= 0 || image.bgra.empty()) {
            return {
                { "width", image.width },
                { "height", image.height },
                { "empty", true }
            };
        }

        const auto samplePixel = [&](int x, int y) {
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4u;
            return std::array<float, 3>{
                image.bgra[i + 2] / 255.0f,
                image.bgra[i + 1] / 255.0f,
                image.bgra[i + 0] / 255.0f
            };
        };

        const std::array<std::array<float, 3>, 4> cornerSamples = {
            samplePixel(0, 0),
            samplePixel((std::max)(0, image.width - 1), 0),
            samplePixel(0, (std::max)(0, image.height - 1)),
            samplePixel((std::max)(0, image.width - 1), (std::max)(0, image.height - 1))
        };
        std::array<float, 3> bg{ 0.0f, 0.0f, 0.0f };
        for (const auto& c : cornerSamples) {
            bg[0] += c[0] * 0.25f;
            bg[1] += c[1] * 0.25f;
            bg[2] += c[2] * 0.25f;
        }

        double brightnessSum = 0.0;
        double saturationSum = 0.0;
        double redSum = 0.0;
        double greenSum = 0.0;
        double blueSum = 0.0;
        double energySum = 0.0;
        int brightPixels = 0;
        int effectPixels = 0;
        int minX = image.width;
        int minY = image.height;
        int maxX = -1;
        int maxY = -1;
        const int total = image.width * image.height;
        std::unordered_map<uint32_t, int> colorBuckets;

        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 4u;
                const float b = image.bgra[i + 0] / 255.0f;
                const float g = image.bgra[i + 1] / 255.0f;
                const float r = image.bgra[i + 2] / 255.0f;
                const float maxC = (std::max)(r, (std::max)(g, b));
                const float minC = (std::min)(r, (std::min)(g, b));
                const float brightness = (r + g + b) / 3.0f;
                const float saturation = maxC > 0.0001f ? (maxC - minC) / maxC : 0.0f;
                const float bgDiff = std::sqrt(
                    (r - bg[0]) * (r - bg[0]) +
                    (g - bg[1]) * (g - bg[1]) +
                    (b - bg[2]) * (b - bg[2]));
                brightnessSum += brightness;
                saturationSum += saturation;
                redSum += r;
                greenSum += g;
                blueSum += b;
                energySum += maxC * maxC;
                {
                    const uint8_t qr = static_cast<uint8_t>(image.bgra[i + 2] >> 2);
                    const uint8_t qg = static_cast<uint8_t>(image.bgra[i + 1] >> 2);
                    const uint8_t qb = static_cast<uint8_t>(image.bgra[i + 0] >> 2);
                    colorBuckets[(static_cast<uint32_t>(qr) << 12) | (static_cast<uint32_t>(qg) << 6) | qb]++;
                }
                if (brightness > 0.65f || maxC > 0.85f) {
                    ++brightPixels;
                }
                if (bgDiff > 0.18f || saturation > 0.35f || brightness > 0.72f) {
                    ++effectPixels;
                    minX = (std::min)(minX, x);
                    minY = (std::min)(minY, y);
                    maxX = (std::max)(maxX, x);
                    maxY = (std::max)(maxY, y);
                }
            }
        }

        const double invTotal = total > 0 ? 1.0 / static_cast<double>(total) : 0.0;
        json bbox = nullptr;
        if (effectPixels > 0) {
            bbox = {
                { "min", json::array({ minX, minY }) },
                { "max", json::array({ maxX, maxY }) },
                { "size", json::array({ maxX - minX + 1, maxY - minY + 1 }) },
                { "center", json::array({ (minX + maxX) * 0.5f, (minY + maxY) * 0.5f }) },
                { "fill", json::array({
                    static_cast<float>(maxX - minX + 1) / static_cast<float>((std::max)(1, image.width)),
                    static_cast<float>(maxY - minY + 1) / static_cast<float>((std::max)(1, image.height))
                }) }
            };
        }

        return {
            { "width", image.width },
            { "height", image.height },
            { "empty", false },
            { "averageBrightness", brightnessSum * invTotal },
            { "averageSaturation", saturationSum * invTotal },
            { "energy", energySum * invTotal },
            { "brightPixelRatio", static_cast<double>(brightPixels) * invTotal },
            { "effectPixelRatio", static_cast<double>(effectPixels) * invTotal },
            { "dominantColor", json::array({ redSum * invTotal, greenSum * invTotal, blueSum * invTotal }) },
            { "estimatedBackgroundColor", json::array({ bg[0], bg[1], bg[2] }) },
            { "uniqueColors", static_cast<int>(colorBuckets.size()) },
            { "effectBounds", std::move(bbox) }
        };
    }

    ImageBuffer CaptureAutomationTargetImage(EngineKernel& kernel, const std::string& target)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        ImageBuffer clientImage;
        if (!CaptureBackBuffer(clientImage) && !CaptureClientArea(Graphics::Instance().GetWindowHandle(), clientImage)) {
            throw MakeError("capture_failed", "Failed to capture the engine window back buffer or client area.");
        }

        if (target == "scene_view") {
            return CropImage(clientImage, editor->GetSceneViewRect());
        }
        if (target == "game_view") {
            return CropImage(clientImage, editor->GetGameViewRect());
        }
        if (target == "effect_editor") {
            return CropImage(clientImage, editor->GetEffectEditorRect());
        }
        if (target == "effect_preview") {
            return CropImage(clientImage, editor->GetEffectPreviewRect());
        }
        if (target == "ui_editor") {
            return CropImage(clientImage, editor->GetUIEditorRect());
        }
        if (target == "window" || target == "display" || target == "client") {
            return clientImage;
        }
        throw MakeError("invalid_param", "target must be window, display, client, scene_view, game_view, effect_editor, effect_preview, or ui_editor.", {
            { "target", target }
        });
    }

    json EffectEditorStateToJson(EditorLayer& editor, Registry* registry, bool includeGraph)
    {
        EffectEditorPanel& panel = editor.GetEffectEditorPanel();
        const std::shared_ptr<CompiledEffectAsset> compiled = panel.GetCompiledForAutomation();
        json out = {
            { "effectEditorActive", editor.IsEffectEditorWorkspaceActive() },
            { "documentPath", panel.GetDocumentPath() },
            { "workspaceRect", Float4ToJson(editor.GetEffectEditorRect()) },
            { "previewRect", Float4ToJson(editor.GetEffectPreviewRect()) },
            { "authoringMode", panel.GetAuthoringModeForAutomation() },
            { "selectedNodeId", panel.GetSelectedNodeIdForAutomation() },
            { "selectedLinkId", panel.GetSelectedLinkIdForAutomation() },
            { "compileDirty", panel.IsCompileDirtyForAutomation() },
            { "previewEntity", Entity::IsNull(panel.GetPreviewEntity()) ? json(nullptr) : json(EntityToString(panel.GetPreviewEntity())) },
            { "previewPlayback", EffectPreviewPlaybackToJson(registry, panel.GetPreviewEntity()) },
            { "compiled", compiled ? EffectCompileResultToJson(*compiled) : json(nullptr) }
        };
        if (includeGraph) {
            out["asset"] = EffectGraphSummaryToJson(panel.GetAssetForAutomation(), std::filesystem::path(panel.GetDocumentPath()));
        }
        return out;
    }

    json HandleEffectGetState(EngineKernel& kernel, Registry* registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        if (params.contains("path") && !params.value("path", std::string{}).empty()) {
            const std::filesystem::path path = ResolveEffectGraphPath(params, PathAccess::ReadAsset, true);
            if (!editor->OpenEffectEditorFromAutomation(path)) {
                throw MakeError("effect_open_failed", "Failed to open effect graph in Effect Editor.", {
                    { "path", ToGenericProjectPath(path) }
                });
            }
        }
        if (params.value("compile", false) && !editor->GetEffectEditorPanel().CompileFromAutomation()) {
            throw MakeError("effect_compile_failed", "Effect Editor document did not compile.");
        }
        return { { "state", EffectEditorStateToJson(*editor, registry, params.value("includeGraph", true)) } };
    }

    json HandleEffectTimelineSeek(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        if (params.contains("path") && !params.value("path", std::string{}).empty()) {
            const std::filesystem::path path = ResolveEffectGraphPath(params, PathAccess::ReadAsset, true);
            if (!editor->OpenEffectEditorFromAutomation(path)) {
                throw MakeError("effect_open_failed", "Failed to open effect graph in Effect Editor.", {
                    { "path", ToGenericProjectPath(path) }
                });
            }
        }
        const float time = params.value("time", params.value("startTime", 0.0f));
        const bool paused = params.value("paused", true);
        if (!editor->GetEffectEditorPanel().SeekTimelineFromAutomation(&registry, time, paused)) {
            throw MakeError("effect_timeline_seek_failed", "Failed to seek the Effect Editor timeline.", {
                { "time", time },
                { "paused", paused }
            });
        }
        return { { "state", EffectEditorStateToJson(*editor, &registry, params.value("includeGraph", false)) } };
    }

    json HandleEffectTimelineStep(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const float deltaTime = params.value("deltaTime", params.value("seconds", 1.0f / 30.0f));
        const bool paused = params.value("paused", true);
        if (!editor->GetEffectEditorPanel().StepTimelineFromAutomation(&registry, deltaTime, paused)) {
            throw MakeError("effect_timeline_step_failed", "Failed to step the Effect Editor timeline.", {
                { "deltaTime", deltaTime },
                { "paused", paused }
            });
        }
        return { { "state", EffectEditorStateToJson(*editor, &registry, params.value("includeGraph", false)) } };
    }

    json HandleEffectSelectNode(EngineKernel& kernel, const json& params, bool focus)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const uint32_t nodeId = params.value("nodeId", 0u);
        const bool ok = focus
            ? editor->GetEffectEditorPanel().FocusNodeFromAutomation(nodeId)
            : editor->GetEffectEditorPanel().SelectNodeFromAutomation(nodeId, params.value("nodeMode", false));
        if (!ok) {
            throw MakeError("node_not_found", "Effect graph node was not found.", { { "nodeId", nodeId } });
        }
        editor->FocusPanelAutomation(EditorLayer::WindowFocusTarget::EffectEditor);
        return { { "state", EffectEditorStateToJson(*editor, kernel.GetGameRegistry(), true) } };
    }

    json HandleEffectAssertPreviewVisible(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        EffectEditorPanel& panel = editor->GetEffectEditorPanel();
        const EntityID previewEntity = panel.GetPreviewEntity();
        const bool live = !Entity::IsNull(previewEntity) && registry.IsAlive(previewEntity);
        const auto compiled = panel.GetCompiledForAutomation();
        const bool compiledValid = compiled && compiled->valid;
        const bool renderable = compiledValid && (compiled->meshRenderer.enabled || compiled->particleRenderer.enabled);
        const auto* playback = live ? registry.GetComponent<EffectPlaybackComponent>(previewEntity) : nullptr;
        const bool playbackOk = playback && !playback->stopRequested && (playback->isPlaying || playback->isPaused);
        bool ok = live && compiledValid;
        if (params.value("requireRenderable", true)) {
            ok = ok && renderable;
        }
        if (params.value("requirePlayback", true)) {
            ok = ok && playbackOk;
        }

        json projection = nullptr;
        if (params.value("assertSceneVisible", false) && live) {
            json assertParams = {
                { "view", params.value("view", std::string("scene_view")) },
                { "entities", json::array({ EntityToString(previewEntity) }) },
                { "requireAll", true },
                { "requireBoundsFullyVisible", params.value("requireBoundsFullyVisible", false) }
            };
            projection = HandleVisualAssertEntitiesVisible(kernel, registry, assertParams);
            ok = ok && projection.value("ok", false);
        }

        return {
            { "ok", ok },
            { "previewEntity", live ? json(EntityToString(previewEntity)) : json(nullptr) },
            { "live", live },
            { "compiledValid", compiledValid },
            { "renderable", renderable },
            { "playbackOk", playbackOk },
            { "playback", EffectPreviewPlaybackToJson(&registry, previewEntity) },
            { "projection", std::move(projection) }
        };
    }

    json HandleEffectCaptureReviewSet(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        if (params.contains("path") && !params.value("path", std::string{}).empty()) {
            const std::filesystem::path path = ResolveEffectGraphPath(params, PathAccess::ReadAsset, true);
            if (!editor->OpenEffectEditorFromAutomation(path)) {
                throw MakeError("effect_open_failed", "Failed to open effect graph in Effect Editor.", {
                    { "path", ToGenericProjectPath(path) }
                });
            }
        }

        EffectEditorPanel& panel = editor->GetEffectEditorPanel();
        if (params.value("compile", true) && !panel.CompileFromAutomation()) {
            throw MakeError("effect_compile_failed", "Effect Editor document did not compile.", {
                { "state", EffectEditorStateToJson(*editor, &registry, true) }
            });
        }

        const float time = params.value("time", params.value("startTime", 0.0f));
        const bool paused = params.value("paused", true);
        if (params.value("play", true)) {
            if (!panel.SeekTimelineFromAutomation(&registry, time, paused)) {
                throw MakeError("effect_timeline_seek_failed", "Failed to prepare Effect Editor preview.", {
                    { "time", time },
                    { "paused", paused }
                });
            }
        }

        editor->FocusPanelAutomation(EditorLayer::WindowFocusTarget::EffectEditor);

        const std::string stem = params.value("stem", std::string("effect_review"));
        const std::filesystem::path dir = params.value("dir", std::string("Saved/AI/screenshots/effect_review"));
        const std::string format = params.value("format", std::string("bmp"));
        const std::vector<std::string> targets = params.contains("targets")
            ? JsonStringList(params, "targets")
            : std::vector<std::string>{ "effect_editor", "window" };

        json screenshots = json::array();
        for (const std::string& target : targets) {
            const std::filesystem::path path = dir / (stem + "_" + target + "." + format);
            json captureParams = {
                { "target", target },
                { "path", path.generic_string() },
                { "format", format },
                { "inline", params.value("inline", false) }
            };
            screenshots.push_back(HandleCaptureScreenshot(kernel, captureParams, path));
        }

        json assertions = nullptr;
        if (params.value("assertPreview", true)) {
            assertions = HandleEffectAssertPreviewVisible(kernel, registry, params);
        }

        return {
            { "screenshots", std::move(screenshots) },
            { "assertions", std::move(assertions) },
            { "state", EffectEditorStateToJson(*editor, &registry, params.value("includeGraph", true)) },
            { "note", "For exact visible-tab review, call editor.focus_panel(effect_editor), wait one rendered frame, then capture effect_editor again." }
        };
    }

    json HandleEffectCaptureMultiTimeReview(EngineKernel& kernel, Registry& registry, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        if (params.contains("path") && !params.value("path", std::string{}).empty()) {
            const std::filesystem::path path = ResolveEffectGraphPath(params, PathAccess::ReadAsset, true);
            if (!editor->OpenEffectEditorFromAutomation(path)) {
                throw MakeError("effect_open_failed", "Failed to open effect graph in Effect Editor.", {
                    { "path", ToGenericProjectPath(path) }
                });
            }
        }
        EffectEditorPanel& panel = editor->GetEffectEditorPanel();
        if (params.value("compile", true) && !panel.CompileFromAutomation()) {
            throw MakeError("effect_compile_failed", "Effect Editor document did not compile.");
        }

        std::vector<float> times;
        if (params.contains("times") && params["times"].is_array()) {
            for (const json& t : params["times"]) {
                times.push_back(t.get<float>());
            }
        }
        if (times.empty()) {
            times = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        }

        const std::string stem = params.value("stem", std::string("effect_multi_review"));
        const std::filesystem::path dir = params.value("dir", std::string("Saved/AI/screenshots/effect_review"));
        const std::string format = params.value("format", std::string("bmp"));
        const std::string target = params.value("target", std::string("effect_editor"));
        json frames = json::array();

        editor->FocusPanelAutomation(EditorLayer::WindowFocusTarget::EffectEditor);
        for (size_t i = 0; i < times.size(); ++i) {
            const float t = times[i];
            if (!panel.SeekTimelineFromAutomation(&registry, t, params.value("paused", true))) {
                throw MakeError("effect_timeline_seek_failed", "Failed to seek Effect Editor timeline.", { { "time", t } });
            }
            const std::filesystem::path path = dir / (stem + "_t" + std::to_string(i) + "." + format);
            json captureParams = {
                { "target", target },
                { "path", path.generic_string() },
                { "format", format },
                { "inline", params.value("inline", false) }
            };
            json screenshot = HandleCaptureScreenshot(kernel, captureParams, path);
            ImageBuffer image = CaptureAutomationTargetImage(kernel, target);
            frames.push_back({
                { "time", t },
                { "screenshot", std::move(screenshot) },
                { "metrics", AnalyzeImageBuffer(image) },
                { "assertions", HandleEffectAssertPreviewVisible(kernel, registry, params) }
            });
        }

        return {
            { "frames", std::move(frames) },
            { "state", EffectEditorStateToJson(*editor, &registry, params.value("includeGraph", false)) }
        };
    }

    json HandleEffectPreviewSpawn(Registry& registry, const json& params)
    {
        std::filesystem::path path;
        EffectGraphAsset asset = LoadEffectGraphAssetFromParams(params, path, PathAccess::ReadAsset);
        auto compiled = EffectCompiler::Compile(asset, ToGenericProjectPath(path));
        if (!compiled || !compiled->valid) {
            throw MakeError("effect_compile_failed", "Effect graph must compile before preview spawn.", {
                { "compile", compiled ? EffectCompileResultToJson(*compiled) : json(nullptr) }
            });
        }

        EntityID entity = registry.CreateEntity();
        registry.AddComponent(entity, NameComponent{ params.value("name", std::string("AI Effect Preview")) });
        TransformComponent transform{};
        if (params.contains("position")) ReadFloat3(params["position"], transform.localPosition);
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;
        registry.AddComponent(entity, transform);
        registry.AddComponent(entity, HierarchyComponent{});
        if (params.value("previewOnly", true)) {
            registry.AddComponent(entity, EffectPreviewTagComponent{});
        }

        EffectAssetComponent assetComponent;
        assetComponent.assetPath = ToGenericProjectPath(path);
        assetComponent.autoPlay = true;
        assetComponent.loop = params.value("loop", true);
        assetComponent.useSelectedMeshFallback = params.value("useSelectedMeshFallback", true);
        registry.AddComponent(entity, assetComponent);

        EffectPlaybackComponent playback;
        playback.isPlaying = true;
        playback.isPaused = params.value("paused", false);
        playback.currentTime = params.value("startTime", 0.0f);
        playback.duration = compiled->duration;
        playback.seed = params.value("seed", asset.previewDefaults.seed);
        playback.loop = assetComponent.loop;
        registry.AddComponent(entity, playback);

        EffectSpawnRequestComponent request;
        request.pending = true;
        request.restartIfActive = true;
        request.startTime = playback.currentTime;
        registry.AddComponent(entity, request);

        if (params.value("select", true)) {
            EditorSelection::Instance().SelectEntity(entity);
        }
        return {
            { "entity", EntityToString(entity) },
            { "assetPath", assetComponent.assetPath },
            { "compile", EffectCompileResultToJson(*compiled) }
        };
    }

    json HandleSaveScene(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        const std::filesystem::path path = params.value("path", std::string{});
        std::filesystem::path safePath;
        if (!path.empty()) {
            safePath = ResolveProjectPath(path.string(), PathAccess::WriteScene, false);
        }

        const bool saved = editor->SaveSceneFromAutomation(safePath.empty() ? path : safePath);
        if (!saved) {
            throw MakeError("scene_save_failed", "Failed to save scene.", { { "path", path.string() } });
        }
        return { { "path", path.empty() ? editor->GetCurrentScenePath() : ToGenericProjectPath(safePath) } };
    }

    json HandleLoadScene(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        const std::filesystem::path path = params.value("path", std::string{});
        if (path.empty()) {
            throw MakeError("missing_param", "path is required.");
        }
        const std::filesystem::path safePath = ResolveProjectPath(path.string(), PathAccess::ReadScene, true);
        if (!editor->LoadSceneFromPath(safePath)) {
            throw MakeError("scene_load_failed", "Failed to load scene.", { { "path", path.string() } });
        }
        return { { "path", ToGenericProjectPath(safePath) } };
    }

    json HandleCaptureScreenshot(EngineKernel& kernel, const json& params, const std::filesystem::path& defaultPath)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        const std::string target = params.value("target", std::string("window"));
        const std::string format = params.value("format", std::string("bmp"));
        const bool inlineResult = params.value("inline", false);

        if (format != "bmp" && format != "png") {
            throw MakeError("invalid_param", "format must be bmp or png.", { { "format", format } });
        }

        const std::string ext = std::string(".") + format;
        std::filesystem::path path = params.value("path", std::string{});
        if (path.empty()) {
            path = defaultPath;
        }
        if (path.extension().empty()) {
            path += ext;
        }
        else {
            // Replace any existing extension with the chosen one
            path.replace_extension(ext);
        }

        const std::filesystem::path safePath = ResolveProjectPath(path.string(), PathAccess::AutomationFile, false);

        ImageBuffer outputImage = CaptureAutomationTargetImage(kernel, target);

        if (params.contains("region")) {
            const json& reg = params["region"];
            if (reg.is_array() && reg.size() == 4) {
                const DirectX::XMFLOAT4 regionRect = {
                    reg[0].get<float>(), reg[1].get<float>(),
                    reg[2].get<float>(), reg[3].get<float>()
                };
                outputImage = CropImage(outputImage, regionRect);
            }
        }

        std::vector<uint8_t> fileBytes;
        if (format == "png") {
            WritePng(safePath, outputImage);
        }
        else {
            WriteBmp24(safePath, outputImage);
        }

        json result = {
            { "path", ToGenericProjectPath(safePath) },
            { "target", target },
            { "format", format },
            { "width", outputImage.width },
            { "height", outputImage.height }
        };

        if (inlineResult) {
            std::ifstream ifs(safePath, std::ios::binary);
            if (ifs.is_open()) {
                fileBytes.assign(
                    std::istreambuf_iterator<char>(ifs),
                    std::istreambuf_iterator<char>());
                result["imageBase64"] = Base64Encode(fileBytes.data(), fileBytes.size());
            }
        }

        return result;
    }

    json HandleVisualEvaluateCapture(EngineKernel& kernel, const json& params)
    {
        const std::string target = params.value("target", std::string("window"));
        ImageBuffer image = CaptureAutomationTargetImage(kernel, target);

        if (params.contains("region")) {
            const json& reg = params["region"];
            if (reg.is_array() && reg.size() == 4) {
                const DirectX::XMFLOAT4 regionRect = {
                    reg[0].get<float>(), reg[1].get<float>(),
                    reg[2].get<float>(), reg[3].get<float>()
                };
                image = CropImage(image, regionRect);
            }
        }

        json out = {
            { "target", target },
            { "metrics", AnalyzeImageBuffer(image) }
        };

        const bool wantSave   = params.value("save", false);
        const bool wantInline = params.value("inline", false);
        if (wantSave || wantInline) {
            const std::string format = params.value("format", std::string("bmp"));
            std::filesystem::path path = params.value("path", std::string("Saved/AI/screenshots/evaluation.bmp"));
            const std::filesystem::path safePath = ResolveProjectPath(path.string(), PathAccess::AutomationFile, false);
            if (format == "png") {
                WritePng(safePath, image);
            }
            else {
                WriteBmp24(safePath, image);
            }
            out["path"] = ToGenericProjectPath(safePath);
            if (wantInline) {
                std::ifstream ifs(safePath, std::ios::binary);
                if (ifs.is_open()) {
                    const std::vector<uint8_t> fileBytes(
                        (std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
                    out["imageBase64"] = Base64Encode(fileBytes.data(), fileBytes.size());
                }
            }
        }
        return out;
    }

    // =========================================================
    // PlayerEditor helpers
    // =========================================================

    std::string ActorEditorModeToString(ActorEditorMode m)
    {
        switch (m) {
        case ActorEditorMode::Enemy: return "Enemy";
        case ActorEditorMode::NPC:   return "NPC";
        default:                     return "Player";
        }
    }

    ActorEditorMode ActorEditorModeFromString(const std::string& s)
    {
        if (s == "Enemy") return ActorEditorMode::Enemy;
        if (s == "NPC")   return ActorEditorMode::NPC;
        return ActorEditorMode::Player;
    }

    std::string StateNodeTypeToString(StateNodeType t)
    {
        switch (t) {
        case StateNodeType::Locomotion: return "Locomotion";
        case StateNodeType::Action:     return "Action";
        case StateNodeType::Dodge:      return "Dodge";
        case StateNodeType::Jump:       return "Jump";
        case StateNodeType::Damage:     return "Damage";
        case StateNodeType::Dead:       return "Dead";
        default:                        return "Custom";
        }
    }

    StateNodeType StateNodeTypeFromString(const std::string& s)
    {
        if (s == "Locomotion") return StateNodeType::Locomotion;
        if (s == "Action")     return StateNodeType::Action;
        if (s == "Dodge")      return StateNodeType::Dodge;
        if (s == "Jump")       return StateNodeType::Jump;
        if (s == "Damage")     return StateNodeType::Damage;
        if (s == "Dead")       return StateNodeType::Dead;
        return StateNodeType::Custom;
    }

    std::string ConditionTypeToString(ConditionType t)
    {
        switch (t) {
        case ConditionType::Timer:     return "Timer";
        case ConditionType::AnimEnd:   return "AnimEnd";
        case ConditionType::Health:    return "Health";
        case ConditionType::Stamina:   return "Stamina";
        case ConditionType::Parameter: return "Parameter";
        default:                       return "Input";
        }
    }

    ConditionType ConditionTypeFromString(const std::string& s)
    {
        if (s == "Timer")     return ConditionType::Timer;
        if (s == "AnimEnd")   return ConditionType::AnimEnd;
        if (s == "Health")    return ConditionType::Health;
        if (s == "Stamina")   return ConditionType::Stamina;
        if (s == "Parameter") return ConditionType::Parameter;
        return ConditionType::Input;
    }

    std::string CompareOpToString(CompareOp op)
    {
        switch (op) {
        case CompareOp::NotEqual:     return "NotEqual";
        case CompareOp::Greater:      return "Greater";
        case CompareOp::Less:         return "Less";
        case CompareOp::GreaterEqual: return "GreaterEqual";
        case CompareOp::LessEqual:    return "LessEqual";
        default:                      return "Equal";
        }
    }

    CompareOp CompareOpFromString(const std::string& s)
    {
        if (s == "NotEqual")     return CompareOp::NotEqual;
        if (s == "Greater")      return CompareOp::Greater;
        if (s == "Less")         return CompareOp::Less;
        if (s == "GreaterEqual") return CompareOp::GreaterEqual;
        if (s == "LessEqual")    return CompareOp::LessEqual;
        return CompareOp::Equal;
    }

    std::string ParameterTypeToString(ParameterType t)
    {
        switch (t) {
        case ParameterType::Int:     return "Int";
        case ParameterType::Bool:    return "Bool";
        case ParameterType::Trigger: return "Trigger";
        default:                     return "Float";
        }
    }

    ParameterType ParameterTypeFromString(const std::string& s)
    {
        if (s == "Int")     return ParameterType::Int;
        if (s == "Bool")    return ParameterType::Bool;
        if (s == "Trigger") return ParameterType::Trigger;
        return ParameterType::Float;
    }

    std::string TimelineTrackTypeToString(TimelineTrackType t)
    {
        switch (t) {
        case TimelineTrackType::Animation:   return "Animation";
        case TimelineTrackType::Hitbox:      return "Hitbox";
        case TimelineTrackType::VFX:         return "VFX";
        case TimelineTrackType::Audio:       return "Audio";
        case TimelineTrackType::CameraShake: return "CameraShake";
        case TimelineTrackType::Camera:      return "Camera";
        case TimelineTrackType::Event:       return "Event";
        case TimelineTrackType::Projectile:  return "Projectile";
        default:                             return "Custom";
        }
    }

    TimelineTrackType TimelineTrackTypeFromString(const std::string& s)
    {
        if (s == "Animation")   return TimelineTrackType::Animation;
        if (s == "Hitbox")      return TimelineTrackType::Hitbox;
        if (s == "VFX")         return TimelineTrackType::VFX;
        if (s == "Audio")       return TimelineTrackType::Audio;
        if (s == "CameraShake") return TimelineTrackType::CameraShake;
        if (s == "Camera")      return TimelineTrackType::Camera;
        if (s == "Event")       return TimelineTrackType::Event;
        if (s == "Projectile")  return TimelineTrackType::Projectile;
        return TimelineTrackType::Custom;
    }

    json StateNodeToJson(const StateNode& s)
    {
        return {
            { "id",               s.id },
            { "name",             s.name },
            { "type",             StateNodeTypeToString(s.type) },
            { "animationIndex",   s.animationIndex },
            { "timelineId",       s.timelineId },
            { "loopAnimation",    s.loopAnimation },
            { "animSpeed",        s.animSpeed },
            { "canInterrupt",     s.canInterrupt },
            { "position",         json::array({ s.position.x, s.position.y }) },
            { "behaviorTreePath", s.behaviorTreePath },
            { "aiNote",           s.aiNote }
        };
    }

    json StateTransitionToJson(const StateTransition& t)
    {
        json conditions = json::array();
        for (const auto& c : t.conditions) {
            conditions.push_back({
                { "type",    ConditionTypeToString(c.type) },
                { "param",   std::string(c.param) },
                { "compare", CompareOpToString(c.compare) },
                { "value",   c.value }
            });
        }
        return {
            { "id",                  t.id },
            { "fromState",           t.fromState },
            { "toState",             t.toState },
            { "priority",            t.priority },
            { "exitTimeNormalized",  t.exitTimeNormalized },
            { "blendDuration",       t.blendDuration },
            { "hasExitTime",         t.hasExitTime },
            { "conditions",          std::move(conditions) }
        };
    }

    json StateMachineAssetToJson(const StateMachineAsset& sm)
    {
        json states = json::array();
        for (const auto& s : sm.states) states.push_back(StateNodeToJson(s));
        json transitions = json::array();
        for (const auto& t : sm.transitions) transitions.push_back(StateTransitionToJson(t));
        json parameters = json::array();
        for (const auto& p : sm.parameters) {
            parameters.push_back({
                { "name",         p.name },
                { "type",         ParameterTypeToString(p.type) },
                { "defaultValue", p.defaultValue }
            });
        }
        return {
            { "name",           sm.name },
            { "defaultStateId", sm.defaultStateId },
            { "states",         std::move(states) },
            { "transitions",    std::move(transitions) },
            { "parameters",     std::move(parameters) }
        };
    }

    json TimelineTrackToJson(const TimelineTrack& t)
    {
        json items = json::array();
        for (const auto& item : t.items) {
            items.push_back({
                { "startFrame", item.startFrame },
                { "endFrame",   item.endFrame },
                { "eventName",  std::string(item.eventName) },
                { "eventData",  std::string(item.eventData) }
            });
        }
        json keyframes = json::array();
        for (const auto& kf : t.keyframes) {
            keyframes.push_back({
                { "frame", kf.frame },
                { "value", json::array({ kf.value[0], kf.value[1], kf.value[2], kf.value[3] }) }
            });
        }
        return {
            { "id",        t.id },
            { "name",      t.name },
            { "type",      TimelineTrackTypeToString(t.type) },
            { "muted",     t.muted },
            { "locked",    t.locked },
            { "color",     t.color },
            { "items",     std::move(items) },
            { "keyframes", std::move(keyframes) }
        };
    }

    json TimelineAssetToJson(const TimelineAsset& tl)
    {
        json tracks = json::array();
        for (const auto& t : tl.tracks) tracks.push_back(TimelineTrackToJson(t));
        return {
            { "id",             tl.id },
            { "name",           tl.name },
            { "fps",            tl.fps },
            { "duration",       tl.duration },
            { "frameCount",     tl.GetFrameCount() },
            { "animationIndex", tl.animationIndex },
            { "tracks",         std::move(tracks) }
        };
    }

    PlayerEditorPanel& RequirePlayerEditorPanel(EngineKernel& kernel)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        return editor->GetPlayerEditorPanel();
    }

    // =========================================================
    // PlayerEditor handlers
    // =========================================================

    json HandlePlayerEditorOpen(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        std::filesystem::path modelPath;
        const std::string modelPathStr = params.value("modelPath", std::string{});
        if (!modelPathStr.empty()) {
            modelPath = ResolveProjectPath(modelPathStr, PathAccess::ReadAsset, true);
        }

        if (!editor->OpenPlayerEditorFromAutomation(modelPath)) {
            throw MakeError("player_editor_open_failed", "Failed to open Player Editor.");
        }

        auto& panel = editor->GetPlayerEditorPanel();
        if (params.contains("actorMode")) {
            panel.SetActorEditorMode(ActorEditorModeFromString(params.value("actorMode", std::string("Player"))));
        }

        return {
            { "active",    editor->IsPlayerWorkspaceActive() },
            { "modelPath", panel.GetCurrentModelPath() },
            { "actorMode", ActorEditorModeToString(panel.GetActorEditorMode()) }
        };
    }

    json HandlePlayerEditorGetStatus(EngineKernel& kernel)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const auto& panel = editor->GetPlayerEditorPanel();
        return {
            { "active",           editor->IsPlayerWorkspaceActive() },
            { "modelPath",        panel.GetCurrentModelPath() },
            { "actorMode",        ActorEditorModeToString(panel.GetActorEditorMode()) },
            { "viewMode",         panel.GetViewMode() == PlayerEditorViewMode::Test ? "Test" : "Edit" },
            { "previewEntity",    Entity::IsNull(panel.GetPreviewEntity()) ? json(nullptr) : json(EntityToString(panel.GetPreviewEntity())) },
            { "isTimelinePlaying",panel.IsTimelinePlaying() },
            { "playheadFrame",    panel.GetPlayheadFrame() }
        };
    }

    json HandlePlayerEditorLoadModel(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const std::filesystem::path path = ResolveProjectPath(
            params.value("path", std::string{}), PathAccess::ReadAsset, true);

        if (!editor->OpenPlayerEditorFromAutomation(path)) {
            throw MakeError("player_editor_load_model_failed", "Failed to load model into Player Editor.", {
                { "path", ToGenericProjectPath(path) }
            });
        }
        return { { "modelPath", editor->GetPlayerEditorPanel().GetCurrentModelPath() } };
    }

    json HandlePlayerEditorGetStateMachine(EngineKernel& kernel)
    {
        return StateMachineAssetToJson(RequirePlayerEditorPanel(kernel).GetStateMachineAsset());
    }

    json HandlePlayerEditorAddState(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sm = panel.GetStateMachineAsset();

        const std::string   name = params.value("name", std::string("New State"));
        const StateNodeType type = StateNodeTypeFromString(params.value("type", std::string("Custom")));

        StateNode* node = sm.AddState(name, type);
        if (params.contains("animationIndex"))   node->animationIndex   = params["animationIndex"].get<int>();
        if (params.contains("loopAnimation"))    node->loopAnimation    = params["loopAnimation"].get<bool>();
        if (params.contains("animSpeed"))        node->animSpeed        = params["animSpeed"].get<float>();
        if (params.contains("canInterrupt"))     node->canInterrupt     = params["canInterrupt"].get<bool>();
        if (params.contains("position"))         ReadFloat2(params["position"], node->position);
        if (params.contains("behaviorTreePath")) node->behaviorTreePath = params["behaviorTreePath"].get<std::string>();
        if (params.contains("aiNote"))           node->aiNote           = params["aiNote"].get<std::string>();

        const uint32_t id = node->id;
        panel.MarkStateMachineDirty();
        panel.SelectStateNode(id);
        panel.RequestGraphFit();

        const StateNode* found = sm.FindState(id);
        return found ? StateNodeToJson(*found) : json{ { "id", id } };
    }

    json HandlePlayerEditorSetState(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sm = panel.GetStateMachineAsset();

        const uint32_t id = params.value("id", 0u);
        StateNode* node = sm.FindState(id);
        if (!node) {
            throw MakeError("not_found", "State node not found.", { { "id", id } });
        }

        if (params.contains("name"))             node->name             = params["name"].get<std::string>();
        if (params.contains("type"))             node->type             = StateNodeTypeFromString(params["type"].get<std::string>());
        if (params.contains("animationIndex"))   node->animationIndex   = params["animationIndex"].get<int>();
        if (params.contains("timelineId"))       node->timelineId       = params["timelineId"].get<uint32_t>();
        if (params.contains("loopAnimation"))    node->loopAnimation    = params["loopAnimation"].get<bool>();
        if (params.contains("animSpeed"))        node->animSpeed        = params["animSpeed"].get<float>();
        if (params.contains("canInterrupt"))     node->canInterrupt     = params["canInterrupt"].get<bool>();
        if (params.contains("position"))         ReadFloat2(params["position"], node->position);
        if (params.contains("behaviorTreePath")) node->behaviorTreePath = params["behaviorTreePath"].get<std::string>();
        if (params.contains("aiNote"))           node->aiNote           = params["aiNote"].get<std::string>();

        panel.MarkStateMachineDirty();
        panel.SelectStateNode(id);

        return StateNodeToJson(*node);
    }

    json HandlePlayerEditorDeleteState(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sm = panel.GetStateMachineAsset();

        const uint32_t id = params.value("id", 0u);
        if (!sm.FindState(id)) {
            throw MakeError("not_found", "State node not found.", { { "id", id } });
        }

        sm.RemoveState(id);
        panel.MarkStateMachineDirty();
        panel.ClearEditorSelection();

        return { { "deleted", id } };
    }

    json HandlePlayerEditorAddTransition(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sm = panel.GetStateMachineAsset();

        const uint32_t fromState = params.value("fromState", 0u);
        const uint32_t toState   = params.value("toState",   0u);
        if (!sm.FindState(fromState)) {
            throw MakeError("not_found", "fromState not found.", { { "fromState", fromState } });
        }
        if (!sm.FindState(toState)) {
            throw MakeError("not_found", "toState not found.", { { "toState", toState } });
        }

        StateTransition* t = sm.AddTransition(fromState, toState);
        if (params.contains("blendDuration"))      t->blendDuration      = params["blendDuration"].get<float>();
        if (params.contains("exitTimeNormalized")) t->exitTimeNormalized = params["exitTimeNormalized"].get<float>();
        if (params.contains("hasExitTime"))        t->hasExitTime        = params["hasExitTime"].get<bool>();
        if (params.contains("priority"))           t->priority           = params["priority"].get<int>();

        const uint32_t id = t->id;
        panel.MarkStateMachineDirty();
        panel.SelectTransition(id);

        for (const auto& tr : sm.transitions) {
            if (tr.id == id) return StateTransitionToJson(tr);
        }
        return { { "id", id } };
    }

    json HandlePlayerEditorSetTransition(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sm = panel.GetStateMachineAsset();

        const uint32_t id = params.value("id", 0u);
        StateTransition* t = nullptr;
        for (auto& tr : sm.transitions) {
            if (tr.id == id) { t = &tr; break; }
        }
        if (!t) {
            throw MakeError("not_found", "Transition not found.", { { "id", id } });
        }

        if (params.contains("blendDuration"))      t->blendDuration      = params["blendDuration"].get<float>();
        if (params.contains("exitTimeNormalized")) t->exitTimeNormalized = params["exitTimeNormalized"].get<float>();
        if (params.contains("hasExitTime"))        t->hasExitTime        = params["hasExitTime"].get<bool>();
        if (params.contains("priority"))           t->priority           = params["priority"].get<int>();

        panel.MarkStateMachineDirty();
        panel.SelectTransition(id);

        return StateTransitionToJson(*t);
    }

    json HandlePlayerEditorDeleteTransition(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sm = panel.GetStateMachineAsset();

        const uint32_t id = params.value("id", 0u);
        bool found = false;
        for (const auto& t : sm.transitions) {
            if (t.id == id) { found = true; break; }
        }
        if (!found) {
            throw MakeError("not_found", "Transition not found.", { { "id", id } });
        }

        sm.RemoveTransition(id);
        panel.MarkStateMachineDirty();
        panel.ClearEditorSelection();

        return { { "deleted", id } };
    }

    json HandlePlayerEditorSelect(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);

        if (params.contains("stateId")) {
            const uint32_t id = params["stateId"].get<uint32_t>();
            panel.SelectStateNode(id);
            return { { "selectedStateId", id } };
        }
        if (params.contains("transitionId")) {
            const uint32_t id = params["transitionId"].get<uint32_t>();
            panel.SelectTransition(id);
            return { { "selectedTransitionId", id } };
        }
        if (params.contains("trackId")) {
            const int id = params["trackId"].get<int>();
            panel.SelectTrack(id);
            return { { "selectedTrackId", id } };
        }

        panel.ClearEditorSelection();
        return { { "cleared", true } };
    }

    json HandlePlayerEditorSetDefaultState(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sm = panel.GetStateMachineAsset();

        const uint32_t id = params.value("id", 0u);
        if (!sm.FindState(id)) {
            throw MakeError("not_found", "State node not found.", { { "id", id } });
        }

        sm.defaultStateId = id;
        panel.MarkStateMachineDirty();
        panel.SelectStateNode(id);

        return { { "defaultStateId", id } };
    }

    json HandlePlayerEditorGetTimeline(EngineKernel& kernel)
    {
        return TimelineAssetToJson(RequirePlayerEditorPanel(kernel).GetTimelineAsset());
    }

    json HandlePlayerEditorAddTrack(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& tl = panel.GetTimelineAsset();

        const std::string     typeStr = params.value("type", std::string("Custom"));
        const std::string     name    = params.value("name", typeStr);
        const TimelineTrackType type  = TimelineTrackTypeFromString(typeStr);

        TimelineTrack* track = tl.AddTrack(type, name);
        if (params.contains("muted"))  track->muted  = params["muted"].get<bool>();
        if (params.contains("locked")) track->locked = params["locked"].get<bool>();

        const int trackId = static_cast<int>(track->id);
        panel.MarkTimelineDirty();
        panel.SelectTrack(trackId);

        return TimelineTrackToJson(*track);
    }

    json HandlePlayerEditorDeleteTrack(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& tl = panel.GetTimelineAsset();

        const uint32_t id = params.value("id", 0u);
        bool found = false;
        for (const auto& t : tl.tracks) {
            if (t.id == id) { found = true; break; }
        }
        if (!found) {
            throw MakeError("not_found", "Track not found.", { { "id", id } });
        }

        tl.RemoveTrack(id);
        panel.MarkTimelineDirty();
        panel.ClearEditorSelection();

        return { { "deleted", id } };
    }

    json HandlePlayerEditorSetPlayhead(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        const int frameCount = panel.GetTimelineAsset().GetFrameCount();

        int frame = params.value("frame", 0);
        frame = ClampInt(frame, 0, frameCount > 0 ? frameCount : 0);
        panel.SetPlayheadFrame(frame);

        return { { "frame", frame } };
    }

    json HandlePlayerEditorTimelinePlay(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        const bool play = params.value("play", true);
        panel.SetTimelinePlaying(play);

        return {
            { "playing", panel.IsTimelinePlaying() },
            { "frame",   panel.GetPlayheadFrame() }
        };
    }

    json HandlePlayerEditorSavePrefab(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        auto& panel = editor->GetPlayerEditorPanel();

        // If path is specified, save directly without opening a dialog
        const std::string pathStr = params.value("path", std::string{});
        if (!pathStr.empty()) {
            const std::filesystem::path safePath = ResolveProjectPath(pathStr, PathAccess::WriteAsset, false);
            if (!PlayerEditorSession::SavePrefabDocumentToPath(panel, safePath.string())) {
                throw MakeError("save_failed", "Failed to save prefab to path.", { { "path", pathStr } });
            }
            return { { "saved", true }, { "path", ToGenericProjectPath(safePath) } };
        }

        const bool saveAs = params.value("saveAs", false);
        if (!PlayerEditorSession::SavePrefabDocument(panel, saveAs)) {
            throw MakeError("save_failed", "Failed to save prefab document.");
        }
        return { { "saved", true } };
    }

    // =========================================================
    // GameLoopEditor helpers
    // =========================================================

    std::string GameLoopNodeTypeToString(GameLoopNodeType t)
    {
        switch (t) {
        case GameLoopNodeType::Scene:  return "Scene";
        case GameLoopNodeType::State:  return "State";
        case GameLoopNodeType::Event:  return "Event";
        case GameLoopNodeType::Action: return "Action";
        case GameLoopNodeType::Battle: return "Battle";
        }
        return "State";
    }

    GameLoopNodeType GameLoopNodeTypeFromString(const std::string& s)
    {
        if (s == "Scene")  return GameLoopNodeType::Scene;
        if (s == "Event")  return GameLoopNodeType::Event;
        if (s == "Action") return GameLoopNodeType::Action;
        if (s == "Battle") return GameLoopNodeType::Battle;
        return GameLoopNodeType::State;
    }

    std::string GameFlowConditionTypeToString(GameFlowConditionType t)
    {
        switch (t) {
        case GameFlowConditionType::InputAction:    return "InputAction";
        case GameFlowConditionType::UIButtonClick:  return "UIButtonClick";
        case GameFlowConditionType::TimerElapsed:   return "TimerElapsed";
        case GameFlowConditionType::FlagEquals:     return "FlagEquals";
        case GameFlowConditionType::BattleResult:   return "BattleResult";
        case GameFlowConditionType::SceneLoaded:    return "SceneLoaded";
        default:                                    return "Event";
        }
    }

    GameFlowConditionType GameFlowConditionTypeFromString(const std::string& s)
    {
        if (s == "InputAction")   return GameFlowConditionType::InputAction;
        if (s == "UIButtonClick") return GameFlowConditionType::UIButtonClick;
        if (s == "TimerElapsed")  return GameFlowConditionType::TimerElapsed;
        if (s == "FlagEquals")    return GameFlowConditionType::FlagEquals;
        if (s == "BattleResult")  return GameFlowConditionType::BattleResult;
        if (s == "SceneLoaded")   return GameFlowConditionType::SceneLoaded;
        return GameFlowConditionType::Event;
    }

    std::string GameFlowActionTypeToString(GameFlowActionType t)
    {
        switch (t) {
        case GameFlowActionType::LoadScene:          return "LoadScene";
        case GameFlowActionType::SetCurrentNode:     return "SetCurrentNode";
        case GameFlowActionType::EmitEvent:          return "EmitEvent";
        case GameFlowActionType::SetFlag:            return "SetFlag";
        case GameFlowActionType::ClearFlag:          return "ClearFlag";
        case GameFlowActionType::StartBattleFlow:    return "StartBattleFlow";
        case GameFlowActionType::ResetBattleFlow:    return "ResetBattleFlow";
        case GameFlowActionType::Fade:               return "Fade";
        case GameFlowActionType::Wait:               return "Wait";
        case GameFlowActionType::ShowLoadingOverlay: return "ShowLoadingOverlay";
        case GameFlowActionType::HideLoadingOverlay: return "HideLoadingOverlay";
        }
        return "LoadScene";
    }

    GameFlowActionType GameFlowActionTypeFromString(const std::string& s)
    {
        if (s == "SetCurrentNode")     return GameFlowActionType::SetCurrentNode;
        if (s == "EmitEvent")          return GameFlowActionType::EmitEvent;
        if (s == "SetFlag")            return GameFlowActionType::SetFlag;
        if (s == "ClearFlag")          return GameFlowActionType::ClearFlag;
        if (s == "StartBattleFlow")    return GameFlowActionType::StartBattleFlow;
        if (s == "ResetBattleFlow")    return GameFlowActionType::ResetBattleFlow;
        if (s == "Fade")               return GameFlowActionType::Fade;
        if (s == "Wait")               return GameFlowActionType::Wait;
        if (s == "ShowLoadingOverlay") return GameFlowActionType::ShowLoadingOverlay;
        if (s == "HideLoadingOverlay") return GameFlowActionType::HideLoadingOverlay;
        return GameFlowActionType::LoadScene;
    }

    json GameFlowConditionToJson(const GameFlowCondition& c)
    {
        return {
            { "type",              GameFlowConditionTypeToString(c.type) },
            { "name",              c.name },
            { "value",             c.value },
            { "seconds",           c.seconds },
            { "expectedFlagValue", c.expectedFlagValue }
        };
    }

    GameFlowCondition GameFlowConditionFromJson(const json& j)
    {
        GameFlowCondition c;
        c.type              = GameFlowConditionTypeFromString(j.value("type", std::string("UIButtonClick")));
        c.name              = j.value("name",  std::string{});
        c.value             = j.value("value", std::string{});
        c.seconds           = j.value("seconds", 0.0f);
        c.expectedFlagValue = j.value("expectedFlagValue", true);
        return c;
    }

    json GameFlowActionToJson(const GameFlowAction& a)
    {
        return {
            { "type",      GameFlowActionTypeToString(a.type) },
            { "target",    a.target },
            { "value",     a.value },
            { "boolValue", a.boolValue },
            { "seconds",   a.seconds },
            { "message",   a.message }
        };
    }

    GameFlowAction GameFlowActionFromJson(const json& j)
    {
        GameFlowAction a;
        a.type      = GameFlowActionTypeFromString(j.value("type", std::string("LoadScene")));
        a.target    = j.value("target",    std::string{});
        a.value     = j.value("value",     std::string{});
        a.boolValue = j.value("boolValue", true);
        a.seconds   = j.value("seconds",   0.0f);
        a.message   = j.value("message",   std::string{});
        return a;
    }

    json GameLoopNodeToJson(const GameLoopNode& n)
    {
        return {
            { "id",        n.id },
            { "name",      n.name },
            { "type",      GameLoopNodeTypeToString(n.type) },
            { "scenePath", n.scenePath },
            { "graphPos",  json::array({ n.graphPos.x, n.graphPos.y }) }
        };
    }

    json GameLoopTransitionToJson(const GameLoopTransition& t)
    {
        json conditions = json::array();
        for (const auto& c : t.conditions) conditions.push_back(GameFlowConditionToJson(c));
        json actions = json::array();
        for (const auto& a : t.actions) actions.push_back(GameFlowActionToJson(a));
        return {
            { "id",                    t.id },
            { "fromNodeId",            t.fromNodeId },
            { "toNodeId",              t.toNodeId },
            { "name",                  t.name },
            { "priority",              t.priority },
            { "conditionMode",         t.conditionMode == GameFlowConditionMode::Any ? "Any" : "All" },
            { "loadingScenePath",      t.loadingScenePath },
            { "loadingMinimumSeconds", t.loadingMinimumSeconds },
            { "conditions",            std::move(conditions) },
            { "actions",               std::move(actions) }
        };
    }

    json GameLoopAssetToJson(const GameLoopAsset& asset)
    {
        json nodes = json::array();
        for (const auto& n : asset.nodes) nodes.push_back(GameLoopNodeToJson(n));
        json transitions = json::array();
        for (const auto& t : asset.transitions) transitions.push_back(GameLoopTransitionToJson(t));
        return {
            { "startNodeId", asset.startNodeId },
            { "nodes",       std::move(nodes) },
            { "transitions", std::move(transitions) }
        };
    }

    GameLoopEditorPanel& RequireGameLoopEditorPanel(EngineKernel& kernel)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        return editor->GetGameLoopEditorPanel();
    }

    // =========================================================
    // GameLoopEditor handlers
    // =========================================================

    json HandleGameLoopEditorOpen(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }

        std::filesystem::path assetPath;
        const std::string pathStr = params.value("path", std::string{});
        if (!pathStr.empty()) {
            assetPath = ResolveProjectPath(pathStr, PathAccess::ReadAsset, true);
        }

        editor->OpenGameLoopEditorFromAutomation(assetPath);

        const auto& panel = editor->GetGameLoopEditorPanel();
        return {
            { "active",      editor->IsGameLoopEditorActive() },
            { "currentPath", panel.GetCurrentPath().generic_string() },
            { "dirty",       panel.IsDirty() }
        };
    }

    json HandleGameLoopEditorGetStatus(EngineKernel& kernel)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const auto& panel = editor->GetGameLoopEditorPanel();
        const auto& result = panel.GetValidateResult();
        return {
            { "active",            editor->IsGameLoopEditorActive() },
            { "currentPath",       panel.GetCurrentPath().generic_string() },
            { "dirty",             panel.IsDirty() },
            { "selectedNodeId",       panel.GetSelectedNodeId() },
            { "selectedTransitionId", panel.GetSelectedTransitionId() },
            { "statusMessage",     panel.GetStatusMessage() },
            { "validationErrors",  result.ErrorCount() },
            { "validationWarnings",result.WarningCount() }
        };
    }

    json HandleGameLoopEditorGetAsset(EngineKernel& kernel)
    {
        return GameLoopAssetToJson(RequireGameLoopEditorPanel(kernel).GetAsset());
    }

    json HandleGameLoopEditorLoad(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const std::filesystem::path path = ResolveProjectPath(
            params.value("path", std::string{}), PathAccess::ReadAsset, true);

        editor->OpenGameLoopEditorFromAutomation(path);

        const auto& panel = editor->GetGameLoopEditorPanel();
        return {
            { "currentPath", panel.GetCurrentPath().generic_string() },
            { "nodeCount",   static_cast<int>(panel.GetAsset().nodes.size()) }
        };
    }

    json HandleGameFlowRegister(EngineKernel& kernel, const json& params)
    {
        const std::filesystem::path path = ResolveProjectPath(
            params.value("path", std::string{}), PathAccess::ReadAsset, true);
        GameLoopAsset asset;
        if (!asset.LoadFromFile(path)) {
            throw MakeError("load_failed", "Failed to load gameflow file.", { { "path", path.generic_string() } });
        }
        kernel.RegisterGameLoopAssetFromEditor(asset, path);
        return {
            { "registered", true },
            { "path",       ToGenericProjectPath(path) },
            { "nodeCount",  static_cast<int>(asset.nodes.size()) }
        };
    }

    json HandleGameLoopEditorSave(EngineKernel& kernel)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);
        panel.SaveAutomation();
        return {
            { "currentPath",  panel.GetCurrentPath().generic_string() },
            { "dirty",        panel.IsDirty() },
            { "statusMessage",panel.GetStatusMessage() }
        };
    }

    json HandleGameLoopEditorValidate(EngineKernel& kernel)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);
        panel.ValidateAutomation();
        const auto& result = panel.GetValidateResult();

        json messages = json::array();
        for (const auto& msg : result.messages) {
            const char* sev =
                msg.severity == GameLoopValidateSeverity::Error   ? "Error" :
                msg.severity == GameLoopValidateSeverity::Warning ? "Warning" : "Info";
            messages.push_back({ { "severity", sev }, { "message", msg.message } });
        }
        return {
            { "errors",   result.ErrorCount() },
            { "warnings", result.WarningCount() },
            { "messages", std::move(messages) }
        };
    }

    json HandleGameLoopEditorAddNode(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        editor->OpenGameLoopEditorFromAutomation();   // ensure visible

        auto& panel = editor->GetGameLoopEditorPanel();

        const std::string typeStr = params.value("type", std::string("State"));
        const GameLoopNodeType type = GameLoopNodeTypeFromString(typeStr);

        DirectX::XMFLOAT2 pos = { 200.0f, 200.0f };
        if (params.contains("position")) ReadFloat2(params["position"], pos);

        uint32_t newId = 0;
        if (type == GameLoopNodeType::Scene) {
            const std::string scenePath = params.value("scenePath", std::string{});
            if (scenePath.empty()) {
                throw MakeError("missing_param", "scenePath is required for Scene nodes.");
            }
            newId = panel.AddSceneNodeAutomation(scenePath, pos);
        }
        else {
            const std::string name = params.value("name", typeStr);
            newId = panel.AddFlowNodeAutomation(type, name, pos);
        }

        if (newId == 0) {
            throw MakeError("add_node_failed", "Failed to add node (invalid scenePath?).", {
                { "type", typeStr }
            });
        }

        const GameLoopNode* node = panel.GetAsset().FindNode(newId);
        return node ? GameLoopNodeToJson(*node) : json{ { "id", newId } };
    }

    json HandleGameLoopEditorSetNode(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);
        auto& asset = panel.GetAsset();

        const uint32_t id = params.value("id", 0u);
        GameLoopNode* node = asset.FindNode(id);
        if (!node) {
            throw MakeError("not_found", "Node not found.", { { "id", id } });
        }

        if (params.contains("name"))      node->name      = params["name"].get<std::string>();
        if (params.contains("scenePath")) node->scenePath = params["scenePath"].get<std::string>();
        if (params.contains("type"))      node->type      = GameLoopNodeTypeFromString(params["type"].get<std::string>());
        if (params.contains("position"))  ReadFloat2(params["position"], node->graphPos);

        panel.MarkDirty();
        panel.SelectNodeById(id);

        return GameLoopNodeToJson(*node);
    }

    json HandleGameLoopEditorDeleteNode(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);

        const uint32_t id = params.value("id", 0u);
        if (!panel.GetAsset().FindNode(id)) {
            throw MakeError("not_found", "Node not found.", { { "id", id } });
        }

        panel.DeleteNodeAutomation(id);
        return { { "deleted", id } };
    }

    json HandleGameLoopEditorAddTransition(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        editor->OpenGameLoopEditorFromAutomation();

        auto& panel = editor->GetGameLoopEditorPanel();
        const auto& asset = panel.GetAsset();

        const uint32_t from = params.value("fromNodeId", 0u);
        const uint32_t to   = params.value("toNodeId",   0u);
        if (!asset.FindNode(from)) {
            throw MakeError("not_found", "fromNodeId not found.", { { "fromNodeId", from } });
        }
        if (!asset.FindNode(to)) {
            throw MakeError("not_found", "toNodeId not found.", { { "toNodeId", to } });
        }

        const uint32_t newId = panel.AddTransitionAutomation(from, to);
        if (newId == 0) {
            throw MakeError("add_transition_failed", "Failed to add transition (same node?).");
        }

        // Apply optional overrides
        for (auto& t : panel.GetAsset().transitions) {
            if (t.id != newId) continue;
            if (params.contains("name"))                 t.name                  = params["name"].get<std::string>();
            if (params.contains("priority"))             t.priority              = params["priority"].get<int>();
            if (params.contains("conditionMode"))        t.conditionMode         = params["conditionMode"].get<std::string>() == "Any" ? GameFlowConditionMode::Any : GameFlowConditionMode::All;
            if (params.contains("loadingScenePath"))     t.loadingScenePath      = params["loadingScenePath"].get<std::string>();
            if (params.contains("loadingMinimumSeconds"))t.loadingMinimumSeconds = params["loadingMinimumSeconds"].get<float>();
            if (params.contains("conditions")) {
                t.conditions.clear();
                for (const auto& cj : params["conditions"]) t.conditions.push_back(GameFlowConditionFromJson(cj));
            }
            if (params.contains("actions")) {
                t.actions.clear();
                for (const auto& aj : params["actions"]) t.actions.push_back(GameFlowActionFromJson(aj));
            }
            panel.SelectTransitionById(newId);
            return GameLoopTransitionToJson(t);
        }
        return { { "id", newId } };
    }

    json HandleGameLoopEditorSetTransition(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);

        const uint32_t id = params.value("id", 0u);
        GameLoopTransition* t = nullptr;
        for (auto& tr : panel.GetAsset().transitions) {
            if (tr.id == id) { t = &tr; break; }
        }
        if (!t) {
            throw MakeError("not_found", "Transition not found.", { { "id", id } });
        }

        if (params.contains("name"))                  t->name                  = params["name"].get<std::string>();
        if (params.contains("priority"))              t->priority              = params["priority"].get<int>();
        if (params.contains("conditionMode"))         t->conditionMode         = params["conditionMode"].get<std::string>() == "Any" ? GameFlowConditionMode::Any : GameFlowConditionMode::All;
        if (params.contains("loadingScenePath"))      t->loadingScenePath      = params["loadingScenePath"].get<std::string>();
        if (params.contains("loadingMinimumSeconds")) t->loadingMinimumSeconds = params["loadingMinimumSeconds"].get<float>();
        if (params.contains("conditions")) {
            t->conditions.clear();
            for (const auto& cj : params["conditions"]) t->conditions.push_back(GameFlowConditionFromJson(cj));
        }
        if (params.contains("actions")) {
            t->actions.clear();
            for (const auto& aj : params["actions"]) t->actions.push_back(GameFlowActionFromJson(aj));
        }

        panel.MarkDirty();
        panel.SelectTransitionById(id);

        return GameLoopTransitionToJson(*t);
    }

    json HandleGameLoopEditorDeleteTransition(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);

        const uint32_t id = params.value("id", 0u);
        bool found = false;
        for (const auto& t : panel.GetAsset().transitions) {
            if (t.id == id) { found = true; break; }
        }
        if (!found) {
            throw MakeError("not_found", "Transition not found.", { { "id", id } });
        }

        panel.DeleteTransitionByIdAutomation(id);
        return { { "deleted", id } };
    }

    json HandleGameLoopEditorSelect(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);

        if (params.contains("nodeId")) {
            const uint32_t id = params["nodeId"].get<uint32_t>();
            panel.SelectNodeById(id);
            return { { "selectedNodeId", id } };
        }
        if (params.contains("transitionId")) {
            const uint32_t id = params["transitionId"].get<uint32_t>();
            panel.SelectTransitionById(id);
            return { { "selectedTransitionId", id } };
        }
        panel.ClearSelection();
        return { { "cleared", true } };
    }

    json HandleGameLoopEditorSetStartNode(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);
        auto& asset = panel.GetAsset();

        const uint32_t id = params.value("id", 0u);
        if (!asset.FindNode(id)) {
            throw MakeError("not_found", "Node not found.", { { "id", id } });
        }

        asset.startNodeId = id;
        panel.MarkDirty();
        panel.SelectNodeById(id);

        return { { "startNodeId", id } };
    }

    // =========================================================
    // PlayerEditor: Socket / Collider / Animator / InputMapping helpers
    // =========================================================

    json SocketToJson(int index, const NodeSocket& s)
    {
        return {
            { "index",          index },
            { "name",           s.name },
            { "parentBoneName", s.parentBoneName },
            { "offsetPos",      json::array({ s.offsetPos.x,    s.offsetPos.y,    s.offsetPos.z }) },
            { "offsetRotDeg",   json::array({ s.offsetRotDeg.x, s.offsetRotDeg.y, s.offsetRotDeg.z }) },
            { "offsetScale",    json::array({ s.offsetScale.x,  s.offsetScale.y,  s.offsetScale.z }) }
        };
    }

    std::string ColliderShapeToString(ColliderShape s)
    {
        switch (s) {
        case ColliderShape::Capsule: return "Capsule";
        case ColliderShape::Box:     return "Box";
        default:                     return "Sphere";
        }
    }

    ColliderShape ColliderShapeFromString(const std::string& s)
    {
        if (s == "Capsule") return ColliderShape::Capsule;
        if (s == "Box")     return ColliderShape::Box;
        return ColliderShape::Sphere;
    }

    std::string ColliderAttributeToString(ColliderAttribute a)
    {
        return a == ColliderAttribute::Attack ? "Attack" : "Body";
    }

    ColliderAttribute ColliderAttributeFromString(const std::string& s)
    {
        return s == "Attack" ? ColliderAttribute::Attack : ColliderAttribute::Body;
    }

    json ColliderElementToJson(int index, const ColliderComponent::Element& e)
    {
        return {
            { "index",       index },
            { "type",        ColliderShapeToString(e.type) },
            { "attribute",   ColliderAttributeToString(e.attribute) },
            { "enabled",     e.enabled },
            { "nodeIndex",   e.nodeIndex },
            { "offsetLocal", json::array({ e.offsetLocal.x, e.offsetLocal.y, e.offsetLocal.z }) },
            { "radius",      e.radius },
            { "height",      e.height },
            { "size",        json::array({ e.size.x, e.size.y, e.size.z }) },
            { "hitVfxPath",  e.hitVfxPath },
            { "hitSfxPath",  e.hitSfxPath }
        };
    }

    std::string ActionTriggerToString(ActionTriggerType t)
    {
        switch (t) {
        case ActionTriggerType::Released:  return "Released";
        case ActionTriggerType::Held:      return "Held";
        case ActionTriggerType::DoubleTap: return "DoubleTap";
        default:                           return "Pressed";
        }
    }

    ActionTriggerType ActionTriggerFromString(const std::string& s)
    {
        if (s == "Released")  return ActionTriggerType::Released;
        if (s == "Held")      return ActionTriggerType::Held;
        if (s == "DoubleTap") return ActionTriggerType::DoubleTap;
        return ActionTriggerType::Pressed;
    }

    json ActionBindingToJson(int index, const ActionBinding& b)
    {
        return {
            { "index",        index },
            { "actionName",   b.actionName },
            { "scancode",     b.scancode },
            { "mouseButton",  b.mouseButton },
            { "gamepadButton",b.gamepadButton },
            { "trigger",      ActionTriggerToString(b.trigger) }
        };
    }

    json AxisBindingToJson(int index, const AxisBinding& b)
    {
        return {
            { "index",       index },
            { "axisName",    b.axisName },
            { "positiveKey", b.positiveKey },
            { "negativeKey", b.negativeKey },
            { "gamepadAxis", b.gamepadAxis },
            { "deadzone",    b.deadzone },
            { "sensitivity", b.sensitivity }
        };
    }

    json InputMapToJson(const InputActionMapAsset& m)
    {
        json actions = json::array();
        for (int i = 0; i < static_cast<int>(m.actions.size()); ++i)
            actions.push_back(ActionBindingToJson(i, m.actions[i]));
        json axes = json::array();
        for (int i = 0; i < static_cast<int>(m.axes.size()); ++i)
            axes.push_back(AxisBindingToJson(i, m.axes[i]));
        return {
            { "name",                m.name },
            { "contextCategory",     m.contextCategory },
            { "holdThresholdFrames", m.holdThresholdFrames },
            { "doubleTapGapFrames",  m.doubleTapGapFrames },
            { "actions",             std::move(actions) },
            { "axes",                std::move(axes) }
        };
    }

    // =========================================================
    // PlayerEditor: Socket handlers
    // =========================================================

    json HandlePlayerEditorGetSockets(EngineKernel& kernel)
    {
        const auto& panel = RequirePlayerEditorPanel(kernel);
        json list = json::array();
        const auto& sockets = panel.GetSockets();
        for (int i = 0; i < static_cast<int>(sockets.size()); ++i)
            list.push_back(SocketToJson(i, sockets[i]));
        return { { "sockets", std::move(list) } };
    }

    json HandlePlayerEditorAddSocket(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sockets = panel.GetSocketsMutable();

        NodeSocket s;
        s.name           = params.value("name", std::string("Socket"));
        s.parentBoneName = params.value("parentBoneName", std::string{});
        if (params.contains("offsetPos"))    ReadFloat3(params["offsetPos"],    s.offsetPos);
        if (params.contains("offsetRotDeg")) ReadFloat3(params["offsetRotDeg"], s.offsetRotDeg);
        if (params.contains("offsetScale"))  ReadFloat3(params["offsetScale"],  s.offsetScale);

        sockets.push_back(std::move(s));
        const int newIdx = static_cast<int>(sockets.size()) - 1;

        panel.MarkSocketDirty();
        panel.SelectSocket(newIdx);

        return SocketToJson(newIdx, sockets[newIdx]);
    }

    json HandlePlayerEditorSetSocket(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sockets = panel.GetSocketsMutable();

        const int idx = params.value("index", -1);
        if (idx < 0 || idx >= static_cast<int>(sockets.size())) {
            throw MakeError("out_of_range", "Socket index out of range.", { { "index", idx } });
        }
        auto& s = sockets[idx];
        if (params.contains("name"))           s.name           = params["name"].get<std::string>();
        if (params.contains("parentBoneName")) s.parentBoneName = params["parentBoneName"].get<std::string>();
        if (params.contains("offsetPos"))      ReadFloat3(params["offsetPos"],    s.offsetPos);
        if (params.contains("offsetRotDeg"))   ReadFloat3(params["offsetRotDeg"], s.offsetRotDeg);
        if (params.contains("offsetScale"))    ReadFloat3(params["offsetScale"],  s.offsetScale);

        panel.MarkSocketDirty();
        panel.SelectSocket(idx);

        return SocketToJson(idx, s);
    }

    json HandlePlayerEditorDeleteSocket(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& sockets = panel.GetSocketsMutable();

        const int idx = params.value("index", -1);
        if (idx < 0 || idx >= static_cast<int>(sockets.size())) {
            throw MakeError("out_of_range", "Socket index out of range.", { { "index", idx } });
        }

        sockets.erase(sockets.begin() + idx);
        panel.MarkSocketDirty();
        panel.ClearEditorSelection();

        return { { "deleted", idx }, { "remaining", static_cast<int>(sockets.size()) } };
    }

    // =========================================================
    // PlayerEditor: Collider handlers
    // =========================================================

    json HandlePlayerEditorGetColliders(EngineKernel& kernel)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);

        if (Entity::IsNull(panel.GetPreviewEntity())) {
            return { { "colliders", json::array() }, { "note", "No preview entity loaded." } };
        }
        const ColliderComponent* comp = panel.GetPreviewColliderForAutomation(false);
        json list = json::array();
        if (comp) {
            for (int i = 0; i < static_cast<int>(comp->elements.size()); ++i)
                list.push_back(ColliderElementToJson(i, comp->elements[i]));
        }
        return { { "colliders", std::move(list) } };
    }

    json HandlePlayerEditorAddCollider(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);

        if (Entity::IsNull(panel.GetPreviewEntity())) {
            throw MakeError("no_preview_entity", "Load a model into the Player Editor first.");
        }

        const ColliderAttribute attr = ColliderAttributeFromString(
            params.value("attribute", std::string("Body")));
        panel.AddColliderAutomation(attr);   // creates component if needed, appends element, selects it

        ColliderComponent* comp = panel.GetPreviewColliderForAutomation(false);
        if (!comp || comp->elements.empty()) {
            throw MakeError("add_collider_failed", "Collider element could not be created.");
        }

        const int newIdx = static_cast<int>(comp->elements.size()) - 1;
        auto& e = comp->elements[newIdx];

        // Apply optional overrides AFTER the element exists
        if (params.contains("type"))        e.type      = ColliderShapeFromString(params["type"].get<std::string>());
        if (params.contains("nodeIndex"))   e.nodeIndex = params["nodeIndex"].get<int>();
        if (params.contains("radius"))      e.radius    = params["radius"].get<float>();
        if (params.contains("height"))      e.height    = params["height"].get<float>();
        if (params.contains("enabled"))     e.enabled   = params["enabled"].get<bool>();
        if (params.contains("hitVfxPath"))  e.hitVfxPath = params["hitVfxPath"].get<std::string>();
        if (params.contains("hitSfxPath"))  e.hitSfxPath = params["hitSfxPath"].get<std::string>();
        if (params.contains("offsetLocal")) {
            DirectX::XMFLOAT3 v{};
            ReadFloat3(params["offsetLocal"], v);
            e.offsetLocal = { v.x, v.y, v.z };
        }
        if (params.contains("size")) {
            DirectX::XMFLOAT3 v{};
            ReadFloat3(params["size"], v);
            e.size = { v.x, v.y, v.z };
        }

        panel.MarkColliderDirty();
        return ColliderElementToJson(newIdx, e);
    }

    json HandlePlayerEditorSetCollider(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        ColliderComponent* comp = panel.GetPreviewColliderForAutomation(false);
        if (!comp) {
            throw MakeError("not_found", "No ColliderComponent on the preview entity.");
        }

        const int idx = params.value("index", -1);
        if (idx < 0 || idx >= static_cast<int>(comp->elements.size())) {
            throw MakeError("out_of_range", "Collider index out of range.", { { "index", idx } });
        }
        auto& e = comp->elements[idx];

        if (params.contains("type"))        e.type      = ColliderShapeFromString(params["type"].get<std::string>());
        if (params.contains("attribute"))   e.attribute = ColliderAttributeFromString(params["attribute"].get<std::string>());
        if (params.contains("enabled"))     e.enabled   = params["enabled"].get<bool>();
        if (params.contains("nodeIndex"))   e.nodeIndex = params["nodeIndex"].get<int>();
        if (params.contains("radius"))      e.radius    = params["radius"].get<float>();
        if (params.contains("height"))      e.height    = params["height"].get<float>();
        if (params.contains("hitVfxPath"))  e.hitVfxPath = params["hitVfxPath"].get<std::string>();
        if (params.contains("hitSfxPath"))  e.hitSfxPath = params["hitSfxPath"].get<std::string>();
        if (params.contains("offsetLocal")) {
            DirectX::XMFLOAT3 v{};
            ReadFloat3(params["offsetLocal"], v);
            e.offsetLocal = { v.x, v.y, v.z };
        }
        if (params.contains("size")) {
            DirectX::XMFLOAT3 v{};
            ReadFloat3(params["size"], v);
            e.size = { v.x, v.y, v.z };
        }

        panel.MarkColliderDirty();
        panel.SelectColliderAutomation(idx);

        return ColliderElementToJson(idx, e);
    }

    json HandlePlayerEditorDeleteCollider(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        ColliderComponent* comp = panel.GetPreviewColliderForAutomation(false);
        if (!comp) {
            throw MakeError("not_found", "No ColliderComponent on the preview entity.");
        }

        const int idx = params.value("index", -1);
        if (idx < 0 || idx >= static_cast<int>(comp->elements.size())) {
            throw MakeError("out_of_range", "Collider index out of range.", { { "index", idx } });
        }

        comp->elements.erase(comp->elements.begin() + idx);
        panel.MarkColliderDirty();
        panel.ClearEditorSelection();

        return { { "deleted", idx }, { "remaining", static_cast<int>(comp->elements.size()) } };
    }

    // =========================================================
    // PlayerEditor: Animator handlers
    // =========================================================

    json HandlePlayerEditorGetAnimations(EngineKernel& kernel)
    {
        const auto& panel = RequirePlayerEditorPanel(kernel);
        const Model* model = panel.GetPreviewModel();
        if (!model) {
            return { { "animations", json::array() }, { "note", "No model loaded." } };
        }

        const auto& anims = model->GetAnimations();
        json list = json::array();
        for (int i = 0; i < static_cast<int>(anims.size()); ++i) {
            list.push_back({
                { "index",         i },
                { "name",          anims[i].name },
                { "secondsLength", anims[i].secondsLength }
            });
        }
        return {
            { "animations",     std::move(list) },
            { "selectedIndex",  panel.GetSelectedAnimationIndex() }
        };
    }

    json HandlePlayerEditorSetAnimator(EngineKernel& kernel, const json& params)
    {
        auto* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        auto& panel = editor->GetPlayerEditorPanel();

        const int idx = params.value("animationIndex", panel.GetSelectedAnimationIndex());

        const Model* model = panel.GetPreviewModel();
        if (model) {
            const auto& anims = model->GetAnimations();
            if (idx < 0 || idx >= static_cast<int>(anims.size())) {
                throw MakeError("out_of_range", "animationIndex out of range.",
                    { { "animationIndex", idx }, { "count", static_cast<int>(anims.size()) } });
            }
        }

        panel.SetSelectedAnimationIndex(idx);

        // Sync preview timeline so the UI updates immediately
        PlayerEditorSession::SyncPreviewTimelinePlayback(panel);

        const std::string animName = (model && idx >= 0 && idx < static_cast<int>(model->GetAnimations().size()))
                                     ? model->GetAnimations()[idx].name : std::string{};
        return {
            { "animationIndex", idx },
            { "animationName",  animName }
        };
    }

    // =========================================================
    // PlayerEditor: Input Mapping handlers
    // =========================================================

    json HandlePlayerEditorGetInputMap(EngineKernel& kernel)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        return InputMapToJson(panel.GetInputMappingTab().GetEditingMap());
    }

    json HandlePlayerEditorAddActionBinding(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& tab   = panel.GetInputMappingTab();
        auto& map   = tab.GetEditingMapMutable();

        ActionBinding b;
        b.actionName   = params.value("actionName",    std::string("NewAction"));
        b.scancode     = params.value("scancode",      0u);
        b.mouseButton  = params.value("mouseButton",   static_cast<uint8_t>(0xFF));
        b.gamepadButton= params.value("gamepadButton", static_cast<uint8_t>(0xFF));
        b.trigger      = ActionTriggerFromString(params.value("trigger", std::string("Pressed")));

        map.actions.push_back(std::move(b));
        tab.MarkDirty();
        panel.SetWorkbenchActiveTab(3); // Input タブを前面に

        const int newIdx = static_cast<int>(map.actions.size()) - 1;
        return ActionBindingToJson(newIdx, map.actions[newIdx]);
    }

    json HandlePlayerEditorSetActionBinding(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& tab   = panel.GetInputMappingTab();
        auto& map   = tab.GetEditingMapMutable();

        const int idx = params.value("index", -1);
        if (idx < 0 || idx >= static_cast<int>(map.actions.size())) {
            throw MakeError("out_of_range", "Action binding index out of range.", { { "index", idx } });
        }
        auto& b = map.actions[idx];
        if (params.contains("actionName"))    b.actionName    = params["actionName"].get<std::string>();
        if (params.contains("scancode"))      b.scancode      = params["scancode"].get<uint32_t>();
        if (params.contains("mouseButton"))   b.mouseButton   = params["mouseButton"].get<uint8_t>();
        if (params.contains("gamepadButton")) b.gamepadButton = params["gamepadButton"].get<uint8_t>();
        if (params.contains("trigger"))       b.trigger       = ActionTriggerFromString(params["trigger"].get<std::string>());

        tab.MarkDirty();
        panel.SetWorkbenchActiveTab(3);

        return ActionBindingToJson(idx, b);
    }

    json HandlePlayerEditorDeleteActionBinding(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& tab   = panel.GetInputMappingTab();
        auto& map   = tab.GetEditingMapMutable();

        const int idx = params.value("index", -1);
        if (idx < 0 || idx >= static_cast<int>(map.actions.size())) {
            throw MakeError("out_of_range", "Action binding index out of range.", { { "index", idx } });
        }

        map.actions.erase(map.actions.begin() + idx);
        tab.MarkDirty();

        return { { "deleted", idx }, { "remaining", static_cast<int>(map.actions.size()) } };
    }

    json HandlePlayerEditorAddAxisBinding(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& tab   = panel.GetInputMappingTab();
        auto& map   = tab.GetEditingMapMutable();

        AxisBinding b;
        b.axisName    = params.value("axisName",    std::string("NewAxis"));
        b.positiveKey = params.value("positiveKey", 0u);
        b.negativeKey = params.value("negativeKey", 0u);
        b.gamepadAxis = params.value("gamepadAxis", static_cast<uint8_t>(0xFF));
        b.deadzone    = params.value("deadzone",    0.1f);
        b.sensitivity = params.value("sensitivity", 1.0f);

        map.axes.push_back(std::move(b));
        tab.MarkDirty();
        panel.SetWorkbenchActiveTab(3);

        const int newIdx = static_cast<int>(map.axes.size()) - 1;
        return AxisBindingToJson(newIdx, map.axes[newIdx]);
    }

    json HandlePlayerEditorSetAxisBinding(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& tab   = panel.GetInputMappingTab();
        auto& map   = tab.GetEditingMapMutable();

        const int idx = params.value("index", -1);
        if (idx < 0 || idx >= static_cast<int>(map.axes.size())) {
            throw MakeError("out_of_range", "Axis binding index out of range.", { { "index", idx } });
        }
        auto& b = map.axes[idx];
        if (params.contains("axisName"))    b.axisName    = params["axisName"].get<std::string>();
        if (params.contains("positiveKey")) b.positiveKey = params["positiveKey"].get<uint32_t>();
        if (params.contains("negativeKey")) b.negativeKey = params["negativeKey"].get<uint32_t>();
        if (params.contains("gamepadAxis")) b.gamepadAxis = params["gamepadAxis"].get<uint8_t>();
        if (params.contains("deadzone"))    b.deadzone    = params["deadzone"].get<float>();
        if (params.contains("sensitivity")) b.sensitivity = params["sensitivity"].get<float>();

        tab.MarkDirty();
        panel.SetWorkbenchActiveTab(3);

        return AxisBindingToJson(idx, b);
    }

    json HandlePlayerEditorDeleteAxisBinding(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequirePlayerEditorPanel(kernel);
        auto& tab   = panel.GetInputMappingTab();
        auto& map   = tab.GetEditingMapMutable();

        const int idx = params.value("index", -1);
        if (idx < 0 || idx >= static_cast<int>(map.axes.size())) {
            throw MakeError("out_of_range", "Axis binding index out of range.", { { "index", idx } });
        }

        map.axes.erase(map.axes.begin() + idx);
        tab.MarkDirty();

        return { { "deleted", idx }, { "remaining", static_cast<int>(map.axes.size()) } };
    }

    // =========================================================
    // GameLoopEditor: reverse_transition / fit_graph handlers
    // =========================================================

    json HandleGameLoopEditorReverseTransition(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);

        const uint32_t id = params.value("id", 0u);
        bool found = false;
        for (const auto& t : panel.GetAsset().transitions)
            if (t.id == id) { found = true; break; }
        if (!found) {
            throw MakeError("not_found", "Transition not found.", { { "id", id } });
        }

        panel.ReverseTransitionByIdAutomation(id);

        // Find it again (from/to have swapped)
        for (const auto& t : panel.GetAsset().transitions) {
            if (t.id == id) return GameLoopTransitionToJson(t);
        }
        return { { "id", id } };
    }

    json HandleGameLoopEditorFitGraph(EngineKernel& kernel)
    {
        auto& panel = RequireGameLoopEditorPanel(kernel);
        panel.RequestFitGraph();
        return { { "fitRequested", true } };
    }

    // =========================================================
    // Scene View handlers
    // =========================================================

    EditorLayer& RequireEditorLayer(EngineKernel& kernel)
    {
        EditorLayer* ed = kernel.GetEditorLayer();
        if (!ed) throw MakeError("editor_unavailable", "EditorLayer is not available.");
        return *ed;
    }

    std::string ShadingModeToString(EditorLayer::SceneShadingMode m)
    {
        switch (m) {
        case EditorLayer::SceneShadingMode::Unlit:     return "unlit";
        case EditorLayer::SceneShadingMode::Wireframe: return "wireframe";
        default:                                       return "lit";
        }
    }
    EditorLayer::SceneShadingMode ShadingModeFromString(const std::string& s)
    {
        const std::string lower = ToLowerCopy(s);
        if (lower == "unlit")     return EditorLayer::SceneShadingMode::Unlit;
        if (lower == "wireframe") return EditorLayer::SceneShadingMode::Wireframe;
        return EditorLayer::SceneShadingMode::Lit;
    }
    std::string GizmoOpToString(EditorLayer::GizmoOperation op)
    {
        switch (op) {
        case EditorLayer::GizmoOperation::Rotate: return "rotate";
        case EditorLayer::GizmoOperation::Scale:  return "scale";
        default:                                  return "translate";
        }
    }
    EditorLayer::GizmoOperation GizmoOpFromString(const std::string& s)
    {
        const std::string lower = ToLowerCopy(s);
        if (lower == "rotate") return EditorLayer::GizmoOperation::Rotate;
        if (lower == "scale")  return EditorLayer::GizmoOperation::Scale;
        return EditorLayer::GizmoOperation::Translate;
    }
    std::string GizmoSpaceToString(EditorLayer::GizmoSpace sp)
    {
        return sp == EditorLayer::GizmoSpace::World ? "world" : "local";
    }
    EditorLayer::GizmoSpace GizmoSpaceFromString(const std::string& s)
    {
        return ToLowerCopy(s) == "world" ? EditorLayer::GizmoSpace::World : EditorLayer::GizmoSpace::Local;
    }
    EditorLayer::SceneViewMode SceneViewModeFromString(const std::string& s)
    {
        return ToLowerCopy(s) == "2d" ? EditorLayer::SceneViewMode::Mode2D : EditorLayer::SceneViewMode::Mode3D;
    }

    json HandleSceneViewGetState(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        const auto pos = ed.GetEditorCameraPosition();
        return {
            { "mode",          SceneViewModeToString(ed.GetSceneViewMode()) },
            { "shadingMode",   ShadingModeToString(ed.GetSceneShadingMode()) },
            { "gizmoOperation",GizmoOpToString(ed.GetGizmoOperation()) },
            { "gizmoSpace",    GizmoSpaceToString(ed.GetGizmoSpace()) },
            { "cameraPosition",json::array({ pos.x, pos.y, pos.z }) },
            { "cameraYaw",     ed.GetEditorCameraYaw() },
            { "cameraPitch",   ed.GetEditorCameraPitch() },
            { "cameraMoveSpeed", ed.GetCameraMoveSpeed() },
            { "visibility", {
                { "gizmo",           ed.GetShowSceneGizmo() },
                { "stats",           ed.GetShowSceneStats() },
                { "selectionOutline",ed.GetShowSelectionOutline() },
                { "lightIcons",      ed.GetShowLightIcons() },
                { "cameraIcons",     ed.GetShowCameraIcons() },
                { "bounds",          ed.GetShowBounds() },
                { "collision",       ed.GetShowCollision() },
                { "inputDebug",      ed.GetShowInputDebug() }
            }}
        };
    }

    json HandleSceneViewSetCamera(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        auto pos = ed.GetEditorCameraPosition();
        float yaw   = ed.GetEditorCameraYaw();
        float pitch = ed.GetEditorCameraPitch();
        if (params.contains("position") && params["position"].is_array() && params["position"].size() >= 3)
            pos = { params["position"][0].get<float>(), params["position"][1].get<float>(), params["position"][2].get<float>() };
        if (params.contains("yaw"))   yaw   = params["yaw"].get<float>();
        if (params.contains("pitch")) pitch = params["pitch"].get<float>();
        ed.SetEditorCameraTransformAutomation(pos, yaw, pitch);
        if (params.contains("moveSpeed")) ed.SetCameraMoveSpeedAutomation(params["moveSpeed"].get<float>());
        return { { "cameraPosition", json::array({ pos.x, pos.y, pos.z }) }, { "yaw", yaw }, { "pitch", pitch } };
    }

    json HandleSceneViewSetLookAt(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        DirectX::XMFLOAT3 pos{}, target{};
        if (!params.contains("position") || !ReadFloat3(params["position"], pos))
            throw MakeError("missing_param", "'position' [x,y,z] is required.");
        if (!params.contains("target") || !ReadFloat3(params["target"], target))
            throw MakeError("missing_param", "'target' [x,y,z] is required.");
        ed.SetEditorCameraLookAt(pos, target);
        return { { "ok", true } };
    }

    json HandleSceneViewSetGizmoOperation(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        const std::string op = params.value("operation", std::string("translate"));
        ed.SetGizmoOperationAutomation(GizmoOpFromString(op));
        return { { "operation", GizmoOpToString(ed.GetGizmoOperation()) } };
    }

    json HandleSceneViewSetGizmoSpace(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        const std::string sp = params.value("space", std::string("local"));
        ed.SetGizmoSpaceAutomation(GizmoSpaceFromString(sp));
        return { { "space", GizmoSpaceToString(ed.GetGizmoSpace()) } };
    }

    json HandleSceneViewSetShadingMode(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        ed.SetSceneShadingModeAutomation(ShadingModeFromString(params.value("mode", std::string("lit"))));
        return { { "shadingMode", ShadingModeToString(ed.GetSceneShadingMode()) } };
    }

    json HandleSceneViewSetMode(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        ed.SetSceneViewMode(SceneViewModeFromString(params.value("mode", std::string("3d"))));
        return { { "mode", SceneViewModeToString(ed.GetSceneViewMode()) } };
    }

    json HandleSceneViewSetVisibility(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        if (params.contains("gizmo"))            ed.SetShowSceneGizmo(params["gizmo"].get<bool>());
        if (params.contains("stats"))            ed.SetShowSceneStats(params["stats"].get<bool>());
        if (params.contains("selectionOutline")) ed.SetShowSelectionOutline(params["selectionOutline"].get<bool>());
        if (params.contains("lightIcons"))       ed.SetShowLightIcons(params["lightIcons"].get<bool>());
        if (params.contains("cameraIcons"))      ed.SetShowCameraIcons(params["cameraIcons"].get<bool>());
        if (params.contains("bounds"))           ed.SetShowBounds(params["bounds"].get<bool>());
        if (params.contains("collision"))        ed.SetShowCollision(params["collision"].get<bool>());
        if (params.contains("inputDebug"))       ed.SetShowInputDebug(params["inputDebug"].get<bool>());
        return { { "ok", true } };
    }

    json HandleSceneViewBookmark(EngineKernel& kernel, const json& params, bool save)
    {
        auto& ed = RequireEditorLayer(kernel);
        const int slot = std::clamp(params.value("slot", 0), 0, 2);
        if (save) ed.SaveCameraBookmarkAutomation(slot);
        else      ed.LoadCameraBookmarkAutomation(slot);
        return { { "slot", slot }, { "action", save ? "saved" : "loaded" } };
    }

    // =========================================================
    // Lighting / PostFX handlers
    // =========================================================

    EntityID FindEntityWithComponent_Environment(Registry& registry)
    {
        for (Archetype* arch : registry.GetAllArchetypes()) {
            if (!arch->GetSignature().test(TypeManager::GetComponentTypeID<EnvironmentComponent>())) continue;
            for (EntityID e : arch->GetEntities()) { if (registry.IsAlive(e)) return e; }
        }
        return Entity::NULL_ID;
    }
    EntityID FindEntityWithComponent_PostEffect(Registry& registry)
    {
        for (Archetype* arch : registry.GetAllArchetypes()) {
            if (!arch->GetSignature().test(TypeManager::GetComponentTypeID<PostEffectComponent>())) continue;
            for (EntityID e : arch->GetEntities()) { if (registry.IsAlive(e)) return e; }
        }
        return Entity::NULL_ID;
    }

    json EnvironmentToJson(const EnvironmentComponent& c)
    {
        return {
            { "enableSkybox",     c.enableSkybox },
            { "skyboxPath",       c.skyboxPath },
            { "diffuseIBLPath",   c.diffuseIBLPath },
            { "specularIBLPath",  c.specularIBLPath }
        };
    }
    json PostEffectToJson(const PostEffectComponent& c)
    {
        return {
            { "enableComputeCulling",  c.enableComputeCulling },
            { "enableAsyncCompute",    c.enableAsyncCompute },
            { "enableGTAO",            c.enableGTAO },
            { "enableSSGI",            c.enableSSGI },
            { "enableSSR",             c.enableSSR },
            { "enableVolumetricFog",   c.enableVolumetricFog },
            { "enableBloom",           c.enableBloom },
            { "enableColorFilter",     c.enableColorFilter },
            { "enableMotionBlur",      c.enableMotionBlur },
            { "luminanceLowerEdge",    c.luminanceLowerEdge },
            { "luminanceHigherEdge",   c.luminanceHigherEdge },
            { "bloomIntensity",        c.bloomIntensity },
            { "gaussianSigma",         c.gaussianSigma },
            { "exposure",              c.exposure },
            { "monoBlend",             c.monoBlend },
            { "hueShift",              c.hueShift },
            { "flashAmount",           c.flashAmount },
            { "vignetteAmount",        c.vignetteAmount },
            { "enableDoF",             c.enableDoF },
            { "focusDistance",         c.focusDistance },
            { "focusRange",            c.focusRange },
            { "bokehRadius",           c.bokehRadius },
            { "motionBlurIntensity",   c.motionBlurIntensity },
            { "motionBlurSamples",     c.motionBlurSamples }
        };
    }

    json HandleLightingOpen(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        ed.OpenLightingWindowFromAutomation();
        return { { "open", true } };
    }

    json HandleLightingGet(Registry& registry)
    {
        json result;
        EntityID envEnt = FindEntityWithComponent_Environment(registry);
        if (!Entity::IsNull(envEnt))
            result["environment"] = EnvironmentToJson(*registry.GetComponent<EnvironmentComponent>(envEnt));
        else
            result["environment"] = nullptr;
        EntityID pfxEnt = FindEntityWithComponent_PostEffect(registry);
        if (!Entity::IsNull(pfxEnt))
            result["postEffects"] = PostEffectToJson(*registry.GetComponent<PostEffectComponent>(pfxEnt));
        else
            result["postEffects"] = nullptr;
        return result;
    }

    json HandleLightingSetEnvironment(Registry& registry, const json& params)
    {
        EntityID ent = FindEntityWithComponent_Environment(registry);
        if (Entity::IsNull(ent)) throw MakeError("entity_not_found", "No entity with EnvironmentComponent found.");
        auto* c = registry.GetComponent<EnvironmentComponent>(ent);
        if (params.contains("enableSkybox"))    c->enableSkybox    = params["enableSkybox"].get<bool>();
        if (params.contains("skyboxPath"))      c->skyboxPath      = params["skyboxPath"].get<std::string>();
        if (params.contains("diffuseIBLPath"))  c->diffuseIBLPath  = params["diffuseIBLPath"].get<std::string>();
        if (params.contains("specularIBLPath")) c->specularIBLPath = params["specularIBLPath"].get<std::string>();
        MarkEntityEdited(registry, ent);
        return { { "environment", EnvironmentToJson(*c) } };
    }

    json HandleLightingSetPostEffects(Registry& registry, const json& params)
    {
        EntityID ent = FindEntityWithComponent_PostEffect(registry);
        if (Entity::IsNull(ent)) throw MakeError("entity_not_found", "No entity with PostEffectComponent found.");
        auto* c = registry.GetComponent<PostEffectComponent>(ent);
        if (params.contains("enableComputeCulling"))  c->enableComputeCulling  = params["enableComputeCulling"].get<bool>();
        if (params.contains("enableAsyncCompute"))    c->enableAsyncCompute    = params["enableAsyncCompute"].get<bool>();
        if (params.contains("enableGTAO"))            c->enableGTAO            = params["enableGTAO"].get<bool>();
        if (params.contains("enableSSGI"))            c->enableSSGI            = params["enableSSGI"].get<bool>();
        if (params.contains("enableSSR"))             c->enableSSR             = params["enableSSR"].get<bool>();
        if (params.contains("enableVolumetricFog"))   c->enableVolumetricFog   = params["enableVolumetricFog"].get<bool>();
        if (params.contains("enableBloom"))           c->enableBloom           = params["enableBloom"].get<bool>();
        if (params.contains("enableColorFilter"))     c->enableColorFilter     = params["enableColorFilter"].get<bool>();
        if (params.contains("enableMotionBlur"))      c->enableMotionBlur      = params["enableMotionBlur"].get<bool>();
        if (params.contains("luminanceLowerEdge"))    c->luminanceLowerEdge    = params["luminanceLowerEdge"].get<float>();
        if (params.contains("luminanceHigherEdge"))   c->luminanceHigherEdge   = params["luminanceHigherEdge"].get<float>();
        if (params.contains("bloomIntensity"))        c->bloomIntensity        = params["bloomIntensity"].get<float>();
        if (params.contains("gaussianSigma"))         c->gaussianSigma         = params["gaussianSigma"].get<float>();
        if (params.contains("exposure"))              c->exposure              = params["exposure"].get<float>();
        if (params.contains("monoBlend"))             c->monoBlend             = params["monoBlend"].get<float>();
        if (params.contains("hueShift"))              c->hueShift              = params["hueShift"].get<float>();
        if (params.contains("flashAmount"))           c->flashAmount           = params["flashAmount"].get<float>();
        if (params.contains("vignetteAmount"))        c->vignetteAmount        = params["vignetteAmount"].get<float>();
        if (params.contains("enableDoF"))             c->enableDoF             = params["enableDoF"].get<bool>();
        if (params.contains("focusDistance"))         c->focusDistance         = params["focusDistance"].get<float>();
        if (params.contains("focusRange"))            c->focusRange            = params["focusRange"].get<float>();
        if (params.contains("bokehRadius"))           c->bokehRadius           = params["bokehRadius"].get<float>();
        if (params.contains("motionBlurIntensity"))   c->motionBlurIntensity   = params["motionBlurIntensity"].get<float>();
        if (params.contains("motionBlurSamples"))     c->motionBlurSamples     = params["motionBlurSamples"].get<int>();
        MarkEntityEdited(registry, ent);
        return { { "postEffects", PostEffectToJson(*c) } };
    }

    // =========================================================
    // UI Editor handlers
    // =========================================================

    std::string UITemplateKindToString(UIEditorTemplateKind k)
    {
        switch (k) {
        case UIEditorTemplateKind::BossHP: return "boss_hp";
        default:                           return "player_hp";
        }
    }
    UIEditorTemplateKind UITemplateKindFromString(const std::string& s)
    {
        if (ToLowerCopy(s) == "boss_hp") return UIEditorTemplateKind::BossHP;
        return UIEditorTemplateKind::PlayerHP;
    }

    std::string UIPartKindToString(UIEditorPartKind k)
    {
        switch (k) {
        case UIEditorPartKind::GaugeRoot:     return "gauge_root";
        case UIEditorPartKind::Image:         return "image";
        case UIEditorPartKind::FillImage:     return "fill_image";
        case UIEditorPartKind::DamagePreview: return "damage_preview";
        case UIEditorPartKind::HPText:        return "hp_text";
        default:                              return "canvas";
        }
    }
    UIEditorPartKind UIPartKindFromString(const std::string& s)
    {
        const std::string lower = ToLowerCopy(s);
        if (lower == "gauge_root")     return UIEditorPartKind::GaugeRoot;
        if (lower == "image")          return UIEditorPartKind::Image;
        if (lower == "fill_image")     return UIEditorPartKind::FillImage;
        if (lower == "damage_preview") return UIEditorPartKind::DamagePreview;
        if (lower == "hp_text")        return UIEditorPartKind::HPText;
        return UIEditorPartKind::Canvas;
    }

    UIEditorPanel& RequireUIEditorPanel(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        if (!ed.IsUIEditorActive()) {
            // Open it if not active so subsequent calls can interact with it
            ed.OpenUIEditorFromAutomation();
        }
        return ed.GetUIEditorPanel();
    }

    json HandleUIEditorOpen(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        ed.OpenUIEditorFromAutomation();
        return { { "open", true } };
    }

    json HandleUIEditorGetState(EngineKernel& kernel)
    {
        auto& panel = RequireUIEditorPanel(kernel);
        const auto& vs = panel.GetViewState();
        const EntityID sel = panel.GetSelectedEntityAutomation();
        const EntityID canvas = panel.FindCanvasAutomation();
        return {
            { "selectedEntity", Entity::IsNull(sel)    ? json(nullptr) : json(EntityToString(sel)) },
            { "canvas",         Entity::IsNull(canvas) ? json(nullptr) : json(EntityToString(canvas)) },
            { "view", {
                { "referenceResolution", json::array({ vs.referenceResolution.x, vs.referenceResolution.y }) },
                { "pan",   json::array({ vs.pan.x, vs.pan.y }) },
                { "zoom",  vs.zoom },
                { "gridSize",   vs.gridSize },
                { "showSafeArea", vs.showSafeArea },
                { "showGrid",   vs.showGrid },
                { "snapToGrid", vs.snapToGrid },
                { "pixelSnap",  vs.pixelSnap }
            }}
        };
    }

    json HandleUIEditorSelect(EngineKernel& kernel, const json& params)
    {
        auto& ed     = RequireEditorLayer(kernel);
        auto& panel  = ed.GetUIEditorPanel();
        Registry* reg = kernel.GetGameRegistry();
        if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
        panel.SetRegistryForAutomation(reg);
        if (params.contains("entity")) {
            const EntityID ent = EntityFromJson(params["entity"]);
            if (!Entity::IsNull(ent) && reg->IsAlive(ent)) panel.SelectEntityAutomation(ent);
        }
        return { { "selectedEntity", EntityToString(panel.GetSelectedEntityAutomation()) } };
    }

    json HandleUIEditorCreateCanvas(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        Registry* reg = kernel.GetGameRegistry();
        if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
        auto& panel = ed.GetUIEditorPanel();
        panel.SetRegistryForAutomation(reg);
        const EntityID canvas = panel.FindOrCreateCanvasAutomation();
        panel.SelectEntityAutomation(canvas);
        return { { "canvas", EntityToString(canvas) } };
    }

    json HandleUIEditorCreateTemplate(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        Registry* reg = kernel.GetGameRegistry();
        if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
        auto& panel = ed.GetUIEditorPanel();
        panel.SetRegistryForAutomation(reg);
        const auto kind = UITemplateKindFromString(params.value("kind", std::string("player_hp")));
        const EntityID ent = panel.CreateTemplateAutomation(kind);
        panel.SelectEntityAutomation(ent);
        return { { "entity", EntityToString(ent) }, { "kind", UITemplateKindToString(kind) } };
    }

    json HandleUIEditorCreatePart(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        Registry* reg = kernel.GetGameRegistry();
        if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
        auto& panel = ed.GetUIEditorPanel();
        panel.SetRegistryForAutomation(reg);
        const auto kind = UIPartKindFromString(params.value("kind", std::string("image")));
        const EntityID ent = panel.CreatePartAutomation(kind);
        panel.SelectEntityAutomation(ent);
        return { { "entity", EntityToString(ent) }, { "kind", UIPartKindToString(kind) } };
    }

    json HandleUIEditorPrefab(EngineKernel& kernel, const json& params, const std::string& action)
    {
        auto& ed = RequireEditorLayer(kernel);
        Registry* reg = kernel.GetGameRegistry();
        if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
        auto& panel = ed.GetUIEditorPanel();
        panel.SetRegistryForAutomation(reg);
        bool ok = false;
        if (action == "save")   ok = panel.SaveSelectedAsPrefabAutomation();
        else if (action == "apply")  ok = panel.ApplySelectedPrefabAutomation();
        else if (action == "revert") ok = panel.RevertSelectedPrefabAutomation();
        else if (action == "unpack") ok = panel.UnpackSelectedPrefabAutomation();
        return { { "action", action }, { "ok", ok } };
    }

    json HandleUIEditorSetView(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireUIEditorPanel(kernel);
        auto& vs = panel.GetViewStateMutable();
        if (params.contains("zoom"))        vs.zoom        = params["zoom"].get<float>();
        if (params.contains("gridSize"))    vs.gridSize    = params["gridSize"].get<float>();
        if (params.contains("showSafeArea"))vs.showSafeArea= params["showSafeArea"].get<bool>();
        if (params.contains("showGrid"))    vs.showGrid    = params["showGrid"].get<bool>();
        if (params.contains("snapToGrid"))  vs.snapToGrid  = params["snapToGrid"].get<bool>();
        if (params.contains("pixelSnap"))   vs.pixelSnap   = params["pixelSnap"].get<bool>();
        if (params.contains("pan") && params["pan"].is_array() && params["pan"].size() >= 2)
            vs.pan = { params["pan"][0].get<float>(), params["pan"][1].get<float>() };
        return { { "ok", true } };
    }

    // =========================================================
    // Sequencer handlers
    // =========================================================

    std::string CinematicTrackTypeToString(CinematicTrackType t)
    {
        switch (t) {
        case CinematicTrackType::Camera:      return "camera";
        case CinematicTrackType::Animation:   return "animation";
        case CinematicTrackType::Effect:      return "effect";
        case CinematicTrackType::Audio:       return "audio";
        case CinematicTrackType::Event:       return "event";
        case CinematicTrackType::CameraShake: return "camera_shake";
        case CinematicTrackType::Bool:        return "bool";
        case CinematicTrackType::Float:       return "float";
        default:                              return "transform";
        }
    }
    CinematicTrackType CinematicTrackTypeFromString(const std::string& s)
    {
        const std::string lower = ToLowerCopy(s);
        if (lower == "camera")       return CinematicTrackType::Camera;
        if (lower == "animation")    return CinematicTrackType::Animation;
        if (lower == "effect")       return CinematicTrackType::Effect;
        if (lower == "audio")        return CinematicTrackType::Audio;
        if (lower == "event")        return CinematicTrackType::Event;
        if (lower == "camera_shake") return CinematicTrackType::CameraShake;
        if (lower == "bool")         return CinematicTrackType::Bool;
        if (lower == "float")        return CinematicTrackType::Float;
        return CinematicTrackType::Transform;
    }

    json CinematicSectionSummaryToJson(const CinematicSection& s)
    {
        return {
            { "sectionId",   s.sectionId },
            { "label",       s.label },
            { "startFrame",  s.startFrame },
            { "endFrame",    s.endFrame },
            { "muted",       s.muted },
            { "locked",      s.locked }
        };
    }
    json CinematicTrackSummaryToJson(const CinematicTrack& t)
    {
        json sections = json::array();
        for (const auto& s : t.sections) sections.push_back(CinematicSectionSummaryToJson(s));
        return {
            { "trackId",     t.trackId },
            { "type",        CinematicTrackTypeToString(t.type) },
            { "displayName", t.displayName },
            { "muted",       t.muted },
            { "sectionCount",static_cast<int>(t.sections.size()) },
            { "sections",    std::move(sections) }
        };
    }
    json CinematicBindingSummaryToJson(const CinematicBinding& b)
    {
        json tracks = json::array();
        for (const auto& t : b.tracks) tracks.push_back(CinematicTrackSummaryToJson(t));
        return {
            { "bindingId",   b.bindingId },
            { "displayName", b.displayName },
            { "entity",      Entity::IsNull(b.targetEntity) ? json(nullptr) : json(EntityToString(b.targetEntity)) },
            { "trackCount",  static_cast<int>(b.tracks.size()) },
            { "tracks",      std::move(tracks) }
        };
    }
    json CinematicSequenceSummaryToJson(const CinematicSequenceAsset& seq)
    {
        json bindings = json::array();
        for (const auto& b : seq.bindings) bindings.push_back(CinematicBindingSummaryToJson(b));
        json masterTracks = json::array();
        for (const auto& t : seq.masterTracks) masterTracks.push_back(CinematicTrackSummaryToJson(t));
        return {
            { "name",          seq.name },
            { "frameRate",     seq.frameRate },
            { "durationFrames",seq.durationFrames },
            { "playRangeStart",seq.playRangeStart },
            { "playRangeEnd",  seq.playRangeEnd },
            { "bindingCount",  static_cast<int>(seq.bindings.size()) },
            { "bindings",      std::move(bindings) },
            { "masterTracks",  std::move(masterTracks) }
        };
    }

    SequencerPanel& RequireSequencerPanel(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        return ed.GetSequencerPanel();
    }

    json HandleSequencerOpen(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        ed.OpenSequencerFromAutomation();
        return { { "open", true } };
    }

    json HandleSequencerNew(EngineKernel& kernel)
    {
        auto& panel = RequireSequencerPanel(kernel);
        panel.NewSequenceAutomation();
        return { { "sequence", CinematicSequenceSummaryToJson(panel.GetSequenceAsset()) } };
    }

    json HandleSequencerLoad(EngineKernel& kernel, const json& params)
    {
        const std::filesystem::path path = ResolveProjectPath(params.value("path", std::string{}), PathAccess::ReadAsset, true);
        auto& panel = RequireSequencerPanel(kernel);
        if (!panel.LoadSequenceAutomation(path.generic_string()))
            throw MakeError("load_failed", "Failed to load sequence.", { { "path", path.string() } });
        return { { "path", ToGenericProjectPath(path) }, { "sequence", CinematicSequenceSummaryToJson(panel.GetSequenceAsset()) } };
    }

    json HandleSequencerSave(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireSequencerPanel(kernel);
        const bool saveAs = params.contains("path") && !params["path"].get<std::string>().empty();
        if (saveAs) {
            // saveAs: modify document path then save
            auto& seq = panel.GetSequenceAssetMutable();
            (void)seq; // path is set inside SaveSequence via dialog in full UI; automation uses direct path
            // Fall through to plain save - full save-as dialog not accessible headlessly
        }
        if (!panel.SaveSequenceAutomation(saveAs))
            throw MakeError("save_failed", "Failed to save sequence.");
        return { { "path", panel.GetDocumentPathAutomation() } };
    }

    json HandleSequencerGet(EngineKernel& kernel)
    {
        auto& panel = RequireSequencerPanel(kernel);
        json result = CinematicSequenceSummaryToJson(panel.GetSequenceAsset());
        result["currentFrame"]     = panel.GetCurrentFrameAutomation();
        result["playing"]          = panel.IsPlayingAutomation();
        result["looping"]          = panel.IsLoopingAutomation();
        result["documentPath"]     = panel.GetDocumentPathAutomation();
        result["selectedBindingId"]= panel.GetSelectedBindingId();
        result["selectedTrackId"]  = panel.GetSelectedTrackId();
        result["selectedSectionId"]= panel.GetSelectedSectionId();
        return result;
    }

    json HandleSequencerSetPlayback(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireSequencerPanel(kernel);
        if (params.contains("playing"))     panel.SetPlayingAutomation(params["playing"].get<bool>());
        if (params.contains("looping"))     panel.SetLoopingAutomation(params["looping"].get<bool>());
        if (params.contains("currentFrame"))panel.SetCurrentFrameAutomation(params["currentFrame"].get<float>());
        auto& seq = panel.GetSequenceAssetMutable();
        if (params.contains("playRangeStart")) seq.playRangeStart = params["playRangeStart"].get<int>();
        if (params.contains("playRangeEnd"))   seq.playRangeEnd   = params["playRangeEnd"].get<int>();
        return { { "playing", panel.IsPlayingAutomation() }, { "currentFrame", panel.GetCurrentFrameAutomation() } };
    }

    json HandleSequencerAddBinding(EngineKernel& kernel, const json& params)
    {
        auto& panel   = RequireSequencerPanel(kernel);
        Registry* reg = kernel.GetGameRegistry();
        if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
        if (params.contains("entity")) {
            const EntityID ent = EntityFromJson(params["entity"]);
            if (!Entity::IsNull(ent) && reg->IsAlive(ent))
                EditorSelection::Instance().SelectEntity(ent);
        }
        const int bindingCountBefore = static_cast<int>(panel.GetSequenceAsset().bindings.size());
        panel.AddBindingFromSelectedEntityAutomation(reg);
        const auto& bindings = panel.GetSequenceAsset().bindings;
        if (static_cast<int>(bindings.size()) <= bindingCountBefore)
            throw MakeError("add_binding_failed", "Failed to add binding. Make sure an entity is selected.");
        const auto& newBinding = bindings.back();
        panel.SelectBindingAutomation(newBinding.bindingId);
        return { { "binding", CinematicBindingSummaryToJson(newBinding) } };
    }

    json HandleSequencerAddTrack(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireSequencerPanel(kernel);
        const uint64_t bindingId = params.value("bindingId", (uint64_t)0);
        auto& bindings = panel.GetSequenceAssetMutable().bindings;
        int bindingIdx = -1;
        for (int i = 0; i < static_cast<int>(bindings.size()); ++i) {
            if (bindings[i].bindingId == bindingId) { bindingIdx = i; break; }
        }
        if (bindingIdx < 0)
            throw MakeError("binding_not_found", "Binding not found.", { { "bindingId", bindingId } });
        const auto trackType = CinematicTrackTypeFromString(params.value("type", std::string("transform")));
        const int trackCountBefore = static_cast<int>(bindings[bindingIdx].tracks.size());
        panel.AddTrackToBindingAutomation(bindingIdx, trackType);
        const auto& tracks = panel.GetSequenceAsset().bindings[bindingIdx].tracks;
        if (static_cast<int>(tracks.size()) <= trackCountBefore)
            throw MakeError("add_track_failed", "Failed to add track.");
        const auto& newTrack = tracks.back();
        panel.SelectTrackAutomation(newTrack.trackId);
        panel.MarkDirtyAutomation();
        return { { "track", CinematicTrackSummaryToJson(newTrack) } };
    }

    json HandleSequencerAddMasterTrack(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireSequencerPanel(kernel);
        const auto trackType = CinematicTrackTypeFromString(params.value("type", std::string("event")));
        const int countBefore = static_cast<int>(panel.GetSequenceAsset().masterTracks.size());
        panel.AddMasterTrackAutomation(trackType);
        const auto& masterTracks = panel.GetSequenceAsset().masterTracks;
        if (static_cast<int>(masterTracks.size()) <= countBefore)
            throw MakeError("add_track_failed", "Failed to add master track.");
        const auto& newTrack = masterTracks.back();
        panel.SelectTrackAutomation(newTrack.trackId);
        panel.MarkDirtyAutomation();
        return { { "track", CinematicTrackSummaryToJson(newTrack) } };
    }

    json HandleSequencerAddSection(EngineKernel& kernel, const json& params)
    {
        auto& panel    = RequireSequencerPanel(kernel);
        const uint64_t trackId    = params.value("trackId",    (uint64_t)0);
        const uint64_t bindingId  = params.value("bindingId",  (uint64_t)0);
        const bool     isMaster   = params.value("master",     false);

        // Find track reference
        CinematicTrack* target = nullptr;
        auto& seq = panel.GetSequenceAssetMutable();
        if (isMaster || bindingId == 0) {
            for (auto& t : seq.masterTracks) { if (t.trackId == trackId) { target = &t; break; } }
        }
        if (!target) {
            for (auto& b : seq.bindings) {
                if (bindingId != 0 && b.bindingId != bindingId) continue;
                for (auto& t : b.tracks) { if (t.trackId == trackId) { target = &t; break; } }
                if (target) break;
            }
        }
        if (!target) throw MakeError("track_not_found", "Track not found.", { { "trackId", trackId } });

        const int sectionCountBefore = static_cast<int>(target->sections.size());
        panel.AddSectionToTrackAutomation(*target);
        if (static_cast<int>(target->sections.size()) <= sectionCountBefore)
            throw MakeError("add_section_failed", "Failed to add section.");
        const auto& newSection = target->sections.back();
        panel.SelectSectionAutomation(newSection.sectionId);
        panel.MarkDirtyAutomation();
        return { { "section", CinematicSectionSummaryToJson(newSection) } };
    }

    json HandleSequencerSelect(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireSequencerPanel(kernel);
        if (params.contains("bindingId"))  panel.SelectBindingAutomation(params["bindingId"].get<uint64_t>());
        if (params.contains("trackId"))    panel.SelectTrackAutomation(params["trackId"].get<uint64_t>());
        if (params.contains("sectionId"))  panel.SelectSectionAutomation(params["sectionId"].get<uint64_t>());
        return {
            { "selectedBindingId", panel.GetSelectedBindingId() },
            { "selectedTrackId",   panel.GetSelectedTrackId() },
            { "selectedSectionId", panel.GetSelectedSectionId() }
        };
    }

    json HandleSequencerSetSequenceParams(EngineKernel& kernel, const json& params)
    {
        auto& panel = RequireSequencerPanel(kernel);
        auto& seq   = panel.GetSequenceAssetMutable();
        if (params.contains("name"))           seq.name           = params["name"].get<std::string>();
        if (params.contains("frameRate"))      seq.frameRate      = params["frameRate"].get<float>();
        if (params.contains("durationFrames")) seq.durationFrames = params["durationFrames"].get<int>();
        if (params.contains("workRangeStart")) seq.workRangeStart = params["workRangeStart"].get<int>();
        if (params.contains("workRangeEnd"))   seq.workRangeEnd   = params["workRangeEnd"].get<int>();
        panel.MarkDirtyAutomation();
        return { { "sequence", CinematicSequenceSummaryToJson(seq) } };
    }

    // =========================================================
    // scene.new
    // =========================================================

    json HandleSceneNew(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        const std::string modeStr = params.value("mode", "3D");
        const EditorLayer::SceneViewMode mode =
            (ToLowerCopy(modeStr) == "2d")
            ? EditorLayer::SceneViewMode::Mode2D
            : EditorLayer::SceneViewMode::Mode3D;
        ed.RequestNewSceneAutomation(mode);
        return { { "ok", true }, { "mode", (mode == EditorLayer::SceneViewMode::Mode2D) ? "2D" : "3D" } };
    }

    // =========================================================
    // editor.undo / redo
    // =========================================================

    json HandleEditorUndo(EngineKernel& kernel)
    {
        RequireEditorLayer(kernel).ExecuteUndoAutomation();
        return { { "ok", true } };
    }

    json HandleEditorRedo(EngineKernel& kernel)
    {
        RequireEditorLayer(kernel).ExecuteRedoAutomation();
        return { { "ok", true } };
    }

    // =========================================================
    // editor.focus_panel
    // =========================================================

    static EditorLayer::WindowFocusTarget PanelNameToFocusTarget(const std::string& name)
    {
        const std::string n = ToLowerCopy(name);
        if (n == "scene_view"   || n == "sceneview")    return EditorLayer::WindowFocusTarget::SceneView;
        if (n == "game_view"    || n == "gameview")     return EditorLayer::WindowFocusTarget::GameView;
        if (n == "hierarchy")                           return EditorLayer::WindowFocusTarget::Hierarchy;
        if (n == "inspector")                           return EditorLayer::WindowFocusTarget::Inspector;
        if (n == "asset_browser"|| n == "assetbrowser") return EditorLayer::WindowFocusTarget::AssetBrowser;
        if (n == "serializer"   || n == "model_serializer") return EditorLayer::WindowFocusTarget::Serializer;
        if (n == "console")                             return EditorLayer::WindowFocusTarget::Console;
        if (n == "sequencer")                           return EditorLayer::WindowFocusTarget::Sequencer;
        if (n == "lighting")                            return EditorLayer::WindowFocusTarget::Lighting;
        if (n == "audio")                               return EditorLayer::WindowFocusTarget::Audio;
        if (n == "render_passes"|| n == "renderpasses") return EditorLayer::WindowFocusTarget::RenderPasses;
        if (n == "grid_settings"|| n == "gridsettings") return EditorLayer::WindowFocusTarget::GridSettings;
        if (n == "gbuffer_debug"|| n == "gbufferdebug") return EditorLayer::WindowFocusTarget::GBufferDebug;
        if (n == "player_editor"|| n == "playereditor") return EditorLayer::WindowFocusTarget::PlayerEditor;
        if (n == "effect_editor"|| n == "effecteditor") return EditorLayer::WindowFocusTarget::EffectEditor;
        if (n == "ui_editor"    || n == "uieditor")     return EditorLayer::WindowFocusTarget::UIEditor;
        if (n == "game_loop_editor" || n == "gamloopeditor") return EditorLayer::WindowFocusTarget::GameLoopEditor;
        return EditorLayer::WindowFocusTarget::None;
    }

    json HandleEditorFocusPanel(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        const std::string panel = params.value("panel", std::string{});
        if (panel.empty()) throw MakeError("missing_param", "'panel' is required.", {});
        const auto target = PanelNameToFocusTarget(panel);
        if (target == EditorLayer::WindowFocusTarget::None)
            throw MakeError("invalid_param", "Unknown panel name.", { { "panel", panel } });
        ed.FocusPanelAutomation(target);
        return { { "ok", true }, { "panel", panel } };
    }

    // =========================================================
    // editor.get_panels / editor.show_panel
    // =========================================================

    json HandleEditorGetPanels(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        return {
            { "panels", {
                { "scene_view",     ed.GetShowSceneView()     },
                { "game_view",      ed.GetShowGameView()      },
                { "hierarchy",      ed.GetShowHierarchy()     },
                { "inspector",      ed.GetShowInspector()     },
                { "asset_browser",  ed.GetShowAssetBrowser()  },
                { "console",        ed.GetShowConsole()       },
                { "serializer",     ed.GetShowSerializer()    },
                { "audio",          ed.GetShowAudioWindow()   },
                { "render_passes",  ed.GetShowRenderPasses()  },
                { "gbuffer_debug",  ed.GetShowGBufferDebug()  },
                { "grid_settings",  ed.GetShowGridSettings()  },
                { "status_bar",     ed.GetShowStatusBar()     }
            } }
        };
    }

    json HandleEditorShowPanel(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        const std::string panel = params.value("panel", std::string{});
        if (panel.empty()) throw MakeError("missing_param", "'panel' is required.", {});
        if (!params.contains("visible"))
            throw MakeError("missing_param", "'visible' is required.", {});
        const bool visible = params["visible"].get<bool>();

        const std::string n = ToLowerCopy(panel);
        if      (n == "scene_view"   ) ed.SetShowSceneView(visible);
        else if (n == "game_view"    ) ed.SetShowGameView(visible);
        else if (n == "hierarchy"    ) ed.SetShowHierarchy(visible);
        else if (n == "inspector"    ) ed.SetShowInspector(visible);
        else if (n == "asset_browser") ed.SetShowAssetBrowser(visible);
        else if (n == "console"      ) ed.SetShowConsole(visible);
        else if (n == "serializer"   ) ed.SetShowSerializer(visible);
        else if (n == "audio"        ) ed.SetShowAudioWindow(visible);
        else if (n == "render_passes") ed.SetShowRenderPasses(visible);
        else if (n == "gbuffer_debug") ed.SetShowGBufferDebug(visible);
        else if (n == "grid_settings") ed.SetShowGridSettings(visible);
        else if (n == "status_bar"   ) ed.SetShowStatusBar(visible);
        else throw MakeError("invalid_param", "Unknown panel name.", { { "panel", panel } });

        return { { "ok", true }, { "panel", panel }, { "visible", visible } };
    }

    // =========================================================
    // scene_view.get_snap / set_snap
    // =========================================================

    json HandleSceneViewGetSnap(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        return {
            { "translate", {
                { "enabled", ed.GetTranslateSnapEnabled() },
                { "step",    ed.GetTranslateSnapStep()    }
            } },
            { "rotate", {
                { "enabled", ed.GetRotateSnapEnabled()    },
                { "step",    ed.GetRotateSnapStep()       }
            } },
            { "scale", {
                { "enabled", ed.GetScaleSnapEnabled()     },
                { "step",    ed.GetScaleSnapStep()        }
            } }
        };
    }

    json HandleSceneViewSetSnap(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        if (params.contains("translateEnabled")) ed.SetTranslateSnapEnabled(params["translateEnabled"].get<bool>());
        if (params.contains("rotateEnabled"))    ed.SetRotateSnapEnabled(params["rotateEnabled"].get<bool>());
        if (params.contains("scaleEnabled"))     ed.SetScaleSnapEnabled(params["scaleEnabled"].get<bool>());
        if (params.contains("translateStep"))    ed.SetTranslateSnapStep(params["translateStep"].get<float>());
        if (params.contains("rotateStep"))       ed.SetRotateSnapStep(params["rotateStep"].get<float>());
        if (params.contains("scaleStep"))        ed.SetScaleSnapStep(params["scaleStep"].get<float>());
        return HandleSceneViewGetSnap(kernel);
    }

    // =========================================================
    // scene_view.set_grid
    // =========================================================

    json HandleSceneViewSetGrid(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        if (params.contains("visible"))       ed.SetSceneGridVisible(params["visible"].get<bool>());
        if (params.contains("cellSize"))      ed.SetSceneGridCellSize(params["cellSize"].get<float>());
        if (params.contains("halfLineCount")) ed.SetSceneGridHalfLineCount(params["halfLineCount"].get<int>());
        return {
            { "ok",            true                          },
            { "visible",       ed.IsSceneGridVisible()       },
            { "cellSize",      ed.GetSceneGridCellSize()     },
            { "halfLineCount", ed.GetSceneGridHalfLineCount()}
        };
    }

    // =========================================================
    // scene_view.set_2d_view
    // =========================================================

    json HandleSceneViewSet2DView(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        if (params.contains("centerX") || params.contains("centerY"))
        {
            const auto cur = ed.Get2DCenter();
            const float cx = params.value("centerX", cur.x);
            const float cy = params.value("centerY", cur.y);
            ed.Set2DCenter({ cx, cy });
        }
        if (params.contains("zoom")) ed.Set2DZoom(params["zoom"].get<float>());
        const auto c = ed.Get2DCenter();
        return { { "ok", true }, { "centerX", c.x }, { "centerY", c.y }, { "zoom", ed.Get2DZoom() } };
    }

    // =========================================================
    // scene_view.align_camera
    // =========================================================

    json HandleSceneViewAlignCamera(EngineKernel& kernel)
    {
        RequireEditorLayer(kernel).AlignCameraToViewAutomation();
        return { { "ok", true } };
    }

    // =========================================================
    // game_view.get_state / set_resolution / set_overlay
    // =========================================================

    static std::string GameViewResolutionToString(EditorLayer::GameViewResolutionPreset p)
    {
        switch (p)
        {
        case EditorLayer::GameViewResolutionPreset::HD1080:           return "hd1080";
        case EditorLayer::GameViewResolutionPreset::HD720:            return "hd720";
        case EditorLayer::GameViewResolutionPreset::Portrait1080x1920:return "portrait_1080x1920";
        case EditorLayer::GameViewResolutionPreset::Portrait750x1334: return "portrait_750x1334";
        default:                                                       return "free";
        }
    }
    static EditorLayer::GameViewResolutionPreset GameViewResolutionFromString(const std::string& s)
    {
        const std::string n = ToLowerCopy(s);
        if (n == "hd1080")              return EditorLayer::GameViewResolutionPreset::HD1080;
        if (n == "hd720")               return EditorLayer::GameViewResolutionPreset::HD720;
        if (n == "portrait_1080x1920")  return EditorLayer::GameViewResolutionPreset::Portrait1080x1920;
        if (n == "portrait_750x1334")   return EditorLayer::GameViewResolutionPreset::Portrait750x1334;
        return EditorLayer::GameViewResolutionPreset::Free;
    }
    static std::string GameViewAspectToString(EditorLayer::GameViewAspectPolicy p)
    {
        switch (p)
        {
        case EditorLayer::GameViewAspectPolicy::Fill:        return "fill";
        case EditorLayer::GameViewAspectPolicy::PixelPerfect:return "pixel_perfect";
        default:                                              return "fit";
        }
    }
    static EditorLayer::GameViewAspectPolicy GameViewAspectFromString(const std::string& s)
    {
        const std::string n = ToLowerCopy(s);
        if (n == "fill")         return EditorLayer::GameViewAspectPolicy::Fill;
        if (n == "pixel_perfect")return EditorLayer::GameViewAspectPolicy::PixelPerfect;
        return EditorLayer::GameViewAspectPolicy::Fit;
    }
    static std::string GameViewScaleToString(EditorLayer::GameViewScalePolicy p)
    {
        switch (p)
        {
        case EditorLayer::GameViewScalePolicy::Scale1x: return "1x";
        case EditorLayer::GameViewScalePolicy::Scale2x: return "2x";
        case EditorLayer::GameViewScalePolicy::Scale3x: return "3x";
        default:                                         return "auto_fit";
        }
    }
    static EditorLayer::GameViewScalePolicy GameViewScaleFromString(const std::string& s)
    {
        const std::string n = ToLowerCopy(s);
        if (n == "1x") return EditorLayer::GameViewScalePolicy::Scale1x;
        if (n == "2x") return EditorLayer::GameViewScalePolicy::Scale2x;
        if (n == "3x") return EditorLayer::GameViewScalePolicy::Scale3x;
        return EditorLayer::GameViewScalePolicy::AutoFit;
    }

    json HandleGameViewGetState(EngineKernel& kernel)
    {
        auto& ed = RequireEditorLayer(kernel);
        const auto sz = ed.GetGameViewSize();
        return {
            { "resolution",   GameViewResolutionToString(ed.GetGameViewResolutionPreset()) },
            { "aspectPolicy", GameViewAspectToString(ed.GetGameViewAspectPolicy())         },
            { "scalePolicy",  GameViewScaleToString(ed.GetGameViewScalePolicy())           },
            { "width",        sz.x },
            { "height",       sz.y },
            { "overlay", {
                { "safeArea",   ed.GetGameViewShowSafeArea()    },
                { "stats",      ed.GetGameViewShowStats()       },
                { "uiOverlay",  ed.GetGameViewShowUIOverlay()   },
                { "2dOverlay",  ed.GetGameViewShow2DOverlay()   }
            } }
        };
    }

    json HandleGameViewSetResolution(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        if (params.contains("resolution"))
            ed.SetGameViewResolutionPreset(GameViewResolutionFromString(params["resolution"].get<std::string>()));
        if (params.contains("aspectPolicy"))
            ed.SetGameViewAspectPolicy(GameViewAspectFromString(params["aspectPolicy"].get<std::string>()));
        if (params.contains("scalePolicy"))
            ed.SetGameViewScalePolicy(GameViewScaleFromString(params["scalePolicy"].get<std::string>()));
        return HandleGameViewGetState(kernel);
    }

    json HandleGameViewSetOverlay(EngineKernel& kernel, const json& params)
    {
        auto& ed = RequireEditorLayer(kernel);
        if (params.contains("safeArea"))  ed.SetGameViewShowSafeArea(params["safeArea"].get<bool>());
        if (params.contains("stats"))     ed.SetGameViewShowStats(params["stats"].get<bool>());
        if (params.contains("uiOverlay")) ed.SetGameViewShowUIOverlay(params["uiOverlay"].get<bool>());
        if (params.contains("2dOverlay")) ed.SetGameViewShow2DOverlay(params["2dOverlay"].get<bool>());
        return HandleGameViewGetState(kernel);
    }

    // =========================================================
    // model_serializer.build
    // =========================================================

    json HandleModelSerializerBuild(EngineKernel& kernel, const json& params)
    {
        if (!params.contains("sourcePath"))
            throw MakeError("missing_param", "'sourcePath' is required.", {});
        if (!params.contains("outputPath"))
            throw MakeError("missing_param", "'outputPath' is required.", {});

        const std::string src = params["sourcePath"].get<std::string>();
        const std::string dst = params["outputPath"].get<std::string>();

        ModelSerializerSettings settings;
        if (params.contains("scaling"))               settings.scaling               = params["scaling"].get<float>();
        if (params.contains("enableSimplification"))  settings.enableSimplification  = params["enableSimplification"].get<bool>();
        if (params.contains("targetTriangleRatio"))   settings.targetTriangleRatio   = params["targetTriangleRatio"].get<float>();
        if (params.contains("targetError"))           settings.targetError           = params["targetError"].get<float>();
        if (params.contains("lockBorder"))            settings.lockBorder            = params["lockBorder"].get<bool>();
        if (params.contains("optimizeVertexCache"))   settings.optimizeVertexCache   = params["optimizeVertexCache"].get<bool>();
        if (params.contains("optimizeOverdraw"))      settings.optimizeOverdraw      = params["optimizeOverdraw"].get<bool>();
        if (params.contains("overdrawThreshold"))     settings.overdrawThreshold     = params["overdrawThreshold"].get<float>();
        if (params.contains("optimizeVertexFetch"))   settings.optimizeVertexFetch   = params["optimizeVertexFetch"].get<bool>();

        const ModelSerializerResult result = ModelAssetSerializer::Build(src, dst, settings);

        return {
            { "success",                     result.success                     },
            { "sourcePath",                  result.sourcePath                  },
            { "outputPath",                  result.outputPath                  },
            { "message",                     result.message                     },
            { "processedMeshCount",          result.processedMeshCount          },
            { "simplifiedMeshCount",         result.simplifiedMeshCount         },
            { "skippedSimplificationMeshCount", result.skippedSimplificationMeshCount },
            { "sourceVertexCount",           result.sourceVertexCount           },
            { "sourceIndexCount",            result.sourceIndexCount            },
            { "outputVertexCount",           result.outputVertexCount           },
            { "outputIndexCount",            result.outputIndexCount            }
        };
    }

    // =========================================================

    // Forward declaration
    json DispatchCommand(EngineKernel& kernel, const json& command);

    json HandleBatch(EngineKernel& kernel, const json& params)
    {
        if (!params.contains("commands") || !params["commands"].is_array()) {
            throw MakeError("missing_param", "params.commands must be an array.");
        }

        json results = json::array();
        for (const json& subCommand : params["commands"]) {
            json entry;
            try {
                json subResult = DispatchCommand(kernel, subCommand);
                entry["ok"] = true;
                entry["result"] = std::move(subResult);
            }
            catch (const json& jsonError) {
                entry["ok"] = false;
                entry["error"] = jsonError;
            }
            catch (const std::exception& e) {
                entry["ok"] = false;
                entry["error"] = MakeError("internal_error", e.what());
            }
            catch (...) {
                entry["ok"] = false;
                entry["error"] = MakeError("internal_error", "Unknown exception.");
            }
            results.push_back(std::move(entry));
        }

        return { { "results", std::move(results) } };
    }

    bool g_ecsWatchEnabled = false;
    uint64_t g_lastBroadcastEcsRevision = UINT64_MAX;

    json DispatchCommand(EngineKernel& kernel, const json& command)
    {
        const std::string name = command.value("command", std::string{});
        const json params = command.value("params", json::object());
        Registry* registry = kernel.GetGameRegistry();

        if (name == "batch") {
            return HandleBatch(kernel, params);
        }
        if (name == "ping") {
            return HandlePing();
        }
        if (name == "get_engine_state") {
            return HandleGetEngineState(kernel);
        }
        if (name == "get_visual_state") {
            return HandleGetVisualState(kernel);
        }
        if (name == "editor.recovery.get_state") {
            return HandleRecoveryGetState(kernel, params);
        }
        if (name == "editor.recovery.restore") {
            return HandleRecoveryRestore(kernel);
        }
        if (name == "editor.recovery.dismiss") {
            return HandleRecoveryDismiss(kernel);
        }
        if (name == "get_component_schema") {
            return HandleGetComponentSchema(params);
        }
        if (name == "asset_browser.list") {
            return HandleAssetBrowserList(params);
        }
        if (name == "asset_browser.search") {
            return HandleAssetBrowserSearch(params);
        }
        if (name == "asset_browser.create_folder") {
            return HandleAssetBrowserCreateFolder(params);
        }
        if (name == "asset_browser.copy") {
            return HandleAssetBrowserCopyMove(params, false);
        }
        if (name == "asset_browser.move") {
            return HandleAssetBrowserCopyMove(params, true);
        }
        if (name == "asset_browser.rename") {
            return HandleAssetBrowserRename(params);
        }
        if (name == "asset_browser.delete") {
            return HandleAssetBrowserDelete(params);
        }
        if (name == "material.create") {
            return HandleMaterialCreate(params);
        }
        if (name == "material.get") {
            return HandleMaterialGet(params);
        }
        if (name == "material.set") {
            return HandleMaterialSet(params);
        }
        if (name == "effect_editor.list_node_types") {
            return HandleEffectListNodeTypes();
        }
        if (name == "effect_editor.create_asset") {
            return HandleEffectCreateAsset(kernel, params);
        }
        if (name == "effect_editor.apply_preset") {
            return HandleEffectApplyPreset(kernel, params);
        }
        if (name == "effect_editor.open_workspace") {
            return HandleEffectOpenWorkspace(kernel, params);
        }
        if (name == "effect_editor.set_preview_view") {
            return HandleEffectSetPreviewView(kernel, params);
        }
        if (name == "effect_editor.get_state") {
            return HandleEffectGetState(kernel, registry, params);
        }
        if (name == "effect_editor.timeline_play") {
            return HandleEffectTimelinePlay(kernel, params);
        }
        if (name == "effect_editor.timeline_stop") {
            return HandleEffectTimelineStop(kernel);
        }
        if (name == "effect_editor.select_node") {
            return HandleEffectSelectNode(kernel, params, false);
        }
        if (name == "effect_editor.focus_node") {
            return HandleEffectSelectNode(kernel, params, true);
        }
        if (name == "effect_editor.get_asset") {
            return HandleEffectGetAsset(params);
        }
        if (name == "effect_editor.set_asset") {
            return HandleEffectSetAsset(kernel, params);
        }
        if (name == "effect_editor.set_semantic_params") {
            return HandleEffectSetSemanticParams(kernel, params);
        }
        if (name == "effect_editor.add_node") {
            return HandleEffectAddNode(kernel, params);
        }
        if (name == "effect_editor.set_node") {
            return HandleEffectSetNode(kernel, params);
        }
        if (name == "effect_editor.delete_node") {
            return HandleEffectDeleteNode(kernel, params);
        }
        if (name == "effect_editor.connect") {
            return HandleEffectConnect(kernel, params);
        }
        if (name == "effect_editor.disconnect") {
            return HandleEffectDisconnect(kernel, params);
        }
        if (name == "effect_editor.compile") {
            return HandleEffectCompile(params);
        }
        if (name == "capture_screenshot") {
            const std::filesystem::path defaultPath =
                std::filesystem::path("Saved") / "AI" / "screenshots" /
                (SanitizeFileStem(command.value("id", std::string("screenshot"))) + ".bmp");
            return HandleCaptureScreenshot(kernel, params, defaultPath);
        }
        if (name == "visual.evaluate_capture") {
            return HandleVisualEvaluateCapture(kernel, params);
        }
        if (name == "ecs.watch") {
            g_ecsWatchEnabled = params.value("enable", true);
            return { { "enabled", g_ecsWatchEnabled } };
        }
        if (!registry) {
            throw MakeError("operation_not_allowed", "Game registry is not available.");
        }
        if (name == "ecs.query") {
            return HandleECSQuery(*registry, params);
        }
        if (name == "ecs.hierarchy") {
            return HandleECSHierarchy(*registry, params);
        }
        if (name == "ecs.diff") {
            return HandleECSDiff(*registry, params);
        }
        if (name == "visual.verify_entity") {
            return HandleVisualVerifyEntity(kernel, *registry, params);
        }
        if (name == "visual.verify_entity_game_view") {
            return HandleVisualVerifyEntityGameView(kernel, *registry, params);
        }
        if (name == "scene_view.frame_entities") {
            return HandleSceneViewFrameEntities(kernel, *registry, params);
        }
        if (name == "scene_view.frame_all") {
            return HandleSceneViewFrameAll(kernel, *registry, params);
        }
        if (name == "camera.frame_entities") {
            return HandleCameraFrameEntities(kernel, *registry, params);
        }
        if (name == "visual.assert_entities_visible") {
            return HandleVisualAssertEntitiesVisible(kernel, *registry, params);
        }
        if (name == "visual.capture_review_set") {
            return HandleVisualCaptureReviewSet(kernel, *registry, params);
        }
        if (name == "effect_editor.timeline_seek") {
            return HandleEffectTimelineSeek(kernel, *registry, params);
        }
        if (name == "effect_editor.timeline_step") {
            return HandleEffectTimelineStep(kernel, *registry, params);
        }
        if (name == "effect_editor.assert_preview_visible") {
            return HandleEffectAssertPreviewVisible(kernel, *registry, params);
        }
        if (name == "effect_editor.capture_review_set") {
            return HandleEffectCaptureReviewSet(kernel, *registry, params);
        }
        if (name == "effect_editor.capture_multi_time_review") {
            return HandleEffectCaptureMultiTimeReview(kernel, *registry, params);
        }
        if (name == "gameplay.get_state") {
            return HandleGameplayGetState(kernel, *registry, params);
        }
        if (name == "gameplay.get_events") {
            return HandleGameplayGetEvents(kernel, params);
        }
        if (name == "gameplay.clear_events") {
            return HandleGameplayClearEvents();
        }
        if (name == "game.play") {
            return HandleGamePlay(kernel);
        }
        if (name == "game.pause") {
            return HandleGamePause(kernel);
        }
        if (name == "game.stop") {
            return HandleGameStop(kernel);
        }
        if (name == "game.step_frames") {
            return HandleGameStepFrames(kernel, params);
        }
        if (name == "game.set_time_scale") {
            return HandleGameSetTimeScale(kernel, params);
        }
        if (name == "game.input.press") {
            return HandleGameInputPress(kernel, *registry, params);
        }
        if (name == "game.input.release") {
            return HandleGameInputRelease(kernel, *registry, params);
        }
        if (name == "game.input.tap") {
            return HandleGameInputTap(kernel, *registry, params);
        }
        if (name == "game.input.axis") {
            return HandleGameInputAxis(kernel, *registry, params);
        }
        if (name == "game.input.mouse_move") {
            return HandleGameInputMouseMove(kernel, params);
        }
        if (name == "list_entities") {
            return HandleListEntities(*registry);
        }
        if (name == "get_entity") {
            return HandleGetEntity(*registry, params);
        }
        if (name == "get_component") {
            return HandleGetComponent(*registry, params);
        }
        if (name == "select_entity") {
            return HandleSelectEntity(*registry, params);
        }
        if (name == "add_component") {
            return HandleAddComponent(*registry, params);
        }
        if (name == "remove_component") {
            return HandleRemoveComponent(*registry, params);
        }
        if (name == "set_component_fields") {
            return HandleSetComponentFields(*registry, params);
        }
        if (name == "create_empty") {
            return HandleCreateEmpty(*registry, params);
        }
        if (name == "create_model_entity") {
            return HandleCreateModelEntity(*registry, params);
        }
        if (name == "set_transform") {
            return HandleSetTransform(*registry, params);
        }
        if (name == "delete_entity") {
            return HandleDeleteEntity(*registry, params);
        }
        if (name == "duplicate_entity") {
            return HandleDuplicateEntity(*registry, params);
        }
        if (name == "reparent_entity") {
            return HandleReparentEntity(*registry, params);
        }
        if (name == "instantiate_prefab") {
            return HandleInstantiatePrefab(*registry, params);
        }
        if (name == "focus_entity") {
            return HandleFocusEntity(kernel, *registry, params);
        }
        if (name == "frame_selection") {
            return HandleFrameSelection(kernel, *registry, params);
        }
        if (name == "raycast_scene_view") {
            return HandleRaycastSceneView(kernel, *registry, params);
        }
        if (name == "place_asset_at_cursor") {
            return HandlePlaceAssetAtCursor(kernel, *registry, params);
        }
        if (name == "prefab.save") {
            return HandlePrefabSave(*registry, params);
        }
        if (name == "prefab.apply") {
            return HandlePrefabApply(*registry, params);
        }
        if (name == "prefab.unpack") {
            return HandlePrefabUnpack(*registry, params);
        }
        if (name == "material.assign") {
            return HandleMaterialAssign(*registry, params);
        }
        if (name == "light.create") {
            return HandleLightCreate(*registry, params);
        }
        if (name == "light.get") {
            return HandleLightGet(*registry, params);
        }
        if (name == "light.set") {
            return HandleLightSet(*registry, params);
        }
        if (name == "light.list") {
            return HandleLightList(*registry);
        }
        if (name == "camera.create") {
            return HandleCameraCreate(*registry, params);
        }
        if (name == "camera.get") {
            return HandleCameraGet(*registry, params);
        }
        if (name == "camera.set") {
            return HandleCameraSet(*registry, params);
        }
        if (name == "camera.list") {
            return HandleCameraList(*registry);
        }
        if (name == "terrain.create") {
            return HandleTerrainCreate(*registry, params);
        }
        if (name == "terrain.list") {
            return HandleTerrainList(*registry);
        }
        if (name == "terrain.apply_brush") {
            return HandleTerrainApplyBrush(*registry, params);
        }
        if (name == "terrain.save") {
            return HandleTerrainSaveLoad(*registry, params, true);
        }
        if (name == "terrain.load") {
            return HandleTerrainSaveLoad(*registry, params, false);
        }
        if (name == "terrain.get") {
            return HandleTerrainGet(*registry, params);
        }
        if (name == "terrain.set_dimensions") {
            return HandleTerrainSetDimensions(*registry, params);
        }
        if (name == "terrain.set_noise") {
            return HandleTerrainSetNoise(*registry, params);
        }
        if (name == "terrain.set_erosion") {
            return HandleTerrainSetErosion(*registry, params);
        }
        if (name == "terrain.run_erosion") {
            return HandleTerrainRunErosion(*registry, params);
        }
        if (name == "terrain.set_auto_splat") {
            return HandleTerrainSetAutoSplat(*registry, params);
        }
        if (name == "terrain.set_water") {
            return HandleTerrainSetWater(*registry, params);
        }
        if (name == "terrain.fit_water") {
            return HandleTerrainFitWater(*registry, params);
        }
        if (name == "terrain.apply_preset") {
            return HandleTerrainApplyPreset(*registry, params);
        }
        if (name == "terrain.regenerate") {
            return HandleTerrainRegenerate(*registry, params);
        }
        if (name == "terrain.set_layer") {
            return HandleTerrainSetLayer(*registry, params);
        }
        if (name == "terrain.get_foliage") {
            return HandleTerrainGetFoliage(*registry, params);
        }
        if (name == "terrain.set_foliage") {
            return HandleTerrainSetFoliage(*registry, params);
        }
        if (name == "terrain.add_foliage_layer") {
            return HandleTerrainAddFoliageLayer(*registry, params);
        }
        if (name == "terrain.set_foliage_layer") {
            return HandleTerrainSetFoliageLayer(*registry, params);
        }
        if (name == "terrain.delete_foliage_layer") {
            return HandleTerrainDeleteFoliageLayer(*registry, params);
        }
        if (name == "terrain.reset_foliage") {
            return HandleTerrainResetFoliage(*registry, params);
        }
        if (name == "effect_editor.preview_spawn") {
            return HandleEffectPreviewSpawn(*registry, params);
        }
        if (name == "save_scene") {
            return HandleSaveScene(kernel, params);
        }
        if (name == "load_scene") {
            return HandleLoadScene(kernel, params);
        }
        if (name == "terrain.open") {
            return HandleTerrainOpen(kernel, params);
        }
        if (name == "terrain.set_brush") {
            return HandleTerrainSetBrush(kernel, params);
        }
        if (name == "coingame.start") {
            const int   total = params.value("totalCoins", 10);
            const float limit = params.value("timeLimitSeconds", 600.0f);
            CoinGameSystem::Start(total, limit);
            return { { "started", true }, { "totalCoins", total }, { "timeLimitSeconds", limit } };
        }
        if (name == "coingame.reset") {
            CoinGameSystem::Reset();
            return { { "reset", true } };
        }
        if (name == "coingame.status") {
            return {
                { "active",        CoinGameSystem::IsActive() },
                { "coinCount",     CoinGameSystem::GetCoinCount() },
                { "totalCoins",    CoinGameSystem::GetTotalCoins() },
                { "remainingTime", CoinGameSystem::GetRemainingTime() },
            };
        }
        if (name == "coingame.tag_coins") {
            if (!registry) throw MakeError("no_registry", "No game registry available.");
            int tagged = 0;
            const auto nameTypeId = TypeManager::GetComponentTypeID<NameComponent>();
            std::vector<EntityID> toTag;
            for (Archetype* arch : registry->GetAllArchetypes()) {
                if (!arch->GetSignature().test(nameTypeId)) continue;
                auto* nameCol = arch->GetColumn(nameTypeId);
                const auto& entities = arch->GetEntities();
                for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                    auto* nc = static_cast<NameComponent*>(nameCol->Get(i));
                    if (nc && nc->name.rfind("Coin_", 0) == 0) {
                        toTag.push_back(entities[i]);
                    }
                }
            }
            for (EntityID e : toTag) {
                CoinTagComponent tag{};
                registry->AddComponent(e, tag);
                tagged++;
            }
            return { { "tagged", tagged } };
        }
        if (name == "play") {
            kernel.Play();
            return { { "mode", ModeToString(kernel.GetMode()) } };
        }
        if (name == "stop") {
            kernel.Stop();
            return { { "mode", ModeToString(kernel.GetMode()) } };
        }
        if (name == "pause") {
            kernel.Pause();
            return { { "mode", ModeToString(kernel.GetMode()) } };
        }
        if (name == "step") {
            kernel.Step();
            return { { "mode", ModeToString(kernel.GetMode()) } };
        }
        if (name == "player_editor.open") {
            return HandlePlayerEditorOpen(kernel, params);
        }
        if (name == "player_editor.get_status") {
            return HandlePlayerEditorGetStatus(kernel);
        }
        if (name == "player_editor.load_model") {
            return HandlePlayerEditorLoadModel(kernel, params);
        }
        if (name == "player_editor.get_state_machine") {
            return HandlePlayerEditorGetStateMachine(kernel);
        }
        if (name == "player_editor.add_state") {
            return HandlePlayerEditorAddState(kernel, params);
        }
        if (name == "player_editor.set_state") {
            return HandlePlayerEditorSetState(kernel, params);
        }
        if (name == "player_editor.delete_state") {
            return HandlePlayerEditorDeleteState(kernel, params);
        }
        if (name == "player_editor.add_transition") {
            return HandlePlayerEditorAddTransition(kernel, params);
        }
        if (name == "player_editor.set_transition") {
            return HandlePlayerEditorSetTransition(kernel, params);
        }
        if (name == "player_editor.delete_transition") {
            return HandlePlayerEditorDeleteTransition(kernel, params);
        }
        if (name == "player_editor.select") {
            return HandlePlayerEditorSelect(kernel, params);
        }
        if (name == "player_editor.set_default_state") {
            return HandlePlayerEditorSetDefaultState(kernel, params);
        }
        if (name == "player_editor.get_timeline") {
            return HandlePlayerEditorGetTimeline(kernel);
        }
        if (name == "player_editor.add_track") {
            return HandlePlayerEditorAddTrack(kernel, params);
        }
        if (name == "player_editor.delete_track") {
            return HandlePlayerEditorDeleteTrack(kernel, params);
        }
        if (name == "player_editor.set_playhead") {
            return HandlePlayerEditorSetPlayhead(kernel, params);
        }
        if (name == "player_editor.timeline_play") {
            return HandlePlayerEditorTimelinePlay(kernel, params);
        }
        if (name == "player_editor.save_prefab") {
            return HandlePlayerEditorSavePrefab(kernel, params);
        }
        if (name == "game_loop_editor.open") {
            return HandleGameLoopEditorOpen(kernel, params);
        }
        if (name == "game_loop_editor.get_status") {
            return HandleGameLoopEditorGetStatus(kernel);
        }
        if (name == "game_loop_editor.get_asset") {
            return HandleGameLoopEditorGetAsset(kernel);
        }
        if (name == "game_loop_editor.load") {
            return HandleGameLoopEditorLoad(kernel, params);
        }
        if (name == "game_loop_editor.save") {
            return HandleGameLoopEditorSave(kernel);
        }
        if (name == "game_loop_editor.validate") {
            return HandleGameLoopEditorValidate(kernel);
        }
        if (name == "game_loop_editor.add_node") {
            return HandleGameLoopEditorAddNode(kernel, params);
        }
        if (name == "game_loop_editor.set_node") {
            return HandleGameLoopEditorSetNode(kernel, params);
        }
        if (name == "game_loop_editor.delete_node") {
            return HandleGameLoopEditorDeleteNode(kernel, params);
        }
        if (name == "game_loop_editor.add_transition") {
            return HandleGameLoopEditorAddTransition(kernel, params);
        }
        if (name == "game_loop_editor.set_transition") {
            return HandleGameLoopEditorSetTransition(kernel, params);
        }
        if (name == "game_loop_editor.delete_transition") {
            return HandleGameLoopEditorDeleteTransition(kernel, params);
        }
        if (name == "game_loop_editor.select") {
            return HandleGameLoopEditorSelect(kernel, params);
        }
        if (name == "game_loop_editor.set_start_node") {
            return HandleGameLoopEditorSetStartNode(kernel, params);
        }
        if (name == "game_loop_editor.reverse_transition") {
            return HandleGameLoopEditorReverseTransition(kernel, params);
        }
        if (name == "game_loop_editor.fit_graph") {
            return HandleGameLoopEditorFitGraph(kernel);
        }
        if (name == "player_editor.get_sockets") {
            return HandlePlayerEditorGetSockets(kernel);
        }
        if (name == "player_editor.add_socket") {
            return HandlePlayerEditorAddSocket(kernel, params);
        }
        if (name == "player_editor.set_socket") {
            return HandlePlayerEditorSetSocket(kernel, params);
        }
        if (name == "player_editor.delete_socket") {
            return HandlePlayerEditorDeleteSocket(kernel, params);
        }
        if (name == "player_editor.get_colliders") {
            return HandlePlayerEditorGetColliders(kernel);
        }
        if (name == "player_editor.add_collider") {
            return HandlePlayerEditorAddCollider(kernel, params);
        }
        if (name == "player_editor.set_collider") {
            return HandlePlayerEditorSetCollider(kernel, params);
        }
        if (name == "player_editor.delete_collider") {
            return HandlePlayerEditorDeleteCollider(kernel, params);
        }
        if (name == "player_editor.get_animations") {
            return HandlePlayerEditorGetAnimations(kernel);
        }
        if (name == "player_editor.set_animator") {
            return HandlePlayerEditorSetAnimator(kernel, params);
        }
        if (name == "player_editor.get_input_map") {
            return HandlePlayerEditorGetInputMap(kernel);
        }
        if (name == "player_editor.add_action_binding") {
            return HandlePlayerEditorAddActionBinding(kernel, params);
        }
        if (name == "player_editor.set_action_binding") {
            return HandlePlayerEditorSetActionBinding(kernel, params);
        }
        if (name == "player_editor.delete_action_binding") {
            return HandlePlayerEditorDeleteActionBinding(kernel, params);
        }
        if (name == "player_editor.add_axis_binding") {
            return HandlePlayerEditorAddAxisBinding(kernel, params);
        }
        if (name == "player_editor.set_axis_binding") {
            return HandlePlayerEditorSetAxisBinding(kernel, params);
        }
        if (name == "player_editor.delete_axis_binding") {
            return HandlePlayerEditorDeleteAxisBinding(kernel, params);
        }

        // ---- Scene View ----
        if (name == "scene_view.get_state")           { return HandleSceneViewGetState(kernel); }
        if (name == "scene_view.set_camera")          { return HandleSceneViewSetCamera(kernel, params); }
        if (name == "scene_view.set_look_at")         { return HandleSceneViewSetLookAt(kernel, params); }
        if (name == "scene_view.set_gizmo_operation") { return HandleSceneViewSetGizmoOperation(kernel, params); }
        if (name == "scene_view.set_gizmo_space")     { return HandleSceneViewSetGizmoSpace(kernel, params); }
        if (name == "scene_view.set_shading_mode")    { return HandleSceneViewSetShadingMode(kernel, params); }
        if (name == "scene_view.set_mode")            { return HandleSceneViewSetMode(kernel, params); }
        if (name == "scene_view.set_visibility")      { return HandleSceneViewSetVisibility(kernel, params); }
        if (name == "scene_view.save_bookmark")       { return HandleSceneViewBookmark(kernel, params, true); }
        if (name == "scene_view.load_bookmark")       { return HandleSceneViewBookmark(kernel, params, false); }
        // ---- Lighting ----
        if (name == "lighting.open")                  { return HandleLightingOpen(kernel); }
        if (name == "lighting.get") {
            Registry* reg = kernel.GetGameRegistry();
            if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
            return HandleLightingGet(*reg);
        }
        if (name == "lighting.set_environment") {
            Registry* reg = kernel.GetGameRegistry();
            if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
            return HandleLightingSetEnvironment(*reg, params);
        }
        if (name == "lighting.set_post_effects") {
            Registry* reg = kernel.GetGameRegistry();
            if (!reg) throw MakeError("registry_unavailable", "Game registry is not available.");
            return HandleLightingSetPostEffects(*reg, params);
        }
        // ---- UI Editor ----
        if (name == "ui_editor.open")             { return HandleUIEditorOpen(kernel); }
        if (name == "ui_editor.get_state")        { return HandleUIEditorGetState(kernel); }
        if (name == "ui_editor.select")           { return HandleUIEditorSelect(kernel, params); }
        if (name == "ui_editor.create_canvas")    { return HandleUIEditorCreateCanvas(kernel); }
        if (name == "ui_editor.create_template")  { return HandleUIEditorCreateTemplate(kernel, params); }
        if (name == "ui_editor.create_part")      { return HandleUIEditorCreatePart(kernel, params); }
        if (name == "ui_editor.save_prefab")      { return HandleUIEditorPrefab(kernel, params, "save"); }
        if (name == "ui_editor.apply_prefab")     { return HandleUIEditorPrefab(kernel, params, "apply"); }
        if (name == "ui_editor.revert_prefab")    { return HandleUIEditorPrefab(kernel, params, "revert"); }
        if (name == "ui_editor.unpack_prefab")    { return HandleUIEditorPrefab(kernel, params, "unpack"); }
        if (name == "ui_editor.set_view")         { return HandleUIEditorSetView(kernel, params); }
        // ---- Sequencer ----
        if (name == "sequencer.open")             { return HandleSequencerOpen(kernel); }
        if (name == "sequencer.new")              { return HandleSequencerNew(kernel); }
        if (name == "sequencer.load")             { return HandleSequencerLoad(kernel, params); }
        if (name == "sequencer.save")             { return HandleSequencerSave(kernel, params); }
        if (name == "sequencer.get")              { return HandleSequencerGet(kernel); }
        if (name == "sequencer.set_playback")     { return HandleSequencerSetPlayback(kernel, params); }
        if (name == "sequencer.add_binding")      { return HandleSequencerAddBinding(kernel, params); }
        if (name == "sequencer.add_track")        { return HandleSequencerAddTrack(kernel, params); }
        if (name == "sequencer.add_master_track") { return HandleSequencerAddMasterTrack(kernel, params); }
        if (name == "sequencer.add_section")      { return HandleSequencerAddSection(kernel, params); }
        if (name == "sequencer.select")           { return HandleSequencerSelect(kernel, params); }
        if (name == "sequencer.set_params")       { return HandleSequencerSetSequenceParams(kernel, params); }

        // ---- scene.new ----
        if (name == "scene.new")                  { return HandleSceneNew(kernel, params); }

        // ---- editor.* ----
        if (name == "editor.undo")                { return HandleEditorUndo(kernel); }
        if (name == "editor.redo")                { return HandleEditorRedo(kernel); }
        if (name == "editor.focus_panel")         { return HandleEditorFocusPanel(kernel, params); }
        if (name == "editor.get_panels")          { return HandleEditorGetPanels(kernel); }
        if (name == "editor.show_panel")          { return HandleEditorShowPanel(kernel, params); }

        // ---- scene_view snap / grid / 2D ----
        if (name == "scene_view.get_snap")        { return HandleSceneViewGetSnap(kernel); }
        if (name == "scene_view.set_snap")        { return HandleSceneViewSetSnap(kernel, params); }
        if (name == "scene_view.set_grid")        { return HandleSceneViewSetGrid(kernel, params); }
        if (name == "scene_view.set_2d_view")     { return HandleSceneViewSet2DView(kernel, params); }
        if (name == "scene_view.align_camera")    { return HandleSceneViewAlignCamera(kernel); }

        // ---- game_view.* ----
        if (name == "game_view.get_state")        { return HandleGameViewGetState(kernel); }
        if (name == "game_view.set_resolution")   { return HandleGameViewSetResolution(kernel, params); }
        if (name == "game_view.set_overlay")      { return HandleGameViewSetOverlay(kernel, params); }

        // ---- model_serializer.* ----
        if (name == "model_serializer.build")     { return HandleModelSerializerBuild(kernel, params); }

        if (name == "fire_ui_button") {
            const std::string buttonId = params.value("buttonId", std::string{});
            if (buttonId.empty()) throw MakeError("missing_param", "buttonId is required.");
            kernel.GetFlowEventQueue().Push("ui.button.clicked", buttonId);
            return { { "fired", true }, { "buttonId", buttonId } };
        }
        if (name == "push_flow_event") {
            const std::string evtName  = params.value("name",  std::string{});
            const std::string evtValue = params.value("value", std::string{});
            if (evtName.empty()) throw MakeError("missing_param", "name is required.");
            kernel.GetFlowEventQueue().Push(evtName, evtValue);
            return { { "pushed", true }, { "name", evtName }, { "value", evtValue } };
        }

        if (name == "gameflow.register")          { return HandleGameFlowRegister(kernel, params); }

        // ---- gameloop_editor.* ----
        if (name == "gameloop_editor.open")       { return HandleGameLoopEditorOpen(kernel, params); }
        if (name == "gameloop_editor.get_status") { return HandleGameLoopEditorGetStatus(kernel); }
        if (name == "gameloop_editor.get_asset")  { return HandleGameLoopEditorGetAsset(kernel); }
        if (name == "gameloop_editor.load")       { return HandleGameLoopEditorLoad(kernel, params); }
        if (name == "gameloop_editor.save")       { return HandleGameLoopEditorSave(kernel); }
        if (name == "gameloop_editor.validate")   { return HandleGameLoopEditorValidate(kernel); }

        throw MakeError("unknown_command", "Unknown AI automation command.", { { "command", name } });
    }

    bool ReadJsonFile(const std::filesystem::path& path, json& out, json& error)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) {
            error = MakeError("file_not_found", "Failed to open command file.", { { "path", path.string() } });
            return false;
        }

        try {
            ifs >> out;
            return true;
        }
        catch (const std::exception& e) {
            error = MakeError("invalid_json", e.what(), { { "path", path.string() } });
            return false;
        }
    }

    void WriteJsonFile(const std::filesystem::path& path, const json& value)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        ofs << value.dump(2);
    }

    void AppendJsonLine(const std::filesystem::path& path, const json& value)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream ofs(path, std::ios::binary | std::ios::app);
        ofs << value.dump() << "\n";
    }

    struct AutomationSessionState
    {
        struct FileBackupRecord
        {
            std::filesystem::path path;
            std::filesystem::path backupPath;
        };

        bool active = false;
        std::string id;
        std::string name;
        std::string goal;
        std::filesystem::path dir;
        std::filesystem::path eventsPath;
        std::filesystem::path manifestPath;
        std::chrono::system_clock::time_point startedAt{};
        uint64_t startFrame = 0;
        uint64_t startEcsRevision = 0;
        size_t startUndoCount = 0;
        uint64_t commandCount = 0;
        bool autoCaptureAfterCommand = false;
        std::vector<std::string> captureTargets;
        bool fileBackupEnabled = true;
        std::vector<std::string> backupRoots;
        std::vector<std::string> backupExtensions;
        std::vector<FileBackupRecord> fileBackups;
    };

    AutomationSessionState g_automationSession;

    bool IsSessionCommandName(const std::string& name)
    {
        return name == "ai_session.begin" ||
               name == "ai_session.status" ||
               name == "ai_session.end" ||
               name == "ai_session.rollback";
    }

    std::string LowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    bool SessionExtensionAllowed(const std::filesystem::path& path, const std::vector<std::string>& extensions)
    {
        const std::string ext = LowerCopy(path.extension().string());
        for (const std::string& allowed : extensions) {
            if (ext == LowerCopy(allowed)) {
                return true;
            }
        }
        return false;
    }

    std::string SessionRelativePathKey(const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::path rel = std::filesystem::relative(path, std::filesystem::current_path(), ec);
        if (ec) {
            rel = path;
        }
        return rel.generic_string();
    }

    void BackupSessionFiles()
    {
        g_automationSession.fileBackups.clear();
        if (!g_automationSession.fileBackupEnabled) {
            return;
        }

        const std::filesystem::path backupRoot = g_automationSession.dir / "file_backups";
        for (const std::string& rootText : g_automationSession.backupRoots) {
            const std::filesystem::path root = rootText.empty() ? std::filesystem::path("Data") : std::filesystem::path(rootText);
            std::error_code ec;
            if (!std::filesystem::exists(root, ec)) {
                continue;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_regular_file(ec)) {
                    continue;
                }
                const std::filesystem::path source = entry.path();
                if (!SessionExtensionAllowed(source, g_automationSession.backupExtensions)) {
                    continue;
                }
                const std::string relKey = SessionRelativePathKey(source);
                const std::filesystem::path backupPath = backupRoot / relKey;
                std::filesystem::create_directories(backupPath.parent_path(), ec);
                std::filesystem::copy_file(source, backupPath, std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) {
                    g_automationSession.fileBackups.push_back({ source, backupPath });
                }
            }
        }
    }

    json RestoreSessionFiles()
    {
        json restored = json::array();
        json deleted = json::array();
        json errors = json::array();
        if (!g_automationSession.fileBackupEnabled) {
            return {
                { "enabled", false },
                { "restored", restored },
                { "deletedCreated", deleted },
                { "errors", errors }
            };
        }

        std::unordered_map<std::string, std::filesystem::path> baseline;
        for (const auto& record : g_automationSession.fileBackups) {
            baseline[SessionRelativePathKey(record.path)] = record.path;
        }

        for (const std::string& rootText : g_automationSession.backupRoots) {
            const std::filesystem::path root = rootText.empty() ? std::filesystem::path("Data") : std::filesystem::path(rootText);
            std::error_code ec;
            if (!std::filesystem::exists(root, ec)) {
                continue;
            }
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_regular_file(ec)) {
                    continue;
                }
                const std::filesystem::path current = entry.path();
                if (!SessionExtensionAllowed(current, g_automationSession.backupExtensions)) {
                    continue;
                }
                const std::string relKey = SessionRelativePathKey(current);
                if (baseline.find(relKey) == baseline.end()) {
                    std::filesystem::remove(current, ec);
                    if (ec) {
                        errors.push_back({ { "path", relKey }, { "error", ec.message() } });
                        ec.clear();
                    }
                    else {
                        deleted.push_back(relKey);
                    }
                }
            }
        }

        for (const auto& record : g_automationSession.fileBackups) {
            std::error_code ec;
            std::filesystem::create_directories(record.path.parent_path(), ec);
            std::filesystem::copy_file(record.backupPath, record.path, std::filesystem::copy_options::overwrite_existing, ec);
            const std::string relKey = SessionRelativePathKey(record.path);
            if (ec) {
                errors.push_back({ { "path", relKey }, { "error", ec.message() } });
            }
            else {
                restored.push_back(relKey);
            }
        }

        return {
            { "enabled", true },
            { "restored", restored },
            { "deletedCreated", deleted },
            { "errors", errors }
        };
    }

    json AutomationSessionManifest(EngineKernel& kernel)
    {
        json out = {
            { "active", g_automationSession.active },
            { "id", g_automationSession.id },
            { "name", g_automationSession.name },
            { "goal", g_automationSession.goal },
            { "dir", g_automationSession.dir.empty() ? json(nullptr) : json(ToGenericProjectPath(g_automationSession.dir)) },
            { "eventsPath", g_automationSession.eventsPath.empty() ? json(nullptr) : json(ToGenericProjectPath(g_automationSession.eventsPath)) },
            { "startFrame", g_automationSession.startFrame },
            { "startEcsRevision", g_automationSession.startEcsRevision },
            { "startUndoCount", g_automationSession.startUndoCount },
            { "currentEcsRevision", UndoSystem::Instance().GetECSRevision() },
            { "currentUndoCount", UndoSystem::Instance().GetECSUndoCount() },
            { "commandCount", g_automationSession.commandCount },
            { "autoCaptureAfterCommand", g_automationSession.autoCaptureAfterCommand },
            { "captureTargets", g_automationSession.captureTargets },
            { "fileBackupEnabled", g_automationSession.fileBackupEnabled },
            { "backupRoots", g_automationSession.backupRoots },
            { "backupExtensions", g_automationSession.backupExtensions },
            { "fileBackupCount", g_automationSession.fileBackups.size() }
        };
        out["engineState"] = HandleGetEngineState(kernel);
        return out;
    }

    json HandleAISessionBegin(EngineKernel& kernel, const json& params)
    {
        if (g_automationSession.active && !params.value("force", false)) {
            throw MakeError("session_active", "An AI automation session is already active.", {
                { "sessionId", g_automationSession.id }
            });
        }

        const std::string name = params.value("name", std::string("AI Session"));
        const std::string goal = params.value("goal", std::string{});
        const std::string explicitId = params.value("sessionId", std::string{});
        const std::string sessionId = explicitId.empty()
            ? SanitizeFileStem(name + "_" + MakeTimestampSuffix())
            : SanitizeFileStem(explicitId);
        const std::filesystem::path dir = std::filesystem::path("Saved") / "AI" / "sessions" / sessionId;

        g_automationSession = AutomationSessionState{};
        g_automationSession.active = true;
        g_automationSession.id = sessionId;
        g_automationSession.name = name;
        g_automationSession.goal = goal;
        g_automationSession.dir = dir;
        g_automationSession.eventsPath = dir / "events.jsonl";
        g_automationSession.manifestPath = dir / "session.json";
        g_automationSession.startedAt = std::chrono::system_clock::now();
        g_automationSession.startFrame = kernel.GetTime().frameCount;
        g_automationSession.startEcsRevision = UndoSystem::Instance().GetECSRevision();
        g_automationSession.startUndoCount = UndoSystem::Instance().GetECSUndoCount();
        g_automationSession.autoCaptureAfterCommand = params.value("autoCaptureAfterCommand", false);
        g_automationSession.captureTargets = JsonStringList(params, "captureTargets");
        if (g_automationSession.captureTargets.empty()) {
            g_automationSession.captureTargets = { "window" };
        }
        g_automationSession.fileBackupEnabled = params.value("backupFiles", true);
        g_automationSession.backupRoots = JsonStringList(params, "backupRoots");
        if (g_automationSession.backupRoots.empty()) {
            g_automationSession.backupRoots = { "Data" };
        }
        g_automationSession.backupExtensions = JsonStringList(params, "backupExtensions");
        if (g_automationSession.backupExtensions.empty()) {
            g_automationSession.backupExtensions = {
                ".scene",
                ".prefab",
                ".material",
                ".mat",
                ".terrain",
                ".effectgraph",
                ".json",
                ".inputmap",
                ".inputprofile",
                ".gameflow"
            };
        }

        std::error_code ec;
        std::filesystem::create_directories(dir / "screenshots", ec);
        std::filesystem::create_directories(dir / "file_backups", ec);
        BackupSessionFiles();

        json manifest = AutomationSessionManifest(kernel);
        manifest["event"] = "begin";
        manifest["createdAt"] = MakeTimestampSuffix();
        WriteJsonFile(g_automationSession.manifestPath, manifest);
        AppendJsonLine(g_automationSession.eventsPath, {
            { "event", "begin" },
            { "timestamp", MakeTimestampSuffix() },
            { "manifest", manifest }
        });
        return manifest;
    }

    json HandleAISessionStatus(EngineKernel& kernel)
    {
        return AutomationSessionManifest(kernel);
    }

    json HandleAISessionRollback(EngineKernel& kernel, const json& params)
    {
        if (!g_automationSession.active) {
            throw MakeError("session_not_active", "No AI automation session is active.");
        }
        Registry* registry = kernel.GetGameRegistry();
        if (!registry) {
            throw MakeError("operation_not_allowed", "No active registry is available for rollback.");
        }

        const size_t currentUndoCount = UndoSystem::Instance().GetECSUndoCount();
        size_t undoSteps = 0;
        if (currentUndoCount > g_automationSession.startUndoCount) {
            undoSteps = currentUndoCount - g_automationSession.startUndoCount;
        }
        if (params.contains("undoSteps")) {
            const int requested = (std::max)(0, params.value("undoSteps", 0));
            undoSteps = (std::min)(undoSteps, static_cast<size_t>(requested));
        }

        for (size_t i = 0; i < undoSteps; ++i) {
            UndoSystem::Instance().Undo(*registry);
        }

        std::vector<EntityID> aliveSelection;
        for (EntityID selected : EditorSelection::Instance().GetSelectedEntities()) {
            if (!Entity::IsNull(selected) && registry->IsAlive(selected)) {
                aliveSelection.push_back(selected);
            }
        }
        if (aliveSelection.empty()) {
            EditorSelection::Instance().Clear();
        }
        else {
            EditorSelection::Instance().SetEntitySelection(aliveSelection, aliveSelection.front());
        }

        json fileRestore = params.value("restoreFiles", true) ? RestoreSessionFiles() : json({ { "enabled", false } });

        json result = {
            { "sessionId", g_automationSession.id },
            { "undone", undoSteps },
            { "files", std::move(fileRestore) },
            { "currentUndoCount", UndoSystem::Instance().GetECSUndoCount() },
            { "currentEcsRevision", UndoSystem::Instance().GetECSRevision() },
            { "state", HandleGetEngineState(kernel) }
        };
        AppendJsonLine(g_automationSession.eventsPath, {
            { "event", "rollback" },
            { "timestamp", MakeTimestampSuffix() },
            { "result", result }
        });
        WriteJsonFile(g_automationSession.manifestPath, AutomationSessionManifest(kernel));
        return result;
    }

    json HandleAISessionEnd(EngineKernel& kernel, const json& params)
    {
        if (!g_automationSession.active) {
            throw MakeError("session_not_active", "No AI automation session is active.");
        }

        const bool success = params.value("success", true);
        json result = AutomationSessionManifest(kernel);
        result["event"] = "end";
        result["success"] = success;
        result["notes"] = params.value("notes", std::string{});
        result["endedAt"] = MakeTimestampSuffix();
        result["active"] = false;
        AppendJsonLine(g_automationSession.eventsPath, {
            { "event", "end" },
            { "timestamp", MakeTimestampSuffix() },
            { "result", result }
        });
        WriteJsonFile(g_automationSession.manifestPath, result);
        g_automationSession.active = false;
        g_ecsWatchEnabled          = false;
        g_lastBroadcastEcsRevision = UINT64_MAX;
        return result;
    }

    json ExecuteSessionCommand(EngineKernel& kernel, const json& command)
    {
        const std::string name = command.value("command", std::string{});
        const json params = command.value("params", json::object());
        if (name == "ai_session.begin") {
            return HandleAISessionBegin(kernel, params);
        }
        if (name == "ai_session.status") {
            return HandleAISessionStatus(kernel);
        }
        if (name == "ai_session.rollback") {
            return HandleAISessionRollback(kernel, params);
        }
        if (name == "ai_session.end") {
            return HandleAISessionEnd(kernel, params);
        }
        throw MakeError("unknown_command", "Unknown AI session command.", { { "command", name } });
    }

    json RecoveryStateToJson(EditorLayer& editor)
    {
        return {
            { "hasCandidate", editor.HasRecoveryCandidateForAutomation() },
            { "autosavePath", editor.GetRecoveryAutosavePathForAutomation().empty()
                ? json(nullptr)
                : json(ToGenericProjectPath(editor.GetRecoveryAutosavePathForAutomation())) },
            { "scenePath", editor.GetRecoveryScenePathForAutomation().empty()
                ? json(nullptr)
                : json(ToGenericProjectPath(editor.GetRecoveryScenePathForAutomation())) }
        };
    }

    json HandleRecoveryGetState(EngineKernel& kernel, const json& params)
    {
        EditorLayer* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        if (params.value("refresh", false)) {
            editor->CheckRecoveryCandidateFromAutomation();
        }
        return RecoveryStateToJson(*editor);
    }

    json HandleRecoveryRestore(EngineKernel& kernel)
    {
        EditorLayer* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const json before = RecoveryStateToJson(*editor);
        const bool restored = editor->RecoverAutosaveFromAutomation();
        return {
            { "restored", restored },
            { "before", before },
            { "after", RecoveryStateToJson(*editor) },
            { "engineState", HandleGetEngineState(kernel) }
        };
    }

    json HandleRecoveryDismiss(EngineKernel& kernel)
    {
        EditorLayer* editor = kernel.GetEditorLayer();
        if (!editor) {
            throw MakeError("operation_not_allowed", "EditorLayer is not available.");
        }
        const json before = RecoveryStateToJson(*editor);
        const bool dismissed = editor->DismissAutosaveRecoveryFromAutomation();
        return {
            { "dismissed", dismissed },
            { "before", before },
            { "after", RecoveryStateToJson(*editor) }
        };
    }

    void RecordAutomationSessionCommand(EngineKernel& kernel,
                                        const json& command,
                                        const json& response,
                                        const json& beforeState,
                                        uint64_t beforeRevision,
                                        size_t beforeUndoCount)
    {
        if (!g_automationSession.active) {
            return;
        }

        ++g_automationSession.commandCount;
        json event = {
            { "event", "command" },
            { "index", g_automationSession.commandCount },
            { "timestamp", MakeTimestampSuffix() },
            { "command", command },
            { "ok", response.value("ok", false) },
            { "before", {
                { "ecsRevision", beforeRevision },
                { "undoCount", beforeUndoCount },
                { "engineState", beforeState }
            } },
            { "after", {
                { "ecsRevision", UndoSystem::Instance().GetECSRevision() },
                { "undoCount", UndoSystem::Instance().GetECSUndoCount() },
                { "engineState", HandleGetEngineState(kernel) }
            } },
            { "response", response }
        };

        if (g_automationSession.autoCaptureAfterCommand || !response.value("ok", false)) {
            json shots = json::array();
            for (const std::string& target : g_automationSession.captureTargets) {
                try {
                    const std::filesystem::path path =
                        g_automationSession.dir / "screenshots" /
                        (std::to_string(g_automationSession.commandCount) + "_" + SanitizeFileStem(target) + ".bmp");
                    json captureParams = {
                        { "target", target },
                        { "path", path.generic_string() },
                        { "format", "bmp" },
                        { "inline", false }
                    };
                    shots.push_back(HandleCaptureScreenshot(kernel, captureParams, path));
                }
                catch (const std::exception& e) {
                    shots.push_back({
                        { "target", target },
                        { "ok", false },
                        { "error", e.what() }
                    });
                }
                catch (...) {
                    shots.push_back({
                        { "target", target },
                        { "ok", false },
                        { "error", "unknown capture error" }
                    });
                }
            }
            event["screenshots"] = std::move(shots);
        }

        AppendJsonLine(g_automationSession.eventsPath, event);
        WriteJsonFile(g_automationSession.manifestPath, AutomationSessionManifest(kernel));
    }

    json ExecuteAutomationCommand(EngineKernel& kernel, const json& command)
    {
        if (!command.is_object()) {
            return MakeResult(json::object(), false, nullptr,
                MakeError("invalid_command", "Command payload must be a JSON object."));
        }

        int version = kProtocolVersion;
        try {
            version = command.value("version", kProtocolVersion);
        }
        catch (const std::exception& e) {
            return MakeResult(command, false, nullptr,
                MakeError("invalid_command", "Command version must be an integer.", { { "reason", e.what() } }));
        }

        if (version != kProtocolVersion) {
            return MakeResult(command, false, nullptr,
                MakeError("unsupported_version", "Unsupported command version."));
        }

        try {
            const std::string name = command.value("command", std::string{});
            if (IsSessionCommandName(name)) {
                json result = ExecuteSessionCommand(kernel, command);
                return MakeResult(command, true, std::move(result), nullptr);
            }

            const bool recordSession = g_automationSession.active;
            const uint64_t beforeRevision = UndoSystem::Instance().GetECSRevision();
            const size_t beforeUndoCount = UndoSystem::Instance().GetECSUndoCount();
            json beforeState = recordSession ? HandleGetEngineState(kernel) : json(nullptr);
            json result = DispatchCommand(kernel, command);
            json response = MakeResult(command, true, std::move(result), nullptr);
            if (recordSession) {
                RecordAutomationSessionCommand(kernel, command, response, beforeState, beforeRevision, beforeUndoCount);
            }
            return response;
        }
        catch (const json& jsonError) {
            json response = MakeResult(command, false, nullptr, jsonError);
            if (g_automationSession.active && !IsSessionCommandName(command.value("command", std::string{}))) {
                RecordAutomationSessionCommand(
                    kernel,
                    command,
                    response,
                    HandleGetEngineState(kernel),
                    UndoSystem::Instance().GetECSRevision(),
                    UndoSystem::Instance().GetECSUndoCount());
            }
            return response;
        }
        catch (const std::exception& e) {
            json response = MakeResult(command, false, nullptr, MakeError("internal_error", e.what()));
            if (g_automationSession.active && !IsSessionCommandName(command.value("command", std::string{}))) {
                RecordAutomationSessionCommand(
                    kernel,
                    command,
                    response,
                    HandleGetEngineState(kernel),
                    UndoSystem::Instance().GetECSRevision(),
                    UndoSystem::Instance().GetECSUndoCount());
            }
            return response;
        }
        catch (...) {
            json response = MakeResult(command, false, nullptr, MakeError("internal_error", "Unknown exception."));
            if (g_automationSession.active && !IsSessionCommandName(command.value("command", std::string{}))) {
                RecordAutomationSessionCommand(
                    kernel,
                    command,
                    response,
                    HandleGetEngineState(kernel),
                    UndoSystem::Instance().GetECSRevision(),
                    UndoSystem::Instance().GetECSUndoCount());
            }
            return response;
        }
    }
}

AIAutomationService::AIAutomationService() = default;
AIAutomationService::~AIAutomationService() = default;

void AIAutomationService::Initialize()
{
    m_lastStateWriteTime    = {};
    m_lastEcsBroadcastTime  = {};
    m_rootDir = std::filesystem::path("Saved") / "AI";
    m_commandsDir = m_rootDir / "commands";
    m_processingDir = m_rootDir / "processing";
    m_resultsDir = m_rootDir / "results";
    m_screenshotsDir = m_rootDir / "screenshots";
    m_stateDir = m_rootDir / "state";

    std::error_code ec;
    std::filesystem::create_directories(m_commandsDir, ec);
    std::filesystem::create_directories(m_processingDir, ec);
    std::filesystem::create_directories(m_resultsDir, ec);
    std::filesystem::create_directories(m_screenshotsDir, ec);
    std::filesystem::create_directories(m_stateDir, ec);
    LOG_INFO("[AIAutomation] Initialized file command interface at %s", m_rootDir.string().c_str());

    m_webSocketServer = std::make_unique<WebSocketServer>(9876);
    if (m_webSocketServer->Start()) {
        LOG_INFO("[AIAutomation] WebSocket command server listening on ws://127.0.0.1:9876");
    }
    else {
        LOG_WARN("[AIAutomation] Failed to start WebSocket command server on ws://127.0.0.1:9876");
        m_webSocketServer.reset();
    }
}

void AIAutomationService::Finalize()
{
    if (m_webSocketServer) {
        m_webSocketServer->Stop();
        m_webSocketServer.reset();
    }
}

bool AIAutomationService::TryStartPendingEffectMultiTimeReview(EngineKernel& kernel, const json& command, const std::string& clientId)
{
    if (!command.is_object() || command.value("command", std::string{}) != "effect_editor.capture_multi_time_review") {
        return false;
    }

    auto sendError = [&](const json& error) {
        if (m_webSocketServer) {
            m_webSocketServer->SendToClient(clientId, MakeResult(command, false, nullptr, error).dump());
        }
    };

    try {
        if (m_pendingEffectMultiTimeReview.active) {
            sendError(MakeError("operation_busy", "An Effect Editor multi-time review is already running."));
            return true;
        }

        int version = kProtocolVersion;
        try {
            version = command.value("version", kProtocolVersion);
        }
        catch (const std::exception& e) {
            sendError(MakeError("invalid_command", "Command version must be an integer.", { { "reason", e.what() } }));
            return true;
        }
        if (version != kProtocolVersion) {
            sendError(MakeError("unsupported_version", "Unsupported command version."));
            return true;
        }

        Registry* registry = kernel.GetGameRegistry();
        if (!registry) {
            sendError(MakeError("operation_not_allowed", "No active registry is available for Effect Editor review."));
            return true;
        }

        EditorLayer* editor = kernel.GetEditorLayer();
        if (!editor) {
            sendError(MakeError("operation_not_allowed", "EditorLayer is not available."));
            return true;
        }

        const json params = command.value("params", json::object());
        if (params.contains("path") && !params.value("path", std::string{}).empty()) {
            const std::filesystem::path path = ResolveEffectGraphPath(params, PathAccess::ReadAsset, true);
            if (!editor->OpenEffectEditorFromAutomation(path)) {
                sendError(MakeError("effect_open_failed", "Failed to open effect graph in Effect Editor.", {
                    { "path", ToGenericProjectPath(path) }
                }));
                return true;
            }
        }

        EffectEditorPanel& panel = editor->GetEffectEditorPanel();
        if (params.value("compile", true) && !panel.CompileFromAutomation()) {
            sendError(MakeError("effect_compile_failed", "Effect Editor document did not compile."));
            return true;
        }

        std::vector<float> times;
        if (params.contains("times") && params["times"].is_array()) {
            for (const json& t : params["times"]) {
                times.push_back(t.get<float>());
            }
        }
        if (times.empty()) {
            times = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        }

        editor->FocusPanelAutomation(EditorLayer::WindowFocusTarget::EffectEditor);
        if (!panel.SeekTimelineFromAutomation(registry, times.front(), params.value("paused", true))) {
            sendError(MakeError("effect_timeline_seek_failed", "Failed to seek Effect Editor timeline.", { { "time", times.front() } }));
            return true;
        }

        m_pendingEffectMultiTimeReview.active = true;
        m_pendingEffectMultiTimeReview.clientId = clientId;
        m_pendingEffectMultiTimeReview.command = command;
        m_pendingEffectMultiTimeReview.params = params;
        m_pendingEffectMultiTimeReview.frames = json::array();
        m_pendingEffectMultiTimeReview.times = std::move(times);
        m_pendingEffectMultiTimeReview.index = 0;
        m_pendingEffectMultiTimeReview.waitFrames = (std::max)(3, params.value("settleFrames", 2));
        m_pendingEffectMultiTimeReview.stem = params.value("stem", std::string("effect_multi_review"));
        m_pendingEffectMultiTimeReview.dir = params.value("dir", std::string("Saved/AI/screenshots/effect_review"));
        m_pendingEffectMultiTimeReview.format = params.value("format", std::string("bmp"));
        m_pendingEffectMultiTimeReview.target = params.value("target", std::string("effect_editor"));
        return true;
    }
    catch (const json& jsonError) {
        sendError(jsonError);
        return true;
    }
    catch (const std::exception& e) {
        sendError(MakeError("internal_error", e.what()));
        return true;
    }
    catch (...) {
        sendError(MakeError("internal_error", "Unknown exception."));
        return true;
    }
}

void AIAutomationService::ProcessPendingEffectMultiTimeReview(EngineKernel& kernel)
{
    if (!m_pendingEffectMultiTimeReview.active || !m_webSocketServer) {
        return;
    }

    PendingEffectMultiTimeReview& job = m_pendingEffectMultiTimeReview;
    if (job.waitFrames > 0) {
        --job.waitFrames;
        if (job.waitFrames > 0) {
            return;
        }
    }

    auto finish = [&](bool ok, json result, json error) {
        m_webSocketServer->SendToClient(job.clientId, MakeResult(job.command, ok, std::move(result), std::move(error)).dump());
        job = PendingEffectMultiTimeReview{};
    };

    try {
        Registry* registry = kernel.GetGameRegistry();
        if (!registry) {
            finish(false, nullptr, MakeError("operation_not_allowed", "No active registry is available for Effect Editor review."));
            return;
        }

        EditorLayer* editor = kernel.GetEditorLayer();
        if (!editor) {
            finish(false, nullptr, MakeError("operation_not_allowed", "EditorLayer is not available."));
            return;
        }

        const float t = job.times[job.index];
        const std::filesystem::path path = job.dir / (job.stem + "_t" + std::to_string(job.index) + "." + job.format);
        json captureParams = {
            { "target", job.target },
            { "path", path.generic_string() },
            { "format", job.format },
            { "inline", job.params.value("inline", false) }
        };
        json screenshot = HandleCaptureScreenshot(kernel, captureParams, path);
        ImageBuffer image = CaptureAutomationTargetImage(kernel, job.target);
        job.frames.push_back({
            { "time", t },
            { "screenshot", std::move(screenshot) },
            { "metrics", AnalyzeImageBuffer(image) },
            { "assertions", HandleEffectAssertPreviewVisible(kernel, *registry, job.params) }
        });

        ++job.index;
        if (job.index >= job.times.size()) {
            json result = {
                { "frames", std::move(job.frames) },
                { "state", EffectEditorStateToJson(*editor, registry, job.params.value("includeGraph", false)) },
                { "async", true },
                { "settleFrames", (std::max)(2, job.params.value("settleFrames", 2)) }
            };
            finish(true, std::move(result), nullptr);
            return;
        }

        const float nextTime = job.times[job.index];
        EffectEditorPanel& panel = editor->GetEffectEditorPanel();
        if (!panel.SeekTimelineFromAutomation(registry, nextTime, job.params.value("paused", true))) {
            finish(false, nullptr, MakeError("effect_timeline_seek_failed", "Failed to seek Effect Editor timeline.", { { "time", nextTime } }));
            return;
        }
        job.waitFrames = (std::max)(2, job.params.value("settleFrames", 2));
    }
    catch (const json& jsonError) {
        finish(false, nullptr, jsonError);
    }
    catch (const std::exception& e) {
        finish(false, nullptr, MakeError("internal_error", e.what()));
    }
    catch (...) {
        finish(false, nullptr, MakeError("internal_error", "Unknown exception."));
    }
}

void AIAutomationService::ProcessPendingCommands(EngineKernel& kernel)
{
    ProcessPendingInjectedInputs(kernel);
    ProcessPendingEffectMultiTimeReview(kernel);

    if (m_webSocketServer && m_webSocketServer->IsRunning()) {
        constexpr int kMaxWebSocketMessagesPerTick = 64;
        int processedMessages = 0;

        WebSocketServer::Message message;
        while (processedMessages < kMaxWebSocketMessagesPerTick && m_webSocketServer->PollMessage(message)) {
            json response;
            try {
                const json command = json::parse(message.text);
                if (TryStartPendingEffectMultiTimeReview(kernel, command, message.clientId)) {
                    ++processedMessages;
                    break;
                }
                response = ExecuteAutomationCommand(kernel, command);
            }
            catch (const std::exception& e) {
                response = MakeResult(json::object(), false, nullptr, MakeError("invalid_json", e.what()));
            }
            catch (...) {
                response = MakeResult(json::object(), false, nullptr, MakeError("invalid_json", "Unknown JSON parse error."));
            }

            m_webSocketServer->SendToClient(message.clientId, response.dump());
            ++processedMessages;
        }

        if (processedMessages == kMaxWebSocketMessagesPerTick) {
            LOG_WARN("[AIAutomation] WebSocket command queue reached per-frame processing limit.");
        }

        if (g_ecsWatchEnabled && m_webSocketServer->GetConnectedClientCount() > 0) {
            const uint64_t rev = UndoSystem::Instance().GetECSRevision();
            if (rev != g_lastBroadcastEcsRevision) {
                constexpr auto kEcsBroadcastInterval = std::chrono::milliseconds(100);
                const auto ecsBroadcastNow = std::chrono::steady_clock::now();
                if (ecsBroadcastNow - m_lastEcsBroadcastTime >= kEcsBroadcastInterval) {
                    g_lastBroadcastEcsRevision = rev;
                    m_lastEcsBroadcastTime = ecsBroadcastNow;
                    const json evt = {
                        { "event",       "ecs.changed" },
                        { "ecsRevision", rev },
                        { "timestamp",   MakeTimestampSuffix() }
                    };
                    m_webSocketServer->BroadcastEvent(evt.dump());
                }
            }
        }
    }

    if (m_commandsDir.empty()) {
        return;
    }

    std::vector<std::filesystem::path> commandFiles;
    std::error_code ec;
    if (!std::filesystem::exists(m_commandsDir, ec)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_commandsDir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            commandFiles.push_back(entry.path());
        }
    }
    std::sort(commandFiles.begin(), commandFiles.end());

    for (const auto& commandPath : commandFiles) {
        const std::filesystem::path processingPath =
            m_processingDir / (commandPath.stem().string() + "_" + MakeTimestampSuffix() + ".json");

        std::error_code moveEc;
        std::filesystem::rename(commandPath, processingPath, moveEc);
        const std::filesystem::path activePath = moveEc ? commandPath : processingPath;

        json command;
        json error;
        json response;
        if (!ReadJsonFile(activePath, command, error)) {
            response = MakeResult(json{ { "id", activePath.stem().string() }, { "command", "" } }, false, nullptr, error);
        }
        else {
            response = ExecuteAutomationCommand(kernel, command);
        }

        const std::string resultName = SanitizeFileStem(JsonStringValue(command, "id", activePath.stem().string()));
        WriteJsonFile(m_resultsDir / (resultName + ".json"), response);

        std::error_code removeEc;
        std::filesystem::remove(activePath, removeEc);
    }

    const auto now = std::chrono::steady_clock::now();
    constexpr auto kStateWriteInterval = std::chrono::seconds(2);
    if (now - m_lastStateWriteTime >= kStateWriteInterval) {
        try {
            json state = HandleGetEngineState(kernel);
            state["version"] = kProtocolVersion;
            WriteJsonFile(m_stateDir / "latest_editor_state.json", state);
            m_lastStateWriteTime = now;
        }
        catch (...) {
        }
    }
}
