#include "ShadowExtractSystem.h"
#include "Component/ShadowSettingsComponent.h"
#include "System/Query.h"

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
