#include "GizmoOverlay.h"
#include "TimelineAsset.h"
#include "Gizmos.h"
#include <DirectXMath.h>

using namespace DirectX;

void GizmoOverlay::DrawActiveHitboxes(Gizmos* gizmo, const TimelineAsset& asset, int currentFrame,
    const float* nodeWorldTransforms, int nodeCount)
{
    if (!gizmo || !nodeWorldTransforms) return;

    for (auto& track : asset.tracks) {
        if (track.type != TimelineTrackType::Hitbox || track.muted) continue;

        for (auto& item : track.items) {
            if (currentFrame < item.startFrame || currentFrame > item.endFrame) continue;

            auto& hb = item.hitbox;
            if (hb.nodeIndex < 0 || hb.nodeIndex >= nodeCount) continue;

            // Get node world transform (4x4 matrix stored row-major, 16 floats per node)
            const float* m = nodeWorldTransforms + hb.nodeIndex * 16;
            XMFLOAT4X4 nodeMat;
            memcpy(&nodeMat, m, sizeof(XMFLOAT4X4));

            XMMATRIX nodeWorld = XMLoadFloat4x4(&nodeMat);
            XMVECTOR offset = XMVectorSet(hb.offsetLocal.x, hb.offsetLocal.y, hb.offsetLocal.z, 1.0f);
            XMVECTOR worldPos = XMVector3Transform(offset, nodeWorld);

            XMFLOAT3 center;
            XMStoreFloat3(&center, worldPos);

            // Decode RGBA color
            float r = ((hb.rgba >> 24) & 0xFF) / 255.0f;
            float g = ((hb.rgba >> 16) & 0xFF) / 255.0f;
            float b = ((hb.rgba >> 8) & 0xFF) / 255.0f;
            float a = (hb.rgba & 0xFF) / 255.0f;

            gizmo->DrawSphere(center, hb.radius, { r, g, b, a });
        }
    }
}
