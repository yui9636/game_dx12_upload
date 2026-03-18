#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "ShaderClass/Shader.h" // EffectShaderの基底クラス定義を含む

// 前方宣言
class Model;
struct RenderContext;

class EffectVariantShader : public EffectShader
{
public:
    EffectVariantShader(ID3D11Device* device);
    virtual ~EffectVariantShader() = default;

    // 描画開始 (VS, Layout, Scene定数バッファ, ステートの設定)
    void Begin(const RenderContext& rc) override;

    // メッシュ描画 (VB/IB設定, スケルトン更新, Drawコール)
    // ※ PSのセットとマテリアル定数バッファ(b1)の更新は呼び出し元(MeshEmitter)で行う前提
    void Draw(const RenderContext& rc, const ModelResource* modelResource) override;

    // 描画終了
    void End(const RenderContext& rc) override;

private:
    // --------------------------------------------------------
    // 定数バッファ構造体 (SlashEffectShader準拠 + Effect.hlsl対応)
    // --------------------------------------------------------

    // b0: シーン情報
    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4   lightDirection;
        DirectX::XMFLOAT4   lightColor;
        DirectX::XMFLOAT4   cameraPosition;
        DirectX::XMFLOAT4X4 lightViewProjection;
        DirectX::XMFLOAT4   shadowColor;
    };

    // b6: スケルトン行列
    struct CbSkeleton
    {
        DirectX::XMFLOAT4X4 boneTransforms[256];
    };

    // --------------------------------------------------------
    // リソース
    // --------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;

    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;    // b0
    Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer; // b6
};