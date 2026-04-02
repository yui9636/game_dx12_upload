#include "PlayerEditorWindow.h"

#include <algorithm>
#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>

#include "EditorTheme.h"
#include "Icon/IconsFontAwesome7.h"
#include "ImGuiRenderer.h"
#include "PlayerEditorPanel.h"
#include "RHI/DX12/DX12Device.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
    constexpr DXGI_FORMAT kSwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr float kDetachedEditorFontSize = 18.0f;
    constexpr uint32_t kSecondaryImGuiFontSlot = 1;
    constexpr wchar_t kPlayerEditorWindowClassName[] = L"MyEnginePlayerEditorWindow";

    void SetupDetachedEditorFonts(ImGuiIO& io)
    {
        ImFont* baseFont = io.Fonts->AddFontFromFileTTF(
            "Data/Font/ArialUni.ttf",
            kDetachedEditorFontSize,
            nullptr,
            io.Fonts->GetGlyphRangesJapanese());
        IM_ASSERT(baseFont != nullptr);
        io.FontDefault = baseFont;

        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig iconConfig;
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;

        ImFont* iconFont = io.Fonts->AddFontFromFileTTF(
            "Data/Font/Font Awesome 7 Free-Solid-900.otf",
            kDetachedEditorFontSize,
            &iconConfig,
            iconRanges);
        IM_ASSERT(iconFont != nullptr);
    }

    void TryEnableDarkTitleBar(HWND hwnd)
    {
        HMODULE dwmApi = LoadLibraryW(L"dwmapi.dll");
        if (!dwmApi) {
            return;
        }

        using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
        auto* setWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
            GetProcAddress(dwmApi, "DwmSetWindowAttribute"));

        if (setWindowAttribute) {
            constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
            BOOL enabled = TRUE;
            setWindowAttribute(
                hwnd,
                DWMWA_USE_IMMERSIVE_DARK_MODE,
                &enabled,
                sizeof(enabled));
        }

        FreeLibrary(dwmApi);
    }
}

PlayerEditorWindow::~PlayerEditorWindow()
{
    Shutdown();
}

bool PlayerEditorWindow::Initialize(HWND ownerWindow, DX12Device* device)
{
    if (m_hwnd && m_imguiContext) {
        return true;
    }

    m_ownerWindow = ownerWindow;
    m_device = device;
    if (!m_device) {
        return false;
    }

    if (!EnsureWindowClassRegistered() || !CreateNativeWindow(ownerWindow) || !CreateDeviceResources()) {
        Shutdown();
        return false;
    }

    ImGuiContext* previousContext = ImGui::GetCurrentContext();
    m_imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imguiContext);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifndef IMGUI_NO_DOCKING_FALLBACK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    io.IniFilename = nullptr;

    ApplyEditorGrayTheme();
    SetupDetachedEditorFonts(io);

    if (!ImGui_ImplWin32_Init(m_hwnd)) {
        ImGui::SetCurrentContext(previousContext);
        Shutdown();
        return false;
    }

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = m_device->GetDevice();
    initInfo.CommandQueue = m_device->GetCommandQueue();
    initInfo.NumFramesInFlight = static_cast<int>(kFrameCount);
    initInfo.RTVFormat = kSwapChainFormat;
    initInfo.SrvDescriptorHeap = ImGuiRenderer::GetDX12SrvHeap();
    initInfo.LegacySingleSrvCpuDescriptor = ImGuiRenderer::GetDX12SrvCpuHandle(kSecondaryImGuiFontSlot);
    initInfo.LegacySingleSrvGpuDescriptor = ImGuiRenderer::GetDX12SrvGpuHandle(kSecondaryImGuiFontSlot);

    if (!ImGui_ImplDX12_Init(&initInfo)) {
        ImGui_ImplWin32_Shutdown();
        ImGui::SetCurrentContext(previousContext);
        Shutdown();
        return false;
    }

    ImGui::SetCurrentContext(previousContext);
    UpdateWindowVisibility(false);
    return true;
}

void PlayerEditorWindow::Shutdown()
{
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    if (m_imguiContext) {
        ImGuiContext* destroyedContext = m_imguiContext;
        ImGuiContext* previousContext = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(destroyedContext);
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::SetCurrentContext(nullptr);
        ImGui::DestroyContext(destroyedContext);
        if (previousContext && previousContext != destroyedContext) {
            ImGui::SetCurrentContext(previousContext);
        }
        m_imguiContext = nullptr;
    }

    WaitForGpu();

    for (UINT i = 0; i < kFrameCount; ++i) {
        m_backBuffers[i].Reset();
        m_commandAllocators[i].Reset();
        m_rtvHandles[i] = {};
    }

    m_commandList.Reset();
    m_rtvHeap.Reset();
    m_swapChain.Reset();
    m_fence.Reset();

    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    m_visible = false;
    m_closeRequested = false;
    m_minimized = false;
    m_needsResize = false;
    m_pendingWidth = 0;
    m_pendingHeight = 0;
    m_frameIndex = 0;
    m_ownerWindow = nullptr;
    m_device = nullptr;
    m_rtvDescriptorSize = 0;
    for (UINT i = 0; i < kFrameCount; ++i) {
        m_fenceValues[i] = 0;
    }
}

