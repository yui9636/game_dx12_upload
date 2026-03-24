#include "DX12Device.h"
#include "Console/Logger.h"
#include <cassert>
#include <stdexcept>
#include <vector>
#include <cstdio>

#ifdef _DEBUG
#include <d3d12sdklayers.h>
#endif

DX12Device::DX12Device(HWND hWnd, uint32_t width, uint32_t height) {
    CreateDevice();
    CreateCommandQueue();
    CreateSwapChain(hWnd, width, height);
    CreateDescriptorHeaps();
    CreateFrameResources();
    CreateFence();
}

DX12Device::~DX12Device() {
    WaitForGPU();
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    if (m_computeFenceEvent) {
        CloseHandle(m_computeFenceEvent);
        m_computeFenceEvent = nullptr;
    }
}

void DX12Device::CreateDevice() {
#ifdef _DEBUG
    // DRED (Device Removed Extended Data) を有効化。
    // debug layer と異なり、break-on-error で強制終了しない。
    // GPU ハング時にどのコマンドで止まったか記録する。
    {
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings)))) {
            dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }
    }
#endif

#ifdef _DEBUG
    // Enable D3D12 debug layer for validation messages
    {
        ComPtr<ID3D12Debug> debugInterface;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)))) {
            debugInterface->EnableDebugLayer();
        }
    }
#endif

    HRESULT hr = CreateDXGIFactory2(
#ifdef _DEBUG
        DXGI_CREATE_FACTORY_DEBUG,
#else
        0,
#endif
        IID_PPV_ARGS(&m_dxgiFactory));
    assert(SUCCEEDED(hr));

    // Try hardware adapter
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; m_dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)))) {
            break;
        }
    }

    if (!m_device) {
        // Fallback to WARP
        ComPtr<IDXGIAdapter> warpAdapter;
        m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
        hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
        assert(SUCCEEDED(hr));
    }

#ifdef _DEBUG
    // Prevent debug layer from force-terminating on validation errors
    {
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, FALSE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);
        }
    }
#endif
}

void DX12Device::CreateCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    HRESULT hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    assert(SUCCEEDED(hr));

    D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
    computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = m_device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_computeQueue));
    assert(SUCCEEDED(hr));
}

void DX12Device::CreateSwapChain(HWND hWnd, uint32_t width, uint32_t height) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = m_dxgiFactory->CreateSwapChainForHwnd(
        m_commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1);
    assert(SUCCEEDED(hr));

    // Disable ALT+ENTER fullscreen
    m_dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

    swapChain1.As(&m_swapChain);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Device::CreateDescriptorHeaps() {
    // RTV heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = FRAME_COUNT + 512; // back buffers + render targets + thumbnails
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }
    // DSV heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 512;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvHeap));
        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }
    // CBV/SRV/UAV heap (shader visible) - 将来のバインドレス用に予約
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 4096;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvSrvUavHeap));
        m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    // CBV/SRV/UAV staging heap (non-shader-visible) - SRV作成用
    // CopyDescriptorsSimple のソースとして使用。Shader-Visible ヒープからの
    // CPU 読み出しは Write-Combined メモリ上で不正確になるため、
    // SRV は必ずこの Non-Shader-Visible ヒープに作成する。
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = 4096;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_cbvSrvUavStagingHeap));
    }
}

void DX12Device::CreateFrameResources() {
    for (uint32_t i = 0; i < FRAME_COUNT; ++i) {
        m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i]));

        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
    }
}

void DX12Device::CreateFence() {
    m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_computeFence));
    m_fenceValues[m_frameIndex] = 1;
    m_computeFenceValue = 1;
    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_computeFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    assert(m_fenceEvent != nullptr);
    assert(m_computeFenceEvent != nullptr);
}

