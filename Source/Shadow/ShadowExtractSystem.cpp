#include "ShadowExtractSystem.h"
#include "Component/ShadowSettingsComponent.h"
#include "System/Query.h"
// ShadowExtractSystem::Extract はこのモジュールの実行時処理を構成する補助処理を行う。

void ShadowExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    Query<ShadowSettingsComponent> settingsQuery(registry);

    settingsQuery.ForEach([&](ShadowSettingsComponent& settings) {
        if (settings.enableShadow) {
            rc.shadowColor = settings.shadowColor;
        }
        else {
        }
        });

}
