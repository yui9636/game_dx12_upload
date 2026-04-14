#include "ResourceManager.h"
#include "Model/Model.h"
#include "Graphics.h"
#include "GpuResourceUtils.h"
#include "PathResolver.h"
#include "RHI/DX11/DX11Texture.h"
#include "RHI/IResourceFactory.h"
#include "RHI/DX12/DX12Texture.h"
#include "RHI/DX12/DX12Device.h"
#include "Console/Logger.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>
#include <windows.h>

namespace {
    std::string NormalizePathLower(const std::string& path)
    {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return normalized;
    }

    bool IsEffectTexturePath(const std::string& normalizedPath)
    {
        return normalizedPath.find("/data/effect/") != std::string::npos;
    }

    bool IsBleedableTextureFormat(DXGI_FORMAT format)
    {
        return format == DXGI_FORMAT_R8G8B8A8_UNORM ||
            format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
            format == DXGI_FORMAT_B8G8R8A8_UNORM ||
            format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    }

    void BleedTransparentPixels(DirectX::ScratchImage& image)
    {
        const DirectX::TexMetadata metadata = image.GetMetadata();
        if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || !IsBleedableTextureFormat(metadata.format)) {
            return;
        }

        constexpr int kBleedPasses = 8;
        constexpr uint8_t kAlphaThreshold = 3;

        for (size_t item = 0; item < metadata.arraySize; ++item) {
            for (size_t mip = 0; mip < metadata.mipLevels; ++mip) {
                DirectX::Image* dstImage = const_cast<DirectX::Image*>(image.GetImage(mip, item, 0));
                if (!dstImage || !dstImage->pixels || dstImage->width == 0 || dstImage->height == 0) {
                    continue;
                }

                const size_t width = dstImage->width;
                const size_t height = dstImage->height;
                const bool isBgra =
                    metadata.format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                    metadata.format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

                std::vector<uint8_t> working(dstImage->rowPitch * height);
                std::vector<uint8_t> source(dstImage->rowPitch * height);
                memcpy(working.data(), dstImage->pixels, working.size());

                for (int pass = 0; pass < kBleedPasses; ++pass) {
                    memcpy(source.data(), working.data(), source.size());
                    bool changed = false;

                    for (size_t y = 0; y < height; ++y) {
                        for (size_t x = 0; x < width; ++x) {
                            uint8_t* dstPixel = working.data() + y * dstImage->rowPitch + x * 4u;
                            const uint8_t* srcPixel = source.data() + y * dstImage->rowPitch + x * 4u;
                            if (srcPixel[3] > kAlphaThreshold) {
                                continue;
                            }

                            int sumR = 0;
                            int sumG = 0;
                            int sumB = 0;
                            int contributorCount = 0;

                            for (int oy = -1; oy <= 1; ++oy) {
                                for (int ox = -1; ox <= 1; ++ox) {
                                    if (ox == 0 && oy == 0) {
                                        continue;
                                    }

                                    const int nx = static_cast<int>(x) + ox;
                                    const int ny = static_cast<int>(y) + oy;
                                    if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) || ny >= static_cast<int>(height)) {
                                        continue;
                                    }

                                    const uint8_t* neighbor = source.data() + static_cast<size_t>(ny) * dstImage->rowPitch + static_cast<size_t>(nx) * 4u;
                                    if (neighbor[3] <= kAlphaThreshold) {
                                        continue;
                                    }

                                    if (isBgra) {
                                        sumR += neighbor[2];
                                        sumG += neighbor[1];
                                        sumB += neighbor[0];
                                    } else {
                                        sumR += neighbor[0];
                                        sumG += neighbor[1];
                                        sumB += neighbor[2];
                                    }
                                    ++contributorCount;
                                }
                            }

                            if (contributorCount == 0) {
                                continue;
                            }

                            const uint8_t outR = static_cast<uint8_t>(sumR / contributorCount);
                            const uint8_t outG = static_cast<uint8_t>(sumG / contributorCount);
                            const uint8_t outB = static_cast<uint8_t>(sumB / contributorCount);
                            if (isBgra) {
                                dstPixel[2] = outR;
                                dstPixel[1] = outG;
                                dstPixel[0] = outB;
                            } else {
                                dstPixel[0] = outR;
                                dstPixel[1] = outG;
                                dstPixel[2] = outB;
                            }
                            changed = true;
                        }
                    }

                    if (!changed) {
                        break;
                    }
                }

