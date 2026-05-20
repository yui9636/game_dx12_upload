// DecalSystem: gathers DecalComponent entities into RenderContext::decals each frame.
#pragma once

class Registry;
struct RenderContext;

namespace DecalSystem {
    // Collects every entity carrying a DecalComponent + TransformComponent and appends
    // a DecalInstance to rc.decals. Entities with no texture resolved are skipped.
    void ExtractDecals(Registry& registry, RenderContext& rc);
}
