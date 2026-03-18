#include "FreeCameraSystem.h"
#include "Registry/Registry.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "Component/TransformComponent.h"
#include "Component/CameraBehaviorComponent.h"
#include <imgui.h>
#include <algorithm>

using namespace DirectX;

void FreeCameraSystem::Update(Registry& registry, float dt) {
    auto archetypes = registry.GetAllArchetypes();
    Signature targetSig = CreateSignature<TransformComponent, CameraFreeControlComponent>();

    for (auto* archetype : archetypes) {
        if (!SignatureMatches(archetype->GetSignature(), targetSig)) continue;

        auto* transCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* ctrlCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraFreeControlComponent>());

        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            auto& trans = *static_cast<TransformComponent*>(transCol->Get(i));
            auto& ctrl = *static_cast<CameraFreeControlComponent*>(ctrlCol->Get(i));

            ImGuiIO& io = ImGui::GetIO();
            XMVECTOR pos = XMLoadFloat3(&trans.localPosition);

            XMVECTOR rot = XMQuaternionRotationRollPitchYaw(ctrl.pitch, ctrl.yaw, 0.0f);
            XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot);

            // ズーム（ホイール）
            XMVECTOR right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot);
            XMVECTOR up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot);

            // =========================================================
            // 1. 右クリック：飛行移動 & 視点回転
            // =========================================================
            // ★ Altキーが押されていない時だけ視点回転（Altはズーム用に予約）
            if (io.MouseDown[ImGuiMouseButton_Right] && !io.KeyAlt) {
                ctrl.yaw += io.MouseDelta.x * ctrl.rotateSpeed;
                ctrl.pitch += io.MouseDelta.y * ctrl.rotateSpeed;
                ctrl.pitch = std::clamp(ctrl.pitch, -1.55f, 1.55f);

                float speed = ctrl.moveSpeed * io.DeltaTime;
                if (io.KeyShift) speed *= 3.0f; // Shiftで加速

                if (ImGui::IsKeyDown(ImGuiKey_W)) pos += forward * speed;
                if (ImGui::IsKeyDown(ImGuiKey_S)) pos -= forward * speed;
                if (ImGui::IsKeyDown(ImGuiKey_D)) pos += right * speed;
                if (ImGui::IsKeyDown(ImGuiKey_A)) pos -= right * speed;
                if (ImGui::IsKeyDown(ImGuiKey_E)) pos += up * speed;
                if (ImGui::IsKeyDown(ImGuiKey_Q)) pos -= up * speed;

                // 回転後のベクトルを再計算
                rot = XMQuaternionRotationRollPitchYaw(ctrl.pitch, ctrl.yaw, 0.0f);
                forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot);
                right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot);
                up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot);

                XMStoreFloat4(&trans.localRotation, rot);
            }

            // =========================================================
            // 2. 中ボタン：パンニング（平行移動）
            // =========================================================
            if (io.MouseDown[ImGuiMouseButton_Middle]) {
                float panSpeed = ctrl.moveSpeed * io.DeltaTime * 0.5f;
                pos -= right * io.MouseDelta.x * panSpeed;
                pos += up * io.MouseDelta.y * panSpeed;
            }

            // =========================================================
            // 3. ズーム操作（最強の2段構え！）
            // =========================================================

            if (io.MouseWheel != 0.0f) {
                // Unity/UE風：ホイール1回で大きく移動するように倍率を調整
                float zoomDist = io.MouseWheel * (ctrl.moveSpeed * 0.5f);
                pos += forward * zoomDist;
                trans.isDirty = true; // TransformSystemに「動いたよ！」と伝える
            }

            // 最後に位置と回転を保存
            XMStoreFloat3(&trans.localPosition, pos);
            XMStoreFloat4(&trans.localRotation, rot);
            trans.isDirty = true;


        }
    }
}