#include "Player.h"
#include "Model/Model.h"
#include "Input/InputActionComponent.h"
#include "Character/LocomotionComponent.h"
#include "Animator/AnimatorComponent.h"
#include "Runner/RunnerComponent.h"
#include "Timeline/TimelineSequencerComponent.h"
#include "Storage/GEStorageCompilerComponent.h"
#include "easing.h"
#include "Camera/CameraController.h"
#include "Effect/HologramManager.h"
#include "Effect/EffectManager.h"
#include "Collision/ColliderComponent.h"
#include "Graphics.h"
#include "Model/ModelRenderer.h"
#include <cmath>
#include "C_PlayerHUD.h"
#include "Character/Enemy/EnemyManager.h"
#include "Character/Enemy/Enemy.h"
#include <Stage\StageInfoComponent.h>

using namespace DirectX;

// -------------------------------------------------------------------------
// コンストラクタ & 初期化
// -------------------------------------------------------------------------
Player::Player()
{
    name = "Player"; // 名前などをセット
}

void Player::Initialize(ID3D11Device* device)
{

}

void Player::Start()
{
    Character::Start();

    auto EnsureComp = [&](auto& ptr) {
        using CompType = typename std::remove_reference<decltype(*ptr)>::type;
        ptr = GetComponent<CompType>();
        if (!ptr) ptr = AddComponent<CompType>();
        ptr->Start();
        };

    EnsureComp(input);
    EnsureComp(locomotion);
    EnsureComp(runner);
    EnsureComp(sequencer);

    animator = GetComponent<AnimatorComponent>();

    auto compiler = GetComponent<GEStorageCompilerComponent>();
    if (!compiler) compiler = AddComponent<GEStorageCompilerComponent>();

    if (input) input->SetMoveDeadzone(0.15f);
    if (sequencer && runner) sequencer->SetRunner(runner.get());

    if (compiler) {

        std::string path = compiler->GetTargetFilePath();
      
        gameplayData = compiler->LoadGameplayDataFromPath(path); 
    }


    auto hud = GetComponent<C_PlayerHUD>();
    if (!hud) hud = AddComponent<C_PlayerHUD>();
    hud->Start();

    const auto& actors = ActorManager::Instance().GetActors();
    for (const auto& actor : actors)
    {
        auto stageInfo = actor->GetComponent<StageInfoComponent>();
        if (stageInfo)
        {
            // ステージ情報が見つかったら、半径をメンバ変数に保持しておく
            // (Player.h に float stageLimitRadius = 50.0f; を追加する前提)
            this->stageLimitRadius = stageInfo->radius;
            break;
        }
    }



    // ★アクションデータベース構築 (7段コンボ + マグネティズム)
    BuildActionDatabase();

    state = State::Locomotion;
    currentActionIdx = -1;
    reservedActionIdx = -1;

    // 物理パラメータ
    gravity = -30.0f; // キビキビ動かす
    friction = 5.0f;
    velocity = { 0,0,0 };
    verticalVelocity = 0.0f;
    health = 100;
    maxHealth = 100;
    isMagnetismActive = false;
    invincibleTimer = 0.0f;
    UpdateTransform();
}

