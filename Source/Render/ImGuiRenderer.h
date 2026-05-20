#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <unordered_map>
#include <vector>
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

    static DX12Device* GetDX12Device() { return s_dx12Device; }
    static ID3D12DescriptorHeap* GetDX12SrvHeap() { return s_imguiSrvHeap.Get(); }
    // DX12 ImGui SRV heap の指定 slot に対応する CPU handle。
    static D3D12_CPU_DESCRIPTOR_HANDLE GetDX12SrvCpuHandle(uint32_t slot);
    // ImGui::Image が保持する GPU visible handle。
    static D3D12_GPU_DESCRIPTOR_HANDLE GetDX12SrvGpuHandle(uint32_t slot);

    // Win32 メッセージハンドラ
    static LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // ITexture を ImGui::Image 用の ImTextureID に変換する。
    static void* GetTextureID(ITexture* texture);
    // completedFenceValue 方式で texture slot 解放を遅延登録する。
    static void DeferUnregisterTexture(ITexture* texture, uint64_t fenceValue);
    // 明示 fence を使って texture slot 解放を遅延登録する。
    static void DeferUnregisterTexture(ITexture* texture, ID3D12Fence* fence, uint64_t fenceValue);
    // 完了済み fence まで進んだ deferred unregister を実行する。
    static void ProcessDeferredUnregisters(uint64_t completedFenceValue);
    // font atlas SRV を作り直す。
    static bool RebuildFontAtlas();

private:
    // backend 共通の ImGui NewFrame。
    static void NewFrame();
    // DX11 backend の ImGui draw data を描画する。
    static void Render(ID3D11DeviceContext* context);
    // texture -> descriptor slot cache を初期状態へ戻す。
    static void ResetTextureCache();
    // DX12 ImGui SRV heap の空き slot を取得する。
    static uint32_t AllocateDX12DescriptorSlot();
    // CPU handle から slot を逆算し、free list へ戻す。
    static void FreeDX12DescriptorSlot(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

    struct DeferredTextureSlot {
        const ITexture* texture = nullptr; // 解放待ちの RHI texture pointer。
        uint32_t slot = 0;                 // ImGui SRV heap 上の slot。
        ID3D12Fence* fence = nullptr;      // 明示 fence。nullptr の場合は completedFenceValue を使う。
        uint64_t fenceValue = 0;           // 解放してよい fence value。
    };

    static bool s_isDX12; // 現在の ImGui backend が DX12 かどうか。
    static DX12Device* s_dx12Device; // DX12 device。非所有。
    static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> s_imguiSrvHeap; // ImGui 専用 shader-visible SRV heap。
    static uint32_t s_descriptorSize; // SRV descriptor の stride。
    static uint32_t s_nextTextureSlot; // まだ使ったことのない次 slot。
    static std::unordered_map<const ITexture*, uint32_t> s_textureSlots; // texture pointer から SRV heap slot への cache。
    static std::vector<DeferredTextureSlot> s_deferredUnregisters; // GPU 完了待ちの slot 解放リスト。
    static std::vector<uint32_t> s_freeSlots; // 再利用可能な ImGui SRV heap slot。
};
