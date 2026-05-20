#pragma once
#include "RenderPass/IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"
#include <memory>

class IShader;
class IPipelineState;
class IBuffer;
class IResourceFactory;

// DecalComponent の texture を GBuffer0 へ投影する pass。
// 不透明描画後・DeferredLighting 前に実行し、貼り付けた色が通常の lighting を受けるようにする。
class DeferredDecalPass : public IRenderPass {
public:
    DeferredDecalPass(IResourceFactory* factory);
    ~DeferredDecalPass() override;

    std::string GetName() const override { return "DeferredDecalPass"; }

    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    // デカール投影用 shader / PSO / constant buffer。
    std::unique_ptr<IShader> m_vs;
    std::unique_ptr<IShader> m_ps;
    std::unique_ptr<IPipelineState> m_pso;
    std::unique_ptr<IBuffer> m_cb;

    ResourceHandle m_hGBuffer0; // Albedo/Metallic。デカール色を書き込む。
    ResourceHandle m_hGBuffer1; // Normal/Roughness。角度フェード判定用に読む。
    ResourceHandle m_hGBuffer2; // WorldPos/Depth。投影座標復元用に読む。
};
