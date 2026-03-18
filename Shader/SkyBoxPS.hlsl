#include "SkyBox.hlsli"

Texture2D skyFacePX : register(t0);
Texture2D skyFaceNX : register(t1);
Texture2D skyFacePY : register(t2);
Texture2D skyFaceNY : register(t3);
Texture2D skyFacePZ : register(t4);
Texture2D skyFaceNZ : register(t5);
SamplerState linearClamp : register(s3);

struct PS_IN
{
    float4 svPosition : SV_POSITION;
    float3 rayDir : TEXCOORD0;
};

float3 SampleSkyFace(float3 dir)
{
    float3 a = abs(dir);
    float2 uv = float2(0.0f, 0.0f);
    float3 color = float3(0.0f, 0.0f, 0.0f);
    float majorAxis = 1.0f;

    // DirectX cubemap face UV convention:
    //   +X: sc=-z, tc=-y   -X: sc=+z, tc=-y
    //   +Y: sc=+x, tc=+z   -Y: sc=+x, tc=-z
    //   +Z: sc=+x, tc=-y   -Z: sc=-x, tc=-y
    if (a.x >= a.y && a.x >= a.z)
    {
        majorAxis = a.x;
        if (dir.x >= 0.0f) {
            uv = float2(-dir.z, -dir.y) / majorAxis;
            color = skyFacePX.SampleLevel(linearClamp, uv * 0.5f + 0.5f, 0).rgb;
        } else {
            uv = float2(dir.z, -dir.y) / majorAxis;
            color = skyFaceNX.SampleLevel(linearClamp, uv * 0.5f + 0.5f, 0).rgb;
        }
    }
    else if (a.y >= a.x && a.y >= a.z)
    {
        majorAxis = a.y;
        if (dir.y >= 0.0f) {
            uv = float2(dir.x, dir.z) / majorAxis;
            color = skyFacePY.SampleLevel(linearClamp, uv * 0.5f + 0.5f, 0).rgb;
        } else {
            uv = float2(dir.x, -dir.z) / majorAxis;
            color = skyFaceNY.SampleLevel(linearClamp, uv * 0.5f + 0.5f, 0).rgb;
        }
    }
    else
    {
        majorAxis = a.z;
        if (dir.z >= 0.0f) {
            uv = float2(dir.x, -dir.y) / majorAxis;
            color = skyFacePZ.SampleLevel(linearClamp, uv * 0.5f + 0.5f, 0).rgb;
        } else {
            uv = float2(-dir.x, -dir.y) / majorAxis;
            color = skyFaceNZ.SampleLevel(linearClamp, uv * 0.5f + 0.5f, 0).rgb;
        }
    }

    return color;
}

float4 main(PS_IN pin) : SV_TARGET
{
    float3 rayDir = normalize(pin.rayDir);
    float3 color = SampleSkyFace(rayDir);

    return float4(color, 1.0f);
}
