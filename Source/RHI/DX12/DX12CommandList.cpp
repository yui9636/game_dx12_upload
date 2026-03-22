#include "DX12CommandList.h"
#include "DX12Texture.h"
#include "DX12Buffer.h"
#include "DX12Shader.h"
#include "DX12State.h"
#include "DX12PipelineState.h"
#include "RHI/ISampler.h"
#include "RHI/IBind.h"
#include <cassert>
#include <cstring>
#include "Console/Logger.h"

DX12CommandList::DX12CommandList(DX12Device* device, DX12RootSignature* rootSig, bool useDeviceFrameAllocator)
    : m_device(device), m_rootSignature(rootSig)
    , m_useDeviceFrameAllocator(useDeviceFrameAllocator)
{
    ID3D12CommandAllocator* initialAllocator = nullptr;
    if (m_useDeviceFrameAllocator) {
        initialAllocator = device->GetCurrentAllocator();
    } else {
        HRESULT allocatorHr = device->GetDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_ownedAllocator));
        assert(SUCCEEDED(allocatorHr));
        initialAllocator = m_ownedAllocator.Get();
    }

    HRESULT hr = device->GetDevice()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        initialAllocator, nullptr,
        IID_PPV_ARGS(&m_commandList));
    assert(SUCCEEDED(hr));

    // Command list starts in recording state; close it so Begin() can reset it
    m_commandList->Close();

    // Create frame-local SRV allocator
    m_frameSrvAllocator = std::make_unique<DX12DescriptorAllocator>(
        device->GetDevice(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        1024, true);

    m_dynamicCbRingSize = 4u * 1024u * 1024u;
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbRingDesc = {};
    cbRingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbRingDesc.Width = m_dynamicCbRingSize;
    cbRingDesc.Height = 1;
    cbRingDesc.DepthOrArraySize = 1;
    cbRingDesc.MipLevels = 1;
    cbRingDesc.SampleDesc.Count = 1;
    cbRingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE,
        &cbRingDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_dynamicCbRing));
    assert(SUCCEEDED(hr));
    if (m_dynamicCbRing) {
        D3D12_RANGE readRange = { 0, 0 };
        hr = m_dynamicCbRing->Map(0, &readRange, reinterpret_cast<void**>(&m_dynamicCbRingCpuBase));
        assert(SUCCEEDED(hr));
    }

    D3D12_INDIRECT_ARGUMENT_DESC drawIndexedArg = {};
    drawIndexedArg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

    D3D12_COMMAND_SIGNATURE_DESC signatureDesc = {};
    signatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
    signatureDesc.NumArgumentDescs = 1;
    signatureDesc.pArgumentDescs = &drawIndexedArg;

    hr = device->GetDevice()->CreateCommandSignature(
        &signatureDesc,
        nullptr,
        IID_PPV_ARGS(&m_drawIndexedInstancedSignature));
    assert(SUCCEEDED(hr));

    D3D12_DESCRIPTOR_HEAP_DESC nullHeapDesc = {};
    nullHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    nullHeapDesc.NumDescriptors = 3;
    nullHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->GetDevice()->CreateDescriptorHeap(&nullHeapDesc, IID_PPV_ARGS(&m_nullSrvHeap));
    assert(SUCCEEDED(hr));

    const UINT srvStride = device->GetCBVSRVUAVDescriptorSize();
    m_nullSrv2D = m_nullSrvHeap->GetCPUDescriptorHandleForHeapStart();
    m_nullSrv2DArray = m_nullSrv2D;
    m_nullSrv2DArray.ptr += srvStride;
    m_nullSrvCube = m_nullSrv2DArray;
    m_nullSrvCube.ptr += srvStride;

    D3D12_SHADER_RESOURCE_VIEW_DESC null2DDesc = {};
    null2DDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    null2DDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    null2DDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    null2DDesc.Texture2D.MipLevels = 1;
    device->GetDevice()->CreateShaderResourceView(nullptr, &null2DDesc, m_nullSrv2D);

    D3D12_SHADER_RESOURCE_VIEW_DESC null2DArrayDesc = {};
    null2DArrayDesc.Format = DXGI_FORMAT_R32_FLOAT;
    null2DArrayDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    null2DArrayDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    null2DArrayDesc.Texture2DArray.MipLevels = 1;
    null2DArrayDesc.Texture2DArray.FirstArraySlice = 0;
    null2DArrayDesc.Texture2DArray.ArraySize = 1;
    device->GetDevice()->CreateShaderResourceView(nullptr, &null2DArrayDesc, m_nullSrv2DArray);

    D3D12_SHADER_RESOURCE_VIEW_DESC nullCubeDesc = {};
    nullCubeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    nullCubeDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    nullCubeDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    nullCubeDesc.TextureCube.MipLevels = 1;
    device->GetDevice()->CreateShaderResourceView(nullptr, &nullCubeDesc, m_nullSrvCube);

    // PSO cache
    m_psoCache = std::make_unique<DX12PSOCache>(device, rootSig);
}

