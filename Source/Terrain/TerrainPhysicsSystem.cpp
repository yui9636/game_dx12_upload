#include "TerrainPhysicsSystem.h"
#include "TerrainComponent.h"
#include "TerrainAsset.h"
#include "Physics/PhysicsManager.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "Component/TransformComponent.h"
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

void TerrainPhysicsSystem::Update(Registry& registry)
{
    Query<TerrainComponent> q(registry);
    q.ForEachWithEntity([&](EntityID entity, TerrainComponent& tc) {
        if (!tc.asset) return;

        bool bodyExists = m_bodyMap.count(entity) > 0 && !m_bodyMap[entity].IsInvalid();

        if (tc.needsRebuild && bodyExists) {
            RemoveBody(entity);
            bodyExists = false;
        }
        if (!bodyExists) {
            RegisterBody(entity, *tc.asset, registry.GetComponent<TransformComponent>(entity));
        }
    });
}

void TerrainPhysicsSystem::RegisterBody(EntityID entity, TerrainAsset& asset, const TransformComponent* transform)
{
    if (asset.heightData.empty()) return;

    auto& pm = PhysicsManager::Instance();
    if (!pm.IsInitialized()) return;
    JPH::BodyInterface& bi = pm.GetBodyInterface();

    const float cellSizeX = asset.worldSizeX / static_cast<float>(asset.resolution - 1);
    const float cellSizeZ = asset.worldSizeZ / static_cast<float>(asset.resolution - 1);
    const DirectX::XMFLOAT3 entityPosition = transform ? transform->worldPosition : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };

    JPH::HeightFieldShapeSettings settings(
        asset.heightData.data(),
        JPH::Vec3(
            entityPosition.x - asset.worldSizeX * 0.5f,
            entityPosition.y - asset.heightScale * 0.5f,
            entityPosition.z - asset.worldSizeZ * 0.5f),
        JPH::Vec3(cellSizeX, asset.heightScale, cellSizeZ),
        asset.resolution);

    auto shapeResult = settings.Create();
    if (shapeResult.HasError()) return;

    JPH::BodyCreationSettings bodySettings(
        shapeResult.Get(),
        JPH::RVec3::sZero(),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);
    bodySettings.mUserData = static_cast<uint64_t>(entity);

    JPH::BodyID bodyID = bi.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);
    m_bodyMap[entity] = bodyID;
}

void TerrainPhysicsSystem::RemoveBody(EntityID entity)
{
    auto it = m_bodyMap.find(entity);
    if (it == m_bodyMap.end() || it->second.IsInvalid()) return;

    auto& pm = PhysicsManager::Instance();
    JPH::BodyInterface& bi = pm.GetBodyInterface();
    bi.RemoveBody(it->second);
    bi.DestroyBody(it->second);
    m_bodyMap.erase(it);
}

void TerrainPhysicsSystem::Clear(Registry& registry)
{
    std::vector<EntityID> keys;
    keys.reserve(m_bodyMap.size());
    for (auto& kv : m_bodyMap) keys.push_back(kv.first);
    for (EntityID e : keys) RemoveBody(e);
}
