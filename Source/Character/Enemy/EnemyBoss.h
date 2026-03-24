#pragma once
#include "Enemy.h"
#include <vector>
#include <DirectXMath.h>
#include "Storage/GameplayAsset.h" 
#include <set>

enum class BossState
{
    Idle,
    Move,
    Attack,
    Hit,
    Counter,
    Dead
};

class EnemyBoss : public Enemy
{
public:
    EnemyBoss();
    ~EnemyBoss() override = default;

    enum Animation
    {
        Idle,       // 0
        Roaring,
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

    void PlayAction(int animIndex);
    bool IsActionPlaying() const;

    BossState GetState() const { return currentState; }


    void OnTriggerEnter(Actor* other, const Collider* selfCol, const Collider* otherCol) override;

    void ClearHitList() { hitList.clear(); }

    float GetStageLimitRadius() const { return stageLimitRadius; }

    virtual std::string GetTypeName() const override { return "EnemyBoss"; }

    bool IsCharacter() const override { return true; }
protected:
    void OnDamaged() override;
    void OnDead() override;

private:
    std::shared_ptr<class AnimatorComponent> animator;
    std::shared_ptr<class RunnerComponent> runner;
    std::shared_ptr<class TimelineSequencerComponent> sequencer;
    std::shared_ptr<class EnemyLocomotionComponent> locomotion;

    GameplayAsset gameplayData;

    BossState currentState = BossState::Idle;
    float stateTimer = 0.0f;

    float accumulatedDamage = 0.0f;
    float burstThreshold = 50.0f;
    float damageDecayTimer = 0.0f;
    const float DECAY_START_TIME = 2.0f;

    std::set<Actor*> hitList;
    int lastHitboxStart = -1;

    float stageLimitRadius = 1000.0f;
};
