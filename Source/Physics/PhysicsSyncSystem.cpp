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
            // --- モード1：エディタ同期 (ECS -> Jolt) ---
            // HierarchySystemで計算された最新のワールド座標をJoltに伝える
            if (trans.isDirty) {
                RVec3 newPos(trans.worldPosition.x, trans.worldPosition.y, trans.worldPosition.z);
                Quat newRot(trans.worldRotation.x, trans.worldRotation.y, trans.worldRotation.z, trans.worldRotation.w);

                // SetPositionAndRotation はテレポート（物理を無視して配置）として機能
                bodyInterface.SetPositionAndRotation(phys.bodyID, newPos, newRot, EActivation::DontActivate);
            }
        }
        else {
            // --- モード2：シミュレーション同期 (Jolt -> ECS) ---
            // ※Static（動かない物体）以外を同期対象にする
            if (bodyInterface.GetMotionType(phys.bodyID) != EMotionType::Static) {
                RVec3 pos;
                Quat rot;
                bodyInterface.GetPositionAndRotation(phys.bodyID, pos, rot);

                // 計算結果をワールドキャッシュに書き戻す
                trans.worldPosition = { (float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ() };
                trans.worldRotation = { rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW() };

                // ※注意：親子関係がある場合、ここから localPosition を逆算する必要がありますが
                // 現段階ではワールド座標の確定を優先します。
            }
        }
        });
}