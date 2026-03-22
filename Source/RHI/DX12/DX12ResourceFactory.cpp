#include "DX12ResourceFactory.h"
#include "DX12Texture.h"
#include "DX12Shader.h"
#include "DX12Buffer.h"
#include "DX12State.h"
#include "DX12PipelineState.h"
#include "Console/Logger.h"
#include <DirectXTex.h>
#include <cassert>
#include <cstring>

namespace {
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

    void LogCubemapFace0Snapshot(DX12Device* device, ID3D12Resource* textureResource, DXGI_FORMAT format) {
        if (!device || !textureResource || format != DXGI_FORMAT_R16G16B16A16_FLOAT) {
            return;
        }

        auto* d3dDevice = device->GetDevice();
        auto* cmdQueue = device->GetCommandQueue();
        D3D12_RESOURCE_DESC desc = textureResource->GetDesc();

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
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> readbackBuffer;
        HRESULT hr = d3dDevice->CreateCommittedResource(
            &readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&readbackBuffer));
        if (FAILED(hr)) {
            return;
        }

        ComPtr<ID3D12CommandAllocator> cmdAllocator;
        hr = d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator));
        if (FAILED(hr)) {
            return;
        }

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        hr = d3dDevice->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr,
            IID_PPV_ARGS(&cmdList));
        if (FAILED(hr)) {
            return;
        }

        D3D12_RESOURCE_BARRIER toCopy = {};
        toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopy.Transition.pResource = textureResource;
        toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &toCopy);

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = readbackBuffer.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = textureResource;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;
        cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER toSrv = {};
        toSrv.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toSrv.Transition.pResource = textureResource;
        toSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toSrv.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toSrv.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &toSrv);

        cmdList->Close();
        ID3D12CommandList* lists[] = { cmdList.Get() };
        cmdQueue->ExecuteCommandLists(1, lists);

        ComPtr<ID3D12Fence> fence;
        hr = d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (FAILED(hr)) {
            return;
        }

        HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        cmdQueue->Signal(fence.Get(), 1);
        if (fence->GetCompletedValue() < 1) {
            fence->SetEventOnCompletion(1, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
        CloseHandle(fenceEvent);

        void* mapped = nullptr;
        D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(totalBytes) };
        hr = readbackBuffer->Map(0, &readRange, &mapped);
        if (FAILED(hr) || !mapped) {
            return;
        }

        const auto* bytes = static_cast<const uint8_t*>(mapped);
        double sumR = 0.0, sumG = 0.0, sumB = 0.0, sumA = 0.0;
        uint32_t sampleCount = 0;
        const UINT stepY = desc.Height > 16 ? desc.Height / 16 : 1;
        const UINT stepX = static_cast<UINT>(desc.Width > 16 ? desc.Width / 16 : 1);
        for (UINT y = 0; y < desc.Height; y += stepY) {
            const uint8_t* row = bytes + static_cast<size_t>(y) * footprint.Footprint.RowPitch;
            for (UINT x = 0; x < desc.Width; x += stepX) {
                const auto* pixel = reinterpret_cast<const uint16_t*>(row + x * 8);
                sumR += HalfToFloatLocal(pixel[0]);
                sumG += HalfToFloatLocal(pixel[1]);
                sumB += HalfToFloatLocal(pixel[2]);
                sumA += HalfToFloatLocal(pixel[3]);
                ++sampleCount;
            }
        }

        readbackBuffer->Unmap(0, nullptr);

        if (sampleCount > 0) {
            LOG_INFO("[DX12ResourceFactory] face0 avg=(%.4f, %.4f, %.4f, %.4f)",
                sumR / sampleCount, sumG / sampleCount, sumB / sampleCount, sumA / sampleCount);
        }
    }
}

std::unique_ptr<ITexture> DX12ResourceFactory::CreateTexture(const std::string& name, const TextureDesc& desc) {
    if (!m_device) return nullptr;
    return std::make_unique<DX12Texture>(
        m_device,
        desc.width,
        desc.height,
        desc.format,
        desc.bindFlags
    );
}

std::unique_ptr<IShader> DX12ResourceFactory::CreateShader(ShaderType type, const std::string& fileName) {
    return std::make_unique<DX12Shader>(type, fileName);
}

std::unique_ptr<IBuffer> DX12ResourceFactory::CreateBuffer(uint32_t size, BufferType type, const void* initialData) {
    if (!m_device) return nullptr;
    return std::make_unique<DX12Buffer>(m_device, size, type, initialData);
}

std::unique_ptr<IBuffer> DX12ResourceFactory::CreateStructuredBuffer(uint32_t elementSize, uint32_t elementCount, const void* initialData) {
    if (!m_device || elementSize == 0 || elementCount == 0) return nullptr;
    return std::make_unique<DX12Buffer>(m_device, elementSize * elementCount, BufferType::Structured, initialData, elementSize);
}

static DXGI_FORMAT ToDXGIFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::R32G32B32A32_UINT:  return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::R32G32B32_FLOAT:    return DXGI_FORMAT_R32G32B32_FLOAT;
    case TextureFormat::R32G32_FLOAT:       return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R16G16_FLOAT:       return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::RGBA8_UNORM:        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R8_UNORM:           return DXGI_FORMAT_R8_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

