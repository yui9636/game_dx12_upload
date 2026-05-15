#pragma once
#include <unordered_map>
#include "Entity/Entity.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

class Registry;

// TerrainComponent の地形データを Jolt HeightFieldShape として物理ワールドに登録する。
class TerrainPhysicsSystem {
public:
    void Update(Registry& registry);
    void Clear(Registry& registry);

private:
    void RegisterBody(EntityID entity, struct TerrainAsset& asset);
    void RemoveBody(EntityID entity);

    std::unordered_map<EntityID, JPH::BodyID> m_bodyMap;
};
