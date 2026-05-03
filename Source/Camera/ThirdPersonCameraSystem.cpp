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

void ThirdPersonCameraSystem::Update(Registry& registry, float dt) {
    // Editor preview must not consume gameplay input or get shaken by gameplay
    // timeline events. Run only while the kernel is in Play mode.
    if (EngineKernel::Instance().GetMode() != EngineMode::Play) {
        return;
    }

    auto archetypes = registry.GetAllArchetypes();
    Signature targetSig = CreateSignature<TransformComponent, CameraTPVControlComponent>();

    const XMFLOAT3 shake = TimelineShakeSystem::GetShakeOffset();

    for (auto* archetype : archetypes) {
        if (!SignatureMatches(archetype->GetSignature(), targetSig)) continue;

        auto* transCol = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* ctrlCol = archetype->GetColumn(TypeManager::GetComponentTypeID<CameraTPVControlComponent>());

        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            auto& trans = *static_cast<TransformComponent*>(transCol->Get(i));
            auto& ctrl = *static_cast<CameraTPVControlComponent*>(ctrlCol->Get(i));

            if (Entity::IsNull(ctrl.target)) continue;

            auto* targetTrans = registry.GetComponent<TransformComponent>(ctrl.target);
            if (!targetTrans) continue;

            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDown[ImGuiMouseButton_Right]) {
                ctrl.yaw += io.MouseDelta.x * 0.005f;
                ctrl.pitch += io.MouseDelta.y * 0.005f;
            }
            ctrl.pitch = std::clamp(ctrl.pitch, -1.5f, 1.5f);

            XMVECTOR targetPos = XMLoadFloat3(&targetTrans->worldPosition);
            XMMATRIX rot = XMMatrixRotationRollPitchYaw(ctrl.pitch, ctrl.yaw, 0.0f);

            XMVECTOR offset = XMVectorSet(0, ctrl.heightOffset, -ctrl.distance, 0);
            XMVECTOR idealPos = targetPos + XMVector3TransformNormal(offset, rot);

            XMVECTOR currentPos = XMLoadFloat3(&trans.localPosition);
            float t = 1.0f - expf(-ctrl.smoothness * dt);
            currentPos = XMVectorLerp(currentPos, idealPos, t);
            XMStoreFloat3(&trans.localPosition, currentPos);

            // Apply timeline-driven hit shake on top of the smoothed follow position.
            trans.localPosition.x += shake.x;
            trans.localPosition.y += shake.y;
            trans.localPosition.z += shake.z;

            XMVECTOR lookTarget = targetPos + XMVectorSet(0, ctrl.heightOffset, 0, 0);
            XMMATRIX lookAtMatrix = XMMatrixLookAtLH(XMLoadFloat3(&trans.localPosition), lookTarget, XMVectorSet(0, 1, 0, 0));

            XMVECTOR outScale, outRot, outTrans;
            XMMatrixDecompose(&outScale, &outRot, &outTrans, XMMatrixInverse(nullptr, lookAtMatrix));
            XMStoreFloat4(&trans.localRotation, outRot);

            trans.isDirty = true;
        }
    }
}
