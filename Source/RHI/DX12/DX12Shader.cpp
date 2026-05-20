// DX12Shader の RHI バックエンド実装をまとめます。
#include "DX12Shader.h"
#include <fstream>
#include <cassert>

DX12Shader::DX12Shader(ShaderType type, const std::string& fileName)
    : m_type(type)
{
    // 事前コンパイル済み .cso をそのまま読み、PSO 作成時の bytecode として保持する。
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        assert(false && "Failed to open shader file");
        return;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    m_byteCode.resize(fileSize);
    file.read(reinterpret_cast<char*>(m_byteCode.data()), fileSize);
}
