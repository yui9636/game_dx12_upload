#include "LocomotionComponent.h"
#include "Character.h"
#include "Animator/AnimatorComponent.h"
#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

using namespace DirectX;

// ----------------------------------------------------------------------------
// ユーティリティ (変更なし)
// ----------------------------------------------------------------------------
float LocomotionComponent::Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float LocomotionComponent::WrapAngle180(float deg) {
    float x = deg;
    while (x > 180.0f) x -= 360.0f;
    while (x < -180.0f) x += 360.0f;
    return x;
}

int LocomotionComponent::AngleDegreeToIndex8(float deg) {
    float d = WrapAngle180(deg);
    float shifted = d + 22.5f;
    while (shifted < 0.0f)   shifted += 360.0f;
    while (shifted >= 360.0f) shifted -= 360.0f;
    int idx = static_cast<int>(shifted / 45.0f);
    if (idx < 0) idx = 0; if (idx > 7) idx = 7;
    return idx;
}

// ----------------------------------------------------------------------------
// メイン処理
// ----------------------------------------------------------------------------

void LocomotionComponent::Start()
{
    if (auto owner = GetActor()) {
        characterWk = std::dynamic_pointer_cast<Character>(owner);
        animatorWk = owner->GetComponent<AnimatorComponent>();
    }
    currentGaitIndex = 0;
    currentSpeed = 0.0f;

    // ★パニグレ/鳴潮風チューニング
    // 初速は爆速、止まるのも一瞬。でも最高速付近では制御しやすい。
    acceleration = 60.0f;   // 中～高速域の伸び
    launchBoost = 5.0f;     // ★ここを上げると初速がドン！と出る
    deceleration = 2000.0f; // ★ほぼ慣性なしで止まる

    walkMaxSpeed = 60.0f;
    joggingMaxSpeed = 160.0f;
    runMaxSpeed = 380.0f;

    // ストライド設定
    animStrideWalk = walkMaxSpeed * animDurationWalk;
    animStrideJog = joggingMaxSpeed * animDurationJog;
    animStrideRun = runMaxSpeed * animDurationRun;
}

void LocomotionComponent::SetMoveInput(const DirectX::XMFLOAT2& moveInput)
{
    inputVector = moveInput;

    const DirectX::XMFLOAT3 cr = Camera::Instance().GetRight();
    const DirectX::XMFLOAT3 cf = Camera::Instance().GetFront();
    float rx = cr.x, rz = cr.z;
    float fx = cf.x, fz = cf.z;

    float lr = std::sqrt(rx * rx + rz * rz); if (lr > 0.0f) { rx /= lr; rz /= lr; }
    float lf = std::sqrt(fx * fx + fz * fz); if (lf > 0.0f) { fx /= lf; fz /= lf; }

    float wx = rx * moveInput.x + fx * moveInput.y;
    float wz = rz * moveInput.x + fz * moveInput.y;

    float s = std::sqrt(wx * wx + wz * wz);
    inputStrength = Clamp01(s);

    if (inputStrength > 0.001f) {
        float inv = 1.0f / s;
        moveDirection = { wx * inv, wz * inv };
    }
}

void LocomotionComponent::StopMovement()
{
    inputVector = { 0.0f, 0.0f };
    inputStrength = 0.0f;
}

void LocomotionComponent::Update(float dt)
{
    auto character = characterWk.lock();
    if (!character) return;

    UpdateGait(inputStrength);
    UpdatePhysics(dt);
    UpdateRotation(dt);
    UpdateDirectionIndex();
    SyncAnimationSpeed();
}

void LocomotionComponent::UpdateGait(float strength)
{
    int target = 0;
    if (strength >= thresholdRunEnter) target = 3;
    else if (strength >= thresholdJogEnter) target = 2;
    else if (strength >= thresholdWalkEnter) target = 1;

    switch (currentGaitIndex)
    {
    case 3:
        if (strength < thresholdRunEnter - 0.05f) currentGaitIndex = 2;
        break;
    case 2:
        if (strength >= thresholdRunEnter) currentGaitIndex = 3;
        else if (strength < thresholdJogEnter - 0.05f) currentGaitIndex = 1;
        break;
    case 1:
        if (strength >= thresholdJogEnter) currentGaitIndex = 2;
        else if (strength < thresholdWalkEnter - 0.02f) currentGaitIndex = 0;
        break;
    default:
        currentGaitIndex = target;
        break;
    }
}

