#pragma once
#include "RHI/ICommandList.h"
#include <wrl.h>

struct ID3D11DeviceContext;
class ITexture;
class IBuffer;
class IShader; 
class ISampler;
class IPipelineState;

class DX11CommandList : public ICommandList {
public:
    DX11CommandList(ID3D11DeviceContext* dc);
    ~DX11CommandList() override;

    // �`��E�o�b�t�@�֘A (�����ς�)
    void Draw(uint32_t vertexCount, uint32_t startVertexLocation) override;
    void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) override;
    void VSSetConstantBuffer(uint32_t slot, IBuffer* buffer) override;
    void PSSetConstantBuffer(uint32_t slot, IBuffer* buffer) override;
    void CSSetConstantBuffer(uint32_t slot, IBuffer* buffer) override;

    // �e�N�X�`���֘A (�����ς�)
    void PSSetTexture(uint32_t slot, ITexture* texture) override;
    void PSSetTextures(uint32_t startSlot, uint32_t numTextures, ITexture* const* ppTextures) override;

    // =========================================================
    // �� �ǉ��F�V�F�[�_�[�̃o�C���h
    // =========================================================
    void VSSetShader(IShader* shader) override;
    void PSSetShader(IShader* shader) override;
    void GSSetShader(IShader* shader) override; // �W�I���g���V�F�[�_�[
    void CSSetShader(IShader* shader) override; // �R���s���[�g�V�F�[�_�[

    void PSSetSampler(uint32_t slot, ISampler* sampler) override;
    void PSSetSamplers(uint32_t startSlot, uint32_t numSamplers, ISampler* const* ppSamplers) override;

    void SetViewport(const RhiViewport& viewport) override;

    void SetInputLayout(IInputLayout* layout) override;
    void SetPrimitiveTopology(PrimitiveTopology topology) override;
    void SetVertexBuffer(uint32_t slot, IBuffer* buffer, uint32_t stride, uint32_t offset = 0) override;
    void SetIndexBuffer(IBuffer* buffer, IndexFormat format, uint32_t offset = 0) override;

    void SetDepthStencilState(IDepthStencilState* state, uint32_t stencilRef = 0) override;
    void SetRasterizerState(IRasterizerState* state) override;
    void SetBlendState(IBlendState* state, const float blendFactor[4] = nullptr, uint32_t sampleMask = 0xFFFFFFFF) override;

    void UpdateBuffer(IBuffer* buffer, const void* data, uint32_t size) override;

    // RHI/DX11/DX11CommandList.h
    void SetRenderTargets(uint32_t numRenderTargets, ITexture* const* renderTargets, ITexture* depthStencil) override;
    void SetRenderTarget(ITexture* renderTarget, ITexture* depthStencil)override;
    void ClearColor(ITexture* renderTarget, const float color[4]) override;
    void ClearDepthStencil(ITexture* depthStencil, float depth, uint8_t stencil) override;

    void TransitionBarrier(ITexture* texture, ResourceState newState) override;

    void SetBindGroup(ShaderStage stage, uint32_t index, IBind* bind) override;

    void SetPipelineState(IPipelineState* pso) override;

    ID3D11DeviceContext* GetNativeContext() override;

private:
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_dc;
};