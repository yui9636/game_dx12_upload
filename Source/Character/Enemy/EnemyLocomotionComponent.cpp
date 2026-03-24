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
        float y = center.y + 0.1f;

        XMVECTOR worldPos = XMVectorSet(x, y, z, 1.0f);
        XMVECTOR clipPos = XMVector3Transform(worldPos, vp);

        if (XMVectorGetW(clipPos) < 0.1f) {
            continue;
        }
        anyPointInFront = true;

        XMVECTOR ndc = clipPos / XMVectorGetW(clipPos);
        float sx = (XMVectorGetX(ndc) + 1.0f) * 0.5f * ImGui::GetIO().DisplaySize.x;
        float sy = (1.0f - XMVectorGetY(ndc)) * 0.5f * ImGui::GetIO().DisplaySize.y;

        points.push_back(ImVec2(sx, sy));
    }

    if (anyPointInFront && points.size() > 1) {
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
    // ---------------------------------------------------
    float moveX = 0.0f;
    float moveZ = 0.0f;

    if (isMoving)
    {
        XMFLOAT3 myPos = character->GetPosition();

        float dx = targetPosition.x - myPos.x;
        float dz = targetPosition.z - myPos.z;
        float distSq = dx * dx + dz * dz;

        if (distSq > arrivalDistance * arrivalDistance)
        {
            float dist = std::sqrt(distSq);
            moveX = dx / dist;
            moveZ = dz / dist;
        }
        else
        {
            isMoving = false;
        }
    }

    // ---------------------------------------------------
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

    character->Move(moveX, moveZ, currentSpeed);

    // ---------------------------------------------------
    // ---------------------------------------------------
    if (currentSpeed > 0.1f && (moveX != 0.0f || moveZ != 0.0f))
    {
        float turnRad = turnSpeed * (3.1415926535f / 180.0f);
        character->Turn(dt, moveX, moveZ, turnRad);
    }

    // ---------------------------------------------------
    // ---------------------------------------------------
    XMFLOAT3 nextPos = character->GetPosition();

    float distFromCenterSq = nextPos.x * nextPos.x + nextPos.z * nextPos.z;

    if (distFromCenterSq > arenaRadius * arenaRadius)
    {
        float distFromCenter = std::sqrt(distFromCenterSq);

        float ratio = arenaRadius / distFromCenter;

        nextPos.x *= ratio;
        nextPos.z *= ratio;

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

        static bool showArena = true;
        ImGui::Checkbox("Show Arena Radius", &showArena);
        ImGui::DragFloat("Arena Radius", &arenaRadius, 0.5f);

        if (showArena) {
            DrawCircle3D({ 0, 0, 0 }, arenaRadius, IM_COL32(0, 100, 255, 200));
        }

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
