#include "DX11Shader.h"
#include "System/Misc.h" // HRTrace ‚È‚Ç‚Ì‚½‚ß
#include <vector>

DX11Shader::DX11Shader(ID3D11Device* device, ShaderType type, const std::string& fileName)
    : m_type(type)
{
    // —B•S—l‚Ì GpuResourceUtils ‚Ìì–@‚Åƒ[ƒh
    FILE* fp = nullptr;
    fopen_s(&fp, fileName.c_str(), "rb");
    _ASSERT_EXPR_A(fp, (std::string("Shader File not found: ") + fileName).c_str());

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    m_byteCode.resize(size);
    fread(m_byteCode.data(), size, 1, fp);
    fclose(fp);

    HRESULT hr = S_OK;
    switch (type) {
    case ShaderType::Vertex:
        hr = device->CreateVertexShader(m_byteCode.data(), m_byteCode.size(), nullptr,
            reinterpret_cast<ID3D11VertexShader**>(m_shader.GetAddressOf()));
        break;
    case ShaderType::Pixel:
        hr = device->CreatePixelShader(m_byteCode.data(), m_byteCode.size(), nullptr,
            reinterpret_cast<ID3D11PixelShader**>(m_shader.GetAddressOf()));
        break;
    case ShaderType::Compute:
        hr = device->CreateComputeShader(m_byteCode.data(), m_byteCode.size(), nullptr,
            reinterpret_cast<ID3D11ComputeShader**>(m_shader.GetAddressOf()));
        break;
    }
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}