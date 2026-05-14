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
    static Skybox* Get(IResourceFactory* factory, const std::string& filename);
    static void ClearCache() { s_cache.clear(); }

    Skybox(IResourceFactory* factory, const char* filename);
    ~Skybox();

    void Draw(const RenderContext& rc, const DirectX::XMFLOAT4X4& viewProjection);

private:
    struct Constants
    {
        DirectX::XMFLOAT4X4 inverseViewProjection;
    };

    // text 用色。ureCube (ResourceManager がキューブマップ SRV を自動作成)
    std::shared_ptr<ITexture> m_cubeTexture;

    // 6面テクスチャ（DX12 用: cubemap DDS から各面を展開）
    std::array<std::shared_ptr<ITexture>, 6> m_faceTextures{};
    bool m_hasFaceTextures = false;

    // RHI リソース
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_ps;
    std::unique_ptr<IBuffer> m_cb;
    std::unique_ptr<IPipelineState> m_pso;

    // DX12 専用ディスクリプタヒープ（6面テクスチャ用）
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dx12SrvHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_dx12SrvGpuBase = {};

    static std::unordered_map<std::string, std::unique_ptr<Skybox>> s_cache;
};