void LocomotionComponent::UpdatePhysics(float dt)
{
    auto character = characterWk.lock();
    if (!character) return;

    float targetSpeed = 0.0f;
    if (currentGaitIndex == 1) targetSpeed = walkMaxSpeed;
    else if (currentGaitIndex == 2) targetSpeed = joggingMaxSpeed;
    else if (currentGaitIndex == 3) targetSpeed = runMaxSpeed;

    targetSpeed *= inputStrength;

    // ★ロケットスタート & 急制動ロジック
    if (currentSpeed < targetSpeed) {
        // 加速時
        float currentAcc = acceleration;
        if (currentSpeed < runMaxSpeed * 0.3f) {
            currentAcc *= launchBoost;
        }
        currentSpeed += currentAcc * dt;
        if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
    }
    else {
        // 減速時
        currentSpeed -= deceleration * dt;
        if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
    }

    // =========================================================================
    // ★修正ポイント: Move() ではなく velocity を直接書き換える
    // =========================================================================
    // Character::Move() を使うと、Character側の摩擦(friction=5.0)が適用されてしまい、
    // どれだけ deceleration を上げても滑ってしまいます。
    // ここで velocity.x / z を直接計算値で上書きすることで、物理演算を完全に制御下に置きます。

    // Y軸（重力）は維持する
    float vy = character->velocity.y;

    if (inputStrength > 0.001f) {
        // 移動中
        character->velocity.x = moveDirection.x * currentSpeed;
        character->velocity.z = moveDirection.y * currentSpeed;
    }
    else {
        // 停止中
        // inputStrengthが0でも、慣性（currentSpeed）が残っていれば滑らせる
        if (currentSpeed > 0.0f) {
            // ここで deceleration 2000.0 の効果が発揮され、一瞬でゼロになる
            if (currentSpeed < 0.0f) currentSpeed = 0.0f;

            character->velocity.x = moveDirection.x * currentSpeed;
            character->velocity.z = moveDirection.y * currentSpeed;
        }
        else {
            // 完全停止
            currentSpeed = 0.0f;
            character->velocity.x = 0.0f;
            character->velocity.z = 0.0f;
        }
    }

    // 重力を戻す
    character->velocity.y = vy;

    // Character側の物理計算（加速・減速）が重複して掛からないようにリセットしておく
    character->Move(0.0f, 0.0f, 0.0f);
}

void LocomotionComponent::UpdateRotation(float dt)
{
    auto character = characterWk.lock();
    if (!character) return;

    // リーン角度の減衰 (直立に戻ろうとする)
    currentLeanAngle = currentLeanAngle * (1.0f - 8.0f * dt); // 戻りも速く

    if (currentSpeed > 0.1f && inputStrength > 0.01f)
    {
        float turnRad = turnSpeed * (3.1415926535f / 180.0f);
        character->Turn(dt, moveDirection.x, moveDirection.y, turnRad);
        isTurningInPlace = false;

        // プロシージャル・リーン (旋回時の傾き)
        float currentYaw = character->GetAngle().y;
        float targetYaw = std::atan2(moveDirection.x, moveDirection.y);
        float diff = WrapAngle180((targetYaw - currentYaw) * (180.0f / 3.1415926535f));

        // 速度が出ているほど傾く + 急旋回で傾く
        float targetLean = -diff * (currentSpeed / runMaxSpeed) * 2.0f;

        // 傾きの反応速度も上げる
        currentLeanAngle += (targetLean - currentLeanAngle) * 10.0f * dt;
    }
    else
    {
        if (inputStrength > 0.1f) {
            float currentYaw = character->GetAngle().y;
            float targetYaw = std::atan2(moveDirection.x, moveDirection.y);
            float diff = WrapAngle180((targetYaw - currentYaw) * (180.0f / 3.1415926535f));

            if (std::abs(diff) > 45.0f) {
                isTurningInPlace = true;
                lastTurnSign = (diff > 0) ? 1 : -1;
                float slowTurnRad = turnSpeed * 0.5f * (3.1415926535f / 180.0f);
                character->Turn(dt, moveDirection.x, moveDirection.y, slowTurnRad);
            }
            else {
                isTurningInPlace = false;
                lastTurnSign = 0;
            }
        }
        else {
            isTurningInPlace = false;
            lastTurnSign = 0;
        }
    }
}

