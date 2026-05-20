#pragma once
#include "RHI/ITexture.h"
#include "DX12Device.h"

class DX12Texture : public ITexture {
public:
    // render graph や FrameBuffer が作る通常の 2D texture。
    // bindFlags に応じて RTV / DSV / SRV descriptor も同時に作成する。
    DX12Texture(DX12Device* device, uint32_t width, uint32_t height,
                TextureFormat format, TextureBindFlags bindFlags,
                const float* optimizedClearColor = nullptr);

    // swapchain back buffer を ITexture として扱うための wrapper。
    // resource と RTV descriptor の実体は swapchain / back buffer heap 側が所有する。
    DX12Texture(DX12Device* device, ID3D12Resource* backBuffer, uint32_t index);

    // file から読み込んだ texture resource を SRV 専用で包む。
    // resource は factory 側で upload 済みの DEFAULT heap resource。
    DX12Texture(DX12Device* device, ComPtr<ID3D12Resource> resource,
                uint32_t width, uint32_t height, DXGI_FORMAT srvFormat,
                bool isCubemap = false);

    // depth array など、配列 texture 全体を SRV として参照するための constructor。
    DX12Texture(DX12Device* device, uint32_t width, uint32_t height,
                TextureFormat format, uint32_t arraySize,
                TextureBindFlags bindFlags);

    // 配列 texture の 1 slice を DSV として扱う wrapper。
    // sharedResource の寿命は full resource 側と共有される。
    DX12Texture(DX12Device* device, ComPtr<ID3D12Resource> sharedResource,
                uint32_t width, uint32_t height, uint32_t arraySlice);

    ~DX12Texture() override;

    uint32_t GetWidth()  const override { return m_width; }
    uint32_t GetHeight() const override { return m_height; }
    TextureFormat GetFormat() const override { return m_format; }
    ResourceState GetCurrentState() const override { return m_currentState; }
    void SetCurrentState(ResourceState state) override { m_currentState = state; }

    // DX12 native object / descriptor への access。descriptor の所有権は DX12Texture が管理する。
    ID3D12Resource*             GetNativeResource() const { return m_resource.Get(); }
    ComPtr<ID3D12Resource>      GetNativeResourceComPtr() const { return m_resource; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return m_rtvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_dsvHandle; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return m_srvHandle; }
    bool HasRTV() const { return m_hasRTV; }
    bool HasDSV() const { return m_hasDSV; }
    bool HasSRV() const { return m_hasSRV; }
    // texture を明示的な fence 値に紐づけて破棄したい場合に設定する。
    // 未設定なら destructor が DX12Device の main fence を使う。
    void SetRetireFence(ID3D12Fence* fence, uint64_t value) {
        m_retireFence = fence;
        m_retireFenceValue = value;
    }

private:
    // RHI の TextureFormat を DXGI_FORMAT へ変換する。
    DXGI_FORMAT ToDXGIFormat(TextureFormat format);
    // bindFlags から D3D12_RESOURCE_FLAGS を作る。
    D3D12_RESOURCE_FLAGS ToResourceFlags(TextureBindFlags flags);

    ComPtr<ID3D12Resource> m_resource;       // texture resource 本体。
    uint32_t m_width = 0, m_height = 0;      // pixel 単位の texture サイズ。
    TextureFormat m_format = TextureFormat::Unknown; // RHI 側の論理 format。
    ResourceState m_currentState = ResourceState::Common; // command list が追跡する現在 state。

    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle = {}; // RTV descriptor。未作成なら ptr=0。
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle = {}; // DSV descriptor。未作成なら ptr=0。
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle = {}; // SRV descriptor。未作成なら ptr=0。
    bool m_hasRTV = false, m_hasDSV = false, m_hasSRV = false; // 各 view を持つか。
    bool m_ownsRTVDescriptor = false; // back buffer wrapper など descriptor 非所有を区別する。
    bool m_ownsDSVDescriptor = false; // true の場合だけ destructor で free list へ戻す。
    bool m_ownsSRVDescriptor = false; // true の場合だけ destructor で free list へ戻す。
    bool m_deferResourceRelease = true; // resource を fence 完了まで遅延破棄するか。
    DX12Device* m_device = nullptr; // descriptor 確保・遅延解放に使う device。非所有。
    ID3D12Fence* m_retireFence = nullptr; // 明示 retire 用 fence。非所有。
    uint64_t m_retireFenceValue = 0; // retireFence がこの値以上なら解放可能。
};