void PlayerEditorWindow::Render(PlayerEditorPanel& panel, Registry* registry, bool* p_open, bool requestFocus, bool* outFocused)
{
    if (outFocused) {
        *outFocused = false;
    }
    if (!p_open) {
        return;
    }

    if (!*p_open) {
        SetVisible(false);
        return;
    }

    if (m_closeRequested) {
        m_closeRequested = false;
        *p_open = false;
        SetVisible(false);
        return;
    }

    if (!m_hwnd || !m_imguiContext) {
        if (!Initialize(m_ownerWindow, m_device)) {
            *p_open = false;
            return;
        }
    }

    UpdateWindowVisibility(true);

    if (requestFocus && m_hwnd) {
        ShowWindow(m_hwnd, m_minimized ? SW_RESTORE : SW_SHOW);
        SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(m_hwnd);
        SetFocus(m_hwnd);
    }

    if (m_needsResize && !m_minimized && m_pendingWidth > 0 && m_pendingHeight > 0) {
        ResizeSwapChain(m_pendingWidth, m_pendingHeight);
        m_needsResize = false;
    }

    if (m_minimized || !m_swapChain || !m_imguiContext) {
        return;
    }

    ImGuiContext* previousContext = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(m_imguiContext);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    panel.DrawDetached(registry, p_open, outFocused);

    ImGui::Render();

    ID3D12CommandAllocator* allocator = m_commandAllocators[m_frameIndex].Get();
    allocator->Reset();
    m_commandList->Reset(allocator, nullptr);

    D3D12_RESOURCE_BARRIER toRenderTarget = {};
    toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRenderTarget.Transition.pResource = m_backBuffers[m_frameIndex].Get();
    toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &toRenderTarget);

    const float clearColor[4] = { 0.10f, 0.10f, 0.11f, 1.0f };
    m_commandList->OMSetRenderTargets(1, &m_rtvHandles[m_frameIndex], FALSE, nullptr);
    m_commandList->ClearRenderTargetView(m_rtvHandles[m_frameIndex], clearColor, 0, nullptr);

    if (ID3D12DescriptorHeap* srvHeap = ImGuiRenderer::GetDX12SrvHeap()) {
        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        m_commandList->SetDescriptorHeaps(1, heaps);
    }
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

    D3D12_RESOURCE_BARRIER toPresent = toRenderTarget;
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &toPresent);

    m_commandList->Close();

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_device->GetCommandQueue()->ExecuteCommandLists(1, commandLists);

    m_swapChain->Present(1, 0);

    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    m_device->GetCommandQueue()->Signal(m_fence.Get(), currentFenceValue);

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
        m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;

    if (outFocused && !*outFocused) {
        *outFocused = IsFocused();
    }

    ImGui::SetCurrentContext(previousContext);
}

void PlayerEditorWindow::SetVisible(bool visible)
{
    UpdateWindowVisibility(visible);
}

bool PlayerEditorWindow::IsFocused() const
{
    if (!m_hwnd) {
        return false;
    }

    HWND foreground = GetForegroundWindow();
    HWND focused = GetFocus();
    return foreground == m_hwnd ||
        (focused != nullptr && (focused == m_hwnd || IsChild(m_hwnd, focused)));
}

LRESULT CALLBACK PlayerEditorWindow::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<PlayerEditorWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->m_hwnd = hwnd;
        }
    }

    auto* self = reinterpret_cast<PlayerEditorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        return self->WndProc(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT PlayerEditorWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (m_imguiContext) {
        ImGuiContext* previousContext = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(m_imguiContext);
        const LRESULT handled = ImGui_ImplWin32_WndProcHandler(m_hwnd, msg, wParam, lParam);
        ImGui::SetCurrentContext(previousContext);
        if (handled) {
            return handled;
        }
    }

    switch (msg) {
    case WM_CLOSE:
        m_closeRequested = true;
        UpdateWindowVisibility(false);
        return 0;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            m_minimized = true;
            return 0;
        }

        m_minimized = false;
        m_pendingWidth = LOWORD(lParam);
        m_pendingHeight = HIWORD(lParam);
        if (m_pendingWidth > 0 && m_pendingHeight > 0) {
            m_needsResize = true;
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

bool PlayerEditorWindow::EnsureWindowClassRegistered()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW existing = {};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(instance, kPlayerEditorWindowClassName, &existing)) {
        return true;
    }

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &PlayerEditorWindow::StaticWndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.lpszClassName = kPlayerEditorWindowClassName;
    return RegisterClassExW(&windowClass) != 0;
}

