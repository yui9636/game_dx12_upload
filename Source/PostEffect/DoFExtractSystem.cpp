#include "DoFExtractSystem.h"
#include "Component/PostEffectComponent.h"
#include "System/Query.h"

// PostEffectComponent から DoF 設定を取り出し、RenderContext の dofData に反映する。
void DoFExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    // PostEffectComponent を持つエンティティを走査するための Query を作成する。
    Query<PostEffectComponent> query(registry);

    // 見つかった PostEffectComponent の被写界深度パラメータを描画用データへコピーする。
    query.ForEach([&](PostEffectComponent& post) {
        // DoF を有効にするかどうかを設定する。
        rc.dofData.enable = post.enableDoF;

        // カメラから焦点面までの距離を設定する。
        rc.dofData.focusDistance = post.focusDistance;

        // 焦点が合っているとみなす範囲を設定する。
        rc.dofData.focusRange = post.focusRange;

        // ぼけの大きさを決める半径を設定する。
        rc.dofData.bokehRadius = post.bokehRadius;
        });
}
