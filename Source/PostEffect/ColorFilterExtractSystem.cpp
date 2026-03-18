#include "ColorFilterExtractSystem.h"
#include "Component/PostEffectComponent.h"
#include "System/Query.h"

void ColorFilterExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    Query<PostEffectComponent> query(registry);
    query.ForEach([&](PostEffectComponent& post) {
        rc.colorFilterData.exposure = post.exposure;
        rc.colorFilterData.monoBlend = post.monoBlend;
        rc.colorFilterData.hueShift = post.hueShift;
        rc.colorFilterData.flashAmount = post.flashAmount;
        rc.colorFilterData.vignetteAmount = post.vignetteAmount;
        });
}