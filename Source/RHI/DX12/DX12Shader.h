#pragma once
#include "RHI/IShader.h"
#include <vector>
#include <string>
#include <cstdint>

// DX12 shader は bytecode だけを保持し、PSO 側で compile する。
class DX12Shader : public IShader {
public:
    // fileName の cso を読み込み、shader stage と bytecode を保持する。
    DX12Shader(ShaderType type, const std::string& fileName);
    ~DX12Shader() override = default;

    ShaderType GetType() const override { return m_type; }

    const void* GetByteCode() const { return m_byteCode.data(); } // PSO compile に渡す bytecode 先頭。
    size_t GetByteCodeSize() const { return m_byteCode.size(); }  // bytecode の byte 数。

private:
    ShaderType m_type;              // vertex / pixel / compute などの shader stage。
    std::vector<uint8_t> m_byteCode; // 読み込んだ cso の生 bytecode。
};
