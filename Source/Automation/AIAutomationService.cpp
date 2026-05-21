#include "Automation/AIAutomationService.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <DirectXMath.h>
#include <nlohmann/json.hpp>
#include <windows.h>

#include "Archetype/Archetype.h"
#include "Asset/PrefabSystem.h"
#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Console/Logger.h"
#include "Engine/EditorSelection.h"
#include "Engine/EngineKernel.h"
#include "Generated/ComponentMeta.generated.h"
#include "Hierarchy/HierarchySystem.h"
#include "Layer/EditorLayer.h"
#include "Layer/GameLayer.h"
#include "Registry/Registry.h"
#include "Render/Graphics.h"
#include "RHI/DX11/DX11Texture.h"
#include "System/PathResolver.h"
#include "System/ResourceManager.h"
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
