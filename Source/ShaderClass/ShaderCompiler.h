#pragma once

#include <d3d11.h>
#include <string>
#include <vector>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "d3dcompiler.lib")

// --------------------------------------------------------
// --------------------------------------------------------
enum EffectShaderFlags
{
    ShaderFlag_None = 0,
    ShaderFlag_Texture = 1 << 0,
    ShaderFlag_Dissolve = 1 << 1,
    ShaderFlag_Distort = 1 << 2,
    ShaderFlag_Lighting = 1 << 3,

    ShaderFlag_Mask = 1 << 4, // (16) USE_MASK
    ShaderFlag_Fresnel = 1 << 5,
    ShaderFlag_Flipbook = 1 << 6,
    ShaderFlag_GradientMap = 1 << 7,
    ShaderFlag_ChromaticAberration = 1 << 8,
    ShaderFlag_DissolveGlow = 1 << 9,
    ShaderFlag_MatCap = 1 << 10,
    ShaderFlag_NormalMap = 1 << 11,
    ShaderFlag_FlowMap = 1 << 12,
    ShaderFlag_SideFade = 1 << 13,
    ShaderFlag_AlphaFade = 1 << 14,
    ShaderFlag_SubTexture = 1 << 15,
    ShaderFlag_Toon = 1 << 16,

};

class ShaderCompiler
{
public:
    static HRESULT CompilePixelShader(
        ID3D11Device* device,
        const std::wstring& hlslFilePath,
        int flags,
        ID3D11PixelShader** outShader
    );
};
