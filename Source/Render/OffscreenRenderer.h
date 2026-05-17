#pragma once
#include <memory>
#include <cstdint>
#include <DirectXMath.h>
#include "Model/ModelRenderer.h"

class ICommandList;
class IBuffer;
class ITexture;
class FrameBuffer;
class DX12RootSignature;

class OffscreenRenderer {
public:
    OffscreenRenderer();
    ~OffscreenRenderer();

    bool Initialize();
    bool IsReady() const { return m_available; }

    std::shared_ptr<FrameBuffer> CreateFrameBuffer(int w, int h,
        float clearR = 0.f, float clearG = 0.f, float clearB = 0.f, float clearA = 0.f);

    void Begin();
    void BeginJob();  // 共有利用前に描画状態を初期化する。
    void Clear(FrameBuffer* fb, float r, float g, float b, float a);
    void SetRenderTarget(FrameBuffer* fb);
    void SetViewport(float w, float h);

    void UploadScene(const DirectX::XMFLOAT4X4& viewProj,
                     const DirectX::XMFLOAT3& camPos,
                     const DirectX::XMFLOAT3& lightDir,
                     const DirectX::XMFLOAT3& lightColor,
                     float renderW, float renderH);
    void BindScene();
    void BindSampler();

    ModelRenderer& GetModelRenderer() { return *m_renderer; }
    ICommandList* GetCommandList() { return m_commandList.get(); }

    void Submit(FrameBuffer* fb);

    // FrameBuffer ラッパーを使わず、外部テクスチャへ直接描画する。
    void ClearExternalRT(ITexture* color, ITexture* depth,
                         float r, float g, float b, float a);
    void ClearExternalDepth(ITexture* depth);
    void SetExternalRenderTarget(ITexture* color, ITexture* depth);
    void RenderQueuedDirect(ITexture* color, ITexture* depth);
    void FinishDirect(ITexture* color);
    void SubmitDirect(ITexture* color);

    bool IsGpuIdle() const;
    uint64_t GetCurrentFenceValue() const { return m_fenceValue; }
    uint64_t GetCompletedFenceValue() const;

private:
    std::unique_ptr<ModelRenderer>     m_renderer;
    std::unique_ptr<ICommandList>      m_commandList;
    std::unique_ptr<DX12RootSignature> m_dx12RootSignature;
    std::unique_ptr<IBuffer>           m_localSceneBuffer;
    bool m_available = false;

    // 非同期 GPU 完了を追跡する DX12 fence。
    void*    m_fencePtr = nullptr;
    void*    m_fenceEvent = nullptr;
    uint64_t m_fenceValue = 0;
};