DX12CommandList::~DX12CommandList() = default;

void DX12CommandList::Begin() {
    auto* allocator = m_useDeviceFrameAllocator ? m_device->GetCurrentAllocator() : m_ownedAllocator.Get();
    allocator->Reset();
    m_commandList->Reset(allocator, nullptr);

    // Reset frame allocator
    m_frameSrvAllocator->Reset();

    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_frameSrvAllocator->GetHeap() };
    m_commandList->SetDescriptorHeaps(1, heaps);

    // Set root signature
    m_commandList->SetGraphicsRootSignature(m_rootSignature->Get());

    m_psoDirty = true;
    m_srvBlockAllocated = false;
    m_dynamicCbRingOffset = 0;
    m_dynamicCbSpills.clear();
}

void DX12CommandList::End() {
    FlushPendingBarriers();
    m_commandList->Close();
}

void DX12CommandList::Submit() {
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_device->GetCommandQueue()->ExecuteCommandLists(1, lists);
}

void DX12CommandList::FlushResourceBarriers() {
    FlushPendingBarriers();
}

void DX12CommandList::RestoreFrameDescriptorHeap() {
    ID3D12DescriptorHeap* heaps[] = { m_frameSrvAllocator->GetHeap() };
    m_commandList->SetDescriptorHeaps(1, heaps);
}

void DX12CommandList::RestoreDescriptorHeap() {
    // ImGui 後はフレームヒープとルートシグネチャの両方を戻す。
    // ルートシグネチャの再設定は全ルートパラメータを無効化するため、
    // フレーム途中の復帰ではなく ImGui 後の最終復帰にだけ使う。
    RestoreFrameDescriptorHeap();
    m_commandList->SetGraphicsRootSignature(m_rootSignature->Get());
}

DX12CommandList::DynamicAllocation DX12CommandList::AllocateDynamicConstantBuffer(const void* data, uint32_t size) {
    DynamicAllocation allocation{};
    if (!data || size == 0) return allocation;

    const uint32_t alignedSize = (size + 255u) & ~255u;
    if (m_dynamicCbRing && (m_dynamicCbRingOffset + alignedSize) <= m_dynamicCbRingSize) {
        allocation.cpuPtr = m_dynamicCbRingCpuBase + m_dynamicCbRingOffset;
        allocation.gpuVA = m_dynamicCbRing->GetGPUVirtualAddress() + m_dynamicCbRingOffset;
        allocation.size = alignedSize;
        memcpy(allocation.cpuPtr, data, size);
        if (alignedSize > size) {
            memset(static_cast<uint8_t*>(allocation.cpuPtr) + size, 0, alignedSize - size);
        }
        m_dynamicCbRingOffset += alignedSize;
        return allocation;
    }

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = alignedSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> spill;
    HRESULT hr = m_device->GetDevice()->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&spill));
    assert(SUCCEEDED(hr));
    if (FAILED(hr) || !spill) {
        return allocation;
    }

    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    hr = spill->Map(0, &readRange, &mapped);
    assert(SUCCEEDED(hr));
    if (FAILED(hr) || !mapped) {
        return allocation;
    }

    memcpy(mapped, data, size);
    if (alignedSize > size) {
        memset(static_cast<uint8_t*>(mapped) + size, 0, alignedSize - size);
    }

    allocation.cpuPtr = mapped;
    allocation.gpuVA = spill->GetGPUVirtualAddress();
    allocation.size = alignedSize;
    m_dynamicCbSpills.push_back(spill);
    return allocation;
}