                memcpy(dstImage->pixels, working.data(), working.size());
            }
        }
    }

    HRESULT LoadImageFromFileGuarded(
        const std::string& resolved,
        DirectX::ScratchImage& image,
        DirectX::TexMetadata& metadata,
        bool& hadStructuredException)
    {
        hadStructuredException = false;
        HRESULT hr = E_FAIL;
        __try {
            hr = GpuResourceUtils::LoadImageFromFile(resolved.c_str(), image, metadata);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            hadStructuredException = true;
            hr = E_FAIL;
        }
        return hr;
    }

    float HalfToFloatLocal(uint16_t value) {
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

    void LogDx12TextureAverage(const char* label, DX12Texture* texture) {
        if (!texture) return;
        auto* device = Graphics::Instance().GetDX12Device();
        if (!device) return;

        auto* resource = texture->GetNativeResource();
        if (!resource) return;

        D3D12_RESOURCE_DESC desc = resource->GetDesc();
        DXGI_FORMAT format = desc.Format;
        if (!(format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM ||
              format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
              format == DXGI_FORMAT_R16G16B16A16_FLOAT)) {
            LOG_INFO("[%s] unsupported format=%d", label, static_cast<int>(format));
            return;
        }

        auto* d3dDevice = device->GetDevice();
        auto* cmdQueue = device->GetCommandQueue();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 totalBytes = 0;
        d3dDevice->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

        D3D12_HEAP_PROPERTIES readbackHeap = {};
        readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = totalBytes;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer;
        HRESULT hr = d3dDevice->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer));
        if (FAILED(hr)) return;

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        hr = d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (FAILED(hr)) return;

        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
        hr = d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));
        if (FAILED(hr)) return;

        D3D12_RESOURCE_BARRIER toCopy = {};
        toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopy.Transition.pResource = resource;
        toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toCopy.Transition.Subresource = 0;
        cmdList->ResourceBarrier(1, &toCopy);

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = readbackBuffer.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = resource;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;
        cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER toSrv = toCopy;
        toSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toSrv.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdList->ResourceBarrier(1, &toSrv);

        cmdList->Close();
        ID3D12CommandList* lists[] = { cmdList.Get() };
        cmdQueue->ExecuteCommandLists(1, lists);

        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        hr = d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (FAILED(hr)) return;

        HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        cmdQueue->Signal(fence.Get(), 1);
        if (fence->GetCompletedValue() < 1) {
            fence->SetEventOnCompletion(1, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
        CloseHandle(fenceEvent);

        void* mapped = nullptr;
        D3D12_RANGE range = { 0, static_cast<SIZE_T>(totalBytes) };
        hr = readbackBuffer->Map(0, &range, &mapped);
        if (FAILED(hr) || !mapped) return;

        const uint8_t* bytes = static_cast<const uint8_t*>(mapped);
        double sumR = 0.0, sumG = 0.0, sumB = 0.0, sumA = 0.0;
        uint32_t sampleCount = 0;
        UINT stepY = desc.Height > 16 ? desc.Height / 16 : 1;
        UINT stepX = static_cast<UINT>(desc.Width > 16 ? desc.Width / 16 : 1);
        for (UINT y = 0; y < desc.Height; y += stepY) {
            const uint8_t* row = bytes + static_cast<size_t>(y) * footprint.Footprint.RowPitch;
            for (UINT x = 0; x < desc.Width; x += stepX) {
                if (format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
                    const auto* pixel = reinterpret_cast<const uint16_t*>(row + x * 8);
                    sumR += HalfToFloatLocal(pixel[0]);
                    sumG += HalfToFloatLocal(pixel[1]);
                    sumB += HalfToFloatLocal(pixel[2]);
                    sumA += HalfToFloatLocal(pixel[3]);
                } else {
                    const auto* pixel = row + x * 4;
                    if (format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
                        sumR += pixel[2] / 255.0;
                        sumG += pixel[1] / 255.0;
                        sumB += pixel[0] / 255.0;
                    } else {
                        sumR += pixel[0] / 255.0;
                        sumG += pixel[1] / 255.0;
                        sumB += pixel[2] / 255.0;
                    }
                    sumA += pixel[3] / 255.0;
                }
                ++sampleCount;
            }
        }

        readbackBuffer->Unmap(0, nullptr);
        if (sampleCount > 0) {
            LOG_INFO("[%s] gpuAvg=(%.4f, %.4f, %.4f, %.4f) format=%d size=%ux%u", label,
                sumR / sampleCount, sumG / sampleCount, sumB / sampleCount, sumA / sampleCount,
                static_cast<int>(format), static_cast<unsigned>(desc.Width), static_cast<unsigned>(desc.Height));
        }
    }
}

void ResourceManager::Clear()
{
    modelMap.clear();
    textureMap.clear();
    m_failedTexturePaths.clear();
}

std::shared_ptr<Model> ResourceManager::GetModel(const std::string& path, float scaling, bool sourceOnly)
{
    if (path.empty()) return nullptr;

    std::string key = path + "_" + std::to_string(scaling);
    if (sourceOnly) key += "_source";
    if (modelMap.count(key)) return modelMap[key];

    std::string resolved = PathResolver::Resolve(path);

    auto model = std::make_shared<Model>(resolved.c_str(), scaling, sourceOnly);

    modelMap[key] = model;
    return model;
}

