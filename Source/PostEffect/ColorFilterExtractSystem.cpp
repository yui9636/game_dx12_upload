#include "ColorFilterExtractSystem.h"
#include "Component/PostEffectComponent.h"
#include "System/Query.h"

// PostEffectComponent からカラーフィルター設定を取り出し、RenderContext の colorFilterData に反映する。
void ColorFilterExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    // PostEffectComponent を持つエンティティを走査するための Query を作成する。
    Query<PostEffectComponent> query(registry);

    // 見つかった PostEffectComponent の色補正関連パラメータを描画用データへコピーする。
    query.ForEach([&](PostEffectComponent& post) {
        // 画面全体の明るさを調整する露光値を設定する。
        rc.colorFilterData.exposure = post.exposure;

        // 元の色とモノクロ表示をどれだけ混ぜるかを設定する。
        rc.colorFilterData.monoBlend = post.monoBlend;

        // 色相をどれだけ回転させるかを設定する。
        rc.colorFilterData.hueShift = post.hueShift;

        // 被弾演出などで使う画面フラッシュ量を設定する。
        rc.colorFilterData.flashAmount = post.flashAmount;

        // 画面端を暗くするビネット量を設定する。
        rc.colorFilterData.vignetteAmount = post.vignetteAmount;
        });
}
