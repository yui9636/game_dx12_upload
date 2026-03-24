#include "EnemyBoss.h"
#include "Collision/ColliderComponent.h"
#include "Animator/AnimatorComponent.h"
#include "C_EnemyHUD.h"
#include "EnemyLocomotionComponent.h"
#include "EnemyAIController.h"
#include "Runner/RunnerComponent.h"
#include "Timeline/TimelineSequencerComponent.h"
#include "Storage/GEStorageCompilerComponent.h"
#include "Graphics.h"
#include "Model/Model.h"
#include "EnemyManager.h"
#include "Camera/CameraController.h"
#include <Stage\StageInfoComponent.h>

using namespace DirectX;

EnemyBoss::EnemyBoss()
{
    name = "MUTANT BOSS";
    health = 10;
    maxHealth = 10;

    currentState = BossState::Idle;
    stateTimer = 0.0f;
    accumulatedDamage = 0.0f;
}

void EnemyBoss::Initialize(ID3D11Device* device)
{
    Character::Start();
    Enemy::Initialize(device);

    auto EnsureComp = [&](auto& ptr) {
        using CompType = typename std::remove_reference<decltype(*ptr)>::type;
        ptr = GetComponent<CompType>();
        if (!ptr) ptr = AddComponent<CompType>();
        };

    EnsureComp(animator);
    EnsureComp(runner);
    EnsureComp(sequencer);
    if (sequencer && runner) sequencer->SetRunner(runner.get());

    auto compiler = GetComponent<GEStorageCompilerComponent>();
    if (!compiler) compiler = AddComponent<GEStorageCompilerComponent>();
    if (compiler) {
        gameplayData = compiler->LoadGameplayDataFromPath("Data/Gameplay/A_EnemyGameplay.json");
    }

    EnsureComp(locomotion);
    if (locomotion) {
        locomotion->Start();
        locomotion->SetArenaRadius(100.0f);
    }

    auto ai = AddComponent<EnemyAIController>();
    if (ai) {
        ai->LoadAIAsset("Data/AI/Boss_Phase1.json");
    }

    if (animator) {
        animator->Start();
        animator->SetRootMotionEnabled(false);
        animator->PlayBase(Idle, true, 0.2f);
    }

    auto hud = AddComponent<C_EnemyHUD>();
    if (hud) hud->SetBossName("MUTANT OVERLORD");

    const auto& actors = ActorManager::Instance().GetActors();
    for (const auto& actor : actors)
    {
        auto stageInfo = actor->GetComponent<StageInfoComponent>();
        if (stageInfo)
        {
            this->stageLimitRadius = stageInfo->radius;
            // this->stageLimitRadius = stageInfo->radius * 0.95f; 
            break;
        }
    }






}