void Player::BuildActionDatabase() {
    actionDatabase.clear();
    auto AddNode = [&](int animId, float inS, float comboS, float cancelS, float magS, float animS) -> int {
        ActionNode node;
        node.animIndex = animId;
        node.inputStart = inS;
        node.comboStart = comboS;
        node.cancelStart = cancelS;
        node.magnetismSpeed = magS;
        node.animSpeed = animS; // 追加したメンバに反映
        actionDatabase.push_back(node);
        return (int)actionDatabase.size() - 1;
        };

    int l1 = AddNode(Combo1, 0.1f, 0.4f, 0.2f, 25.0f, 0.75f);
    int l2 = AddNode(Combo2, 0.1f, 0.4f, 0.2f, 12.0f, 0.75f);
    int l3 = AddNode(Combo3, 0.1f, 0.4f, 0.2f, 12.0f, 0.75f);
    int l4 = AddNode(Combo9, 0.1f, 0.45f, 0.3f, 15.0f, 0.75f);
    int l5 = AddNode(Combo5, 0.1f, 0.45f, 0.3f, 10.0f, 0.75f);
    int l6 = AddNode(Combo6, 0.1f, 0.5f, 0.3f, 12.0f, 0.75f);
    int l7 = AddNode(Combo7, 0.0f, 1.0f, 0.5f, 30.0f, 0.75f); // フィニッシュは少しタメる

    // --- 強攻撃 3段 (H1~H3) ---
    // 強攻撃は 0.7f ~ 0.8f に落として「重さ」を表現します
    int h1 = AddNode(Skill_Attack3_root, 0.2f, 0.6f, 0.4f, 10.0f, 0.65f);
    int h2 = AddNode(Skill_Attack1_root, 0.2f, 0.6f, 0.4f, 12.0f, 0.65f);
    int h3 = AddNode(DashAttack1_root, 0.0f, 1.0f, 0.7f, 35.0f, 0.85f);

    // 弱コンボリンク (1->2->3->4->5->6->7)
    actionDatabase[l1].nextLight = l2; actionDatabase[l2].nextLight = l3;
    actionDatabase[l3].nextLight = l4; actionDatabase[l4].nextLight = l5;
    actionDatabase[l5].nextLight = l6; actionDatabase[l6].nextLight = l7;

    // 強コンボリンク (1->2->3)
    actionDatabase[h1].nextHeavy = h2; actionDatabase[h2].nextHeavy = h3;

    // 弱から強への派生（ニーア風のショートカット）
    actionDatabase[l1].nextHeavy = h1; actionDatabase[l2].nextHeavy = h1;
    actionDatabase[l3].nextHeavy = h2; actionDatabase[l4].nextHeavy = h2;
    actionDatabase[l5].nextHeavy = h3; actionDatabase[l6].nextHeavy = h3;
}

void Player::Update(float dt)
{
   

    auto sequencer = GetComponent<TimelineSequencerComponent>();
    if (sequencer)
    {
        int currentFrame = sequencer->GetCurrentFrame();
        const auto& items = sequencer->GetItems();
        bool inHitbox = false;

        for (const auto& it : items) {
            // Hitboxバー(Type 0)の中にいるかチェック
            if (it.type == 0 && currentFrame >= it.start && currentFrame <= it.end) {
                inHitbox = true;
                // 「新しいバー」に入った瞬間だけリストをクリアする
                if (this->lastHitboxStart != it.start) {
                    this->hitList.clear();
                    this->lastHitboxStart = it.start;
                }
                break;
            }
        }
        // どのバーにも触れていない期間はメモをリセット
        if (!inHitbox) { lastHitboxStart = -1; }
    }



    if (input) input->Update(dt);

    // 重力処理 (地上にいなければ適用)
    if (!isGround) {
        verticalVelocity += customGravity * dt;
    }

    switch (state)
    {
    case State::Locomotion:
        UpdateLocomotion(dt);
        break;

    case State::Action:
        UpdateAction(dt);
        break;

    case State::Dodge:
     

        // スライド移動
        {
            float moveSpeed = 6.0f * dodgeMoveScale * dt;
            DirectX::XMFLOAT3 pos = GetPosition();
            pos.x += sinf(angle.y) * moveSpeed;
            pos.z += cosf(angle.y) * moveSpeed;
            SetPosition(pos);
        }

        if (runner && runner->GetTimeNormalized() >= 0.9f) {
            state = State::Locomotion;
        }
        break;

    case State::Damage:
        Character::UpdateVelocity(dt);
        Character::UpdateTransform();

        // 硬直時間が終わるか、アニメーションが終わったら復帰
        stateTimer -= dt;
        if (stateTimer <= 0.0f)
        {
            state = State::Locomotion;
            // 無敵時間はCharacter::Updateで管理されているのでここでは気にしなくてOK
        }
        return; // ここでリターンして操作を受け付けないようにする
    case State::Dead:
        velocity = { 0,0,0 };
        Character::UpdateTransform();
        break;
    }

    // 物理移動 (Component計算結果の反映)
    Character::UpdateVelocity(dt);

    Character::ApplyStageConstraint(this->stageLimitRadius);

    Character::UpdateTransform();
    Actor::Update(dt);
}

