#pragma once

class Registry;

// ============================================================================
// Converts TimelineAsset data into TimelineItemBuffer at runtime
// Bridges editor-authored timeline data to existing gameplay systems
// ============================================================================

class TimelineRuntimeSystem
{
public:
    static void Update(Registry& registry, float dt);
};
