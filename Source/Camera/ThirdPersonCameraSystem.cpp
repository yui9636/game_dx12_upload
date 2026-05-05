#include "ThirdPersonCameraSystem.h"
#include "Registry/Registry.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "Component/TransformComponent.h"
#include "Component/CameraBehaviorComponent.h"
#include "Engine/EngineKernel.h"
#include "Engine/EngineMode.h"
#include "Gameplay/TimelineShakeSystem.h"
#include <imgui.h>
#include <algorithm>

using namespace DirectX;

// =========================================================
// CameraTPVControlComponent を持つ三人称カメラを更新します。
// =========================================================
void ThirdPersonCameraSystem::Update(Registry& registry, float dt) {
    // エディタプレビュー中にゲーム入力や戦闘用シェイクを消費しないよう、
    // Play 中だけ三人称カメラを動かします。
    if (EngineKernel::Instance().GetMode() != EngineMode::Play) {
        return;
    }

    // 対象となるコンポーネント構成を作成します。
    auto archetypes = registry.GetAllArchetypes();
    Signature targetSig = CreateSignature<TransformComponent, CameraTPVControlComponent>();

    // TimelineShakeSystem から、現在フレームのカメラシェイク量を取得します。
    const XMFLOAT3 shake = TimelineShakeSystem::GetShakeOffset();

    for (auto* archetype : archetypes) {
        // Transform と三人称カメラ制御を持たない Archetype は処理対象外です。
        if (!SignatureMatches(archetype->GetSignature(), targetSig)) continue;

        // Transform とカメラ制御コンポーネントの列を取得します。
        auto* transCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* ctrlCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraTPVControlComponent>());

        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            // 同じ行にある Transform と三人称カメラ制御情報を取得します。
            auto& trans = *static_cast<TransformComponent*>(transCol->Get(i));
            auto& ctrl = *static_cast<CameraTPVControlComponent*>(ctrlCol->Get(i));

            // 追従対象が未設定なら、このカメラは更新しません。
            if (Entity::IsNull(ctrl.target)) continue;

            // 追従対象の Transform が無ければ位置を計算できないためスキップします。
            auto* targetTrans = registry.GetComponent<TransformComponent>(ctrl.target);
            if (!targetTrans) continue;

            // 右クリック中はマウス移動でカメラの yaw / pitch を更新します。
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDown[ImGuiMouseButton_Right]) {
                ctrl.yaw += io.MouseDelta.x * 0.005f;
                ctrl.pitch += io.MouseDelta.y * 0.005f;
            }

            // カメラが上下を向きすぎないよう pitch を制限します。
            ctrl.pitch = std::clamp(ctrl.pitch, -1.5f, 1.5f);

            // 追従対象のワールド位置を取得します。
            XMVECTOR targetPos = XMLoadFloat3(&targetTrans->worldPosition);

            // yaw / pitch から追従カメラの回転行列を作ります。
            XMMATRIX rot = XMMatrixRotationRollPitchYaw(ctrl.pitch, ctrl.yaw, 0.0f);

            // 対象の背後かつ少し上にある理想カメラ位置を計算します。
            XMVECTOR offset = XMVectorSet(0, ctrl.heightOffset, -ctrl.distance, 0);
            XMVECTOR idealPos = targetPos + XMVector3TransformNormal(offset, rot);

            // 現在位置から理想位置へ指数補間で滑らかに近づけます。
            XMVECTOR currentPos = XMLoadFloat3(&trans.localPosition);
            float t = 1.0f - expf(-ctrl.smoothness * dt);
            currentPos = XMVectorLerp(currentPos, idealPos, t);
            XMStoreFloat3(&trans.localPosition, currentPos);

            // Timeline 由来のヒットシェイクを、追従後の位置へ加算します。
            trans.localPosition.x += shake.x;
            trans.localPosition.y += shake.y;
            trans.localPosition.z += shake.z;

            // 対象の高さ補正位置を見るように注視点を作ります。
            XMVECTOR lookTarget = targetPos + XMVectorSet(0, ctrl.heightOffset, 0, 0);

            // LookAt 行列を作り、その逆行列からカメラのローカル回転を取り出します。
            XMMATRIX lookAtMatrix = XMMatrixLookAtLH(XMLoadFloat3(&trans.localPosition), lookTarget, XMVectorSet(0, 1, 0, 0));

            // 行列分解で回転成分を取り出し、Transform に反映します。
            XMVECTOR outScale, outRot, outTrans;
            XMMatrixDecompose(&outScale, &outRot, &outTrans, XMMatrixInverse(nullptr, lookAtMatrix));
            XMStoreFloat4(&trans.localRotation, outRot);

            // TransformSystem に再計算を要求します。
            trans.isDirty = true;
        }
    }
}
