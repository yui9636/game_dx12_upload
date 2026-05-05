#include "BloomExtractSystem.h"
#include "Component/PostEffectComponent.h"
#include "System/Query.h"

// PostEffectComponent から Bloom 設定を取り出し、RenderContext の bloomData に反映する。
void BloomExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    // PostEffectComponent を持つエンティティを走査するための Query を作成する。
    Query<PostEffectComponent> query(registry);

    // 見つかった PostEffectComponent の Bloom 関連パラメータを描画用データへコピーする。
    query.ForEach([&](PostEffectComponent& post) {
        // Bloom 抽出を開始する下限輝度を設定する。
        rc.bloomData.luminanceLowerEdge = post.luminanceLowerEdge;

        // Bloom 抽出で完全に反応する上限輝度を設定する。
        rc.bloomData.luminanceHigherEdge = post.luminanceHigherEdge;

        // Bloom の最終的な発光強度を設定する。
        rc.bloomData.bloomIntensity = post.bloomIntensity;

        // ぼかし処理で使うガウス分布の広がりを設定する。
        rc.bloomData.gaussianSigma = post.gaussianSigma;
        });
}