void DX12Device::WaitForGPU() {
    if (!m_commandQueue || !m_fence || !m_fenceEvent) return;

    const uint64_t fenceValue = m_fenceValues[m_frameIndex];
    m_commandQueue->Signal(m_fence.Get(), fenceValue);

    if (m_fence->GetCompletedValue() < fenceValue) {
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    m_fenceValues[m_frameIndex]++;

    if (m_computeQueue && m_computeFence && m_computeFenceEvent) {
        const uint64_t computeFenceValue = m_computeFenceValue;
        m_computeQueue->Signal(m_computeFence.Get(), computeFenceValue);
        if (m_computeFence->GetCompletedValue() < computeFenceValue) {
            m_computeFence->SetEventOnCompletion(computeFenceValue, m_computeFenceEvent);
            WaitForSingleObjectEx(m_computeFenceEvent, INFINITE, FALSE);
        }
        ++m_computeFenceValue;
    }
}

void DX12Device::MoveToNextFrame() {
    const uint64_t currentFenceValue = m_fenceValues[m_frameIndex];
    m_commandQueue->Signal(m_fence.Get(), currentFenceValue);

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
        m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::AllocateRTVDescriptor() {
    if (!m_freeRTVList.empty()) {
        auto handle = m_freeRTVList.back();
        m_freeRTVList.pop_back();
        return handle;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(m_nextRtvDescriptor++) * m_rtvDescriptorSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::AllocateDSVDescriptor() {
    if (!m_freeDSVList.empty()) {
        auto handle = m_freeDSVList.back();
        m_freeDSVList.pop_back();
        return handle;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(m_nextDsvDescriptor++) * m_dsvDescriptorSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::AllocateSRVDescriptor() {
    if (!m_freeSRVList.empty()) {
        auto handle = m_freeSRVList.back();
        m_freeSRVList.pop_back();
        return handle;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cbvSrvUavStagingHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(m_nextStagingSrvDescriptor++) * m_cbvSrvUavDescriptorSize;
    return handle;
}

void DX12Device::DeferFreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, ID3D12Fence* fence,
                                      uint64_t fenceValue, DescriptorType type) {
    if (!handle.ptr) return;
    m_deferredFrees.push_back({ handle, fence, fenceValue, type });
}

void DX12Device::ProcessDeferredFrees() {
    auto it = m_deferredFrees.begin();
    while (it != m_deferredFrees.end()) {
        if (!it->fence || it->fence->GetCompletedValue() >= it->fenceValue) {
            switch (it->type) {
            case DescriptorType::SRV: m_freeSRVList.push_back(it->handle); break;
            case DescriptorType::RTV: m_freeRTVList.push_back(it->handle); break;
            case DescriptorType::DSV: m_freeDSVList.push_back(it->handle); break;
            }
            it = m_deferredFrees.erase(it);
        } else {
            ++it;
        }
    }
}

void DX12Device::FlushDebugMessages() {
#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (FAILED(m_device.As(&infoQueue))) return;

    const UINT64 messageCount = infoQueue->GetNumStoredMessages();
    if (messageCount == 0) return;

    // 最初のフレームのみ最大50件出力（大量ログ防止）
    UINT64 logCount = (messageCount > 50) ? 50 : messageCount;
    for (UINT64 i = 0; i < logCount; ++i) {
        SIZE_T msgLen = 0;
        infoQueue->GetMessage(i, nullptr, &msgLen);
        if (msgLen == 0) continue;

        std::vector<char> buf(msgLen);
        auto* msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
        infoQueue->GetMessage(i, msg, &msgLen);

        if (msg->ID == static_cast<D3D12_MESSAGE_ID>(820)) {
            continue;
        }

        const char* severity = "INFO";
        if (msg->Severity == D3D12_MESSAGE_SEVERITY_ERROR) severity = "ERROR";
        else if (msg->Severity == D3D12_MESSAGE_SEVERITY_WARNING) severity = "WARN";
        else if (msg->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) severity = "CORRUPT";

        LOG_ERROR("[D3D12 %s #%d] %s", severity, (int)msg->ID, msg->pDescription);
    }

    if (messageCount > logCount) {
        LOG_ERROR("[D3D12] ... and %llu more messages", messageCount - logCount);
    }

    infoQueue->ClearStoredMessages();
#endif
}

uint64_t DX12Device::ExecuteComputeCommandLists(ID3D12CommandList* const* lists, uint32_t count)
{
    if (!m_computeQueue || !m_computeFence || !lists || count == 0) {
        return 0;
    }

    m_computeQueue->ExecuteCommandLists(count, lists);
    const uint64_t fenceValue = m_computeFenceValue++;
    m_computeQueue->Signal(m_computeFence.Get(), fenceValue);
    return fenceValue;
}

void DX12Device::QueueGraphicsWaitForCompute(uint64_t fenceValue)
{
    if (!m_commandQueue || !m_computeFence || fenceValue == 0) {
        return;
    }
    m_commandQueue->Wait(m_computeFence.Get(), fenceValue);
}