std::unique_ptr<IInputLayout> DX12ResourceFactory::CreateInputLayout(const InputLayoutDesc& desc, const IShader* vs) {
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements(desc.count);
    for (uint32_t i = 0; i < desc.count; ++i) {
        elements[i].SemanticName = desc.elements[i].semanticName;
        elements[i].SemanticIndex = desc.elements[i].semanticIndex;
        elements[i].Format = ToDXGIFormat(desc.elements[i].format);
        elements[i].InputSlot = desc.elements[i].inputSlot;
        elements[i].AlignedByteOffset = desc.elements[i].byteOffset;
        elements[i].InputSlotClass = desc.elements[i].perInstance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elements[i].InstanceDataStepRate = desc.elements[i].perInstance ? desc.elements[i].instanceDataStepRate : 0;
    }
    return std::make_unique<DX12InputLayout>(elements);
}

std::unique_ptr<IPipelineState> DX12ResourceFactory::CreatePipelineState(const PipelineStateDesc& desc) {
    return std::make_unique<DX12PipelineState>(desc);
}

// ========================================================
// DX12 テクスチャファイル読み込み（アップロードヒープ経由）
// ========================================================
std::unique_ptr<ITexture> DX12ResourceFactory::CreateTextureFromMemory(
    const DirectX::ScratchImage& image,
    const DirectX::TexMetadata& metadata)
{
    if (!m_device) return nullptr;

    auto* d3dDevice = m_device->GetDevice();
    auto* cmdQueue = m_device->GetCommandQueue();

    // 1. デフォルトヒープにテクスチャリソース作成
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = static_cast<UINT64>(metadata.width);
    texDesc.Height = static_cast<UINT>(metadata.height);
    texDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
    texDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    texDesc.Format = metadata.format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    ComPtr<ID3D12Resource> textureResource;
    HRESULT hr = d3dDevice->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&textureResource));
    if (FAILED(hr)) return nullptr;

    // 2. サブリソース数の計算とフットプリント取得
    const UINT numSubresources = static_cast<UINT>(metadata.mipLevels * metadata.arraySize);
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(numSubresources);
    std::vector<UINT> numRows(numSubresources);
    std::vector<UINT64> rowSizeInBytes(numSubresources);
    UINT64 totalBytes = 0;

    d3dDevice->GetCopyableFootprints(
        &texDesc, 0, numSubresources, 0,
        layouts.data(), numRows.data(), rowSizeInBytes.data(), &totalBytes);

    // 3. アップロードバッファ作成
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = totalBytes;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> uploadBuffer;
    hr = d3dDevice->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE,
        &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&uploadBuffer));
    if (FAILED(hr)) return nullptr;

    // 4. アップロードバッファにデータコピー
    BYTE* mappedData = nullptr;
    hr = uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    if (FAILED(hr)) return nullptr;

    for (UINT i = 0; i < numSubresources; ++i) {
        // サブリソースインデックスからMipLevelとArraySliceを取得
        UINT mipLevel = i % static_cast<UINT>(metadata.mipLevels);
        UINT arraySlice = i / static_cast<UINT>(metadata.mipLevels);

        const DirectX::Image* srcImage = image.GetImage(mipLevel, arraySlice, 0);
        if (!srcImage) continue;

        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& layout = layouts[i];
        BYTE* dstSlice = mappedData + layout.Offset;

        for (UINT row = 0; row < numRows[i]; ++row) {
            const BYTE* srcRow = srcImage->pixels + row * srcImage->rowPitch;
            BYTE* dstRow = dstSlice + row * layout.Footprint.RowPitch;
            memcpy(dstRow, srcRow, static_cast<size_t>(rowSizeInBytes[i]));
        }
    }
    uploadBuffer->Unmap(0, nullptr);

    // 5. 一時コマンドリストでコピー実行
    ComPtr<ID3D12CommandAllocator> cmdAllocator;
    hr = d3dDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAllocator));
    if (FAILED(hr)) return nullptr;

    ComPtr<ID3D12GraphicsCommandList> cmdList;
    hr = d3dDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator.Get(), nullptr,
        IID_PPV_ARGS(&cmdList));
    if (FAILED(hr)) return nullptr;

    // サブリソースごとに CopyTextureRegion
    for (UINT i = 0; i < numSubresources; ++i) {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = textureResource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = i;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = uploadBuffer.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[i];

        cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    // バリア: COPY_DEST → PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textureResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->Close();

    // 実行 + フェンス待ち
    ID3D12CommandList* ppCmdLists[] = { cmdList.Get() };
    cmdQueue->ExecuteCommandLists(1, ppCmdLists);

    // フェンスで完了待ち
    ComPtr<ID3D12Fence> fence;
    hr = d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) return nullptr;

    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    cmdQueue->Signal(fence.Get(), 1);
    if (fence->GetCompletedValue() < 1) {
        fence->SetEventOnCompletion(1, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    CloseHandle(fenceEvent);

    // 6. DX12Texture にラップして返す
    bool isCubemap = metadata.IsCubemap();
    if (metadata.arraySize >= 6) {
        LOG_INFO("[DX12ResourceFactory] width=%u height=%u array=%u mip=%u format=%d cube=%d",
            static_cast<unsigned>(metadata.width),
            static_cast<unsigned>(metadata.height),
            static_cast<unsigned>(metadata.arraySize),
            static_cast<unsigned>(metadata.mipLevels),
            static_cast<int>(metadata.format),
            isCubemap ? 1 : 0);
        LogCubemapFace0Snapshot(m_device, textureResource.Get(), metadata.format);
    }
    return std::make_unique<DX12Texture>(
        m_device, textureResource,
        static_cast<uint32_t>(metadata.width),
        static_cast<uint32_t>(metadata.height),
        metadata.format, isCubemap);
}
