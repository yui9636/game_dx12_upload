#pragma once
#include "RHI/IShader.h"
#include <vector>
#include <string>
#include <cstdint>

// DX12 shader holds bytecode only (PSO compiles it)
class DX12Shader : public IShader {
public:
    DX12Shader(ShaderType type, const std::string& fileName);
    ~DX12Shader() override = default;

    ShaderType GetType() const override { return m_type; }

    const void* GetByteCode() const { return m_byteCode.data(); }
    size_t GetByteCodeSize() const { return m_byteCode.size(); }

private:
    ShaderType m_type;
    std::vector<uint8_t> m_byteCode;
};
