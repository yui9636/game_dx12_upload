#include "PostProcessPass.h"
#include "../Graphics.h"
#include "../PostEffect.h"
#include "RenderGraph/FrameGraphResources.h"

void PostProcessPass::Setup(FrameGraphBuilder& builder)
{
    // ====================================================
    // ★ 1. Blackboard（掲示板）から必要なチケットを探す
    // ※ ここで探す名前は、後で RenderPipeline 側で登録する名前と一致させます
    // ====================================================
    m_hSceneColor = builder.GetHandle("SceneColor");
    m_hDepth = builder.GetHandle("GBufferDepth");
    m_hVelocity = builder.GetHandle("GBuffer3"); // Velocity用の名前

    // ====================================================
    // ★ 2. グラフに「読み込み（ShaderResource）として使う」と宣言する
    // これにより、RenderGraph が自動でバリアを張ってくれます！
    // ====================================================
    if (m_hSceneColor.IsValid()) builder.Read(m_hSceneColor);
    if (m_hDepth.IsValid())      builder.Read(m_hDepth);
    if (m_hVelocity.IsValid())   builder.Read(m_hVelocity);
}

void PostProcessPass::Execute(FrameGraphResources& resources, const RenderQueue& queue, RenderContext& rc) {
    Graphics& g = Graphics::Instance();
    auto postEffect = g.GetPostEffect();
    if (!postEffect) return;

    // ====================================================
    // ★ 3. グラフの受付（Resources）にチケットを渡し、物理テクスチャをもらう
    // ====================================================
    ITexture* srcTex = resources.GetTexture(m_hSceneColor);
    ITexture* depthTex = resources.GetTexture(m_hDepth);
    ITexture* velTex = resources.GetTexture(m_hVelocity);

    // ====================================================
    // ★ 4. 出力先（Display）は、ImGui等が絡むためまだ古いシステムから直接もらう
    // ====================================================
    FrameBuffer* displayFB = g.GetFrameBuffer(FrameBufferId::Display);
    ITexture* dstTex = displayFB ? displayFB->GetColorTexture(0) : nullptr;

    if (!srcTex || !dstTex) return;

    // ====================================================
    // ★ 5. PostEffect にすべてを託す！
    // ====================================================
    postEffect->Process(rc, srcTex, dstTex, depthTex, velTex);
}