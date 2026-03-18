// ==================================================================================
// ParticleMesh.hlsl
// メッシュパーティクル用の共通定義
// ==================================================================================

// 既存のエフェクト定義をインクルード (VS_OUT, CbScene, MaterialConstantsなどを再利用)
#include "Effect.hlsl"

#define _COMPUTE_PARTICLE_DISABLE_CBSCENE_

// パーティクルデータの定義をインクルード
#include "compute_particle.hlsli"

// --------------------------------------------------------
// 入力レイアウト (VS_IN)
// C++側の EffectVariantShader のInputLayoutと完全に一致させる
// --------------------------------------------------------
struct VS_IN
{
    float3 pos : POSITION;
    float4 boneWeights : BONE_WEIGHTS; // パーティクルでは未使用だがレイアウト維持のため必要
    uint4 boneIndices : BONE_INDICES; // 同上
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 color : COLOR0;
};

// --------------------------------------------------------
// リソース定義
// --------------------------------------------------------
// パーティクルデータ (t0: StructuredBuffer)
// ※ t0は EffectPS側で "TexSlot0" として使われることがあるが、
//    VSとPSでステージが違うため競合しない（ただしバインド時に注意が必要）
StructuredBuffer<particle_data> g_Particles : register(t0);

// --------------------------------------------------------
// ヘルパー関数: クォータニオンによるベクトル回転
// --------------------------------------------------------
float3 RotateVector(float3 v, float4 q)
{
    float3 t = 2.0f * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}