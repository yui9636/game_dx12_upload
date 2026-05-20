// EffectMeshPass のレンダーパス宣言をまとめます。
#pragma once

#include <memory>
#include "IRenderPass.h"
#include "RenderGraph/FrameGraphTypes.h"

class EffectMeshShader;

// effect system が生成した mesh packet を DX12 専用 shader で SceneColor へ重ねる pass。
class EffectMeshPass : public IRenderPass
{
public:
    EffectMeshPass();
    ~EffectMeshPass() override;

    std::string GetName() const override { return "EffectMeshPass"; }
    void Setup(FrameGraphBuilder& builder, const RenderContext& rc) override;
    void Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) override;

private:
    ResourceHandle m_hSceneColor; // エフェクト mesh の描画先。
    ResourceHandle m_hDepth;      // depth test 用。
    // Effect mesh 専用 shader。初回 Execute 時に遅延作成する。
    std::unique_ptr<EffectMeshShader> m_shader;
};
