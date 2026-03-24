#include "PhysicsSystem.h"
#include <System\Query.h>


using namespace DirectX;

void PhysicsSystem::Update(Registry& registry, float deltaTime) {
    auto& physicsMgr = PhysicsManager::Instance();
    
    physicsMgr.Update(deltaTime);

    JPH::BodyInterface& bodyInterface = physicsMgr.GetBodyInterface();

    Query<PhysicsComponent, TransformComponent> query(registry);

    query.ForEach([&](const PhysicsComponent& phys, TransformComponent& trans) {
        
        JPH::RVec3 joltPos = bodyInterface.GetPosition(phys.bodyID);
        JPH::Quat joltRot = bodyInterface.GetRotation(phys.bodyID);

        trans.localPosition = { (float)joltPos.GetX(), (float)joltPos.GetY(), (float)joltPos.GetZ() };
        trans.localRotation = { joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW() };

    });
}
