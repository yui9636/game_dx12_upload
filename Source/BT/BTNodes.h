#pragma once
#include "BehaviorTree.h"
#include "Character/Enemy/EnemyBoss.h"
#include "Character/Enemy/EnemyLocomotionComponent.h"

using namespace DirectX;

// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
class BTAction_Log : public BTAction {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        std::string msg = "Log Node Executed";
        if (paramsString.count("Message")) msg = paramsString["Message"];

        std::string out = "[BT] " + msg + "\n";
        OutputDebugStringA(out.c_str());

        return BTStatus::Success;
    }
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
class BTAction_Wait : public BTAction {
    float timer = 0.0f;
    float duration = 1.0f;

protected:
    void OnEnter(BTContext& ctx) override {
        timer = 0.0f;
        duration = 1.0f;
        if (paramsFloat.count("Time")) duration = paramsFloat["Time"];
    }

    BTStatus OnUpdate(BTContext& ctx) override {
        timer += ctx.deltaTime;
        if (timer >= duration) {
            return BTStatus::Success;
        }
        return BTStatus::Running;
    }
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
class BTAction_MoveTo : public BTAction {
    float stopDistSq = 1.0f;

protected:
    void OnEnter(BTContext& ctx) override {
        auto targetPtr = ctx.target.lock();
        if (!ctx.owner || !targetPtr) return;

        float dist = 2.0f;
        if (paramsFloat.count("StopDist")) dist = paramsFloat["StopDist"];
        stopDistSq = dist * dist;

        auto boss = std::dynamic_pointer_cast<EnemyBoss>(ctx.owner);
        if (boss) {
            if (auto loco = boss->GetComponent<EnemyLocomotionComponent>()) {
                loco->MoveTo(targetPtr->GetPosition());
            }
        }
    }

    BTStatus OnUpdate(BTContext& ctx) override {
        auto targetPtr = ctx.target.lock();
        if (!ctx.owner || !targetPtr) return BTStatus::Failure;

        auto boss = std::dynamic_pointer_cast<EnemyBoss>(ctx.owner);
        if (!boss) return BTStatus::Failure;

        auto loco = boss->GetComponent<EnemyLocomotionComponent>();
        if (!loco) return BTStatus::Failure;

        XMVECTOR vOwner = XMLoadFloat3(&ctx.owner->GetPosition());
        XMVECTOR vTarget = XMLoadFloat3(&targetPtr->GetPosition());
        vOwner = XMVectorSetY(vOwner, 0.0f);
        vTarget = XMVectorSetY(vTarget, 0.0f);

        float distSq = XMVectorGetX(XMVector3LengthSq(vTarget - vOwner));

        if (distSq <= stopDistSq) {
            loco->Stop();
            return BTStatus::Success;
        }

        loco->MoveTo(targetPtr->GetPosition());

        return BTStatus::Running;
    }

    void OnExit(BTContext& ctx, BTStatus result) override {
        if (auto boss = std::dynamic_pointer_cast<EnemyBoss>(ctx.owner)) {
            if (auto loco = boss->GetComponent<EnemyLocomotionComponent>()) {
                loco->Stop();
            }
        }
    }
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
class BTAction_PlayAnim : public BTAction {
    bool isTriggered = false;

protected:
    void OnEnter(BTContext& ctx) override {
        isTriggered = false;
        if (!ctx.owner) return;

        auto boss = std::dynamic_pointer_cast<EnemyBoss>(ctx.owner);
        if (boss) {
            int animID = 0;
            if (paramsInt.count("AnimID")) animID = paramsInt["AnimID"];

            boss->PlayAction(animID);
            isTriggered = true;
        }
    }

    BTStatus OnUpdate(BTContext& ctx) override {
        auto boss = std::dynamic_pointer_cast<EnemyBoss>(ctx.owner);
        if (!boss) return BTStatus::Failure;

        if (boss->IsActionPlaying()) {
            return BTStatus::Running;
        }
        return BTStatus::Success;
    }
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
class BTAction_RotateToTarget : public BTAction {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        auto targetPtr = ctx.target.lock();
        if (!ctx.owner || !targetPtr) return BTStatus::Failure;

        XMVECTOR vOwner = XMLoadFloat3(&ctx.owner->GetPosition());
        XMVECTOR vTarget = XMLoadFloat3(&targetPtr->GetPosition());
        XMVECTOR vDir = XMVectorSetY(vTarget - vOwner, 0.0f);

        float distSq = XMVectorGetX(XMVector3LengthSq(vDir));
        if (distSq < 0.0001f) return BTStatus::Success;

        vDir = XMVector3Normalize(vDir);

        float targetAngle = atan2f(XMVectorGetX(vDir), XMVectorGetZ(vDir));

        // XMQuaternionRotationRollPitchYaw(Pitch, Yaw, Roll)
        XMVECTOR qRot = XMQuaternionRotationRollPitchYaw(0.0f, targetAngle, 0.0f);

