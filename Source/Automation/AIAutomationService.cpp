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
        if (name == "effect_editor.preview_spawn") {
            return HandleEffectPreviewSpawn(*registry, params);
        }
        if (name == "save_scene") {
            return HandleSaveScene(kernel, params);
        }
        if (name == "load_scene") {
            return HandleLoadScene(kernel, params);
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
