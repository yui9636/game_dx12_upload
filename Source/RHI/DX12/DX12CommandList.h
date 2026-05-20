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
    // 一時 constant buffer 用 ring buffer から切り出した領域。
    struct DynamicAllocation {
        void* cpuPtr = nullptr;                  // CPU 書き込み先。
        D3D12_GPU_VIRTUAL_ADDRESS gpuVA = 0;     // shader へ渡す GPU virtual address。
        uint32_t size = 0;                       // 256 byte alignment 済みの確保サイズ。
    };

    // null SRV を作るときの view dimension。
    enum class NullSrvKind {
        Texture2D,
        Texture2DArray,
        TextureCube
    };

    // pixel shader の tN slot に貼る texture と fallback null SRV。
    struct PixelTextureBinding {
        uint32_t slot;        // shader register tN。
        ITexture* texture;    // nullptr の場合は nullKind の null SRV を bind する。
        NullSrvKind nullKind; // texture が無いときに使う null SRV の種類。
    };

    // useDeviceFrameAllocator=true のときは DX12Device の frame allocator を使う。
    // worker command list など独立 lifetime が必要な場合は owned allocator を持つ。
    DX12CommandList(DX12Device* device, DX12RootSignature* rootSig, bool useDeviceFrameAllocator = true);
    ~DX12CommandList() override;

    // DX12 固有の frame lifecycle。
    void Begin();
    void End();
    void Submit();
    void FlushResourceBarriers();
    void DiscardResourceBarriers();
    void RestoreFrameDescriptorHeap();
    void RestoreDescriptorHeap();  // ImGui 描画後にフレームヒープとルートシグネチャを復元
    // 複数 texture を 1 つの SRV table にまとめて bind する。
    void BindPixelTextureTable(const PixelTextureBinding* bindings, uint32_t count);
    // transient constant buffer 領域へ data をコピーして GPU address を返す。
    DynamicAllocation AllocateDynamicConstantBuffer(const void* data, uint32_t size);
    void VSSetDynamicConstantBuffer(uint32_t slot, const void* data, uint32_t size);
    void PSSetDynamicConstantBuffer(uint32_t slot, const void* data, uint32_t size);

    // ICommandList の実装。
    void Draw(uint32_t vertexCount, uint32_t startVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) override;
    void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) override;
    void DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) override;
    void ExecuteIndexedIndirect(IBuffer* argumentBuffer, uint32_t argumentOffsetBytes) override;
    void ExecuteIndexedIndirectMulti(IBuffer* argumentBuffer, uint32_t argumentOffsetBytes,
        uint32_t maxCommandCount, uint32_t commandStride,
        IBuffer* countBuffer = nullptr, uint32_t countBufferOffset = 0) override;
    void Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override;

    // ComputeCullingPass が使う DX12 固有の compute bind 補助処理。
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
    // 遅延中の PipelineStateDesc から PSO を確定して command list に設定する。
    void FlushPSO();
    // 溜めた resource barrier をまとめて発行する。
    void FlushPendingBarriers();
    // RHI の ResourceState を D3D12_RESOURCE_STATES に変換する。
    D3D12_RESOURCE_STATES ToD3D12State(ResourceState state);
    // slot 番号に応じた 2D null SRV を返す互換用 helper。
    D3D12_CPU_DESCRIPTOR_HANDLE GetNullSrvHandle(uint32_t slot) const;
    // texture dimension に応じた null SRV を返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetNullSrvHandle(NullSrvKind kind) const;

    DX12Device* m_device;                         // command list が使う DX12Device。非所有。
    DX12RootSignature* m_rootSignature;           // graphics root signature。非所有。
    bool m_useDeviceFrameAllocator = true;        // true なら device の frame allocator を Reset に使う。
    ComPtr<ID3D12CommandAllocator> m_ownedAllocator; // worker 用など自前 allocator。
    ComPtr<ID3D12GraphicsCommandList> m_commandList; // 実際に録画する DX12 command list。
    ComPtr<ID3D12DescriptorHeap> m_nullSrvHeap;   // null texture 用 SRV descriptor heap。
    ComPtr<ID3D12Resource> m_dynamicCbRing;       // dynamic CB 用 upload heap ring buffer。
    ComPtr<ID3D12CommandSignature> m_drawIndexedInstancedSignature; // ExecuteIndirect 用 signature。
    uint8_t* m_dynamicCbRingCpuBase = nullptr;    // dynamicCbRing の CPU map 先。
    uint32_t m_dynamicCbRingSize = 0;             // ring buffer 全体サイズ。
    uint32_t m_dynamicCbRingOffset = 0;           // 次に確保する ring buffer offset。
    std::vector<ComPtr<ID3D12Resource>> m_dynamicCbSpills; // ring が足りない frame の一時退避 buffer。

    // SRV copy 用の frame-local descriptor allocator。
    std::unique_ptr<DX12DescriptorAllocator> m_frameSrvAllocator;

    // 遅延 PSO state 追跡。
    PipelineStateDesc m_pendingDesc; // 次の draw/dispatch で必要になる PSO 設定。
    bool m_psoDirty = true;          // pendingDesc が native PSO に未反映なら true。

    // PSO cache を保持する。
    std::unique_ptr<DX12PSOCache> m_psoCache;

    // batch submit 用に保留する barrier。
    std::vector<D3D12_RESOURCE_BARRIER> m_pendingBarriers; // batch submit 用 barrier cache。

    // SRV staging block。root signature の t0-t63 table に対応する。
    static constexpr uint32_t kSrvSlotCount = 64;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvBlockCpuBase = {}; // 現在の SRV table CPU 先頭。
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvBlockGpuBase = {}; // 現在の SRV table GPU 先頭。
    D3D12_CPU_DESCRIPTOR_HANDLE m_nullSrv2D = {};       // Texture2D 用 null SRV。
    D3D12_CPU_DESCRIPTOR_HANDLE m_nullSrv2DArray = {};  // Texture2DArray 用 null SRV。
    D3D12_CPU_DESCRIPTOR_HANDLE m_nullSrvCube = {};     // TextureCube 用 null SRV。
    bool m_srvBlockAllocated = false;                   // 今 frame の SRV block 確保済みか。
    bool m_srvDirtyAfterDraw = false;                   // Draw後にtrue。次 bind で新 block を確保する。
    void EnsureSrvBlock();
};

