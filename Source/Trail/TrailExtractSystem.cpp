#include "TrailExtractSystem.h"
#include "Component/TrailComponent.h"
#include "Component/ComponentSignature.h"
#include "Archetype/Archetype.h"
#include "Registry/Registry.h"
#include "RenderContext/RenderQueue.h"
#include "RenderContext/RenderContext.h"
#include "Type/TypeInfo.h"
#include <cmath>
#include <algorithm>

using namespace DirectX;

static XMFLOAT3 Sub3(const XMFLOAT3& a, const XMFLOAT3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

static XMFLOAT3 Norm3(const XMFLOAT3& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 0.0001f) return { 0.0f, 1.0f, 0.0f };
    float inv = 1.0f / len;
    return { v.x * inv, v.y * inv, v.z * inv };
}

static XMFLOAT3 Cross3(const XMFLOAT3& a, const XMFLOAT3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

static XMFLOAT4 Lerp4(const XMFLOAT4& a, const XMFLOAT4& b, float t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
             a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t };
}

void TrailExtractSystem::Extract(Registry& registry, RenderQueue& queue, const RenderContext& rc)
{
    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!archetype ||
            !SignatureMatches(archetype->GetSignature(), CreateSignature<TrailComponent>())) {
            continue;
        }

        auto* trailCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TrailComponent>());
        if (!trailCol) continue;

        auto* trails = static_cast<TrailComponent*>(trailCol->Get(0));

        for (size_t row = 0; row < archetype->GetEntityCount(); ++row)
        {
            auto& trail = trails[row];
            if (!trail.enabled || trail.points.size() < 2) continue;

            TrailPacket packet;
            const size_t pointCount = trail.points.size();
            const XMFLOAT3 camPos = rc.cameraPosition;

            for (size_t pi = 0; pi < pointCount; ++pi)
            {
                const auto& pt = trail.points[pi];
                float t = static_cast<float>(pi) / static_cast<float>(pointCount - 1);

                XMFLOAT3 tangent;
                if (pi + 1 < pointCount)
                    tangent = Norm3(Sub3(trail.points[pi + 1].position, pt.position));
                else
                    tangent = Norm3(Sub3(pt.position, trail.points[pi - 1].position));

                XMFLOAT3 viewDir = Norm3(Sub3(camPos, pt.position));
                XMFLOAT3 side = Norm3(Cross3(viewDir, tangent));

                float halfWidth = trail.width * 0.5f * (1.0f - t * 0.5f);
                XMFLOAT4 color = Lerp4(trail.colorStart, trail.colorEnd, t);

                float age = trail.totalTime - pt.timeStamp;
                float ageFade = 1.0f - (std::min)(age / (std::max)(trail.lifetime, 0.01f), 1.0f);
                color.w *= ageFade;

                TrailVertex v0, v1;
                v0.position = { pt.position.x - side.x * halfWidth, pt.position.y - side.y * halfWidth, pt.position.z - side.z * halfWidth };
                v0.color = color;
                v0.texcoord = { t, 0.0f };

                v1.position = { pt.position.x + side.x * halfWidth, pt.position.y + side.y * halfWidth, pt.position.z + side.z * halfWidth };
                v1.color = color;
                v1.texcoord = { t, 1.0f };

                packet.vertices.push_back(v0);
                packet.vertices.push_back(v1);
            }

            for (uint32_t vi = 0; vi + 3 < static_cast<uint32_t>(packet.vertices.size()); vi += 2)
            {
                packet.indices.push_back(vi);
                packet.indices.push_back(vi + 1);
                packet.indices.push_back(vi + 2);
                packet.indices.push_back(vi + 1);
                packet.indices.push_back(vi + 3);
                packet.indices.push_back(vi + 2);
            }

            if (!packet.indices.empty()) {
                queue.trailPackets.push_back(std::move(packet));
            }
        }
    }
}
