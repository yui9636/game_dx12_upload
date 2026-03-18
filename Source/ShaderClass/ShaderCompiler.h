#pragma once

#include <d3d11.h>
#include <string>
#include <vector>
#include <d3dcompiler.h> // シェーダーコンパイル用API
#include <wrl/client.h>

// ライブラリのリンク指定
#pragma comment(lib, "d3dcompiler.lib")

// --------------------------------------------------------
// 機能フラグ
// ビット演算で複数の機能を組み合わせます (例: Texture | Dissolve)
// --------------------------------------------------------
enum EffectShaderFlags
{
    ShaderFlag_None = 0,
    ShaderFlag_Texture = 1 << 0, // (1) テクスチャ (USE_TEXTURE)
    ShaderFlag_Dissolve = 1 << 1, // (2) 溶解 (USE_DISSOLVE)
    ShaderFlag_Distort = 1 << 2, // (4) 歪み (USE_DISTORT)
    ShaderFlag_Lighting = 1 << 3, // (8) ライティング (USE_LIGHTING)

    ShaderFlag_Mask = 1 << 4, // (16) USE_MASK
    ShaderFlag_Fresnel = 1 << 5, // (32) フレネル反射 (USE_FRESNEL)
    ShaderFlag_Flipbook = 1 << 6, // (64) フリップブックアニメーション (USE_FLIPBOOK)
    ShaderFlag_GradientMap = 1 << 7, // (128) グラデーションマップ (USE_GRADIENT_MAP)
    ShaderFlag_ChromaticAberration = 1 << 8, // (256) 色収差 (USE_CHROMATIC_ABERRATION)
    ShaderFlag_DissolveGlow = 1 << 9, // (512) 溶解グロー効果 (USE_DISSOLVE_GLOW)
    ShaderFlag_MatCap = 1 << 10, // (1024) マットキャップ (USE_MATCAP)
    ShaderFlag_NormalMap = 1 << 11, // (2048) 法線マップ (USE_NORMAL_MAP)
    ShaderFlag_FlowMap = 1 << 12,
    ShaderFlag_SideFade = 1 << 13,
    ShaderFlag_AlphaFade = 1 << 14,
    ShaderFlag_SubTexture = 1 << 15,
    ShaderFlag_Toon = 1 << 16,

    // 必要に応じてフラグを追加...
};

class ShaderCompiler
{
public:
    /// @brief HLSLファイルを指定された機能構成でコンパイルし、PixelShaderを生成する
    /// @param device D3D11デバイス
    /// @param hlslFilePath HLSLファイルのパス (例: L"Data/Shader/EffectPS.hlsl")
    /// @param flags 有効化したい機能フラグの組み合わせ (EffectShaderFlags)
    /// @param outShader 生成されたシェーダーを受け取るポインタ
    static HRESULT CompilePixelShader(
        ID3D11Device* device,
        const std::wstring& hlslFilePath,
        int flags,
        ID3D11PixelShader** outShader
    );
};