void Player::Render(ModelRenderer* renderer)
{
    Actor::Render(renderer);
}

void Player::UpdateLocomotion(float dt)
{
    if (TryDodge()) return;

    if (locomotion && input) {
        auto out = input->GetOutput();
        locomotion->SetMoveInput(out.move);
    }

    SyncLocomotionAnimation();
    if (input)
    {
        // 弱攻撃
        if (input->ConsumeBuffered(InputActionComponent::ActionType::AttackLight, 10)) {
            // ダッシュ中ならダッシュ攻撃
            int gait = locomotion ? locomotion->GetGaitIndex() : 0;
            if (gait >= 2) { // Jog or Run
                PlayAction(10); // [10] = DashAttack
            }
            else {
                PlayAction(0);  // [0] = Light1
            }
            return;
        }
        // 強攻撃
        if (input->ConsumeBuffered(InputActionComponent::ActionType::AttackHeavy, 12)) {
            PlayAction(7);      // [7] = Heavy1
            return;
        }
    }
}

void Player::SyncLocomotionAnimation()
{
    if (!locomotion || !animator) return;
    int gait = locomotion->GetGaitIndex();
    int animId = Idle;

    if (locomotion->IsTurningInPlace()) {
        animId = (locomotion->GetLastTurnSign() > 0) ? Idle_Trun_R90 : Idle_Trun_L90;
    }
    else if (gait == 0) animId = Idle;
    else if (gait == 1) animId = Walk_Front;
    else if (gait == 2) animId = Jogging_F;
    else if (gait == 3) animId = Run;

    animator->PlayBase(animId, true, 0.2f, 1.0f);
}

void Player::UpdateAction(float dt)
{
    // キャンセル回避
    float t01 = (runner) ? runner->GetTimeNormalized() : 0.0f;
    if (t01 >= currentActionData.cancelStart) {
        if (TryDodge()) return;
    }

    if (!runner || !animator || !sequencer) return;

    float currentTime = runner->GetTimeSeconds();
    t01 = runner->GetTimeNormalized();

    animator->SetActionTime(currentTime);

    // 攻撃中はLocomotionの移動を停止(減衰)
    if (locomotion) {
        locomotion->StopMovement();
    }

    // 移動処理 (ルートモーション + マグネティズム)
    // マグネティズム更新 (回転補正付き)
    RotateToNearestEnemyInstant();
    UpdateMagnetism(dt);

    DirectX::XMFLOAT3 finalPos = GetPosition();

    // マグネティズムが効いていない時だけルートモーションを加算
    if (!isMagnetismActive && animator)
    {
        const auto& rootDelta = animator->GetRootMotionDelta();
        if (!std::isnan(rootDelta.x) && !std::isnan(rootDelta.z)) {
            finalPos.x += rootDelta.x;
            finalPos.z += rootDelta.z;
        }
    }
    SetPosition(finalPos);

    // コンボ遷移判定
    if (t01 >= currentActionData.inputStart && t01 <= currentActionData.inputEnd)
    {
        if (currentActionData.nextLight != -1 && input &&
            input->ConsumeBuffered(InputActionComponent::ActionType::AttackLight, 10)) {
            reservedActionIdx = currentActionData.nextLight;
        }
        if (currentActionData.nextHeavy != -1 && input &&
            input->ConsumeBuffered(InputActionComponent::ActionType::AttackHeavy, 12)) {
            reservedActionIdx = currentActionData.nextHeavy;
        }
    }

    // 遷移実行
    if (t01 >= currentActionData.comboStart)
    {
        if (reservedActionIdx != -1) {
            PlayAction(reservedActionIdx);
            return;
        }
    }

    // 終了
    if (currentTime >= runner->GetClipLength())
    {
        state = State::Locomotion;
        if (locomotion) locomotion->StopMovement();
        animator->StopAction(0.2f);
    }

    // Collider同期
    if (auto collider = GetComponent<ColliderComponent>()) {
        int currentFrame = static_cast<int>(runner->GetTimeSeconds() * 60.0f);
        collider->SyncFromSequencer(sequencer.get(), currentFrame);
    }
}

