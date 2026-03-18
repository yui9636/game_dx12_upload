#include "MotionBlurExtractSystem.h"
#include "Component/PostEffectComponent.h"
#include "System/Query.h"

void MotionBlurExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    Query<PostEffectComponent> query(registry);
    query.ForEach([&](PostEffectComponent& post) {
        rc.motionBlurData.intensity = post.motionBlurIntensity;
        rc.motionBlurData.samples = static_cast<float>(post.motionBlurSamples);
        });
}