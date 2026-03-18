#pragma once
#include "../IShader.h"
#include <d3d11.h>
#include <wrl.h>
#include <string>
#include <vector>

class DX11Shader : public IShader {
public:
    // コンストラクタでコンパイル済みシェーダー(.cso)をロードする
    DX11Shader(ID3D11Device* device, ShaderType type, const std::string& fileName);
    ~DX11Shader() override = default;

    ShaderType GetType() const override { return m_type; }

    IUnknown* GetNative() const { return m_shader.Get(); }

    // 頂点シェーダーの場合のみ、InputLayout生成のためにバイトコードが必要になる
    const void* GetByteCode() const { return m_byteCode.data(); }
    size_t GetByteCodeSize() const { return m_byteCode.size(); }

private:
    ShaderType m_type;
    Microsoft::WRL::ComPtr<IUnknown> m_shader; // VS, PS 等を共通で持つための ComPtr
    std::vector<uint8_t> m_byteCode;           // ロードしたバイナリを保持
};