void LocomotionComponent::UpdateDirectionIndex()
{
    auto character = characterWk.lock();
    if (!character) return;

    if (currentSpeed > 0.05f)
    {
        float vx = moveDirection.x;
        float vz = moveDirection.y;
        float deg = std::atan2(vx, vz) * (180.0f / 3.1415926535f);

        float charYawDeg = character->GetAngle().y * (180.0f / 3.1415926535f);
        float localDeg = WrapAngle180(deg - charYawDeg);

        int candidate = AngleDegreeToIndex8(localDeg);
        float center = candidate * 45.0f;
        float diff = std::abs(WrapAngle180(localDeg - center));

        if (diff < switchHalfAngleDegree) {
            directionIndex8 = candidate;
        }
        lastValidDirectionIndex8 = directionIndex8;
    }
    else
    {
        directionIndex8 = lastValidDirectionIndex8;
    }
}

void LocomotionComponent::SyncAnimationSpeed()
{
    auto animator = animatorWk.lock();
    if (!animator) return;

    float warpFactor = 1.0f;
    float stride = 1.0f;
    float duration = 1.0f;

    if (currentGaitIndex == 1) { // Walk
        stride = animStrideWalk;
        duration = animDurationWalk;
    }
    else if (currentGaitIndex == 2) { // Jog
        stride = animStrideJog;
        duration = animDurationJog;
    }
    else if (currentGaitIndex == 3) { // Run
        stride = animStrideRun;
        duration = animDurationRun;
    }

    if (currentGaitIndex > 0 && stride > 0.001f)
    {
        // 足滑り防止
        warpFactor = (currentSpeed * duration) / stride;

        // キビキビ動くキャラの場合、再生速度の上限を緩める
        if (warpFactor < 0.5f) warpFactor = 0.5f;
        if (warpFactor > 3.0f) warpFactor = 3.0f; // 爆速スタート時は3倍速くらい許容する

        if (currentSpeed < 5.0f) warpFactor = 1.0f;
    }

    int currentAnimId = animator->GetBaseAnimIndex();
    if (currentAnimId >= 0)
    {
        animator->PlayBase(currentAnimId, true, 0.15f, warpFactor); // ブレンドも少し早めに
    }
}

void LocomotionComponent::OnGUI()
{
    if (ImGui::CollapsingHeader("Locomotion Stats"))
    {
        ImGui::Text("Gait: %d", currentGaitIndex);
        ImGui::Text("Phys Speed: %.2f", currentSpeed);
        ImGui::Text("Input: %.2f", inputStrength);

        ImGui::SeparatorText("Responsiveness");
        ImGui::DragFloat("Base Accel", &acceleration, 1.0f, 1.0f, 200.0f);
        ImGui::DragFloat("Launch Boost", &launchBoost, 0.1f, 1.0f, 10.0f);
        ImGui::DragFloat("Brake Force", &deceleration, 10.0f, 10.0f, 5000.0f);

        ImGui::SeparatorText("Speed Warping");
        ImGui::DragFloat("Stride Walk", &animStrideWalk, 1.0f, 10.0f, 300.0f);
        ImGui::DragFloat("Stride Jog", &animStrideJog, 1.0f, 10.0f, 500.0f);
        ImGui::DragFloat("Stride Run", &animStrideRun, 1.0f, 10.0f, 800.0f);
    }
}