#include "EnemyBoss.h"
#include "Collision/ColliderComponent.h"
#include "Animator/AnimatorComponent.h"
#include "C_EnemyHUD.h"
#include "EnemyLocomotionComponent.h"
#include "EnemyAIController.h" // ★必須
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
    health = 10; // テスト用に少し増やしておきましょう
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

    // ゲームプレイデータ（攻撃モーションの定義など）のロード
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

    // ★AIコントローラーの初期化と「脳」のロード
    // Component::Start() でもロードしていますが、明示的にここでパスを指定する方が安全です
    auto ai = AddComponent<EnemyAIController>();
    if (ai) {
        // エディタで作ったJSONを指定
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
        // StageInfoComponentを持っているActorを探す
        auto stageInfo = actor->GetComponent<StageInfoComponent>();
        if (stageInfo)
        {
            this->stageLimitRadius = stageInfo->radius;
            // 少しだけマージンを持たせて、プレイヤーより内側にする等の調整も可能
            // this->stageLimitRadius = stageInfo->radius * 0.95f; 
            break;
        }
    }






}

void EnemyBoss::Update(float dt)
{
    // -------------------------------------------------------------
    // 1. タイムラインと当たり判定の更新
    // -------------------------------------------------------------
    if (sequencer)
    {
        int currentFrame = sequencer->GetCurrentFrame();
        const auto& items = sequencer->GetItems();
        bool inHitbox = false;

        for (const auto& it : items) {
            // Hitboxバー(Type 0)の中にいるかチェック
            if (it.type == 0 && currentFrame >= it.start && currentFrame <= it.end) {
                inHitbox = true;
                // 「新しいバー」に入った瞬間だけリストをクリアする（多段ヒット防止）
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
    // 2. ダメージ蓄積（バーストカウンター用）の減衰
    // -------------------------------------------------------------
    if (currentState != BossState::Counter && accumulatedDamage > 0.0f) {
        damageDecayTimer += dt;
        if (damageDecayTimer > DECAY_START_TIME) {
            accumulatedDamage -= 30.0f * dt;
            if (accumulatedDamage < 0.0f) accumulatedDamage = 0.0f;
        }
    }

    // -------------------------------------------------------------
    // 3. ステートマシン (肉体の制御)
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
        return; // 死んでいるならここより下の処理はしない

    case BossState::Hit:
        // ヒットリアクション中は動けない
        stateTimer -= dt;
        if (stateTimer <= 0.0f) {
            // リアクション終了 -> アイドルに戻る（これでAIが再び命令可能になる）
            currentState = BossState::Idle;
            if (animator) animator->StopAction();
        }
        if (locomotion) locomotion->Stop();
        break;

    case BossState::Attack:
    case BossState::Counter:
        if (runner && sequencer)
        {
            // 攻撃アニメーションが終わったかチェック
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
        // 移動アニメーションの制御 (View)
        // ※移動命令自体は AIController -> LocomotionComponent が行う
        if (animator && locomotion)
        {
            float speed = locomotion->GetCurrentSpeed();

            // AIが移動させているなら「走り」、止まっているなら「待機」
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
    // 4. 物理とトランスフォームの更新
    // -------------------------------------------------------------
    Character::UpdateVelocity(dt);
    Character::ApplyStageConstraint(this->stageLimitRadius);
    Character::UpdateTransform();

    // コライダーの位置をアニメーションに同期
    if (auto collider = GetComponent<ColliderComponent>()) {
        int currentFrame = static_cast<int>(runner->GetTimeSeconds() * 60.0f);
        collider->SyncFromSequencer(sequencer.get(), currentFrame);
    }
}

// ----------------------------------------------------------------------------
// BTAction_PlayAnim から呼ばれる関数
// AIが「この攻撃を出せ」と命令してくるところ
// ----------------------------------------------------------------------------
void EnemyBoss::PlayAction(int animIndex)
{
    // 死んでる時やリアクション中、カウンター中はAIの命令を無視する
    if (currentState == BossState::Dead ||
        currentState == BossState::Hit ||
        currentState == BossState::Counter)
    {
        return;
    }

    if (!runner || !sequencer || !animator) return;

    // Timelineデータのセットアップ
    if (animIndex < (int)gameplayData.timelines.size()) {
        sequencer->GetItemsMutable() = gameplayData.timelines[animIndex];
    }
    else {
        sequencer->GetItemsMutable().clear();
    }

    // Curve設定の適用
    if (animIndex < (int)gameplayData.curves.size()) {
        sequencer->SetCurveSettings(gameplayData.curves[animIndex]);
    }

    // アニメーション時間の取得
    float duration = 1.0f;
    if (auto model = GetModelRaw()) {
        const auto& anims = model->GetAnimations();
        if (animIndex < (int)anims.size()) {
            duration = anims[animIndex].secondsLength;
        }
    }

    // ログ出力（デバッグ用）
    char buf[256];
    sprintf_s(buf, "[EnemyBoss] AI Ordered Action: [%d] Dur: %.2f\n", animIndex, duration);
    OutputDebugStringA(buf);

    // 再生開始
    runner->SetClipLength(duration);
    runner->SetTimeSeconds(0.0f);
    runner->SetLoop(false);
    runner->Play();

    animator->PlayAction(animIndex, false, 0.1f, true);

    // ステートを Attack に変更 -> これにより IsActionPlaying() が true になる
    currentState = BossState::Attack;

    // 攻撃中は足をとめる
    if (locomotion) locomotion->Stop();
}

// ----------------------------------------------------------------------------
// AIが「今忙しいか？」を確認する関数
// ----------------------------------------------------------------------------
bool EnemyBoss::IsActionPlaying() const
{
    // 攻撃中だけでなく、被ダメージ中や死亡時も「忙しい」とみなす
    // これにより、リアクション中にAIが次の行動（移動など）を被せてくるのを防ぐ
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

    // カウンター中は無敵（あるいはダメージ軽減）
    if (currentState == BossState::Counter) {
        OutputDebugStringA("[Enemy] Invincible! (Countering)\n");
        return;
    }

    int damageTaken = lastDamage;
    accumulatedDamage += (float)damageTaken;
    damageDecayTimer = 0.0f;

    // バーストカウンター判定
    if (accumulatedDamage >= burstThreshold)
    {
        OutputDebugStringA("[Enemy] BURST COUNTER ACTIVATED!\n");
        accumulatedDamage = 0.0f;
        currentState = BossState::Counter; // ステート強制変更

        // 現在の行動をキャンセル
        if (runner) runner->Stop();
        if (locomotion) locomotion->Stop();
        if (animator) animator->StopAction();

        // カウンター咆哮の再生
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
        // 通常ヒットリアクション
        currentState = BossState::Hit; // ステート強制変更
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

    // 攻撃判定
    if (selfCol->attribute == ColliderAttribute::Attack &&
        otherCol->attribute == ColliderAttribute::Body)
    {
        if (hitList.find(other) != hitList.end()) return;

        if (otherChar)
        {
            hitList.insert(other);
            OutputDebugStringA("[HIT] Boss Attack hit Character!\n");

            // ダメージ適用
            otherChar->ApplyDamage(1, 0.5f);

            // ヒットストップとシェイク
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