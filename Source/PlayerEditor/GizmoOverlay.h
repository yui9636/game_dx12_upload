#pragma once

struct TimelineAsset;
class Gizmos;

namespace GizmoOverlay
{
    // 指定フレームで有効な hitbox アイテムを球ギズモとして描画する。
    // EditorLayer::DrawSceneView() などの SceneView 描画から呼び出される。
    void DrawActiveHitboxes(Gizmos* gizmo, const TimelineAsset& asset, int currentFrame,
        const float* nodeWorldTransforms, int nodeCount);
}
