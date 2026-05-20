#include "DecalSystem.h"

#include "System/Query.h"
#include "System/ResourceManager.h"
#include "Component/DecalComponent.h"
#include "Component/TransformComponent.h"
#include "RenderContext/RenderContext.h"
#include "Registry/Registry.h"
#include "Console/Logger.h"

using namespace DirectX;

void DecalSystem::ExtractDecals(Registry& registry, RenderContext& rc)
{
    Query<DecalComponent, TransformComponent> query(registry);

    static int s_frame = 0;
    const bool logThisFrame = (s_frame++ % 120) == 0;
    int seen = 0;
    query.ForEach([&](const DecalComponent& decal, const TransformComponent& trans) {
        ++seen;
        if (decal.texturePath.empty()) {
            if (logThisFrame) LOG_INFO("[DecalSystem] decal entity has empty texturePath");
            return;
        }
        auto texture = ResourceManager::Instance().GetTexture(decal.texturePath);
        if (!texture) {
            if (logThisFrame) LOG_WARN("[DecalSystem] texture not found: %s", decal.texturePath.c_str());
            return;
        }

        // Base transform of the decal box. Normally the decal entity's own world
        // matrix. When followTarget is set, the box tracks that entity's world position
        // (decal's own Transform position becomes a local offset) while keeping the
        // decal's own rotation and scale.
        XMMATRIX world;
        if (decal.followTarget != 0 && decal.followTarget != Entity::NULL_ID) {
            if (auto* targetTransform = registry.GetComponent<TransformComponent>(decal.followTarget)) {
                const XMVECTOR followPos =
                    XMVectorAdd(XMLoadFloat3(&targetTransform->worldPosition),
                                XMLoadFloat3(&trans.localPosition));
                world = XMMatrixAffineTransformation(
                    XMLoadFloat3(&trans.localScale),
                    XMVectorZero(),
                    XMLoadFloat4(&trans.localRotation),
                    followPos);
            } else {
                world = XMLoadFloat4x4(&trans.worldMatrix);
            }
        } else {
            world = XMLoadFloat4x4(&trans.worldMatrix);
        }

        // Projection box = unit cube scaled by 'size', then oriented/positioned by the
        // base transform above.
        const XMMATRIX boxScale = XMMatrixScaling(
            (std::max)(decal.size.x, 0.0001f),
            (std::max)(decal.size.y, 0.0001f),
            (std::max)(decal.size.z, 0.0001f));
        const XMMATRIX boxWorld = boxScale * world;

        XMVECTOR det{};
        const XMMATRIX invBoxWorld = XMMatrixInverse(&det, boxWorld);
        if (XMVectorGetX(det) == 0.0f) {
            return; // Degenerate box (zero scale axis); skip.
        }

        DecalInstance inst{};
        XMStoreFloat4x4(&inst.worldMatrix, boxWorld);
        XMStoreFloat4x4(&inst.invWorldMatrix, invBoxWorld);

        // Projection axis = box local +Z expressed in world space.
        const XMVECTOR axis = XMVector3Normalize(
            XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), world));
        XMStoreFloat3(&inst.projectionAxis, axis);

        inst.texture = texture.get();
        inst.tintOpacity = { decal.tint.x, decal.tint.y, decal.tint.z,
                             (std::max)(0.0f, (std::min)(decal.opacity, 1.0f)) };
        inst.angleFade = (std::max)(0.0f, (std::min)(decal.angleFade, 0.99f));

        rc.decals.push_back(inst);
        if (logThisFrame) {
            LOG_INFO("[DecalSystem] decal OK tex='%s' boxCenter=(%.2f,%.2f,%.2f) size=(%.2f,%.2f,%.2f)",
                decal.texturePath.c_str(),
                trans.worldMatrix._41, trans.worldMatrix._42, trans.worldMatrix._43,
                decal.size.x, decal.size.y, decal.size.z);
        }
    });

    if (logThisFrame && (seen > 0 || !rc.decals.empty())) {
        LOG_INFO("[DecalSystem] entities with DecalComponent=%d, extracted=%zu", seen, rc.decals.size());
    }
}
