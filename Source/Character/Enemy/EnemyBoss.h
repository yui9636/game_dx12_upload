#pragma once
#include "Enemy.h"
#include <vector>
#include <DirectXMath.h>
#include "Storage/GameplayAsset.h" 
#include <set>

// ボスの状態定義
enum class BossState
{
    Idle,       // 待機 (AI有効)
    Move,       // 移動 (AI有効)
    Attack,     // 攻撃中 (移動不可、スーパーアーマー等はアニメーション依存)
    Hit,        // 被弾怯み (操作不可)
    Counter,    // ★無敵反撃 (完全無敵、操作不可)
    Dead        // 死亡
};

class EnemyBoss : public Enemy
{
public:
    EnemyBoss();
    ~EnemyBoss() override = default;

    enum Animation
    {
        Idle,       // 0
        Roaring,    // 1 (カウンターで使用)
        Attack1,    // 2
        Attack2,    // 3
        JumpAttack, // 4
        Die,        // 5
        Hit,        // 6
        Walk,       // 7
        Run         // 8
    };

    void Initialize(ID3D11Device* device) override;
    void Update(float dt) override;

    // AIからの命令
    void PlayAction(int animIndex);
    bool IsActionPlaying() const;

    // 現在のステート取得（AIが判断に使用）
    BossState GetState() const { return currentState; }


    void OnTriggerEnter(Actor* other, const Collider* selfCol, const Collider* otherCol) override;

    // ★追加: 攻撃済みリストのクリア関数 (Player同様)
    void ClearHitList() { hitList.clear(); }

    float GetStageLimitRadius() const { return stageLimitRadius; }

    virtual std::string GetTypeName() const override { return "EnemyBoss"; }

    bool IsCharacter() const override { return true; }
protected:
    void OnDamaged() override;
    void OnDead() override;

private:
    // --- コンポーネント ---
    std::shared_ptr<class AnimatorComponent> animator;
    std::shared_ptr<class RunnerComponent> runner;
    std::shared_ptr<class TimelineSequencerComponent> sequencer;
    std::shared_ptr<class EnemyLocomotionComponent> locomotion;

    // --- データ ---
    GameplayAsset gameplayData;

    // --- ステート管理 ---
    BossState currentState = BossState::Idle;
    float stateTimer = 0.0f; // Hit硬直などの汎用タイマー

    // --- カウンターシステム ---
    float accumulatedDamage = 0.0f;     // 蓄積ダメージ
    float burstThreshold = 50.0f;      // このダメージを超えるとキレる (HPに応じて変えても良い)
    float damageDecayTimer = 0.0f;      // ダメージ蓄積の減衰用
    const float DECAY_START_TIME = 2.0f; // 2秒殴られなければ蓄積が減り始める

    std::set<Actor*> hitList;
    int lastHitboxStart = -1;

    float stageLimitRadius = 1000.0f;
};