#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <cstdint>
#include <vector>

using Microsoft::WRL::ComPtr;

class DX12Device {
public:
    DX12Device(HWND hWnd, uint32_t width, uint32_t height);
    ~DX12Device();

    ID3D12Device*               GetDevice()            const { return m_device.Get(); }
    ID3D12CommandQueue*         GetCommandQueue()      const { return m_commandQueue.Get(); }
    ID3D12CommandQueue*         GetComputeQueue()      const { return m_computeQueue.Get(); }
    ID3D12Fence*                GetComputeFence()      const { return m_computeFence.Get(); }
    IDXGISwapChain3*            GetSwapChain()         const { return m_swapChain.Get(); }
    ID3D12DescriptorHeap*       GetRTVHeap()           const { return m_rtvHeap.Get(); }
    ID3D12DescriptorHeap*       GetDSVHeap()           const { return m_dsvHeap.Get(); }
    ID3D12DescriptorHeap*       GetCBVSRVUAVHeap()    const { return m_cbvSrvUavHeap.Get(); }
    uint32_t                    GetRTVDescriptorSize() const { return m_rtvDescriptorSize; }
    uint32_t                    GetDSVDescriptorSize() const { return m_dsvDescriptorSize; }
    uint32_t                    GetCBVSRVUAVDescriptorSize() const { return m_cbvSrvUavDescriptorSize; }

    // Frame sync
    void WaitForGPU();
    void MoveToNextFrame();
    uint32_t GetCurrentBackBufferIndex() const { return m_frameIndex; }

    ID3D12CommandAllocator* GetCurrentAllocator() const { return m_commandAllocators[m_frameIndex].Get(); }
    ID3D12Resource*         GetCurrentBackBuffer() const { return m_backBuffers[m_frameIndex].Get(); }
    ID3D12Resource*         GetBackBuffer(uint32_t index) const { return m_backBuffers[index].Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE AllocateRTVDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDSVDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSRVDescriptor();

    // Deferred descriptor free: schedule release after GPU completes fence
    enum class DescriptorType { SRV, RTV, DSV };
    void DeferFreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, ID3D12Fence* fence,
                             uint64_t fenceValue, DescriptorType type);
    void ProcessDeferredFrees();

    // Main fence access (fallback for textures without explicit retire fence)
    ID3D12Fence* GetMainFence() const { return m_fence.Get(); }
    uint64_t GetMainFenceCurrentValue() const { return m_fenceValues[m_frameIndex]; }

    void FlushDebugMessages();
    uint64_t ExecuteComputeCommandLists(ID3D12CommandList* const* lists, uint32_t count);
    void QueueGraphicsWaitForCompute(uint64_t fenceValue);

    static constexpr uint32_t FRAME_COUNT = 2;

private:
    void CreateDevice();
    void CreateCommandQueue();
    void CreateSwapChain(HWND hWnd, uint32_t width, uint32_t height);
    void CreateDescriptorHeaps();
    void CreateFrameResources();
    void CreateFence();

    ComPtr<IDXGIFactory4>            m_dxgiFactory;
    ComPtr<ID3D12Device>             m_device;
    ComPtr<ID3D12CommandQueue>       m_commandQueue;
    ComPtr<ID3D12CommandQueue>       m_computeQueue;
    ComPtr<IDXGISwapChain3>          m_swapChain;

    // Descriptor heaps
    ComPtr<ID3D12DescriptorHeap>     m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap>     m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap>     m_cbvSrvUavHeap;
    ComPtr<ID3D12DescriptorHeap>     m_cbvSrvUavStagingHeap;
    uint32_t m_rtvDescriptorSize = 0;
    uint32_t m_dsvDescriptorSize = 0;
    uint32_t m_cbvSrvUavDescriptorSize = 0;
    uint32_t m_nextRtvDescriptor = FRAME_COUNT;
    uint32_t m_nextDsvDescriptor = 0;
    uint32_t m_nextCbvSrvUavDescriptor = 0;
    uint32_t m_nextStagingSrvDescriptor = 0;

    // Frame resources
    ComPtr<ID3D12CommandAllocator>   m_commandAllocators[FRAME_COUNT];
    ComPtr<ID3D12Resource>           m_backBuffers[FRAME_COUNT];

    // Fence
    ComPtr<ID3D12Fence>              m_fence;
    ComPtr<ID3D12Fence>              m_computeFence;
    HANDLE                           m_fenceEvent = nullptr;
    HANDLE                           m_computeFenceEvent = nullptr;
    uint64_t                         m_fenceValues[FRAME_COUNT] = {};
    uint64_t                         m_computeFenceValue = 1;
    uint32_t                         m_frameIndex = 0;

    // Deferred descriptor free
    struct DeferredDescriptorFree {
        D3D12_CPU_DESCRIPTOR_HANDLE handle;
        ID3D12Fence* fence;
        uint64_t fenceValue;
        DescriptorType type;
    };
    std::vector<DeferredDescriptorFree> m_deferredFrees;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_freeSRVList;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_freeRTVList;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_freeDSVList;
};
