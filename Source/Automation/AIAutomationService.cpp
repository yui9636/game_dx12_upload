#include "Automation/AIAutomationService.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <nlohmann/json.hpp>
#include <windows.h>

#include "Archetype/Archetype.h"
#include "Asset/PrefabSystem.h"
#include "Asset/AssetManager.h"
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

    json MakeResult(const json& command, bool ok, json result, json error)
    {
        json out;
        out["version"] = kProtocolVersion;
        out["id"] = command.value("id", std::string{});
        out["command"] = command.value("command", std::string{});
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
        std::filesystem::path path = params.value("path", std::string{});
        if (path.empty()) {
            path = defaultPath;
        }
        if (path.extension().empty()) {
            path += ".bmp";
        }

        const std::filesystem::path safePath = ResolveProjectPath(path.string(), PathAccess::AutomationFile, false);

        ImageBuffer clientImage;
        if (!CaptureBackBuffer(clientImage) && !CaptureClientArea(Graphics::Instance().GetWindowHandle(), clientImage)) {
            throw MakeError("capture_failed", "Failed to capture the engine window back buffer or client area.");
        }

        ImageBuffer outputImage = clientImage;
        if (target == "scene_view") {
            outputImage = CropImage(clientImage, editor->GetSceneViewRect());
        }
        else if (target == "game_view") {
            outputImage = CropImage(clientImage, editor->GetGameViewRect());
        }
        else if (target == "window" || target == "display" || target == "client") {
            outputImage = std::move(clientImage);
        }
        else {
            throw MakeError("invalid_param", "target must be window, display, client, scene_view, or game_view.", {
                { "target", target }
            });
        }

        WriteBmp24(safePath, outputImage);
        return {
            { "path", ToGenericProjectPath(safePath) },
            { "target", target },
            { "width", outputImage.width },
            { "height", outputImage.height }
        };
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

    json DispatchCommand(EngineKernel& kernel, const json& command)
    {
        const std::string name = command.value("command", std::string{});
        const json params = command.value("params", json::object());
        Registry* registry = kernel.GetGameRegistry();

        if (name == "ping") {
            return HandlePing();
        }
        if (name == "get_engine_state") {
            return HandleGetEngineState(kernel);
        }
        if (name == "get_visual_state") {
            return HandleGetVisualState(kernel);
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
        if (name == "effect_editor.open_workspace") {
            return HandleEffectOpenWorkspace(kernel, params);
        }
        if (name == "effect_editor.timeline_play") {
            return HandleEffectTimelinePlay(kernel, params);
        }
        if (name == "effect_editor.timeline_stop") {
            return HandleEffectTimelineStop(kernel);
        }
        if (name == "effect_editor.get_asset") {
            return HandleEffectGetAsset(params);
        }
        if (name == "effect_editor.set_asset") {
            return HandleEffectSetAsset(kernel, params);
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
        if (!registry) {
            throw MakeError("operation_not_allowed", "Game registry is not available.");
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
        if (name == "camera.create") {
            return HandleCameraCreate(*registry, params);
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
}

void AIAutomationService::Initialize()
{
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
}

void AIAutomationService::Finalize()
{
}

void AIAutomationService::ProcessPendingCommands(EngineKernel& kernel)
{
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
        else if (command.value("version", kProtocolVersion) != kProtocolVersion) {
            response = MakeResult(command, false, nullptr, MakeError("unsupported_version", "Unsupported command version."));
        }
        else {
            try {
                json result = DispatchCommand(kernel, command);
                response = MakeResult(command, true, std::move(result), nullptr);
            }
            catch (const json& jsonError) {
                response = MakeResult(command, false, nullptr, jsonError);
            }
            catch (const std::exception& e) {
                response = MakeResult(command, false, nullptr, MakeError("internal_error", e.what()));
            }
            catch (...) {
                response = MakeResult(command, false, nullptr, MakeError("internal_error", "Unknown exception."));
            }
        }

        const std::string resultName = SanitizeFileStem(command.value("id", activePath.stem().string()));
        WriteJsonFile(m_resultsDir / (resultName + ".json"), response);

        std::error_code removeEc;
        std::filesystem::remove(activePath, removeEc);
    }

    try {
        json state = HandleGetEngineState(kernel);
        state["version"] = kProtocolVersion;
        WriteJsonFile(m_stateDir / "latest_editor_state.json", state);
    }
    catch (...) {
    }
}
