#include "EnemyLocomotionComponent.h"
#include"Character/Character.h"
#include "Actor/Actor.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "Camera/Camera.h"

using namespace DirectX;

static void DrawCircle3D(const DirectX::XMFLOAT3& center, float radius, ImU32 color, float thickness = 2.0f)
{
    auto* drawList = ImGui::GetBackgroundDrawList();
    const auto& camera = Camera::Instance();

    // ビュー・プロジェクション行列の取得
    XMMATRIX view = XMLoadFloat4x4(&camera.GetView());
    XMMATRIX proj = XMLoadFloat4x4(&camera.GetProjection());
    XMMATRIX vp = view * proj;

    const int segments = 64;
    std::vector<ImVec2> points;
    bool anyPointInFront = false;

    for (int i = 0; i <= segments; ++i)
    {
        float angle = (float)i * 6.2831853f / (float)segments;
        float x = center.x + cosf(angle) * radius;
        float z = center.z + sinf(angle) * radius;
        float y = center.y + 0.1f; // 地面より少し浮かせる

        XMVECTOR worldPos = XMVectorSet(x, y, z, 1.0f);
        XMVECTOR clipPos = XMVector3Transform(worldPos, vp);

        // カメラの後ろにある点は描画しないためのチェック
        if (XMVectorGetW(clipPos) < 0.1f) {
            continue;
        }
        anyPointInFront = true;

        // スクリーン座標変換
        XMVECTOR ndc = clipPos / XMVectorGetW(clipPos);
        float sx = (XMVectorGetX(ndc) + 1.0f) * 0.5f * ImGui::GetIO().DisplaySize.x;
        float sy = (1.0f - XMVectorGetY(ndc)) * 0.5f * ImGui::GetIO().DisplaySize.y;

        points.push_back(ImVec2(sx, sy));
    }

    // 線をつなぐ
    if (anyPointInFront && points.size() > 1) {
        // 円として閉じたい場合は pointsの最初と最後がつながるように調整が必要ですが、
        // 簡易的に Polyline で描画します
        drawList->AddPolyline(points.data(), (int)points.size(), color, false, thickness);
    }
}

void EnemyLocomotionComponent::Start()
{
    if (auto owner = GetActor()) {
        characterWk = std::dynamic_pointer_cast<Character>(owner);
    }
    currentSpeed = 0.0f;
    isMoving = false;
}

// AIから「ここへ行け」と言われたらセットするだけ
void EnemyLocomotionComponent::MoveTo(const DirectX::XMFLOAT3& targetPos)
{
    targetPosition = targetPos;
    isMoving = true;
}

void EnemyLocomotionComponent::Stop()
{
    isMoving = false;
}

void EnemyLocomotionComponent::Update(float dt)
{
    auto character = characterWk.lock();
    if (!character) return;

    // ---------------------------------------------------
    // 1. 移動ベクトルの計算 (NavMeshなしの直進ロジック)
    // ---------------------------------------------------
    float moveX = 0.0f;
    float moveZ = 0.0f;

    if (isMoving)
    {
        XMFLOAT3 myPos = character->GetPosition();

        // Y軸を無視して方向を計算
        float dx = targetPosition.x - myPos.x;
        float dz = targetPosition.z - myPos.z;
        float distSq = dx * dx + dz * dz;

        // 到着判定 (距離の2乗で比較して平方根計算を節約)
        if (distSq > arrivalDistance * arrivalDistance)
        {
            // まだ着いていない -> 正規化して方向を得る
            float dist = std::sqrt(distSq);
            moveX = dx / dist;
            moveZ = dz / dist;
        }
        else
        {
            // 到着した
            isMoving = false;
        }
    }

    // ---------------------------------------------------
    // 2. 加減速と移動実行
    // ---------------------------------------------------
    float targetSpeed = isMoving ? moveSpeed : 0.0f;

    if (currentSpeed < targetSpeed) {
        currentSpeed += acceleration * dt;
        if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
    }
    else {
        currentSpeed -= deceleration * dt;
        if (currentSpeed < 0.0f) currentSpeed = 0.0f;
    }

    // Characterに移動命令 (方向 * 速度)
    // ※Character::Moveの実装に依存しますが、通常は (DirX, DirZ, Speed) を渡す想定
    character->Move(moveX, moveZ, currentSpeed);

    // ---------------------------------------------------
    // 3. 旋回 (移動している方向を向く)
    // ---------------------------------------------------
    if (currentSpeed > 0.1f && (moveX != 0.0f || moveZ != 0.0f))
    {
        // ラジアンに変換してTurnを呼ぶ
        float turnRad = turnSpeed * (3.1415926535f / 180.0f);
        character->Turn(dt, moveX, moveZ, turnRad);
    }

    // ---------------------------------------------------
    // 4. ★最重要: 円形ステージの外に出ないように補正
    // ---------------------------------------------------
    XMFLOAT3 nextPos = character->GetPosition();

    // 中心(0,0,0)からの距離を計算
    float distFromCenterSq = nextPos.x * nextPos.x + nextPos.z * nextPos.z;

    // 半径の2乗と比較
    if (distFromCenterSq > arenaRadius * arenaRadius)
    {
        // はみ出している！ -> 円周上に押し戻す
        float distFromCenter = std::sqrt(distFromCenterSq);

        // 補正比率 (半径 / 現在距離)
        // 例: 半径20mなのに22m地点にいたら、20/22倍の位置に戻す
        float ratio = arenaRadius / distFromCenter;

        nextPos.x *= ratio;
        nextPos.z *= ratio;

        // 補正した位置を適用
        character->SetPosition(nextPos);
        character->UpdateTransform();
    }
}

void EnemyLocomotionComponent::OnGUI()
{
    if (ImGui::CollapsingHeader("Enemy Locomotion"))
    {
        ImGui::Text("State: %s", isMoving ? "Moving" : "Stopped");
        ImGui::Text("Speed: %.2f", currentSpeed);
        ImGui::DragFloat("Max Speed", &moveSpeed, 0.1f);
        ImGui::DragFloat("Turn Speed", &turnSpeed, 1.0f);
        ImGui::DragFloat("Accel", &acceleration, 0.1f);
        ImGui::Separator();

        // --- 行動範囲の可視化設定 ---
        static bool showArena = true;
        ImGui::Checkbox("Show Arena Radius", &showArena);
        ImGui::DragFloat("Arena Radius", &arenaRadius, 0.5f);

        if (showArena) {
            // 中心(0,0,0)に行動限界の青い円を描画
            // ※ステージ中心が原点でない場合はここを調整してください
            DrawCircle3D({ 0, 0, 0 }, arenaRadius, IM_COL32(0, 100, 255, 200));
        }

        // デバッグ用: 強制移動テスト
        static float debugTarget[3] = { 0,0,0 };
        ImGui::InputFloat3("Debug Target", debugTarget);
        if (ImGui::Button("Go to Target")) {
            MoveTo({ debugTarget[0], debugTarget[1], debugTarget[2] });
        }
        if (ImGui::Button("Stop")) {
            Stop();
        }
    }
}