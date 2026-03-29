#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <backends/imgui_impl_dx12.h>
#include <ImGuizmo.h>
#include <profiler.h>
#include <cstdint>
#include "ImGuiRenderer.h"
#include "Graphics.h"
#include "RHI/ITexture.h"
#include "RHI/DX11/DX11Texture.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/DX12/DX12Texture.h"

#ifndef IMGUI_HAS_DOCK
#define IMGUI_NO_DOCKING_FALLBACK
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

bool ImGuiRenderer::s_isDX12 = false;
DX12Device* ImGuiRenderer::s_dx12Device = nullptr;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ImGuiRenderer::s_imguiSrvHeap;
uint32_t ImGuiRenderer::s_descriptorSize = 0;
uint32_t ImGuiRenderer::s_nextTextureSlot = 1;
std::unordered_map<const ITexture*, uint32_t> ImGuiRenderer::s_textureSlots;
std::vector<ImGuiRenderer::DeferredTextureSlot> ImGuiRenderer::s_deferredUnregisters;
std::vector<uint32_t> ImGuiRenderer::s_freeSlots;

void ImGuiRenderer::Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* dc)
{
    s_isDX12 = false;
    s_dx12Device = nullptr;
    ResetTextureCache();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

#ifndef IMGUI_NO_DOCKING_FALLBACK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
    io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;
#endif

    ImGui::StyleColorsDark();

#ifndef IMGUI_NO_DOCKING_FALLBACK
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
#endif

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(device, dc);

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "Data/Font/ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    IM_ASSERT(font != nullptr);
}

void ImGuiRenderer::InitializeDX12(HWND hWnd, DX12Device* dx12Device)
{
    s_isDX12 = true;
    s_dx12Device = dx12Device;
    ResetTextureCache();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

#ifndef IMGUI_NO_DOCKING_FALLBACK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hWnd);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1024;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    dx12Device->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&s_imguiSrvHeap));
    s_descriptorSize = dx12Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = dx12Device->GetDevice();
    initInfo.CommandQueue = dx12Device->GetCommandQueue();
    initInfo.NumFramesInFlight = DX12Device::FRAME_COUNT;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.SrvDescriptorHeap = s_imguiSrvHeap.Get();
    initInfo.LegacySingleSrvCpuDescriptor = s_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
    initInfo.LegacySingleSrvGpuDescriptor = s_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart();

    ImGui_ImplDX12_Init(&initInfo);

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "Data/Font/ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    IM_ASSERT(font != nullptr);
}

void ImGuiRenderer::Finalize()
{
    ResetTextureCache();

    if (s_isDX12) {
        ImGui_ImplDX12_Shutdown();
    }
    else {
        ImGui_ImplDX11_Shutdown();
    }

    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_imguiSrvHeap.Reset();
    s_dx12Device = nullptr;
    s_descriptorSize = 0;
}

void ImGuiRenderer::Begin()
{
    if (s_isDX12) {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }
    else {
        NewFrame();
    }
}

void ImGuiRenderer::NewFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderer::Render(ID3D11DeviceContext* /*context*/)
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiRenderer::End()
{
    ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
    Render(dc);
}

void ImGuiRenderer::RenderDX12(ID3D12GraphicsCommandList* commandList)
{
    if (s_imguiSrvHeap) {
        ID3D12DescriptorHeap* heaps[] = { s_imguiSrvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
    }
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

LRESULT ImGuiRenderer::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

void* ImGuiRenderer::GetTextureID(ITexture* texture)
{
    if (!texture) {
        return nullptr;
    }

    if (!s_isDX12) {
        auto* dx11Texture = dynamic_cast<DX11Texture*>(texture);
        return dx11Texture ? reinterpret_cast<void*>(dx11Texture->GetNativeSRV()) : nullptr;
    }

    auto* dx12Texture = dynamic_cast<DX12Texture*>(texture);
    if (!dx12Texture || !dx12Texture->HasSRV() || !s_dx12Device || !s_imguiSrvHeap) {
        return nullptr;
    }

    auto found = s_textureSlots.find(texture);
    uint32_t slot = 0;
    if (found != s_textureSlots.end()) {
        slot = found->second;
    }
    else {
        const uint32_t maxSlots = 1024;
        if (!s_freeSlots.empty()) {
            slot = s_freeSlots.back();
            s_freeSlots.pop_back();
        } else {
            if (s_nextTextureSlot >= maxSlots) {
                return nullptr;
            }
            slot = s_nextTextureSlot++;
        }

        if (slot >= maxSlots) {
            return nullptr;
        }
        s_textureSlots.emplace(texture, slot);

        D3D12_CPU_DESCRIPTOR_HANDLE dstCpu = s_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
        dstCpu.ptr += static_cast<SIZE_T>(slot) * s_descriptorSize;
        s_dx12Device->GetDevice()->CopyDescriptorsSimple(
            1,
            dstCpu,
            dx12Texture->GetSRV(),
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = s_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += static_cast<UINT64>(slot) * s_descriptorSize;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(gpuHandle.ptr));
}

void ImGuiRenderer::DeferUnregisterTexture(ITexture* texture, uint64_t fenceValue)
{
    if (!texture || !s_isDX12) {
        return;
    }

    auto it = s_textureSlots.find(texture);
    if (it == s_textureSlots.end()) {
        return;
    }

    s_deferredUnregisters.push_back({ it->first, it->second, fenceValue });
    s_textureSlots.erase(it);
}

void ImGuiRenderer::ProcessDeferredUnregisters(uint64_t completedFenceValue)
{
    auto it = s_deferredUnregisters.begin();
    while (it != s_deferredUnregisters.end()) {
        if (it->fenceValue <= completedFenceValue) {
            s_freeSlots.push_back(it->slot);
            it = s_deferredUnregisters.erase(it);
        } else {
            ++it;
        }
    }
}

bool ImGuiRenderer::RebuildFontAtlas()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();

    if (s_isDX12) {
        ImGui_ImplDX12_InvalidateDeviceObjects();
        return ImGui_ImplDX12_CreateDeviceObjects();
    }

    ImGui_ImplDX11_InvalidateDeviceObjects();
    return ImGui_ImplDX11_CreateDeviceObjects();
}

void ImGuiRenderer::ResetTextureCache()
{
    s_textureSlots.clear();
    s_deferredUnregisters.clear();
    s_freeSlots.clear();
    s_nextTextureSlot = 1;
}
