#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <string>

class CurlNoiseGenerator
{
public:
    struct Config
    {
        int width = 32;        // テクスチャサイズ (例: 32x32x32)
        int height = 32;
        int depth = 32;
        float frequency = 0.1f; // ノイズの細かさ
        int seed = 1337;
    };

    // 3Dテクスチャを作成してSRVを返す
    static HRESULT CreateCurlNoiseTexture(
        ID3D11Device* device,
        const Config& config,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV
    );
};