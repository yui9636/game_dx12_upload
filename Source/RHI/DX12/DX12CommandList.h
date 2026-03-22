#pragma once
#include "RHI/ICommandList.h"
#include "RHI/PipelineStateDesc.h"
#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "DX12DescriptorAllocator.h"
#include "DX12PSOCache.h"
#include <unordered_map>
#include <memory>

class DX12CommandList : public ICommandList {
public:
    struct DynamicAllocation {
        void* cpuPtr = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuVA = 0;
        uint32_t size = 0;
    };

    enum class NullSrvKind {
        Texture2D,
        Texture2DArray,
        TextureCube
    };

    struct PixelTextureBinding {
        uint32_t slot;
        ITexture* texture;
        NullSrvKind nullKind;
    };

    DX12CommandList(DX12Device* device, DX12RootSignature* rootSig, bool useDeviceFrameAllocator = true);
    ~DX12CommandList() override;

    // Frame lifecycle (DX12 specific)
    void Begin();
    void End();
    void Submit();
    void FlushResourceBarriers();
    void RestoreFrameDescriptorHeap();
    void RestoreDescriptorHeap();  // ImGui 描画後にフレームヒープとルートシグネチャを復元
    void BindPixelTextureTable(const PixelTextureBinding* bindings, uint32_t count);
    DynamicAllocation AllocateDynamicConstantBuffer(const void* data, uint32_t size);
    void VSSetDynamicConstantBuffer(uint32_t slot, const void* data, uint32_t size);
    void PSSetDynamicConstantBuffer(uint32_t slot, const void* data, uint32_t size);

    // ICommandList implementation
    void Draw(uint32_t vertexCount, uint32_t startVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) override;
    void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) override;
    void DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) override;
    void ExecuteIndexedIndirect(IBuffer* argumentBuffer, uint32_t argumentOffsetBytes) override;
    void Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override;

    // Compute binding helpers (DX12-specific, used by ComputeCullingPass)
    void SetComputeRootSignature(ID3D12RootSignature* rootSig);
    void SetComputePipelineState(ID3D12PipelineState* pso);
    void SetComputeRootCBV(uint32_t slot, D3D12_GPU_VIRTUAL_ADDRESS gpuVA);
    void SetComputeRootSRV(uint32_t slot, D3D12_GPU_VIRTUAL_ADDRESS gpuVA);
    void SetComputeRootUAV(uint32_t slot, D3D12_GPU_VIRTUAL_ADDRESS gpuVA);
    void BufferBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    void CopyBufferRegion(ID3D12Resource* dst, uint64_t dstOffset, ID3D12Resource* src, uint64_t srcOffset, uint64_t numBytes);

    void PSSetTexture(uint32_t slot, ITexture* texture) override;
    void PSSetTextures(uint32_t startSlot, uint32_t numTextures, ITexture* const* ppTextures) override;

    void VSSetConstantBuffer(uint32_t slot, IBuffer* buffer) override;
    void PSSetConstantBuffer(uint32_t slot, IBuffer* buffer) override;
    void CSSetConstantBuffer(uint32_t slot, IBuffer* buffer) override;

    void VSSetShader(IShader* shader) override;
    void PSSetShader(IShader* shader) override;
    void GSSetShader(IShader* shader) override;
    void CSSetShader(IShader* shader) override;

    void PSSetSampler(uint32_t slot, ISampler* sampler) override;
    void PSSetSamplers(uint32_t startSlot, uint32_t numSamplers, ISampler* const* ppSamplers) override;

    void SetViewport(const RhiViewport& viewport) override;
    void SetInputLayout(IInputLayout* layout) override;
    void SetPrimitiveTopology(PrimitiveTopology topology) override;
    void SetVertexBuffer(uint32_t slot, IBuffer* buffer, uint32_t stride, uint32_t offset = 0) override;
    void SetIndexBuffer(IBuffer* buffer, IndexFormat format, uint32_t offset = 0) override;

    void SetDepthStencilState(IDepthStencilState* state, uint32_t stencilRef = 0) override;
    void SetRasterizerState(IRasterizerState* state) override;
    void SetBlendState(IBlendState* state, const float blendFactor[4] = nullptr, uint32_t sampleMask = 0xFFFFFFFF) override;

    void SetRenderTargets(uint32_t numRTs, ITexture* const* rts, ITexture* depthStencil) override;
    void SetRenderTarget(ITexture* rt, ITexture* depthStencil) override;
    void ClearColor(ITexture* renderTarget, const float color[4]) override;
    void ClearDepthStencil(ITexture* depthStencil, float depth, uint8_t stencil) override;

    void TransitionBarrier(ITexture* texture, ResourceState newState) override;
    void SetBindGroup(ShaderStage stage, uint32_t index, IBind* bind) override;
    void SetPipelineState(IPipelineState* pso) override;
    void UpdateBuffer(IBuffer* buffer, const void* data, uint32_t size) override;

    ID3D11DeviceContext* GetNativeContext() override;

    ID3D12GraphicsCommandList* GetNativeCommandList() const { return m_commandList.Get(); }

private:
    void FlushPSO();
    void FlushPendingBarriers();
    D3D12_RESOURCE_STATES ToD3D12State(ResourceState state);
    D3D12_CPU_DESCRIPTOR_HANDLE GetNullSrvHandle(uint32_t slot) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetNullSrvHandle(NullSrvKind kind) const;

    DX12Device* m_device;
    DX12RootSignature* m_rootSignature;
    bool m_useDeviceFrameAllocator = true;
    ComPtr<ID3D12CommandAllocator> m_ownedAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12DescriptorHeap> m_nullSrvHeap;
    ComPtr<ID3D12Resource> m_dynamicCbRing;
    ComPtr<ID3D12CommandSignature> m_drawIndexedInstancedSignature;
    uint8_t* m_dynamicCbRingCpuBase = nullptr;
    uint32_t m_dynamicCbRingSize = 0;
    uint32_t m_dynamicCbRingOffset = 0;
    std::vector<ComPtr<ID3D12Resource>> m_dynamicCbSpills;

    // Frame-local descriptor allocator for SRV copies
    std::unique_ptr<DX12DescriptorAllocator> m_frameSrvAllocator;

    // Deferred PSO state tracking
    PipelineStateDesc m_pendingDesc;
    bool m_psoDirty = true;

    // PSO cache
    std::unique_ptr<DX12PSOCache> m_psoCache;

    // Pending barriers for batch submission
    std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers;

    // SRV staging block (64 slots matching root signature t0-t63)
    static constexpr uint32_t kSrvSlotCount = 64;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvBlockCpuBase = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvBlockGpuBase = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_nullSrv2D = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_nullSrv2DArray = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_nullSrvCube = {};
    bool m_srvBlockAllocated = false;
    void EnsureSrvBlock();
};

