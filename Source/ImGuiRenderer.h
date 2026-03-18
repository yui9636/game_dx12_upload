#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <unordered_map>
#include <wrl/client.h>

class DX12Device;
class ITexture;

class ImGuiRenderer
{
public:
    // DX11 初期化
    static void Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* dc);

    // DX12 初期化
    static void InitializeDX12(HWND hWnd, DX12Device* dx12Device);

    // 後始末
    static void Finalize();

    // フレーム開始。DX11/DX12 を内部で分岐する。
    static void Begin();

    // DX11 描画終了
    static void End();

    // DX12 描画終了。ImGui::Render + RenderDrawData をまとめて行う。
    static void RenderDX12(ID3D12GraphicsCommandList* commandList);

    // Win32 メッセージハンドラ
    static LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // ITexture を ImGui::Image 用の ImTextureID に変換する。
    static void* GetTextureID(ITexture* texture);

private:
    static void NewFrame();
    static void Render(ID3D11DeviceContext* context);
    static void ResetTextureCache();

    static bool s_isDX12;
    static DX12Device* s_dx12Device;
    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> s_imguiSrvHeap;
    static uint32_t s_descriptorSize;
    static uint32_t s_nextTextureSlot;
    static std::unordered_map<const ITexture*, uint32_t> s_textureSlots;
};