void Player::UpdateMagnetism(float dt) {
    auto target = EnemyManager::Instance().GetNearestEnemy(GetPosition(), 1000.0f);

    if (!target) {
        isMagnetismActive = false;
        return;
    }

    XMVECTOR VMy = XMLoadFloat3(&position);
    XMVECTOR VEn = XMLoadFloat3(&target->GetPosition());
    XMVECTOR VDir = XMVectorSetY(VEn - VMy, 0.0f);

    float distToEnemy = XMVectorGetX(XMVector3Length(VDir));

    // 2. 向きの即時同期（旋回）
    // 敵が存在する限り、距離に関わらず1フレームで angle.y を敵の方向へ書き換えます。
    // 0.0001fはゼロ除算を避けるための計算上の安全策であり、仕様としての距離制限ではありません。
    if (distToEnemy > 0.0001f) {
        angle.y = std::atan2(XMVectorGetX(VDir), XMVectorGetZ(VDir));
    }

    // 3. 吸着移動の実行
    // 速度設定がある場合、理想距離などの停止判定を介さず、どの距離からでもターゲットへ吸い付きます。
    if (currentActionData.magnetismSpeed > 0.0f) {
        isMagnetismActive = true;
        float step = currentActionData.magnetismSpeed * dt;

        // std::min の使用禁止に従い、三項演算子で移動量を制御します。
        // ターゲットを通り過ぎないように、現在の距離と移動ステップを比較します。
        float moveDist = (step < distToEnemy) ? step : distToEnemy;

        XMVECTOR VNorm = XMVector3Normalize(VDir);
        position.x += XMVectorGetX(VNorm) * moveDist;
        position.z += XMVectorGetZ(VNorm) * moveDist;
    }
    else {
        isMagnetismActive = false;
    }
}

void Player::RotateToNearestEnemyInstant() {
    auto target = EnemyManager::Instance().GetNearestEnemy(GetPosition(), 50.0f);
    if (target) {
        float dx = target->GetPosition().x - position.x;
        float dz = target->GetPosition().z - position.z;
        if (dx * dx + dz * dz > 0.001f) {
            angle.y = std::atan2(dx, dz);
        }
    }
}

// -------------------------------------------------------------------------
// アクション開始
// -------------------------------------------------------------------------
void Player::PlayAction(int actionNodeIndex)
{
    if (actionNodeIndex < 0 || actionNodeIndex >= (int)actionDatabase.size()) return;

    currentActionIdx = actionNodeIndex;
    currentActionData = actionDatabase[actionNodeIndex];
    reservedActionIdx = -1;

    // 初動で向き直り
    RotateToNearestEnemyInstant();


    // マグネティズム起動判定 (至近距離対応版)
    isMagnetismActive = false;
    if (currentActionData.magnetismSpeed > 0.0f) {
        float range = currentActionData.magnetismRange;
        auto target = EnemyManager::Instance().GetNearestEnemy(GetPosition(), range);
        if (target) {
            XMVECTOR VMy = XMLoadFloat3(&position);
            XMVECTOR VEn = XMLoadFloat3(&target->GetPosition());
            XMVECTOR VDir = VEn - VMy;
            VDir = XMVectorSetY(VDir, 0.0f);

            float dist = XMVectorGetX(XMVector3Length(VDir));

            // ★修正: 理想距離 (1.2m) より遠ければ、例え1.3mでも発動させる
            float idealDist = 0.2f;
            if (dist > idealDist) {
                XMVECTOR VDest = VEn - XMVector3Normalize(VDir) * idealDist;
                VDest = XMVectorSetY(VDest, position.y);

                XMStoreFloat3(&magTargetPos, VDest);
                isMagnetismActive = true;
                magnetismTimer = 0.0f;

                // 時間計算 (最低0.1秒はかける)
                magnetismDuration = ((dist - idealDist) / currentActionData.magnetismSpeed);
                if (magnetismDuration < 0.1f) magnetismDuration = 0.1f;
                // 上限キャップ (0.4秒以上はかけない)
                if (magnetismDuration > 0.4f) magnetismDuration = 0.4f;
            }
        }
    }

    state = State::Action;

    // タイムライン設定
    int animId = currentActionData.animIndex;
    if (animId < (int)gameplayData.timelines.size()) {
        sequencer->GetItemsMutable() = gameplayData.timelines[animId];
    }
    else {
        sequencer->GetItemsMutable().clear();
    }
    if (animId < (int)gameplayData.curves.size()) {
        sequencer->SetCurveSettings(gameplayData.curves[animId]);
    }
    else {
        sequencer->SetCurveSettings({});
    }

    // 再生
    float duration = 1.0f;
    if (auto model = GetModelRaw()) {
        const auto& anims = model->GetAnimations();
        if (animId < (int)anims.size()) {
            duration = anims[animId].secondsLength;
        }
    }
    runner->SetClipLength(duration);
    runner->SetTimeSeconds(0.0f);
    runner->SetLoop(false);
    runner->SetPlaySpeed(currentActionData.animSpeed);
    runner->Play();

    animator->PlayAction(animId, false, 0.1f, true);
}