bool PlayerEditorWindow::CreateNativeWindow(HWND ownerWindow)
{
    RECT ownerRect = {};
    int posX = CW_USEDEFAULT;
    int posY = CW_USEDEFAULT;
    if (ownerWindow && GetWindowRect(ownerWindow, &ownerRect)) {
        posX = ownerRect.left + 48;
        posY = ownerRect.top + 48;
    }

    RECT windowRect = { 0, 0, 1400, 900 };
    const DWORD style = WS_OVERLAPPEDWINDOW;
    const DWORD exStyle = WS_EX_APPWINDOW;
    AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);

    m_hwnd = CreateWindowExW(
        exStyle,
        kPlayerEditorWindowClassName,
        L"Player Editor",
        style,
        posX,
        posY,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        ownerWindow,
        nullptr,
        GetModuleHandleW(nullptr),
        this);

    if (m_hwnd) {
        TryEnableDarkTitleBar(m_hwnd);
    }

    return m_hwnd != nullptr;
}

bool PlayerEditorWindow::CreateDeviceResources()
{
    RECT clientRect = {};
    GetClientRect(m_hwnd, &clientRect);
    UINT width = (std::max)(1L, clientRect.right - clientRect.left);
    UINT height = (std::max)(1L, clientRect.bottom - clientRect.top);

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = kFrameCount;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = kSwapChainFormat;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = m_device->IsTearingSupported() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(factory->CreateSwapChainForHwnd(
        m_device->GetCommandQueue(), m_hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1))) {
        return false;
    }

    factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    if (FAILED(swapChain1.As(&m_swapChain))) {
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = kFrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)))) {
        return false;
    }

    m_rtvDescriptorSize = m_device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (UINT i = 0; i < kFrameCount; ++i) {
        if (FAILED(m_device->GetDevice()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])))) {
            return false;
        }
    }

    if (FAILED(m_device->GetDevice()->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[0].Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)))) {
        return false;
    }
    m_commandList->Close();

    if (FAILED(m_device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        return false;
    }

    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        return false;
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_fenceValues[m_frameIndex] = 1;
    RebuildRenderTargets();
    return true;
}

void PlayerEditorWindow::RebuildRenderTargets()
{
    if (!m_swapChain || !m_rtvHeap) {
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; ++i) {
        m_backBuffers[i].Reset();
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        m_rtvHandles[i] = handle;
        m_device->GetDevice()->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, handle);
        handle.ptr += m_rtvDescriptorSize;
    }
}

void PlayerEditorWindow::ResizeSwapChain(UINT width, UINT height)
{
    if (!m_swapChain || width == 0 || height == 0) {
        return;
    }

    WaitForGpu();

    for (UINT i = 0; i < kFrameCount; ++i) {
        m_backBuffers[i].Reset();
        m_rtvHandles[i] = {};
    }

    DXGI_SWAP_CHAIN_DESC desc = {};
    m_swapChain->GetDesc(&desc);
    if (FAILED(m_swapChain->ResizeBuffers(kFrameCount, width, height, kSwapChainFormat, desc.Flags))) {
        return;
    }

    for (UINT i = 0; i < kFrameCount; ++i) {
        m_fenceValues[i] = 0;
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_fenceValues[m_frameIndex] = 1;
    RebuildRenderTargets();
}

void PlayerEditorWindow::WaitForGpu()
{
    if (!m_device || !m_fence || !m_fenceEvent) {
        return;
    }

    UINT64 fenceValue = 0;
    for (UINT i = 0; i < kFrameCount; ++i) {
        fenceValue = (std::max)(fenceValue, m_fenceValues[i]);
    }
    if (fenceValue == 0) {
        fenceValue = 1;
    }

    m_device->GetCommandQueue()->Signal(m_fence.Get(), fenceValue);
    if (m_fence->GetCompletedValue() < fenceValue) {
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }
}

void PlayerEditorWindow::UpdateWindowVisibility(bool visible)
{
    if (!m_hwnd) {
        return;
    }

    if (visible) {
        ShowWindow(m_hwnd, m_minimized ? SW_RESTORE : SW_SHOW);
    }
    else {
        ShowWindow(m_hwnd, SW_HIDE);
    }
    m_visible = visible;
}