void DX12CommandList::VSSetDynamicConstantBuffer(uint32_t slot, const void* data, uint32_t size) {
    auto allocation = AllocateDynamicConstantBuffer(data, size);
    if (!allocation.gpuVA) return;
    if (slot < 8) {
        m_commandList->SetGraphicsRootConstantBufferView(slot, allocation.gpuVA);
    }
}

void DX12CommandList::PSSetDynamicConstantBuffer(uint32_t slot, const void* data, uint32_t size) {
    auto allocation = AllocateDynamicConstantBuffer(data, size);
    if (!allocation.gpuVA) return;
    if (slot < 8) {
        m_commandList->SetGraphicsRootConstantBufferView(slot, allocation.gpuVA);
    }
}

// ── Draw ──

void DX12CommandList::FlushPSO() {
    if (!m_psoDirty) return;
    auto* pso = m_psoCache->GetOrCreate(m_pendingDesc);
    if (pso) {
        m_commandList->SetPipelineState(pso);
        m_psoDirty = false;
    }
}

void DX12CommandList::Draw(uint32_t vertexCount, uint32_t startVertex) {
    FlushPSO();
    FlushPendingBarriers();
    m_commandList->DrawInstanced(vertexCount, 1, startVertex, 0);
    m_srvBlockAllocated = false; // 次のドローでフレッシュなディスクリプタブロックを確保
}

void DX12CommandList::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) {
    FlushPSO();
    FlushPendingBarriers();
    m_commandList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
    m_srvBlockAllocated = false;
}

void DX12CommandList::DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) {
    FlushPSO();
    FlushPendingBarriers();
    m_commandList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
    m_srvBlockAllocated = false;
}

void DX12CommandList::DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) {
    FlushPSO();
    FlushPendingBarriers();
    m_commandList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
    m_srvBlockAllocated = false;
}

void DX12CommandList::ExecuteIndexedIndirect(IBuffer* argumentBuffer, uint32_t argumentOffsetBytes) {
    if (!argumentBuffer || !m_drawIndexedInstancedSignature) return;

    auto* dx12Buffer = static_cast<DX12Buffer*>(argumentBuffer);
    if (!dx12Buffer || !dx12Buffer->GetNativeResource()) return;

    FlushPSO();
    FlushPendingBarriers();
    m_commandList->ExecuteIndirect(
        m_drawIndexedInstancedSignature.Get(),
        1,
        dx12Buffer->GetNativeResource(),
        argumentOffsetBytes,
        nullptr,
        0);
    m_srvBlockAllocated = false;
}

void DX12CommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z) {
    FlushPendingBarriers();
    m_commandList->Dispatch(x, y, z);
}

void DX12CommandList::SetComputeRootSignature(ID3D12RootSignature* rootSig) {
    m_commandList->SetComputeRootSignature(rootSig);
}

void DX12CommandList::SetComputePipelineState(ID3D12PipelineState* pso) {
    m_commandList->SetPipelineState(pso);
}

void DX12CommandList::SetComputeRootCBV(uint32_t slot, D3D12_GPU_VIRTUAL_ADDRESS gpuVA) {
    m_commandList->SetComputeRootConstantBufferView(slot, gpuVA);
}

void DX12CommandList::SetComputeRootSRV(uint32_t slot, D3D12_GPU_VIRTUAL_ADDRESS gpuVA) {
    m_commandList->SetComputeRootShaderResourceView(slot, gpuVA);
}

void DX12CommandList::SetComputeRootUAV(uint32_t slot, D3D12_GPU_VIRTUAL_ADDRESS gpuVA) {
    m_commandList->SetComputeRootUnorderedAccessView(slot, gpuVA);
}

void DX12CommandList::BufferBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    if (!resource || before == after) return;
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);
}

void DX12CommandList::CopyBufferRegion(ID3D12Resource* dst, uint64_t dstOffset, ID3D12Resource* src, uint64_t srcOffset, uint64_t numBytes) {
    m_commandList->CopyBufferRegion(dst, dstOffset, src, srcOffset, numBytes);
}

// ── Shader ──

