#pragma once
#include "../IResourceFactory.h"
#include <d3d11.h>

class DX11ResourceFactory : public IResourceFactory {
public:
    DX11ResourceFactory(ID3D11Device* device) : m_device(device) {}
    ~DX11ResourceFactory() override = default;

    std::unique_ptr<ITexture> CreateTexture(const std::string& name, const TextureDesc& desc) override;
    std::unique_ptr<IShader> CreateShader(ShaderType type, const std::string& fileName) override;
    std::unique_ptr<IBuffer> CreateBuffer(uint32_t size, BufferType type, const void* initialData = nullptr) override;
    std::unique_ptr<IBuffer> CreateStructuredBuffer(uint32_t elementSize, uint32_t elementCount, const void* initialData = nullptr) override;
    std::unique_ptr<IInputLayout> CreateInputLayout(const InputLayoutDesc& desc, const IShader* vs) override;
    std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc) override;
    std::unique_ptr<ITexture> CreateTextureFromMemory(
        const DirectX::ScratchImage& image,
        const DirectX::TexMetadata& metadata) override;

private:
    ID3D11Device* m_device;
};
