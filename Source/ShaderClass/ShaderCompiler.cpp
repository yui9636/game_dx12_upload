#include "ShaderCompiler.h"
#include <iostream>
#include <vector>

HRESULT ShaderCompiler::CompilePixelShader(
    ID3D11Device* device,
    const std::wstring& hlslFilePath,
    int flags,
    ID3D11PixelShader** outShader)
{
    // 1. マクロ定義 (D3D_SHADER_MACRO) の配列を動的に組み立てる
    std::vector<D3D_SHADER_MACRO> macros;

    // フラグが立っていれば、HLSL側に #define を送る
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



    // 【重要】配列の最後は必ず NULL で閉じるルールがある
    macros.push_back({ nullptr, nullptr });

    // 2. コンパイル実行
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // デバッグ時は最適化を切り、デバッグ情報を付加する
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        hlslFilePath.c_str(),       // HLSLファイルパス
        macros.data(),              // ★作成したマクロ定義を渡す
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",                     // エントリーポイント関数名
        "ps_5_0",                   // シェーダーモデル (SM5.0)
        compileFlags,
        0,
        blob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            // コンパイルエラーの内容をデバッグ出力に表示
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return hr;
    }

    // 3. シェーダーオブジェクトの生成
    return device->CreatePixelShader(
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        nullptr,
        outShader
    );
}