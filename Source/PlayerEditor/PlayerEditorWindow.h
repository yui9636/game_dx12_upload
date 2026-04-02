#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

class DX12Device;
class Registry;
class PlayerEditorPanel;
struct ImGuiContext;

class PlayerEditorWindow
{
public:
    PlayerEditorWindow() = default;
    ~PlayerEditorWindow();

    bool Initialize(HWND ownerWindow, DX12Device* device);
    void Shutdown();

    void Render(PlayerEditorPanel& panel, Registry* registry, bool* p_open, bool requestFocus, bool* outFocused);
    void SetVisible(bool visible);
    bool IsFocused() const;

private:
    static constexpr UINT kFrameCount = 2;

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

    bool EnsureWindowClassRegistered();
    bool CreateNativeWindow(HWND ownerWindow);
    bool CreateDeviceResources();
    void RebuildRenderTargets();
    void ResizeSwapChain(UINT width, UINT height);
    void WaitForGpu();
    void UpdateWindowVisibility(bool visible);

    HWND m_ownerWindow = nullptr;
    HWND m_hwnd = nullptr;
    DX12Device* m_device = nullptr;
    ImGuiContext* m_imguiContext = nullptr;

    bool m_visible = false;
    bool m_closeRequested = false;
    bool m_minimized = false;
    bool m_needsResize = false;
    UINT m_pendingWidth = 0;
    UINT m_pendingHeight = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[kFrameCount];
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[kFrameCount] = {};
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[kFrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValues[kFrameCount] = {};
    UINT m_frameIndex = 0;
    UINT m_rtvDescriptorSize = 0;
};
