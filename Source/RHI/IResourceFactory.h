#pragma once
#include <memory>
#include <string>
#include "RenderGraph/FrameGraphTypes.h"
#include "ITexture.h"

namespace DirectX { class ScratchImage; struct TexMetadata; }

class IShader;
class IBuffer;
class IInputLayout;
class IPipelineState;
enum class ShaderType;
enum class BufferType;
struct PipelineStateDesc;

// ====================================================
// InputLayout 記述子 (API非依存)
// ====================================================
constexpr uint32_t kAppendAlignedElement = 0xFFFFFFFF;

struct InputLayoutElement {
    const char* semanticName;
    uint32_t semanticIndex;
    TextureFormat format;
    uint32_t inputSlot;
    uint32_t byteOffset;
    bool perInstance = false;
    uint32_t instanceDataStepRate = 0;
};

struct InputLayoutDesc {
    const InputLayoutElement* elements;
    uint32_t count;
};

// ====================================================
// RHI リソース生成ファクトリー (インターフェース)
// ====================================================
class IResourceFactory {
public:
    virtual ~IResourceFactory() = default;

    virtual std::unique_ptr<ITexture> CreateTexture(const std::string& name, const TextureDesc& desc) = 0;
    virtual std::unique_ptr<IShader> CreateShader(ShaderType type, const std::string& fileName) = 0;
    virtual std::unique_ptr<IBuffer> CreateBuffer(uint32_t size, BufferType type, const void* initialData = nullptr) = 0;
    virtual std::unique_ptr<IBuffer> CreateStructuredBuffer(uint32_t elementSize, uint32_t elementCount, const void* initialData = nullptr) = 0;
    virtual std::unique_ptr<IInputLayout> CreateInputLayout(const InputLayoutDesc& desc, const IShader* vs) = 0;
    virtual std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc) = 0;

    // テクスチャファイル読み込み（DirectXTex ScratchImage → API別テクスチャ生成）
    virtual std::unique_ptr<ITexture> CreateTextureFromMemory(
        const DirectX::ScratchImage& image,
        const DirectX::TexMetadata& metadata) = 0;
};