std::shared_ptr<Model> ResourceManager::CreateModelInstance(const std::string& path, float scaling, bool sourceOnly)
{
    if (path.empty()) return nullptr;

    std::string resolved = PathResolver::Resolve(path);
    return std::make_shared<Model>(resolved.c_str(), scaling, sourceOnly);
}

void ResourceManager::InvalidateModel(const std::string& path)
{
    if (path.empty()) {
        return;
    }

    std::vector<std::string> keysToErase;
    auto collectKeys = [&](const std::string& basePath) {
        if (basePath.empty()) {
            return;
        }

        const std::string prefix = basePath + "_";
        for (const auto& entry : modelMap) {
            if (entry.first.rfind(prefix, 0) == 0) {
                keysToErase.push_back(entry.first);
            }
        }
    };

    collectKeys(path);
    collectKeys(PathResolver::Resolve(path));

    std::sort(keysToErase.begin(), keysToErase.end());
    keysToErase.erase(std::unique(keysToErase.begin(), keysToErase.end()), keysToErase.end());
    for (const std::string& key : keysToErase) {
        modelMap.erase(key);
    }
}

std::shared_ptr<ITexture> ResourceManager::GetTexture(const std::string& path)
{
    if (path.empty()) return nullptr;
    if (textureMap.count(path)) return textureMap[path];
    if (m_failedTexturePaths.count(path)) return nullptr;

    std::string resolved = PathResolver::Resolve(path);
    if (m_failedTexturePaths.count(resolved)) return nullptr;
    if (!std::filesystem::exists(resolved) || !std::filesystem::is_regular_file(resolved)) {
        LOG_WARN("[ResourceManager] Texture path does not exist: %s", resolved.c_str());
        m_failedTexturePaths.insert(path);
        m_failedTexturePaths.insert(resolved);
        return nullptr;
    }

    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata;
    HRESULT hr = E_FAIL;
    bool hadStructuredException = false;
    try {
        hr = LoadImageFromFileGuarded(resolved, image, metadata, hadStructuredException);
    } catch (...) {
        LOG_WARN("[ResourceManager] Texture load threw exception: %s", resolved.c_str());
        m_failedTexturePaths.insert(path);
        m_failedTexturePaths.insert(resolved);
        return nullptr;
    }
    if (hadStructuredException) {
        LOG_WARN("[ResourceManager] Texture load raised structured exception: %s", resolved.c_str());
        m_failedTexturePaths.insert(path);
        m_failedTexturePaths.insert(resolved);
        return nullptr;
    }
    if (FAILED(hr)) {
        LOG_WARN("[ResourceManager] Failed to load texture: %s (hr=0x%08X)", resolved.c_str(), static_cast<unsigned>(hr));
        m_failedTexturePaths.insert(path);
        m_failedTexturePaths.insert(resolved);
        return nullptr;
    }

    std::string normalized = NormalizePathLower(resolved);

    const bool looksLikeIblCube = normalized.find("/data/texture/ibl/") != std::string::npos
        && metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D
        && metadata.arraySize >= 6;
    if (looksLikeIblCube) {
        metadata.miscFlags |= DirectX::TEX_MISC_TEXTURECUBE;
        LOG_INFO("[ResourceManager] IBL cube normalize path=%s cube=%d array=%u mip=%u format=%d",
            resolved.c_str(), metadata.IsCubemap() ? 1 : 0,
            static_cast<unsigned>(metadata.arraySize),
            static_cast<unsigned>(metadata.mipLevels),
            static_cast<int>(metadata.format));
    }

    if (IsEffectTexturePath(normalized)) {
        BleedTransparentPixels(image);
    }

    auto* factory = Graphics::Instance().GetResourceFactory();
    if (!factory) return nullptr;

    auto texture = std::shared_ptr<ITexture>(
        factory->CreateTextureFromMemory(image, metadata).release());

    if (texture && normalized.find("prototypemap_orange.png") != std::string::npos) {
        if (auto* dx12Texture = dynamic_cast<DX12Texture*>(texture.get())) {
            LogDx12TextureAverage("ResourceManagerTexture", dx12Texture);
        }
    }

    textureMap[path] = texture;
    return texture;
}


std::shared_ptr<MaterialAsset> ResourceManager::GetMaterial(const std::string& path) {
    if (path.empty()) return nullptr;

    auto it = m_materials.find(path);
    if (it != m_materials.end()) {
        return it->second;
    }

    auto material = std::make_shared<MaterialAsset>(path);
    m_materials[path] = material;
    return material;
}

std::shared_ptr<MaterialAsset> ResourceManager::GetDefaultMaterial() {
    if (!m_defaultMaterial) {
        m_defaultMaterial = std::make_shared<MaterialAsset>("Default");
        m_defaultMaterial->baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_defaultMaterial->metallic = 1.0f;
        m_defaultMaterial->roughness = 1.0f;
        m_defaultMaterial->shaderId = 1; // PBR
    }
    return m_defaultMaterial;
}
