struct VS_OUT_QUAD
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

// 頂点バッファ不要！ SV_VertexIDだけでフルスクリーンを描画する黒魔術
VS_OUT_QUAD main(uint id : SV_VertexID)
{
    VS_OUT_QUAD output;
    // id=0, 1, 2 から、画面を覆う巨大な三角形のUV座標と頂点座標を生成する
    output.uv = float2((id << 1) & 2, id & 2);
    output.pos = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}