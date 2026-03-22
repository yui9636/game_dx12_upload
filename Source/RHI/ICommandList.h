#pragma once
#include <cstdint>
#include "ITexture.h"

// Forward declarations
struct ID3D11DeviceContext;
class ITexture;
class IBuffer;
class IShader;
class ISampler;
class IInputLayout;
class IDepthStencilState;
class IRasterizerState;
class IBlendState;
class IBind;
class IPipelineState;

enum class ShaderStage {
    Vertex,
    Pixel,
    Compute,
    All
};

enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
    LineList,
    PointList
};

enum class IndexFormat {
    Uint16,
    Uint32
};

struct RhiViewport {
    float topLeftX;
    float topLeftY;
    float width;
    float height;
    float minDepth;
    float maxDepth;

    RhiViewport() : topLeftX(0), topLeftY(0), width(0), height(0), minDepth(0.0f), maxDepth(1.0f) {}
    RhiViewport(float x, float y, float w, float h, float minD = 0.0f, float maxD = 1.0f)
        : topLeftX(x), topLeftY(y), width(w), height(h), minDepth(minD), maxDepth(maxD) {
    }
};


class ICommandList {
public:
    virtual ~ICommandList() = default;

    virtual void Draw(uint32_t vertexCount, uint32_t startVertexLocation) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) = 0;
    virtual void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) = 0;
    virtual void DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) = 0;
    virtual void ExecuteIndexedIndirect(IBuffer* argumentBuffer, uint32_t argumentOffsetBytes) = 0;
    virtual void Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) = 0;

    // �� UINT �� uint32_t �ɕύX
    virtual void PSSetTexture(uint32_t slot, ITexture* texture) = 0;
    virtual void PSSetTextures(uint32_t startSlot, uint32_t numTextures, ITexture* const* ppTextures) = 0;

    virtual void VSSetConstantBuffer(uint32_t slot, IBuffer* buffer) = 0;
    virtual void PSSetConstantBuffer(uint32_t slot, IBuffer* buffer) = 0;
    virtual void CSSetConstantBuffer(uint32_t slot, IBuffer* buffer) = 0;

    virtual void VSSetShader(IShader* shader) = 0;
    virtual void PSSetShader(IShader* shader) = 0;
    virtual void GSSetShader(IShader* shader) = 0; // ���[�e�B���e�B�ɂ���̂Œǉ�
    virtual void CSSetShader(IShader* shader) = 0;

    virtual void PSSetSampler(uint32_t slot, ISampler* sampler) = 0;
    virtual void PSSetSamplers(uint32_t startSlot, uint32_t numSamplers, ISampler* const* ppSamplers) = 0;

    virtual void SetViewport(const RhiViewport& viewport) = 0;

    void SetViewport(float x, float y, float w, float h, float minD = 0.0f, float maxD = 1.0f) {
        SetViewport(RhiViewport(x, y, w, h, minD, maxD));
    }

    // Input Assembler (IA)
    virtual void SetInputLayout(IInputLayout* layout) = 0;
    virtual void SetPrimitiveTopology(PrimitiveTopology topology) = 0;
    virtual void SetVertexBuffer(uint32_t slot, IBuffer* buffer, uint32_t stride, uint32_t offset = 0) = 0;
    virtual void SetIndexBuffer(IBuffer* buffer, IndexFormat format, uint32_t offset = 0) = 0;

    // Output Merger & Rasterizer (OM / RS)
    virtual void SetDepthStencilState(IDepthStencilState* state, uint32_t stencilRef = 0) = 0;
    virtual void SetRasterizerState(IRasterizerState* state) = 0;
    virtual void SetBlendState(IBlendState* state, const float blendFactor[4] = nullptr, uint32_t sampleMask = 0xFFFFFFFF) = 0;

    // RHI/ICommandList.h �� public ���ɒǉ�
    virtual void SetRenderTargets(uint32_t numRenderTargets, ITexture* const* renderTargets, ITexture* depthStencil) = 0;

    // �� �ǉ��F�P�ꃌ���_�[�^�[�Q�b�g�̐ݒ�i����܂ł̂��̂��ێ��A�܂��͐����j
    virtual void SetRenderTarget(ITexture* renderTarget, ITexture* depthStencil) = 0;

    // �� �ǉ��F�N���A����
    virtual void ClearColor(ITexture* renderTarget, const float color[4]) = 0;
    virtual void ClearDepthStencil(ITexture* depthStencil, float depth, uint8_t stencil) = 0;

    virtual void TransitionBarrier(ITexture* texture, ResourceState newState) = 0;

    virtual void SetBindGroup(ShaderStage stage, uint32_t index, IBind* bind) = 0;

    virtual void SetPipelineState(IPipelineState* pso) = 0;

    virtual void UpdateBuffer(IBuffer* buffer, const void* data, uint32_t size) = 0;

    virtual ID3D11DeviceContext* GetNativeContext() = 0;
};
