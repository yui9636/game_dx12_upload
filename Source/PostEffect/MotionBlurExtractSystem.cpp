#include "MotionBlurExtractSystem.h"
#include "Component/PostEffectComponent.h"
#include "System/Query.h"

// PostEffectComponent からモーションブラー設定を取り出し、RenderContext の motionBlurData に反映する。
void MotionBlurExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    // PostEffectComponent を持つエンティティを走査するための Query を作成する。
    Query<PostEffectComponent> query(registry);

    // 見つかった PostEffectComponent のモーションブラー関連パラメータを描画用データへコピーする。
    query.ForEach([&](PostEffectComponent& post) {
        // モーションブラーの掛かり具合を設定する。
        rc.motionBlurData.intensity = post.motionBlurIntensity;

        // ブラー計算に使うサンプル数を float 形式で描画側へ渡す。
        rc.motionBlurData.samples = static_cast<float>(post.motionBlurSamples);
        });
}
