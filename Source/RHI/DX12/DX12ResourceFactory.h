// DX12ResourceFactory は RHI の生成要求を DX12 実装クラスへ変換する。
#pragma once

#include "RHI/IResourceFactory.h"
#include "DX12Device.h"

class DX12ResourceFactory : public IResourceFactory {
public:
    // device は外部所有。生成する texture / buffer / shader が内部で参照する。
    explicit DX12ResourceFactory(DX12Device* device) : m_device(device) {}
    ~DX12ResourceFactory() override = default;

    // TextureDesc に従って render target / depth / SRV などの DX12Texture を作る。
    std::unique_ptr<ITexture> CreateTexture(const std::string& name, const TextureDesc& desc) override;

    // cso file を読み込み、DX12Shader として bytecode を保持する。
    std::unique_ptr<IShader> CreateShader(ShaderType type, const std::string& fileName) override;

    // RHI buffer 種別に応じて DX12Buffer を作る。
    std::unique_ptr<IBuffer> CreateBuffer(uint32_t size, BufferType type, const void* initialData = nullptr) override;
    // StructuredBuffer 用に elementSize * elementCount の buffer を作る。
    std::unique_ptr<IBuffer> CreateStructuredBuffer(uint32_t elementSize, uint32_t elementCount, const void* initialData = nullptr) override;

    // RHI input layout を DX12_INPUT_ELEMENT_DESC の集合へ変換する。
    std::unique_ptr<IInputLayout> CreateInputLayout(const InputLayoutDesc& desc, const IShader* vs) override;

    // PipelineStateDesc を保持する wrapper を作る。native PSO は cache 側で遅延生成される。
    std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc) override;

    // DirectXTex の ScratchImage から DEFAULT heap texture を upload し、SRV 付き DX12Texture を返す。
    std::unique_ptr<ITexture> CreateTextureFromMemory(
        const DirectX::ScratchImage& image,
        const DirectX::TexMetadata& metadata) override;

private:
    DX12Device* m_device; // resource 作成に使う DX12Device。非所有。
};
