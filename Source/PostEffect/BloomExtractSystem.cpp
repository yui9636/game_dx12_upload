#include "BloomExtractSystem.h"
#include "Component/PostEffectComponent.h"
#include "System/Query.h"

void BloomExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    Query<PostEffectComponent> query(registry);
    query.ForEach([&](PostEffectComponent& post) {
        rc.bloomData.luminanceLowerEdge = post.luminanceLowerEdge;
        rc.bloomData.luminanceHigherEdge = post.luminanceHigherEdge;
        rc.bloomData.bloomIntensity = post.bloomIntensity;
        rc.bloomData.gaussianSigma = post.gaussianSigma;
        });
}