        XMFLOAT4 rot;
        XMStoreFloat4(&rot, qRot);
        ctx.owner->SetRotation(rot);

        return BTStatus::Success;
    }
};


class BTDecorator_CheckHP : public BTDecorator {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        if (!child) return BTStatus::Failure;

        auto boss = std::dynamic_pointer_cast<EnemyBoss>(ctx.owner);
        if (!boss) return BTStatus::Failure;

        float threshold = 0.5f;
        if (paramsFloat.count("Threshold")) threshold = paramsFloat["Threshold"];

        bool inverse = false;
        // if (paramsInt.count("Inverse")) inverse = (paramsInt["Inverse"] != 0);

        float maxHp = (float)boss->GetMaxHealth();
        if (maxHp <= 0.0f) maxHp = 1.0f;
        float currentHp = (float)boss->GetHealth();
        float ratio = currentHp / maxHp;

        bool isBelow = (ratio <= threshold);

        if (isBelow != inverse) {
            return child->Tick(ctx);
        }

        return BTStatus::Failure;
    }
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
class BTDecorator_Cooldown : public BTDecorator {
    float cooldownTime = 5.0f;
    float timer = -1.0f;
    bool isCoolingDown = false;

protected:
    void OnEnter(BTContext& ctx) override {
        if (paramsFloat.count("Time")) cooldownTime = paramsFloat["Time"];
    }

    BTStatus OnUpdate(BTContext& ctx) override {
        if (!child) return BTStatus::Failure;

        if (isCoolingDown) {
            timer -= ctx.deltaTime;
            if (timer > 0.0f) {
                return BTStatus::Failure;
            }
            isCoolingDown = false;
        }

        BTStatus result = child->Tick(ctx);

        if (result == BTStatus::Success) {
            isCoolingDown = true;
            timer = cooldownTime;
        }

        return result;
    }
};

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
class BTDecorator_Loop : public BTDecorator {
    int targetCount = 3;
    int currentCount = 0;

protected:
    void OnEnter(BTContext& ctx) override {
        currentCount = 0;
        if (paramsInt.count("Count")) targetCount = paramsInt["Count"];
        if (targetCount <= 0) targetCount = 1;
    }

    BTStatus OnUpdate(BTContext& ctx) override {
        if (!child) return BTStatus::Failure;

        BTStatus result = child->Tick(ctx);

        if (result == BTStatus::Running) {
            return BTStatus::Running;
        }

        if (result == BTStatus::Failure) {
            return BTStatus::Failure;
        }

        currentCount++;
        if (currentCount >= targetCount) {
            return BTStatus::Success;
        }

        child->Reset();
        return BTStatus::Running;
    }
};

class BTDecorator_CheckDist : public BTDecorator {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        if (!child) return BTStatus::Failure;

        auto targetPtr = ctx.target.lock();
        if (!ctx.owner || !targetPtr) return BTStatus::Failure;

        float range = 10.0f;
        if (paramsFloat.count("Range")) range = paramsFloat["Range"];

        bool inverse = false;
        if (paramsInt.count("Inverse")) inverse = (paramsInt["Inverse"] != 0);

        XMVECTOR v1 = XMLoadFloat3(&ctx.owner->GetPosition());
        XMVECTOR v2 = XMLoadFloat3(&targetPtr->GetPosition());
        v1 = XMVectorSetY(v1, 0.0f);
        v2 = XMVectorSetY(v2, 0.0f);

        float distSq = XMVectorGetX(XMVector3LengthSq(v1 - v2));
        bool inRange = (distSq <= range * range);

        if (inRange == !inverse) {
            return child->Tick(ctx);
        }

        return BTStatus::Failure;
    }
};

class BTDecorator_CheckWall : public BTDecorator {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        if (!child) return BTStatus::Failure;
        if (!ctx.owner) return BTStatus::Failure;

        float margin = 5.0f;
        if (paramsFloat.count("Margin")) margin = paramsFloat["Margin"];

        bool inverse = false;
        if (paramsInt.count("Inverse")) inverse = (paramsInt["Inverse"] != 0);

        XMVECTOR vPos = XMLoadFloat3(&ctx.owner->GetPosition());
        vPos = XMVectorSetY(vPos, 0.0f);
        float distFromCenter = XMVectorGetX(XMVector3Length(vPos));

        float threshold = ctx.stageRadius - margin;
        bool isNearWall = (distFromCenter >= threshold);

        if (isNearWall == !inverse) {
            return child->Tick(ctx);
        }

        return BTStatus::Failure;
    }
};

class BTDecorator_Probability : public BTDecorator {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        if (!child) return BTStatus::Failure;

        float chance = 0.5f;
        if (paramsFloat.count("Chance")) chance = paramsFloat["Chance"];

        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dis(0.0f, 1.0f);

        if (dis(gen) <= chance) {
            return child->Tick(ctx);
        }
        return BTStatus::Failure;
    }
};







