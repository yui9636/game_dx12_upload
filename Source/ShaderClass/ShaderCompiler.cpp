#include "ShaderCompiler.h"
#include <iostream>
#include <vector>

HRESULT ShaderCompiler::CompilePixelShader(
    ID3D11Device* device,
    const std::wstring& hlslFilePath,
    int flags,
    ID3D11PixelShader** outShader)
{
    std::vector<D3D_SHADER_MACRO> macros;

    if (flags & ShaderFlag_Texture)  macros.push_back({ "USE_TEXTURE", "1" });
    if (flags & ShaderFlag_Dissolve) macros.push_back({ "USE_DISSOLVE", "1" });
    if (flags & ShaderFlag_Distort)  macros.push_back({ "USE_DISTORT", "1" });
    if (flags & ShaderFlag_Lighting) macros.push_back({ "USE_LIGHTING", "1" });

    if (flags & ShaderFlag_Mask)     macros.push_back({ "USE_MASK", "1" });
    if (flags & ShaderFlag_Fresnel)  macros.push_back({ "USE_FRESNEL", "1" });
    if (flags & ShaderFlag_Flipbook) macros.push_back({ "USE_FLIPBOOK", "1" });
    if (flags & ShaderFlag_GradientMap) macros.push_back({ "USE_GRADIENT_MAP", "1" });
    if (flags & ShaderFlag_ChromaticAberration) macros.push_back({ "USE_CHROMATIC_ABERRATION", "1" });
    if (flags & ShaderFlag_DissolveGlow) macros.push_back({ "USE_DISSOLVE_GLOW", "1" });
    if (flags & ShaderFlag_MatCap)    macros.push_back({ "USE_MATCAP", "1" });
    if(flags & ShaderFlag_NormalMap) macros.push_back({ "USE_NORMAL_MAP", "1" });
    if (flags & ShaderFlag_FlowMap) macros.push_back({ "USE_FLOW_MAP", "1" });
    if (flags & ShaderFlag_SideFade) macros.push_back({ "USE_SIDE_FADE", "1" });
    if (flags & ShaderFlag_AlphaFade) macros.push_back({ "USE_ALPHA_FADE", "1" });
    if (flags & ShaderFlag_SubTexture) macros.push_back({ "USE_SUB_TEXTURE", "1" });
    if (flags & ShaderFlag_Toon) macros.push_back({ "USE_TOON", "1" });



    macros.push_back({ nullptr, nullptr });

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        hlslFilePath.c_str(),
        macros.data(),
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_5_0",
        compileFlags,
        0,
        blob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return hr;
    }

    return device->CreatePixelShader(
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        nullptr,
        outShader
    );
}
