struct PS_IN
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

float4 main(PS_IN pin) : SV_TARGET
{
    // Simple trail: vertex color with smooth alpha falloff
    float4 c = pin.color;
    // Soft edge fade on V axis (0..1 across width)
    float edgeFade = 1.0f - abs(pin.texcoord.y * 2.0f - 1.0f);
    edgeFade = edgeFade * edgeFade; // quadratic falloff
    c.a *= edgeFade;
    return c;
}
