// BTNodes.h 修正版
#pragma once
#include "BehaviorTree.h"
#include "Character/Enemy/EnemyBoss.h"
#include "Character/Enemy/EnemyLocomotionComponent.h"

using namespace DirectX;

// ============================================================================
// アクションノード群 (Actions)
// ============================================================================

// ----------------------------------------------------------------------------
// デバッグ用ログ出力
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
// 指定時間待機
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
// 移動 (ターゲットへの接近)
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

        // 停止距離に入ったら成功
        if (distSq <= stopDistSq) {
            loco->Stop();
            return BTStatus::Success;
        }

        // ターゲットの位置更新 (追尾)
        loco->MoveTo(targetPtr->GetPosition());

        return BTStatus::Running;
    }

    void OnExit(BTContext& ctx, BTStatus result) override {
        // 中断時も確実に停止させる安全策
        if (auto boss = std::dynamic_pointer_cast<EnemyBoss>(ctx.owner)) {
            if (auto loco = boss->GetComponent<EnemyLocomotionComponent>()) {
                loco->Stop();
            }
        }
    }
};

// ----------------------------------------------------------------------------
// アニメーション再生 (攻撃)
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

        // 再生中は Running、終われば Success
        if (boss->IsActionPlaying()) {
            return BTStatus::Running;
        }
        return BTStatus::Success;
    }
};

// ----------------------------------------------------------------------------
// ターゲットへの旋回アクション
// パラメータ: なし (即座に向きを変える例)
// ----------------------------------------------------------------------------
class BTAction_RotateToTarget : public BTAction {
protected:
    BTStatus OnUpdate(BTContext& ctx) override {
        auto targetPtr = ctx.target.lock();
        if (!ctx.owner || !targetPtr) return BTStatus::Failure;

        // 1. ターゲットへの方向ベクトルを計算
        XMVECTOR vOwner = XMLoadFloat3(&ctx.owner->GetPosition());
        XMVECTOR vTarget = XMLoadFloat3(&targetPtr->GetPosition());
        XMVECTOR vDir = XMVectorSetY(vTarget - vOwner, 0.0f); // Y軸（高さ）は無視

        // 2. 距離が近すぎる場合は計算しない（ゼロ除算防止）
        float distSq = XMVectorGetX(XMVector3LengthSq(vDir));
        if (distSq < 0.0001f) return BTStatus::Success;

        vDir = XMVector3Normalize(vDir);

        // 3. Y軸周りの目標角度（Yaw）を計算
        // atan2f(x, z) でラジアン角を取得
        float targetAngle = atan2f(XMVectorGetX(vDir), XMVectorGetZ(vDir));

        // 4. Yaw角からクォータニオンを作成
        // XMQuaternionRotationRollPitchYaw(Pitch, Yaw, Roll)
        XMVECTOR qRot = XMQuaternionRotationRollPitchYaw(0.0f, targetAngle, 0.0f);

        // 5. Actorに回転をセット
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

        bool inverse = false; // trueなら「閾値より大きい」で成功
        // if (paramsInt.count("Inverse")) inverse = (paramsInt["Inverse"] != 0);

        // ボスのHP率を計算
        float maxHp = (float)boss->GetMaxHealth();
        if (maxHp <= 0.0f) maxHp = 1.0f; // ゼロ除算防止
        float currentHp = (float)boss->GetHealth();
        float ratio = currentHp / maxHp;

        bool isBelow = (ratio <= threshold);

        // 条件成立なら子を実行、そうでなければ失敗
        if (isBelow != inverse) {
            return child->Tick(ctx);
        }

        return BTStatus::Failure;
    }
};

// ----------------------------------------------------------------------------
// クールダウンデコレーター
// パラメータ: "Time" (Float)
// 用途: 一度成功したら、指定時間が経過するまで失敗を返す（技の連発防止）。
// ----------------------------------------------------------------------------
class BTDecorator_Cooldown : public BTDecorator {
    float cooldownTime = 5.0f;
    float timer = -1.0f; // 負の値ならクールダウンしていない
    bool isCoolingDown = false;

protected:
    void OnEnter(BTContext& ctx) override {
        if (paramsFloat.count("Time")) cooldownTime = paramsFloat["Time"];
    }

    BTStatus OnUpdate(BTContext& ctx) override {
        if (!child) return BTStatus::Failure;

        // クールダウン中の処理
        if (isCoolingDown) {
            timer -= ctx.deltaTime;
            if (timer > 0.0f) {
                return BTStatus::Failure; // まだクールダウン中
            }
            isCoolingDown = false; // クールダウン終了
        }

        // 子を実行
        BTStatus result = child->Tick(ctx);

        // 子が成功したらクールダウン開始
        if (result == BTStatus::Success) {
            isCoolingDown = true;
            timer = cooldownTime;
        }

        return result;
    }
};

// ----------------------------------------------------------------------------
// ループデコレーター
// パラメータ: "Count" (Int)
// 用途: 子ノードを指定回数成功させるまでループする（3連撃など）。
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

        // 子を実行
        BTStatus result = child->Tick(ctx);

        if (result == BTStatus::Running) {
            return BTStatus::Running;
        }

        if (result == BTStatus::Failure) {
            return BTStatus::Failure; // 子が失敗したらループ中断
        }

        // 子が成功した場合
        currentCount++;
        if (currentCount >= targetCount) {
            return BTStatus::Success; // ループ完了
        }

        // まだ回数が残っている場合、子をリセットして Running を返す（次フレームで再実行）
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

        // 条件成立なら子を実行
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

        // パラメータ取得 (デフォルトは端から5m以内)
        float margin = 5.0f;
        if (paramsFloat.count("Margin")) margin = paramsFloat["Margin"];

        bool inverse = false;
        if (paramsInt.count("Inverse")) inverse = (paramsInt["Inverse"] != 0);

        // 現在地とステージ中心(0,0,0)との距離を計算
        // ※ステージが原点以外にある場合は、StageInfoComponentから中心座標も取得して計算する必要があります
        XMVECTOR vPos = XMLoadFloat3(&ctx.owner->GetPosition());
        vPos = XMVectorSetY(vPos, 0.0f); // 高さは無視
        float distFromCenter = XMVectorGetX(XMVector3Length(vPos));

        // 「半径 - マージン」より外側にいるか？
        // 例: 半径50m, マージン5m -> 中心から45m以上離れていたら「壁際」
        float threshold = ctx.stageRadius - margin;
        bool isNearWall = (distFromCenter >= threshold);

        // 条件成立なら子を実行
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







