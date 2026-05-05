// 敵 AI の視野判定を行い、ターゲット情報を Aggro と Blackboard に書き込むシステム実装。
#include "PerceptionSystem.h"

#include <cmath>
#include <vector>

#include <DirectXMath.h>

#include "Archetype/Archetype.h"
#include "Component/ActorTypeComponent.h"
#include "Component/ComponentSignature.h"
#include "Component/TransformComponent.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"

#include "AggroComponent.h"
#include "BlackboardComponent.h"
#include "PerceptionComponent.h"

// このファイル内だけで使う補助関数と補助型を定義する無名名前空間。
namespace
{
    // 知覚判定の候補になる敵対対象の一時データ。
    struct CandidateTarget
    {
        EntityID entity;
        DirectX::XMFLOAT3 worldPos;
    };

    // 敵 AI が検知対象にするプレイヤー候補を収集する。
    void CollectHostileCandidates(Registry& registry, std::vector<CandidateTarget>& out)
    {
        Signature sig = CreateSignature<ActorTypeComponent, TransformComponent>();
        for (auto* arch : registry.GetAllArchetypes()) {
            if (!SignatureMatches(arch->GetSignature(), sig)) continue;
            auto* atCol = arch->GetColumn(TypeManager::GetComponentTypeID<ActorTypeComponent>());
            auto* trCol = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
            if (!atCol || !trCol) continue;
            const auto& ents = arch->GetEntities();
            for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                const auto& at = *static_cast<ActorTypeComponent*>(atCol->Get(i));
                if (at.type != ActorType::Player) continue;
                const auto& tr = *static_cast<TransformComponent*>(trCol->Get(i));
                CandidateTarget c;
                c.entity = ents[i];
                c.worldPos = tr.worldPosition;
                out.push_back(c);
            }
        }
    }

    // 検知したターゲット情報をブラックボードへ書き込む。
    void WriteTargetIntoBlackboard(BlackboardComponent& bb, EntityID target,
                                   const DirectX::XMFLOAT3& targetPos, float distance)
    {
        BlackboardValue v;
        v.type = BlackboardValueType::Entity;
        v.entity = target;
        bb.entries["Target"] = v;

        BlackboardValue p;
        p.type = BlackboardValueType::Vector3;
        p.v3   = targetPos;
        bb.entries["TargetPos"] = p;

        BlackboardValue d;
        d.type = BlackboardValueType::Float;
        d.f    = distance;
        bb.entries["TargetDist"] = d;

        BlackboardValue lst;
        lst.type = BlackboardValueType::Float;
        lst.f    = 0.0f;
        bb.entries["LastSeenTime"] = lst;
    }

    // ターゲットを見失った状態をブラックボードへ反映する。
    void ClearTargetInBlackboard(BlackboardComponent& bb, float timeSinceSighted)
    {
        BlackboardValue v;
        v.type = BlackboardValueType::Entity;
        v.entity = Entity::NULL_ID;
        bb.entries["Target"] = v;

        BlackboardValue lst;
        lst.type = BlackboardValueType::Float;
        lst.f    = timeSinceSighted;
        bb.entries["LastSeenTime"] = lst;
    }

    // Transform のワールド行列から XZ 平面上の前方向を取得する。
    DirectX::XMFLOAT3 GetForwardXZ(const TransformComponent& tr)
    {
        using namespace DirectX;
        XMMATRIX W = XMLoadFloat4x4(&tr.worldMatrix);
        XMVECTOR fwd = XMVector3Normalize(W.r[2]);
        XMFLOAT3 f{};
        XMStoreFloat3(&f, fwd);
        f.y = 0.0f;
        const float len = std::sqrt(f.x * f.x + f.z * f.z);
        if (len > 0.0001f) {
            f.x /= len;
            f.z /= len;
        } else {
            f = { 0.0f, 0.0f, 1.0f };
        }
        return f;
    }
}

// 敵の視野判定を行い、Aggro と Blackboard を更新する。
void PerceptionSystem::Update(Registry& registry, float dt)
{
    std::vector<CandidateTarget> hostiles;
    CollectHostileCandidates(registry, hostiles);

    Signature sig = CreateSignature<
        EnemyTagComponent, PerceptionComponent, AggroComponent,
        TransformComponent, BlackboardComponent>();

    for (auto* arch : registry.GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;

        auto* perCol  = arch->GetColumn(TypeManager::GetComponentTypeID<PerceptionComponent>());
        auto* aggroCol= arch->GetColumn(TypeManager::GetComponentTypeID<AggroComponent>());
        auto* trCol   = arch->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* bbCol   = arch->GetColumn(TypeManager::GetComponentTypeID<BlackboardComponent>());
        if (!perCol || !aggroCol || !trCol || !bbCol) continue;

        const auto& ents = arch->GetEntities();
        for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
            const EntityID self = ents[i];
            auto& per   = *static_cast<PerceptionComponent*>(perCol->Get(i));
            auto& aggro = *static_cast<AggroComponent*>(aggroCol->Get(i));
            auto& tr    = *static_cast<TransformComponent*>(trCol->Get(i));
            auto& bb    = *static_cast<BlackboardComponent*>(bbCol->Get(i));

            if (!per.sightEnabled) continue;

            const DirectX::XMFLOAT3 fwd = GetForwardXZ(tr);
            const float halfFOV = per.sightFOV * 0.5f;
            const float cosHalfFOV = std::cos(halfFOV);

            EntityID bestTarget = Entity::NULL_ID;
            DirectX::XMFLOAT3 bestPos{ 0.0f, 0.0f, 0.0f };
            float bestDist = 1e9f;

            for (const auto& cand : hostiles) {
                if (cand.entity == self) continue;

                const float dx = cand.worldPos.x - tr.worldPosition.x;
                const float dz = cand.worldPos.z - tr.worldPosition.z;
                const float dist2D = std::sqrt(dx * dx + dz * dz);
                if (dist2D > per.sightRadius) continue;

                if (dist2D < 0.001f) {
                } else {
                    const float ndx = dx / dist2D;
                    const float ndz = dz / dist2D;
                    const float dot = fwd.x * ndx + fwd.z * ndz;
                    if (dot < cosHalfFOV) continue;
                }

                if (dist2D < bestDist) {
                    bestDist = dist2D;
                    bestTarget = cand.entity;
                    bestPos = cand.worldPos;
                }
            }

            if (!Entity::IsNull(bestTarget)) {
                aggro.currentTarget    = bestTarget;
                aggro.timeSinceSighted = 0.0f;
                if (aggro.threat < 1.0f) aggro.threat += dt;
                if (aggro.threat > 1.0f) aggro.threat = 1.0f;
                WriteTargetIntoBlackboard(bb, bestTarget, bestPos, bestDist);
            } else {
                aggro.timeSinceSighted += dt;
                if (aggro.timeSinceSighted >= aggro.loseTargetAfter) {
                    aggro.currentTarget = Entity::NULL_ID;
                    aggro.threat        = 0.0f;
                }
                ClearTargetInBlackboard(bb, aggro.timeSinceSighted);
            }
        }
    }
}
