struct PS_IN
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

float4 main(PS_IN pin) : SV_TARGET
{
    // シンプルな軌跡: 頂点色に滑らかな alpha 減衰を掛ける。
    float4 c = pin.color;
    // V 軸方向（幅 0..1）で縁を柔らかくフェードする。
    float edgeFade = 1.0f - abs(pin.texcoord.y * 2.0f - 1.0f);
    edgeFade = edgeFade * edgeFade; // 二次減衰
    c.a *= edgeFade;
    return c;
}
