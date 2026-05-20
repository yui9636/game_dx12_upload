#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include <vector>

using Microsoft::WRL::ComPtr;

class DX12Device {
public:
    // DX12 の device / queue / swapchain / fence をまとめて初期化する。
    DX12Device(HWND hWnd, uint32_t width, uint32_t height);
    ~DX12Device();

    // DX12 ネイティブオブジェクトへの参照。所有権は DX12Device が持つ。
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

    // swapchain と back buffer を新しい window size に合わせて作り直す。
    void ResizeSwapChain(uint32_t width, uint32_t height);

    // GPU の全 work が終わるまで CPU を待たせる。resize や終了処理で使う。
    void WaitForGPU();
    // Present 後に次の back buffer と fence 値へ進める。
    void MoveToNextFrame();
    uint32_t GetCurrentBackBufferIndex() const { return m_frameIndex; }

    // 現在の frame index に対応する command allocator / back buffer。
    ID3D12CommandAllocator* GetCurrentAllocator() const { return m_commandAllocators[m_frameIndex].Get(); }
    ID3D12Resource*         GetCurrentBackBuffer() const { return m_backBuffers[m_frameIndex].Get(); }
    ID3D12Resource*         GetBackBuffer(uint32_t index) const { return m_backBuffers[index].Get(); }

    // CPU visible descriptor を 1 個確保する。解放は DeferFreeDescriptor で行う。
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateRTVDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDSVDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateSRVDescriptor();

    // 遅延 descriptor 解放は GPU fence 完了後に実行するよう予約する。
    enum class DescriptorType { SRV, RTV, DSV };
    void DeferFreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, ID3D12Fence* fence,
                             uint64_t fenceValue, DescriptorType type);
    // GPU が参照し終わるまで resource の ComPtr を保持し、use-after-free を防ぐ。
    void DeferReleaseResource(ComPtr<ID3D12Resource> resource, ID3D12Fence* fence,
                              uint64_t fenceValue);
    // fence 完了済みの descriptor / resource を実際に free list へ戻す。
    void ProcessDeferredFrees();

    // 明示的な retire fence を持たない texture 用に main fence へアクセスする。
    ID3D12Fence* GetMainFence() const { return m_fence.Get(); }
    uint64_t GetMainFenceCurrentValue() const { return m_fenceValues[m_frameIndex]; }

    // debug layer の蓄積メッセージをログへ吐き出す。
    void FlushDebugMessages();
    // compute queue へ command list を投入し、完了待ち用 fence 値を返す。
    uint64_t ExecuteComputeCommandLists(ID3D12CommandList* const* lists, uint32_t count);
    // graphics queue が compute fence の完了を待つようにする。
    void QueueGraphicsWaitForCompute(uint64_t fenceValue);
    bool IsTearingSupported() const { return m_allowTearing; }

    static constexpr uint32_t FRAME_COUNT = 2;

private:
    static constexpr uint32_t RTV_DESCRIPTOR_COUNT = FRAME_COUNT + 4096;
    static constexpr uint32_t DSV_DESCRIPTOR_COUNT = 2048;
    static constexpr uint32_t SRV_DESCRIPTOR_COUNT = 16384;

    void CreateDevice();
    void CreateCommandQueue();
    void CreateSwapChain(HWND hWnd, uint32_t width, uint32_t height);
    void CreateDescriptorHeaps();
    void CreateFrameResources();
    void CreateFence();

    ComPtr<IDXGIFactory4>            m_dxgiFactory;    // adapter / swapchain 作成用 factory。
    ComPtr<ID3D12Device>             m_device;         // DX12 device 本体。
    ComPtr<ID3D12CommandQueue>       m_commandQueue;   // graphics 用 queue。
    ComPtr<ID3D12CommandQueue>       m_computeQueue;   // async compute 用 queue。
    ComPtr<IDXGISwapChain3>          m_swapChain;      // window 表示用 swapchain。

    // descriptor heap 群。
    ComPtr<ID3D12DescriptorHeap>     m_rtvHeap;                 // RTV を置く CPU descriptor heap。
    ComPtr<ID3D12DescriptorHeap>     m_dsvHeap;                 // DSV を置く CPU descriptor heap。
    ComPtr<ID3D12DescriptorHeap>     m_cbvSrvUavHeap;           // shader visible な将来用 heap。
    ComPtr<ID3D12DescriptorHeap>     m_cbvSrvUavStagingHeap;    // SRV 作成・コピー元に使う CPU heap。
    uint32_t m_rtvDescriptorSize = 0;                           // RTV heap の 1 descriptor 幅。
    uint32_t m_dsvDescriptorSize = 0;                           // DSV heap の 1 descriptor 幅。
    uint32_t m_cbvSrvUavDescriptorSize = 0;                     // CBV/SRV/UAV heap の 1 descriptor 幅。
    uint32_t m_nextRtvDescriptor = FRAME_COUNT;                 // back buffer 分を避けた次の RTV index。
    uint32_t m_nextDsvDescriptor = 0;                           // 次に新規確保する DSV index。
    uint32_t m_nextCbvSrvUavDescriptor = 0;                     // shader visible heap 用の予約 index。
    uint32_t m_nextStagingSrvDescriptor = 0;                    // staging SRV heap の次 index。

    // frame resource 群。
    ComPtr<ID3D12CommandAllocator>   m_commandAllocators[FRAME_COUNT]; // frame ごとの command allocator。
    ComPtr<ID3D12Resource>           m_backBuffers[FRAME_COUNT];       // swapchain back buffer。

    // fence 管理。
    ComPtr<ID3D12Fence>              m_fence;                   // graphics queue の同期 fence。
    ComPtr<ID3D12Fence>              m_computeFence;            // compute queue の同期 fence。
    HANDLE                           m_fenceEvent = nullptr;    // graphics fence 完了待ちイベント。
    HANDLE                           m_computeFenceEvent = nullptr; // compute fence 完了待ちイベント。
    uint64_t                         m_fenceValues[FRAME_COUNT] = {}; // frame slot ごとの次 fence 値。
    uint64_t                         m_computeFenceValue = 1;   // compute queue の次 fence 値。
    uint32_t                         m_frameIndex = 0;          // 現在の swapchain buffer index。
    bool                             m_allowTearing = false;    // variable refresh / tearing present が可能か。

    // 遅延 descriptor 解放。GPU がまだ descriptor を参照している間は free list に戻さない。
    struct DeferredDescriptorFree {
        D3D12_CPU_DESCRIPTOR_HANDLE handle; // 後で再利用する CPU descriptor。
        ID3D12Fence* fence;                 // 完了判定に使う fence。
        uint64_t fenceValue;                // この値以上になったら解放可能。
        DescriptorType type;                // 戻す free list の種類。
    };
    struct DeferredResourceRelease {
        ComPtr<ID3D12Resource> resource;    // fence 完了まで生かす GPU resource。
        ID3D12Fence* fence;                 // resource 参照完了を判定する fence。
        uint64_t fenceValue;                // この値以上で ComPtr を破棄できる。
    };
    std::vector<DeferredDescriptorFree> m_deferredFrees;        // fence 待ち中の descriptor。
    std::vector<DeferredResourceRelease> m_deferredResources;   // fence 待ち中の resource。
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_freeSRVList;     // 再利用可能な SRV descriptor。
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_freeRTVList;     // 再利用可能な RTV descriptor。
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_freeDSVList;     // 再利用可能な DSV descriptor。
};
