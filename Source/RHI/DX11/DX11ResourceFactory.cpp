#include "DX11ResourceFactory.h"
#include "DX11Texture.h"
#include "DX11Shader.h"
#include "DX11Buffer.h"
#include "DX11State.h"
#include "RHI/IPipelineState.h"
#include "RHI/DX11/DX11PipelineState.h"
#include <stdexcept>
#include <DirectXTex.h>
#include <wrl/client.h>

std::unique_ptr<ITexture> DX11ResourceFactory::CreateTexture(const std::string& name, const TextureDesc& desc) {
    if (!m_device) return nullptr;
    return std::make_unique<DX11Texture>(
        m_device,
        desc.width,
        desc.height,
        desc.format,
        desc.bindFlags
    );
}

std::unique_ptr<IShader> DX11ResourceFactory::CreateShader(ShaderType type, const std::string& fileName) {
    if (!m_device) return nullptr;
    return std::make_unique<DX11Shader>(m_device, type, fileName);
}

std::unique_ptr<IBuffer> DX11ResourceFactory::CreateBuffer(uint32_t size, BufferType type, const void* initialData) {
    if (!m_device) return nullptr;
    return std::make_unique<DX11Buffer>(m_device, size, type, initialData);
}

std::unique_ptr<IBuffer> DX11ResourceFactory::CreateStructuredBuffer(uint32_t elementSize, uint32_t elementCount, const void* initialData) {
    if (!m_device || elementSize == 0 || elementCount == 0) return nullptr;
    return std::make_unique<DX11Buffer>(m_device, elementSize * elementCount, BufferType::Structured, initialData, elementSize);
}

static DXGI_FORMAT ToDXGIFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::R32G32B32A32_UINT:  return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::R32G32B32_FLOAT:    return DXGI_FORMAT_R32G32B32_FLOAT;
    case TextureFormat::R32G32_FLOAT:       return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R16G16_FLOAT:       return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::RGBA8_UNORM:        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R8_UNORM:           return DXGI_FORMAT_R8_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

std::unique_ptr<IInputLayout> DX11ResourceFactory::CreateInputLayout(const InputLayoutDesc& desc, const IShader* vs) {
    if (!m_device || !vs) return nullptr;

    auto* dx11VS = static_cast<const DX11Shader*>(vs);

    std::vector<D3D11_INPUT_ELEMENT_DESC> elements(desc.count);
    for (uint32_t i = 0; i < desc.count; ++i) {
        elements[i].SemanticName = desc.elements[i].semanticName;
        elements[i].SemanticIndex = desc.elements[i].semanticIndex;
        elements[i].Format = ToDXGIFormat(desc.elements[i].format);
        elements[i].InputSlot = desc.elements[i].inputSlot;
        elements[i].AlignedByteOffset = desc.elements[i].byteOffset;
        elements[i].InputSlotClass = desc.elements[i].perInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
        elements[i].InstanceDataStepRate = desc.elements[i].perInstance ? desc.elements[i].instanceDataStepRate : 0;
    }

    ID3D11InputLayout* layout = nullptr;
    HRESULT hr = m_device->CreateInputLayout(
        elements.data(), (UINT)elements.size(),
        dx11VS->GetByteCode(), dx11VS->GetByteCodeSize(),
        &layout
    );
    if (FAILED(hr) || !layout) return nullptr;

    return std::make_unique<DX11InputLayout>(layout);
}

std::unique_ptr<IPipelineState> DX11ResourceFactory::CreatePipelineState(const PipelineStateDesc& desc) {
    return std::make_unique<DX11PipelineState>(desc);
}

std::unique_ptr<ITexture> DX11ResourceFactory::CreateTextureFromMemory(
    const DirectX::ScratchImage& image,
    const DirectX::TexMetadata& metadata)
{
    if (!m_device) return nullptr;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = DirectX::CreateShaderResourceView(
        m_device, image.GetImages(), image.GetImageCount(), metadata, &srv);
    if (FAILED(hr)) return nullptr;
    return std::make_unique<DX11Texture>(srv.Get());
}
