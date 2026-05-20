#pragma once

#include <memory>
#include <array>
#include <DirectXMath.h>
#include <string>
#include <unordered_map>
#include <wrl/client.h>
#include <d3d12.h>
#include "RHI/ITexture.h"

// RHI 前方宣言
class IShader;
class IBuffer;
class ICommandList;
class IPipelineState;
class IResourceFactory;
struct RenderContext;

class Skybox
{
public:
    // filename ごとに Skybox を cache し、同じ cubemap の重複生成を避ける。
    static Skybox* Get(IResourceFactory* factory, const std::string& filename);
    static void ClearCache() { s_cache.clear(); }

    // cubemap texture、shader、constant buffer、PSO を作成する。
    Skybox(IResourceFactory* factory, const char* filename);
    ~Skybox();

    // viewProjection から逆行列を作り、画面全体の skybox を描画する。
    void Draw(const RenderContext& rc, const DirectX::XMFLOAT4X4& viewProjection);

private:
    struct Constants
    {
        DirectX::XMFLOAT4X4 inverseViewProjection; // pixel shader で view ray を復元するための逆 VP。
    };

    // 通常の cubemap SRV。ResourceManager が所有する texture を共有する。
    std::shared_ptr<ITexture> m_cubeTexture;

    // DX12 用の fallback。cubemap DDS を 6 面 texture として展開したもの。
    std::array<std::shared_ptr<ITexture>, 6> m_faceTextures{};
    bool m_hasFaceTextures = false; // 6 面 texture がすべて作成済みか。

    // RHI リソース
    std::unique_ptr<IShader> m_vs; // skybox vertex shader。
    std::unique_ptr<IShader> m_ps; // skybox pixel shader。
    std::unique_ptr<IBuffer> m_cb; // Constants 用 constant buffer。
    std::unique_ptr<IPipelineState> m_pso; // skybox 用 PSO。

    // DX12 専用ディスクリプタヒープ（6面テクスチャ用）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dx12SrvHeap; // 旧 DX12 path 用 SRV heap。
    D3D12_GPU_DESCRIPTOR_HANDLE m_dx12SrvGpuBase = {}; // 旧 DX12 path 用 GPU handle 先頭。

    static std::unordered_map<std::string, std::unique_ptr<Skybox>> s_cache; // filename -> Skybox cache。
};
