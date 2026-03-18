

// UIパラメータ (Slot: b0) -> Sprite3D::UIConstants
cbuffer UIParams : register(b0)
{
    float4 ColorCore; // 発光カラー (RGBA)
    float Progress; // 進行度 (0.0f ~ 1.0f)
    float3 padding; // パディング (12bytes)
};

// 行列データ (Slot: b1) -> Sprite3D::MatrixData
cbuffer MatrixBuffer : register(b1)
{
    matrix World; // ワールド変換行列
    matrix View; // ビュー変換行列
    matrix Projection; // プロジェクション変換行列
};

// --------------------------------------------------------
// 入出力構造体
// --------------------------------------------------------

// 頂点シェーダーへの入力
struct VS_IN
{
    float3 position : POSITION; // 3D座標
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};

// ピクセルシェーダーへの入力 (VSからの出力)
struct VS_OUT
{
    float4 position : SV_POSITION; // スクリーン座標
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};