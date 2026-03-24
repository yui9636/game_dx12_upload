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

            XMVECTOR right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot);
            XMVECTOR up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot);

            // =========================================================
            // =========================================================
            if (io.MouseDown[ImGuiMouseButton_Right] && !io.KeyAlt) {
                ctrl.yaw += io.MouseDelta.x * ctrl.rotateSpeed;
                ctrl.pitch += io.MouseDelta.y * ctrl.rotateSpeed;
                ctrl.pitch = std::clamp(ctrl.pitch, -1.55f, 1.55f);

                float speed = ctrl.moveSpeed * io.DeltaTime;
                if (io.KeyShift) speed *= 3.0f;

                if (ImGui::IsKeyDown(ImGuiKey_W)) pos += forward * speed;
                if (ImGui::IsKeyDown(ImGuiKey_S)) pos -= forward * speed;
                if (ImGui::IsKeyDown(ImGuiKey_D)) pos += right * speed;
                if (ImGui::IsKeyDown(ImGuiKey_A)) pos -= right * speed;
                if (ImGui::IsKeyDown(ImGuiKey_E)) pos += up * speed;
                if (ImGui::IsKeyDown(ImGuiKey_Q)) pos -= up * speed;

                rot = XMQuaternionRotationRollPitchYaw(ctrl.pitch, ctrl.yaw, 0.0f);
                forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot);
                right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot);
                up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot);

                XMStoreFloat4(&trans.localRotation, rot);
            }

            // =========================================================
            // =========================================================
            if (io.MouseDown[ImGuiMouseButton_Middle]) {
                float panSpeed = ctrl.moveSpeed * io.DeltaTime * 0.5f;
                pos -= right * io.MouseDelta.x * panSpeed;
                pos += up * io.MouseDelta.y * panSpeed;
            }

            // =========================================================
            // =========================================================

            if (io.MouseWheel != 0.0f) {
                float zoomDist = io.MouseWheel * (ctrl.moveSpeed * 0.5f);
                pos += forward * zoomDist;
                trans.isDirty = true;
            }

            XMStoreFloat3(&trans.localPosition, pos);
            XMStoreFloat4(&trans.localRotation, rot);
            trans.isDirty = true;


        }
    }
}