void EnemyBoss::Update(float dt)
{
    // -------------------------------------------------------------
    // -------------------------------------------------------------
    if (sequencer)
    {
        int currentFrame = sequencer->GetCurrentFrame();
        const auto& items = sequencer->GetItems();
        bool inHitbox = false;

        for (const auto& it : items) {
            if (it.type == 0 && currentFrame >= it.start && currentFrame <= it.end) {
                inHitbox = true;
                if (this->lastHitboxStart != it.start) {
                    this->hitList.clear();
                    this->lastHitboxStart = it.start;
                }
                break;
            }
        }
        if (!inHitbox) { lastHitboxStart = -1; }
    }

    Enemy::Update(dt);

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    if (currentState != BossState::Counter && accumulatedDamage > 0.0f) {
        damageDecayTimer += dt;
        if (damageDecayTimer > DECAY_START_TIME) {
            accumulatedDamage -= 30.0f * dt;
            if (accumulatedDamage < 0.0f) accumulatedDamage = 0.0f;
        }
    }

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    switch (currentState)
    {
    case BossState::Dead:
        Character::UpdateVelocity(dt);
        Character::UpdateTransform();
        stateTimer -= dt;
        if (stateTimer <= 0.0f)
        {
            if (auto hud = GetComponent<C_EnemyHUD>()) hud->Finalize();

            auto self = std::dynamic_pointer_cast<Enemy>(shared_from_this());
            if (self) {
                EnemyManager::Instance().Remove(self);
                ActorManager::Instance().Remove(self);
            }
        }
        return;

    case BossState::Hit:
        stateTimer -= dt;
        if (stateTimer <= 0.0f) {
            currentState = BossState::Idle;
            if (animator) animator->StopAction();
        }
        if (locomotion) locomotion->Stop();
        break;

    case BossState::Attack:
    case BossState::Counter:
        if (runner && sequencer)
        {
            if (runner->GetTimeSeconds() >= runner->GetClipLength())
            {
                currentState = BossState::Idle;
                if (animator) animator->StopAction();

                if (currentState == BossState::Counter) {
                    OutputDebugStringA("[Enemy] Counter Finished.\n");
                }
            }
        }
        break;

    case BossState::Idle:
    case BossState::Move:
        if (animator && locomotion)
        {
            float speed = locomotion->GetCurrentSpeed();

            if (speed > 0.1f) {
                if (currentState != BossState::Move) {
                    currentState = BossState::Move;
                    animator->PlayBase(Run, true, 0.2f);
                }
            }
            else {
                if (currentState != BossState::Idle) {
                    currentState = BossState::Idle;
                    animator->PlayBase(Idle, true, 0.2f);
                }
            }
        }
        break;
    }

    // -------------------------------------------------------------
    // -------------------------------------------------------------
    Character::UpdateVelocity(dt);
    Character::ApplyStageConstraint(this->stageLimitRadius);
    Character::UpdateTransform();

    if (auto collider = GetComponent<ColliderComponent>()) {
        int currentFrame = static_cast<int>(runner->GetTimeSeconds() * 60.0f);
        collider->SyncFromSequencer(sequencer.get(), currentFrame);
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void EnemyBoss::PlayAction(int animIndex)
{
    if (currentState == BossState::Dead ||
        currentState == BossState::Hit ||
        currentState == BossState::Counter)
    {
        return;
    }

    if (!runner || !sequencer || !animator) return;

    if (animIndex < (int)gameplayData.timelines.size()) {
        sequencer->GetItemsMutable() = gameplayData.timelines[animIndex];
    }
    else {
        sequencer->GetItemsMutable().clear();
    }

    if (animIndex < (int)gameplayData.curves.size()) {
        sequencer->SetCurveSettings(gameplayData.curves[animIndex]);
    }

    float duration = 1.0f;
    if (auto model = GetModelRaw()) {
        const auto& anims = model->GetAnimations();
        if (animIndex < (int)anims.size()) {
            duration = anims[animIndex].secondsLength;
        }
    }

    char buf[256];
    sprintf_s(buf, "[EnemyBoss] AI Ordered Action: [%d] Dur: %.2f\n", animIndex, duration);
    OutputDebugStringA(buf);

    runner->SetClipLength(duration);
    runner->SetTimeSeconds(0.0f);
    runner->SetLoop(false);
    runner->Play();

    animator->PlayAction(animIndex, false, 0.1f, true);

    currentState = BossState::Attack;

    if (locomotion) locomotion->Stop();
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
bool EnemyBoss::IsActionPlaying() const
{
    return (currentState == BossState::Attack ||
        currentState == BossState::Counter ||
        currentState == BossState::Hit ||
        currentState == BossState::Dead);
}

void EnemyBoss::OnDamaged()
{
    Enemy::OnDamaged();
    if (health <= 0) {
        OnDead();
        return;
    }

    if (currentState == BossState::Counter) {
        OutputDebugStringA("[Enemy] Invincible! (Countering)\n");
        return;
    }

    int damageTaken = lastDamage;
    accumulatedDamage += (float)damageTaken;
    damageDecayTimer = 0.0f;

    if (accumulatedDamage >= burstThreshold)
    {
        OutputDebugStringA("[Enemy] BURST COUNTER ACTIVATED!\n");
        accumulatedDamage = 0.0f;
        currentState = BossState::Counter;

        if (runner) runner->Stop();
        if (locomotion) locomotion->Stop();
        if (animator) animator->StopAction();

        int counterAnim = Roaring;
        if (counterAnim < (int)gameplayData.timelines.size())
            sequencer->GetItemsMutable() = gameplayData.timelines[counterAnim];

        float duration = 2.5f;
        if (auto model = GetModelRaw()) {
            const auto& anims = model->GetAnimations();
            if (counterAnim < (int)anims.size()) duration = anims[counterAnim].secondsLength;
        }

        runner->SetClipLength(duration);
        runner->SetTimeSeconds(0.0f);
        runner->Play();

        if (animator) animator->PlayAction(counterAnim, false, 0.1f, true);
    }
    else
    {
        currentState = BossState::Hit;
        if (runner) runner->Stop();
        if (locomotion) locomotion->Stop();

        if (animator) animator->PlayAction(Hit, false, 0.1f, true);

        stateTimer = 0.5f;
        if (auto model = GetModelRaw()) {
            const auto& anims = model->GetAnimations();
            if (Hit < (int)anims.size()) stateTimer = anims[Hit].secondsLength;
        }
    }
}

void EnemyBoss::OnDead()
{
    currentState = BossState::Dead;

    if (runner) runner->Stop();
    if (locomotion) locomotion->Stop();
    if (animator) animator->StopAction();
    if (animator) animator->PlayBase(Die, false, 0.1f, 1.0f);

    if (auto collider = GetComponent<ColliderComponent>()) {
        collider->SetEnabled(false);
    }

    Enemy::OnDead();

    stateTimer = 3.0f;
    if (auto model = GetModelRaw()) {
        const auto& anims = model->GetAnimations();
        if (Die < (int)anims.size()) {
            stateTimer = anims[Die].secondsLength;
        }
    }
}

void EnemyBoss::OnTriggerEnter(Actor* other, const Collider* selfCol, const Collider* otherCol)
{
    Character* otherChar = dynamic_cast<Character*>(other);

    if (selfCol->attribute == ColliderAttribute::Attack &&
        otherCol->attribute == ColliderAttribute::Body)
    {
        if (hitList.find(other) != hitList.end()) return;

        if (otherChar)
        {
            hitList.insert(other);
            OutputDebugStringA("[HIT] Boss Attack hit Character!\n");

            otherChar->ApplyDamage(1, 0.5f);

            if (sequencer)
            {
                const GESequencerItem* shakeItem = sequencer->GetActiveShakeItem();
                if (shakeItem)
                {
                    if (runner) {
                        runner->RequestHitStop(
                            shakeItem->shake.hitStopDuration,
                            shakeItem->shake.timeScale
                        );
                    }
                    if (auto camCtrl = CameraController::Instance()) {
                        camCtrl->AddShake(
                            shakeItem->shake.amplitude,
                            shakeItem->shake.duration,
                            shakeItem->shake.frequency,
                            shakeItem->shake.decay
                        );
                    }
                }
            }
        }
    }
}
