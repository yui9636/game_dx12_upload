#include "ShadowExtractSystem.h"
#include "Component/ShadowSettingsComponent.h"
#include "System/Query.h"

void ShadowExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    Query<ShadowSettingsComponent> settingsQuery(registry);

    settingsQuery.ForEach([&](ShadowSettingsComponent& settings) {
        // 影が有効な場合のみ、影の色をコンテキストに書き込む
        if (settings.enableShadow) {
            rc.shadowColor = settings.shadowColor;
        }
        else {
            // 影を無効にしたい場合は、ここで処理を分岐させます
            // (例えば、影の色を環境光と同じにして見えなくする等)
        }
        });

    // ★重要: 今までここにあった「カスケード更新」や「メッシュの描画ループ」は
    // すべて ShadowPass.cpp の方へ引っ越し済みなため、完全に削除されました！
}