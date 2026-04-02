#pragma once

struct TimelineAsset;
class Gizmos;

namespace GizmoOverlay
{
    // Draw hitbox sphere gizmos for active items at the given frame
    // Called from EditorLayer::DrawSceneView() or similar
    void DrawActiveHitboxes(Gizmos* gizmo, const TimelineAsset& asset, int currentFrame,
        const float* nodeWorldTransforms, int nodeCount);
}
