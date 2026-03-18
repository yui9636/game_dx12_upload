#include "PhysicsSystem.h"
#include <System\Query.h>


using namespace DirectX;

void PhysicsSystem::Update(Registry& registry, float deltaTime) {
    auto& physicsMgr = PhysicsManager::Instance();
    
    // 1. Joltのシミュレーションを1フレーム進める
    physicsMgr.Update(deltaTime);

    // 2. JoltのBodyInterface（データを読み書きする窓口）を取得
    JPH::BodyInterface& bodyInterface = physicsMgr.GetBodyInterface();

    // 3. ECSから「物理ボディ」と「トランスフォーム」を持つエンティティを全検索
    Query<PhysicsComponent, TransformComponent> query(registry);

    // 4. 爆速ループでJoltの計算結果をECSに書き戻す
    query.ForEach([&](const PhysicsComponent& phys, TransformComponent& trans) {
        
        // Joltから最新のワールド座標と回転を取得
        JPH::RVec3 joltPos = bodyInterface.GetPosition(phys.bodyID);
        JPH::Quat joltRot = bodyInterface.GetRotation(phys.bodyID);

        // ECSのTransform(ローカル座標)に上書きする
        // ※ルートオブジェクトであることを前提としています
        trans.localPosition = { (float)joltPos.GetX(), (float)joltPos.GetY(), (float)joltPos.GetZ() };
        trans.localRotation = { joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW() };

        // 注意: この後 TransformSystem::Update が走ることで、
        // worldMatrix 等の計算が正しく行われます。
    });
}