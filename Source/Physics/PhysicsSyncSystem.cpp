#include "PhysicsSyncSystem.h"
#include "Component/TransformComponent.h"
#include "Component/PhysicsComponent.h"
#include "Physics/PhysicsManager.h"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <System\Query.h>

using namespace JPH;

void PhysicsSyncSystem::Update(Registry& registry, bool isSimulation) {
    auto& physicsMgr = PhysicsManager::Instance();
    BodyInterface& bodyInterface = physicsMgr.GetBodyInterface();

    Query<TransformComponent, PhysicsComponent> query(registry);

    query.ForEachWithEntity([&](EntityID entity, TransformComponent& trans, PhysicsComponent& phys) {
        if (phys.bodyID.IsInvalid()) return;

        if (!isSimulation) {
            if (trans.isDirty) {
                RVec3 newPos(trans.worldPosition.x, trans.worldPosition.y, trans.worldPosition.z);
                Quat newRot(trans.worldRotation.x, trans.worldRotation.y, trans.worldRotation.z, trans.worldRotation.w);

                bodyInterface.SetPositionAndRotation(phys.bodyID, newPos, newRot, EActivation::DontActivate);
            }
        }
        else {
            if (bodyInterface.GetMotionType(phys.bodyID) != EMotionType::Static) {
                RVec3 pos;
                Quat rot;
                bodyInterface.GetPositionAndRotation(phys.bodyID, pos, rot);

                trans.worldPosition = { (float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ() };
                trans.worldRotation = { rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW() };

            }
        }
        });
}