bool Player::TryDodge()
{
    if (!input || !runner) return false;

    if (input->ConsumeBuffered(InputActionComponent::ActionType::Dodge, 10))
    {
        if (dodgeGauge && !dodgeGauge->TryConsumeStamina()) return false;
        input->SetCooldown(InputActionComponent::ActionType::Dodge, 10);

        auto moveInput = input->GetOutput().move;
        float lenSq = moveInput.x * moveInput.x + moveInput.y * moveInput.y;

        int animId = Dodge_Back;
        if (lenSq > 0.01f) {
            animId = Dodge_Front;
            const auto& cam = Camera::Instance();
            XMFLOAT3 f = cam.GetFront(); XMFLOAT3 r = cam.GetRight();
            f.y = 0; r.y = 0;
            XMVECTOR F = XMVector3Normalize(XMLoadFloat3(&f));
            XMVECTOR R = XMVector3Normalize(XMLoadFloat3(&r));
            XMVECTOR D = XMVectorScale(R, moveInput.x) + XMVectorScale(F, moveInput.y);

            XMFLOAT3 dir; XMStoreFloat3(&dir, D);
            if (fabs(dir.x) > 0.001f || fabs(dir.z) > 0.001f) {
                angle.y = atan2f(dir.x, dir.z);
            }
        }

        state = State::Dodge;
        // 回避中は無敵等はここで管理しても良い

        //HologramManager::Instance().Spawn(shared_from_this(), 0.35f);
        //HologramManager::Instance().Spawn(shared_from_this(), 0.50f);

        int motionId = (animId == Dodge_Back) ? Idle : Run_Fast;

        runner->SetClipLength(0.4f);
        runner->SetTimeSeconds(0.0f);
        runner->SetLoop(false);
        runner->Play();

        animator->PlayBase(motionId, true, 0.1f, 1.0f);
        animator->StopAction(0.1f);

        return true;
    }
    return false;
}

bool Player::IsAttackAction(int animIndex) const {
    return (animIndex >= Combo1);
}
void Player::AddCombo() {
    comboCount++;
    comboTimer = COMBO_TIMEOUT;
}
void Player::OnGUI() {
    if (ImGui::Begin("Player Debug")) {
        const char* stateNames[] = { "Locomotion", "Action", "Dodge", "Jump", "Damage", "Dead" };
        ImGui::Text("State: %s", stateNames[(int)state]);
        if (state == State::Action) {
            ImGui::Text("Node: %d (AnimID: %d)", currentActionIdx, currentActionData.animIndex);
            if (runner) {
                float t = runner->GetTimeNormalized();
                ImGui::ProgressBar(t, ImVec2(-1, 0), "Time");
            }
        }
        if (locomotion) locomotion->OnGUI();
        if (animator) animator->OnGUI();
        if (dodgeGauge) dodgeGauge->OnGUI();
    }
    ImGui::End();
}

