#include "FreeCameraSystem.h"
#include "Registry/Registry.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "Component/TransformComponent.h"
#include "Component/CameraBehaviorComponent.h"
#include <imgui.h>
#include <algorithm>

using namespace DirectX;
// FreeControlComponent を持つカメラを ImGui 入力に応じて操作する。
void FreeCameraSystem::Update(Registry& registry, float dt) {
    // 対象となるコンポーネント構成を作成します。
    auto archetypes = registry.GetAllArchetypes();
    Signature targetSig = CreateSignature<TransformComponent, CameraFreeControlComponent>();

    for (auto* archetype : archetypes) {
        // Transform と FreeControl を持たない Archetype は処理対象外です。
        if (!SignatureMatches(archetype->GetSignature(), targetSig)) continue;

        // Transform と操作用コンポーネントの列を取得します。
        auto* transCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* ctrlCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraFreeControlComponent>());

        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            // 同じ行にある Transform とカメラ操作情報を取得します。
            auto& trans = *static_cast<TransformComponent*>(transCol->Get(i));
            auto& ctrl = *static_cast<CameraFreeControlComponent*>(ctrlCol->Get(i));

            // ImGui の入力状態を取得します。
            ImGuiIO& io = ImGui::GetIO();

            // 現在のカメラ位置を SIMD ベクトルとして読み込みます。
            XMVECTOR pos = XMLoadFloat3(&trans.localPosition);

            // yaw / pitch から現在のカメラ回転を作ります。
            XMVECTOR rot = XMQuaternionRotationRollPitchYaw(ctrl.pitch, ctrl.yaw, 0.0f);

            // カメラのローカル軸を、現在の回転でワールド方向へ変換します。
            XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot);
            XMVECTOR right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot);
            XMVECTOR up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot);
// 右クリック中は、マウス移動で向き変更、WASD/EQ で移動します。
if (ctrl.isHovered && io.MouseDown[ImGuiMouseButton_Right] && !io.KeyAlt) {
                // マウス移動量を yaw / pitch に加算します。
                ctrl.yaw += io.MouseDelta.x * ctrl.rotateSpeed;
                ctrl.pitch += io.MouseDelta.y * ctrl.rotateSpeed;

                // 真上・真下を向きすぎて操作不能にならないよう pitch を制限します。
                ctrl.pitch = std::clamp(ctrl.pitch, -1.55f, 1.55f);

                // 基本移動速度を計算します。
                float speed = ctrl.moveSpeed * io.DeltaTime;

                // Shift 中は高速移動にします。
                if (io.KeyShift) speed *= 3.0f;

                // WASD で水平移動、E/Q で上下移動を行います。
                if (ImGui::IsKeyDown(ImGuiKey_W)) pos += forward * speed;
                if (ImGui::IsKeyDown(ImGuiKey_S)) pos -= forward * speed;
                if (ImGui::IsKeyDown(ImGuiKey_D)) pos += right * speed;
                if (ImGui::IsKeyDown(ImGuiKey_A)) pos -= right * speed;
                if (ImGui::IsKeyDown(ImGuiKey_E)) pos += up * speed;
                if (ImGui::IsKeyDown(ImGuiKey_Q)) pos -= up * speed;

                // 更新後の yaw / pitch から回転とローカル軸を作り直します。
                rot = XMQuaternionRotationRollPitchYaw(ctrl.pitch, ctrl.yaw, 0.0f);
                forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot);
                right = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot);
                up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot);

                // Transform に回転を反映します。
                XMStoreFloat4(&trans.localRotation, rot);
            }
// 中クリックドラッグ中は、現在のカメラ平面上でパン移動します。
if (ctrl.isHovered && io.MouseDown[ImGuiMouseButton_Middle]) {
                // マウス移動量をカメラの right / up 方向に変換します。
                float panSpeed = ctrl.moveSpeed * io.DeltaTime * 0.5f;
                pos -= right * io.MouseDelta.x * panSpeed;
                pos += up * io.MouseDelta.y * panSpeed;
            }
// マウスホイールで、カメラの前後方向へズーム移動します。
if (ctrl.isHovered && io.MouseWheel != 0.0f) {
                // ホイール量に応じて前後移動距離を計算します。
                float zoomDist = io.MouseWheel * (ctrl.moveSpeed * 0.5f);
                pos += forward * zoomDist;
                trans.isDirty = true;
            }

            // 計算した位置と回転を Transform に書き戻します。
            XMStoreFloat3(&trans.localPosition, pos);
            XMStoreFloat4(&trans.localRotation, rot);

            // TransformSystem に再計算を要求します。
            trans.isDirty = true;
        }
    }
}
