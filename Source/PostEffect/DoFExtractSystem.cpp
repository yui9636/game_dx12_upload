#include "DoFExtractSystem.h"
#include "Component/PostEffectComponent.h"
#include "System/Query.h"

void DoFExtractSystem::Extract(Registry& registry, RenderContext& rc) {
    Query<PostEffectComponent> query(registry);
    query.ForEach([&](PostEffectComponent& post) {
        rc.dofData.enable = post.enableDoF; //
        rc.dofData.focusDistance = post.focusDistance; //
        rc.dofData.focusRange = post.focusRange; //
        rc.dofData.bokehRadius = post.bokehRadius; //
        });
}