void Player::OnTriggerEnter(Actor* other, const Collider* selfCol, const Collider* otherCol)
{
    Character* otherChar = dynamic_cast<Character*>(other);

    // ---------------------------------------------------------
    // パターンA: 自分が攻撃して、相手(敵)の体に当たった
    // ---------------------------------------------------------
    if (selfCol->attribute == ColliderAttribute::Attack &&
        otherCol->attribute == ColliderAttribute::Body)
    {
        if (hitList.find(other) != hitList.end()) {
            return; // すでに叩いているので何もしない
        }

        if (otherChar)
        {
            hitList.insert(other);

            OutputDebugStringA("[HIT] Player Attack hit Enemy!\n");

            // 敵へのダメージ (例: 1ダメージ, 無敵0.2秒)
            otherChar->ApplyDamage(1, 0.2f);

            // プレイヤーのコンボ加算
            this->AddCombo();

            auto sequencer = GetComponent<TimelineSequencerComponent>();
            if (sequencer)
            {
                // 現在再生中のシェイク設定（Type 4）を取得
                const GESequencerItem* shakeItem = sequencer->GetActiveShakeItem();

                if (shakeItem)
                {
                    // A. ヒットストップの発動 (時間の停止)
                    if (auto runner = sequencer->GetRunner())
                    {
                        runner->RequestHitStop(
                            shakeItem->shake.hitStopDuration,
                            shakeItem->shake.timeScale
                        );
                    }

                    // B. カメラシェイクの発動 (衝撃的な揺れ)
                    if (auto camCtrl = CameraController::Instance())
                    {
                        camCtrl->AddShake(
                            shakeItem->shake.amplitude,
                            shakeItem->shake.duration,
                            shakeItem->shake.frequency,
                            shakeItem->shake.decay
                        );
                    }

                    OutputDebugStringA("[HIT] Success: Applied Timeline Shake/Hitstop\n");
                }
            }
        }
    }
  
}






void Player::OnDamaged()
{
    // 親クラスの処理（無敵時間のセット等はここで行われる）
    Character::OnDamaged();

    // HPが尽きたら死亡へ
    if (health <= 0)
    {
        OnDead();
        return;
    }

    // 生きていれば被弾モーション再生
    // (回避中やスーパーアーマー中はステートを変えない判定を入れても良い)
    if (state != State::Dead)
    {
        state = State::Damage;

        // 被弾アニメーション再生 (例: Damage_Front_Small)
        // 配列外参照を防ぐためインデックスチェック推奨
        int damageAnim = Damage_Front_Small;

        // ランナーやロコモーションを止める
        if (runner) runner->Stop();
        if (locomotion) locomotion->StopMovement();

        if (animator)
        {
            animator->StopAction();
            animator->PlayBase(damageAnim, false, 0.1f); // ループしない
        }

        // 硬直時間をセット (アニメーションの長さを取得するのがベストだが、仮で0.5秒)
        stateTimer = 0.5f;
        if (auto model = GetModelRaw()) {
            const auto& anims = model->GetAnimations();
            if (damageAnim < (int)anims.size()) {
                stateTimer = anims[damageAnim].secondsLength;
            }
        }
    }
}

void Player::OnDead()
{
    // 既に死んでいたら何もしない
    if (state == State::Dead) return;

    state = State::Dead;
    OutputDebugStringA("[Player] YOU DIED.\n");

    // 全動作停止
    if (runner) runner->Stop();
    if (locomotion) locomotion->StopMovement();

    // コライダーを無効化（死体蹴り防止）
    if (auto collider = GetComponent<ColliderComponent>()) {
        collider->SetEnabled(false);
    }

    // 死亡アニメーション
    if (animator)
    {
        animator->StopAction();
        animator->PlayBase(Die, false, 0.1f); // Dieアニメーション
    }
}