void DX12CommandList::VSSetShader(IShader* shader) {
    m_pendingDesc.vertexShader = shader;
    m_psoDirty = true;
}

void DX12CommandList::PSSetShader(IShader* shader) {
    m_pendingDesc.pixelShader = shader;
    m_psoDirty = true;
}

void DX12CommandList::GSSetShader(IShader* shader) {
    m_pendingDesc.geometryShader = shader;
    m_psoDirty = true;
}

void DX12CommandList::CSSetShader(IShader* shader) {
    m_pendingDesc.computeShader = shader;
    // Compute uses a separate pipeline; mark dirty
    m_psoDirty = true;
}

// ── Constant buffers ──

void DX12CommandList::VSSetConstantBuffer(uint32_t slot, IBuffer* buffer) {
    if (!buffer) return;
    auto* dx12Buf = static_cast<DX12Buffer*>(buffer);
    // Map slot to root parameter
    if (slot < 8) {
        m_commandList->SetGraphicsRootConstantBufferView(slot, dx12Buf->GetGPUVirtualAddress());
    }
}

void DX12CommandList::PSSetConstantBuffer(uint32_t slot, IBuffer* buffer) {
    if (!buffer) return;
    auto* dx12Buf = static_cast<DX12Buffer*>(buffer);
    if (slot < 8) {
        m_commandList->SetGraphicsRootConstantBufferView(slot, dx12Buf->GetGPUVirtualAddress());
    }
}

void DX12CommandList::CSSetConstantBuffer(uint32_t slot, IBuffer* buffer) {
    if (!buffer) return;
    auto* dx12Buf = static_cast<DX12Buffer*>(buffer);
    if (slot < 8) {
        m_commandList->SetComputeRootConstantBufferView(slot, dx12Buf->GetGPUVirtualAddress());
    }
}

// ── Textures (SRV) ──

