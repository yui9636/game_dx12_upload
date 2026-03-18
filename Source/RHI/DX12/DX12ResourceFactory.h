#pragma once
#include "RHI/IResourceFactory.h"
#include "DX12Device.h"

class DX12ResourceFactory : public IResourceFactory {
public:
    DX12ResourceFactory(DX12Device* device) : m_device(device) {}
    ~DX12ResourceFactory() override = default;

    std::unique_ptr<ITexture> CreateTexture(const std::string& name, const TextureDesc& desc) override;
    std::unique_ptr<IShader> CreateShader(ShaderType type, const std::string& fileName) override;
    std::unique_ptr<IBuffer> CreateBuffer(uint32_t size, BufferType type, const void* initialData = nullptr) override;
    std::unique_ptr<IInputLayout> CreateInputLayout(const InputLayoutDesc& desc, const IShader* vs) override;
    std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc) override;
    std::unique_ptr<ITexture> CreateTextureFromMemory(
        const DirectX::ScratchImage& image,
        const DirectX::TexMetadata& metadata) override;

private:
    DX12Device* m_device;
};
