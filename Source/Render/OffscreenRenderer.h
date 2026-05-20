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

    // thumbnail / preview 用の独立 command list と renderer を初期化する。
    bool Initialize();
    bool IsReady() const { return m_available; }

    // preview 描画用の color/depth 付き FrameBuffer を作成する。
    std::shared_ptr<FrameBuffer> CreateFrameBuffer(int w, int h,
        float clearR = 0.f, float clearG = 0.f, float clearB = 0.f, float clearA = 0.f);

    // 古い API 互換。実体は BeginJob() と同じ。
    void Begin();
    void BeginJob();  // 共有利用前に描画状態を初期化する。
    // 対象 FrameBuffer の color/depth を clear する。
    void Clear(FrameBuffer* fb, float r, float g, float b, float a);
    // 対象 FrameBuffer を render target として設定する。
    void SetRenderTarget(FrameBuffer* fb);
    // preview 描画用 viewport を設定する。
    void SetViewport(float w, float h);

    // preview scene 用の CbScene を更新する。
    void UploadScene(const DirectX::XMFLOAT4X4& viewProj,
                     const DirectX::XMFLOAT3& camPos,
                     const DirectX::XMFLOAT3& lightDir,
                      const DirectX::XMFLOAT3& lightColor,
                      float renderW, float renderH);
    // UploadScene 済みの scene buffer を command list に bind する。
    void BindScene();
    // preview 用 sampler を bind する。現状は backend 側 static sampler 依存。
    void BindSampler();

    ModelRenderer& GetModelRenderer() { return *m_renderer; }
    ICommandList* GetCommandList() { return m_commandList.get(); }

    // queue 済み model draw を実行し、color を shader resource 状態へ戻して submit する。
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
    std::unique_ptr<ModelRenderer>     m_renderer; // preview 専用の model renderer。
    std::unique_ptr<ICommandList>      m_commandList; // preview 描画で使う独立 command list。
    std::unique_ptr<DX12RootSignature> m_dx12RootSignature; // DX12 preview command list 用 root signature。
    std::unique_ptr<IBuffer>           m_localSceneBuffer; // preview scene 定数バッファ。
    bool m_available = false; // 初期化済みで描画可能か。

    // 非同期 GPU 完了を追跡する DX12 fence。
    void*    m_fencePtr = nullptr;   // ID3D12Fence* を void* として保持し、header の DX12 依存を抑える。
    void*    m_fenceEvent = nullptr; // fence completion 用 event handle。
    uint64_t m_fenceValue = 0;       // 最後に signal した fence value。
};