void DX12CommandList::EnsureSrvBlock() {
    // ヒープは Begin() で設定済み。Skybox 等で切り替えた場合は RestoreDescriptorHeap() で復元。
    // ここでは SetDescriptorHeaps / SetGraphicsRootSignature を呼ばない。
    // SetGraphicsRootSignature を再呼出しすると全ルートパラメータ（CBV等）が無効化される。

    if (!m_srvBlockAllocated) {
        // 64スロット分の連続ディスクリプタブロックを確保
        m_srvBlockCpuBase = m_frameSrvAllocator->AllocateBlock(kSrvSlotCount);
        m_srvBlockGpuBase = m_frameSrvAllocator->GetGPUHandle(m_srvBlockCpuBase);
        m_srvBlockAllocated = true;

        for (uint32_t slot = 0; slot < kSrvSlotCount; ++slot) {
            auto cpuDst = m_frameSrvAllocator->GetCPUHandleAtOffset(m_srvBlockCpuBase, slot);
            auto cpuSrc = GetNullSrvHandle(slot);
            m_device->GetDevice()->CopyDescriptorsSimple(
                1, cpuDst, cpuSrc,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
    }

}

D3D12_CPU_DESCRIPTOR_HANDLE DX12CommandList::GetNullSrvHandle(uint32_t slot) const {
    if (slot == 4 || slot == 5) return m_nullSrv2DArray;
    if (slot == 8) return m_nullSrvCube;
    if (slot == 33 || slot == 34) return m_nullSrvCube;
    return m_nullSrv2D;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12CommandList::GetNullSrvHandle(NullSrvKind kind) const {
    switch (kind) {
    case NullSrvKind::Texture2DArray:
        return m_nullSrv2DArray;
    case NullSrvKind::TextureCube:
        return m_nullSrvCube;
    case NullSrvKind::Texture2D:
    default:
        return m_nullSrv2D;
    }
}

void DX12CommandList::BindPixelTextureTable(const PixelTextureBinding* bindings, uint32_t count) {
    if (!bindings || count == 0) return;
    EnsureSrvBlock();

    for (uint32_t i = 0; i < count; ++i) {
        const auto& binding = bindings[i];
        if (binding.slot >= kSrvSlotCount) continue;

        auto cpuDst = m_frameSrvAllocator->GetCPUHandleAtOffset(m_srvBlockCpuBase, binding.slot);
        D3D12_CPU_DESCRIPTOR_HANDLE cpuSrc = GetNullSrvHandle(binding.nullKind);
        if (binding.texture) {
            auto* dx12Tex = dynamic_cast<DX12Texture*>(binding.texture);
            if (dx12Tex && dx12Tex->HasSRV()) {
                cpuSrc = dx12Tex->GetSRV();
            }
        }

        m_device->GetDevice()->CopyDescriptorsSimple(
            1, cpuDst, cpuSrc,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    m_commandList->SetGraphicsRootDescriptorTable(DX12RootSignature::SRVTable, m_srvBlockGpuBase);
}

void DX12CommandList::PSSetTexture(uint32_t slot, ITexture* texture) {
    PSSetTextures(slot, 1, &texture);
}

void DX12CommandList::PSSetTextures(uint32_t startSlot, uint32_t numTextures, ITexture* const* ppTextures) {
    if (numTextures == 0 || !ppTextures) return;
    EnsureSrvBlock();

    for (uint32_t i = 0; i < numTextures; ++i) {
        uint32_t slot = startSlot + i;
        if (slot >= kSrvSlotCount) continue;

        auto cpuDst = m_frameSrvAllocator->GetCPUHandleAtOffset(m_srvBlockCpuBase, slot);
        D3D12_CPU_DESCRIPTOR_HANDLE cpuSrc = GetNullSrvHandle(slot);
        if (ppTextures[i]) {
            auto* dx12Tex = static_cast<DX12Texture*>(ppTextures[i]);
            if (dx12Tex->HasSRV()) {
                cpuSrc = dx12Tex->GetSRV();
            }
        }
        m_device->GetDevice()->CopyDescriptorsSimple(
            1, cpuDst, cpuSrc,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // ディスクリプタの書き込み完了後に SRV テーブルを再バインドする。
    // これより前にバインドすると、一部ドライバで古い/null 記述子を参照し続けることがある。
    m_commandList->SetGraphicsRootDescriptorTable(DX12RootSignature::SRVTable, m_srvBlockGpuBase);
}

// ── Samplers ──

void DX12CommandList::PSSetSampler(uint32_t slot, ISampler* sampler) {
    // DX12: samplers are baked into root signature or separate heap
    // For now, no-op (static samplers would handle this in a more complete implementation)
}

void DX12CommandList::PSSetSamplers(uint32_t startSlot, uint32_t numSamplers, ISampler* const* ppSamplers) {
    // No-op for now (see above)
}

// ── Viewport ──

void DX12CommandList::SetViewport(const RhiViewport& viewport) {
    D3D12_VIEWPORT vp;
    vp.TopLeftX = viewport.topLeftX;
    vp.TopLeftY = viewport.topLeftY;
    vp.Width = viewport.width;
    vp.Height = viewport.height;
    vp.MinDepth = viewport.minDepth;
    vp.MaxDepth = viewport.maxDepth;
    m_commandList->RSSetViewports(1, &vp);

    D3D12_RECT scissor;
    scissor.left = static_cast<LONG>(viewport.topLeftX);
    scissor.top = static_cast<LONG>(viewport.topLeftY);
    scissor.right = static_cast<LONG>(viewport.topLeftX + viewport.width);
    scissor.bottom = static_cast<LONG>(viewport.topLeftY + viewport.height);
    m_commandList->RSSetScissorRects(1, &scissor);
}

// ── Input Assembler ──

void DX12CommandList::SetInputLayout(IInputLayout* layout) {
    // In DX12, input layout is part of PSO; store for PSO compilation
    m_pendingDesc.inputLayout = layout;
    m_psoDirty = true;
}

void DX12CommandList::SetPrimitiveTopology(PrimitiveTopology topology) {
    D3D12_PRIMITIVE_TOPOLOGY d3dTopo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    switch (topology) {
    case PrimitiveTopology::TriangleList:  d3dTopo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    case PrimitiveTopology::TriangleStrip: d3dTopo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
    case PrimitiveTopology::LineList:      d3dTopo = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
    case PrimitiveTopology::PointList:     d3dTopo = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
    }
    m_commandList->IASetPrimitiveTopology(d3dTopo);
    m_pendingDesc.primitiveTopology = topology;
}

void DX12CommandList::SetVertexBuffer(uint32_t slot, IBuffer* buffer, uint32_t stride, uint32_t offset) {
    if (!buffer) return;
    auto* dx12Buf = static_cast<DX12Buffer*>(buffer);

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = dx12Buf->GetGPUVirtualAddress() + offset;
    vbView.SizeInBytes = dx12Buf->GetSize() - offset;
    vbView.StrideInBytes = stride;
    m_commandList->IASetVertexBuffers(slot, 1, &vbView);
}

void DX12CommandList::SetIndexBuffer(IBuffer* buffer, IndexFormat format, uint32_t offset) {
    if (!buffer) return;
    auto* dx12Buf = static_cast<DX12Buffer*>(buffer);

    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = dx12Buf->GetGPUVirtualAddress() + offset;
    ibView.SizeInBytes = dx12Buf->GetSize() - offset;
    ibView.Format = (format == IndexFormat::Uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    m_commandList->IASetIndexBuffer(&ibView);
}

// ── States (deferred into PSO) ──

void DX12CommandList::SetDepthStencilState(IDepthStencilState* state, uint32_t stencilRef) {
    m_pendingDesc.depthStencilState = state;
    m_psoDirty = true;
    m_commandList->OMSetStencilRef(stencilRef);
}

void DX12CommandList::SetRasterizerState(IRasterizerState* state) {
    m_pendingDesc.rasterizerState = state;
    m_psoDirty = true;
}

void DX12CommandList::SetBlendState(IBlendState* state, const float blendFactor[4], uint32_t sampleMask) {
    m_pendingDesc.blendState = state;
    m_pendingDesc.sampleMask = sampleMask;
    m_psoDirty = true;
    if (blendFactor) {
        m_commandList->OMSetBlendFactor(blendFactor);
    }
}

// ── Render targets ──

void DX12CommandList::SetRenderTarget(ITexture* rt, ITexture* depthStencil) {
    if (rt == nullptr) {
        SetRenderTargets(0, nullptr, depthStencil);
    } else {
        SetRenderTargets(1, &rt, depthStencil);
    }
}

void DX12CommandList::SetRenderTargets(uint32_t numRTs, ITexture* const* rts, ITexture* depthStencil) {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8] = {};
    uint32_t rtvCount = 0;

    if (numRTs > 0 && rts) {
        rtvCount = numRTs;
        for (uint32_t i = 0; i < numRTs; ++i) {
            if (rts[i]) {
                auto* dx12Tex = static_cast<DX12Texture*>(rts[i]);
                rtvHandles[i] = dx12Tex->GetRTV();
            }
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE* pDSV = nullptr;
    if (depthStencil) {
        auto* dx12DS = static_cast<DX12Texture*>(depthStencil);
        dsvHandle = dx12DS->GetDSV();
        pDSV = &dsvHandle;
    }

    m_commandList->OMSetRenderTargets(rtvCount, rtvCount > 0 ? rtvHandles : nullptr, FALSE, pDSV);

    // Track RT/DSV formats for PSO compilation
    m_pendingDesc.numRenderTargets = rtvCount;
    for (uint32_t i = 0; i < 8; ++i) m_pendingDesc.rtvFormats[i] = TextureFormat::Unknown;
    for (uint32_t i = 0; i < rtvCount; ++i) {
        if (rts && rts[i]) m_pendingDesc.rtvFormats[i] = rts[i]->GetFormat();
    }
    m_pendingDesc.dsvFormat = depthStencil ? depthStencil->GetFormat() : TextureFormat::Unknown;
    m_psoDirty = true;
}

void DX12CommandList::ClearColor(ITexture* renderTarget, const float color[4]) {
    if (!renderTarget) return;
    FlushPendingBarriers();
    auto* dx12RT = static_cast<DX12Texture*>(renderTarget);
    if (dx12RT->HasRTV()) {
        m_commandList->ClearRenderTargetView(dx12RT->GetRTV(), color, 0, nullptr);
    }
}

void DX12CommandList::ClearDepthStencil(ITexture* depthStencil, float depth, uint8_t stencil) {
    if (!depthStencil) return;
    FlushPendingBarriers();
    auto* dx12DS = static_cast<DX12Texture*>(depthStencil);
    if (dx12DS->HasDSV()) {
        m_commandList->ClearDepthStencilView(
            dx12DS->GetDSV(),
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            depth, stencil, 0, nullptr);
    }
}

// ── Barriers ──

void DX12CommandList::TransitionBarrier(ITexture* texture, ResourceState newState) {
    if (!texture) return;
    ResourceState oldState = texture->GetCurrentState();
    if (oldState == newState) return;

    auto* dx12Tex = static_cast<DX12Texture*>(texture);

    for (auto& pending : m_pendingBarriers) {
        if (pending.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) continue;
        if (pending.Transition.pResource != dx12Tex->GetNativeResource()) continue;
        if (pending.Transition.Subresource != D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) continue;

        const D3D12_RESOURCE_STATES d3dNewState = ToD3D12State(newState);

        if (pending.Transition.StateAfter == d3dNewState) {
            texture->SetCurrentState(newState);
            return;
        }

        if (pending.Transition.StateBefore == d3dNewState) {
            pending = m_pendingBarriers.back();
            m_pendingBarriers.pop_back();
            texture->SetCurrentState(newState);
            return;
        }

        pending.Transition.StateAfter = d3dNewState;
        texture->SetCurrentState(newState);
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = dx12Tex->GetNativeResource();
    barrier.Transition.StateBefore = ToD3D12State(oldState);
    barrier.Transition.StateAfter = ToD3D12State(newState);
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_pendingBarriers.push_back(barrier);
    texture->SetCurrentState(newState);
}

void DX12CommandList::FlushPendingBarriers() {
    if (!m_pendingBarriers.empty()) {
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.reserve(m_pendingBarriers.size());
        for (const auto& barrier : m_pendingBarriers) {
            if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION &&
                barrier.Transition.StateBefore == barrier.Transition.StateAfter) {
                continue;
            }
            barriers.push_back(barrier);
        }

        if (!barriers.empty()) {
            m_commandList->ResourceBarrier(
                static_cast<UINT>(barriers.size()),
                barriers.data());
        }
        m_pendingBarriers.clear();
    }
}

D3D12_RESOURCE_STATES DX12CommandList::ToD3D12State(ResourceState state) {
    switch (state) {
    case ResourceState::Common:          return D3D12_RESOURCE_STATE_COMMON;
    case ResourceState::RenderTarget:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case ResourceState::DepthWrite:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case ResourceState::DepthRead:       return D3D12_RESOURCE_STATE_DEPTH_READ;
    case ResourceState::ShaderResource:  return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                                              | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case ResourceState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case ResourceState::Present:         return D3D12_RESOURCE_STATE_PRESENT;
    default:                             return D3D12_RESOURCE_STATE_COMMON;
    }
}

// ── Misc ──

void DX12CommandList::SetBindGroup(ShaderStage stage, uint32_t index, IBind* bind) {
    // Not used in current architecture
}

void DX12CommandList::SetPipelineState(IPipelineState* pso) {
    if (!pso) return;
    auto* dx12PSO = static_cast<DX12PipelineState*>(pso);
    m_pendingDesc = pso->GetDesc();
    SetPrimitiveTopology(m_pendingDesc.primitiveTopology);
    if (dx12PSO->GetNativePSO()) {
        m_commandList->SetPipelineState(dx12PSO->GetNativePSO());
        m_psoDirty = false;
    } else {
        // Factory 製 PSO はネイティブ PSO を持たないため、
        // 次の Draw で PSOCache から正しい PSO をコンパイルさせる。
        m_psoDirty = true;
    }
}

void DX12CommandList::UpdateBuffer(IBuffer* buffer, const void* data, uint32_t size) {
    if (!buffer || !data) return;
    auto* dx12Buf = static_cast<DX12Buffer*>(buffer);
    void* mapped = dx12Buf->Map();
    if (mapped) {
        memcpy(mapped, data, size);
        dx12Buf->Unmap();
    }
}

ID3D11DeviceContext* DX12CommandList::GetNativeContext() {
    // DX12 has no equivalent to ID3D11DeviceContext
    return nullptr;
}
