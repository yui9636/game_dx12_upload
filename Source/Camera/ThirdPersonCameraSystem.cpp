#include "ThirdPersonCameraSystem.h"
#include "Registry/Registry.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "Component/TransformComponent.h"
#include "Component/CameraBehaviorComponent.h"
#include "Component/CameraComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"
#include "Engine/EngineKernel.h"
#include "Engine/EngineMode.h"
#include "Gameplay/TimelineShakeSystem.h"
#include "Model/ModelResource.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    struct MainCameraRef
    {
        EntityID entity = Entity::NULL_ID;
        TransformComponent* transform = nullptr;
    };

    struct TargetCameraFrame
    {
        float radius = 1.0f;
        float height = 2.0f;
        bool valid = false;
    };

    MainCameraRef FindMainCamera(Registry& registry)
    {
        MainCameraRef result;
        const Signature cameraSig = CreateSignature<TransformComponent, CameraMainTagComponent>();
        for (auto* archetype : registry.GetAllArchetypes()) {
            if (!SignatureMatches(archetype->GetSignature(), cameraSig)) {
                continue;
            }

            auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
            const auto& entities = archetype->GetEntities();
            if (!transformColumn || entities.empty()) {
                continue;
            }

            result.entity = entities[0];
            result.transform = static_cast<TransformComponent*>(transformColumn->Get(0));
            return result;
        }
        return result;
    }

    XMVECTOR FlattenAndNormalizeForward(XMVECTOR forward)
    {
        forward = XMVectorSetY(forward, 0.0f);
        const float lengthSq = XMVectorGetX(XMVector3LengthSq(forward));
        if (lengthSq <= 0.0001f) {
            return XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }
        return XMVector3Normalize(forward);
    }

    XMVECTOR ResolveTargetForward(const TransformComponent& targetTransform, const CameraTPVControlComponent& control)
    {
        if (control.followTargetFacing) {
            XMMATRIX targetWorld = XMLoadFloat4x4(&targetTransform.worldMatrix);
            XMVECTOR forward = FlattenAndNormalizeForward(targetWorld.r[2]);
            if (std::fabs(control.yaw) > 0.0001f) {
                forward = XMVector3TransformNormal(forward, XMMatrixRotationY(control.yaw));
            }
            return FlattenAndNormalizeForward(forward);
        }

        return XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMMatrixRotationY(control.yaw));
    }

    XMVECTOR ResolveTargetRight(XMVECTOR forward)
    {
        return XMVector3Normalize(XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward));
    }

    float ResolveFollowAlpha(float smoothness, float dt)
    {
        if (dt <= 0.0f || smoothness <= 0.0f) {
            return 1.0f;
        }
        return std::clamp(1.0f - std::exp(-smoothness * dt), 0.0f, 1.0f);
    }

    bool IsPreviewOnlyEntity(Registry& registry, EntityID entity)
    {
        const auto* preview = registry.GetComponent<EffectPreviewTagComponent>(entity);
        return preview && preview->previewOnly;
    }

    bool TryMergeMeshBoundsRecursive(
        Registry& registry,
        EntityID entity,
        BoundingBox& inOutBounds,
        bool& hasBounds,
        int depth = 0)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity) || depth > 32) {
            return hasBounds;
        }

        const auto* transform = registry.GetComponent<TransformComponent>(entity);
        const auto* mesh = registry.GetComponent<MeshComponent>(entity);
        if (transform && mesh && mesh->model) {
            if (const auto modelResource = mesh->model->GetModelResource()) {
                BoundingBox meshBounds{};
                modelResource->GetLocalBounds().Transform(meshBounds, XMLoadFloat4x4(&transform->worldMatrix));
                if (!hasBounds) {
                    inOutBounds = meshBounds;
                    hasBounds = true;
                }
                else {
                    BoundingBox merged{};
                    BoundingBox::CreateMerged(merged, inOutBounds, meshBounds);
                    inOutBounds = merged;
                }
            }
        }

        const auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        if (!hierarchy) {
            return hasBounds;
        }

        EntityID child = hierarchy->firstChild;
        while (!Entity::IsNull(child)) {
            TryMergeMeshBoundsRecursive(registry, child, inOutBounds, hasBounds, depth + 1);
            const auto* childHierarchy = registry.GetComponent<HierarchyComponent>(child);
            child = childHierarchy ? childHierarchy->nextSibling : Entity::NULL_ID;
        }
        return hasBounds;
    }

    TargetCameraFrame ResolveTargetCameraFrame(Registry& registry, EntityID targetEntity)
    {
        BoundingBox bounds{};
        bool hasBounds = false;
        TryMergeMeshBoundsRecursive(registry, targetEntity, bounds, hasBounds);
        if (!hasBounds) {
            return {};
        }

        TargetCameraFrame frame{};
        const XMFLOAT3 extents = bounds.Extents;
        frame.radius = std::sqrt(extents.x * extents.x + extents.y * extents.y + extents.z * extents.z);
        frame.height = extents.y * 2.0f;
        frame.radius = frame.radius > 0.01f ? frame.radius : 1.0f;
        frame.height = frame.height > 0.01f ? frame.height : 2.0f;
        frame.valid = true;
        return frame;
    }
}

// TPVControlComponent を持つ Entity を追従対象として Main Camera を更新する。
void ThirdPersonCameraSystem::Update(Registry& registry, float dt) {
    // エディタプレビュー中にゲーム入力や戦闘用シェイクを消費しないよう、
    // Play 中だけ三人称カメラを動かします。
    if (EngineKernel::Instance().GetMode() != EngineMode::Play) {
        return;
    }

    MainCameraRef mainCamera = FindMainCamera(registry);
    if (Entity::IsNull(mainCamera.entity) || !mainCamera.transform) {
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
            const auto& entities = archetype->GetEntities();
            if (i >= entities.size() || entities[i] == mainCamera.entity) {
                continue;
            }
            if (IsPreviewOnlyEntity(registry, entities[i])) {
                continue;
            }
            const EntityID targetEntity = entities[i];

            // 同じ行にある追従対象 Transform と三人称カメラ制御情報を取得します。
            auto& targetTrans = *static_cast<TransformComponent*>(transCol->Get(i));
            auto& ctrl = *static_cast<CameraTPVControlComponent*>(ctrlCol->Get(i));
            const TargetCameraFrame frame = ResolveTargetCameraFrame(registry, targetEntity);
            const float resolvedDistance = frame.valid ? (std::max)(ctrl.distance, frame.radius * 2.2f) : ctrl.distance;
            const float resolvedHeightOffset = frame.valid ? (std::max)(ctrl.heightOffset, frame.height * 0.55f) : ctrl.heightOffset;
            const float resolvedLookAtHeight = frame.valid ? (std::max)(ctrl.lookAtHeight, frame.height * 0.45f) : ctrl.lookAtHeight;

            // 必要なゲームだけ手動オービットを許可する。
            ImGuiIO& io = ImGui::GetIO();
            if (ctrl.allowManualOrbit && io.MouseDown[ImGuiMouseButton_Right]) {
                ctrl.yaw += io.MouseDelta.x * 0.005f;
                ctrl.pitch += io.MouseDelta.y * 0.005f;
            }

            // カメラが上下を向きすぎないよう pitch を制限します。
            ctrl.pitch = std::clamp(ctrl.pitch, -1.5f, 1.5f);

            // 追従対象のワールド位置を取得します。
            XMVECTOR targetPos = XMLoadFloat3(&targetTrans.worldPosition);

            const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            const XMVECTOR forward = ResolveTargetForward(targetTrans, ctrl);
            const XMVECTOR right = ResolveTargetRight(forward);

            // 対象の向きに固定した後方上オフセットを作る。
            const float pitch = std::clamp(ctrl.pitch, -1.2f, 1.2f);
            const float horizontalDistance = resolvedDistance * std::cos(pitch);
            const float verticalOffset = resolvedHeightOffset + resolvedDistance * std::sin(pitch);
            XMVECTOR idealPos =
                targetPos
                - forward * horizontalDistance
                + right * ctrl.shoulderOffset
                + up * verticalOffset
                + forward * ctrl.forwardOffset;

            // 現在位置から理想位置へ指数補間で滑らかに近づけます。
            XMVECTOR currentPos = XMLoadFloat3(&mainCamera.transform->localPosition);
            const float positionAlpha = ResolveFollowAlpha(ctrl.smoothness, dt);
            currentPos = XMVectorLerp(currentPos, idealPos, positionAlpha);
            XMStoreFloat3(&mainCamera.transform->localPosition, currentPos);

            // Timeline 由来のヒットシェイクを、追従後の位置へ加算します。
            mainCamera.transform->localPosition.x += shake.x;
            mainCamera.transform->localPosition.y += shake.y;
            mainCamera.transform->localPosition.z += shake.z;

            // 対象の高さ補正位置を見るように注視点を作ります。
            XMVECTOR lookTarget = targetPos + up * resolvedLookAtHeight + forward * ctrl.lookAheadDistance;

            // LookAt 行列を作り、その逆行列からカメラのローカル回転を取り出します。
            XMMATRIX lookAtMatrix = XMMatrixLookAtLH(XMLoadFloat3(&mainCamera.transform->localPosition), lookTarget, up);

            // 行列分解で回転成分を取り出し、Transform に反映します。
            XMVECTOR outScale, outRot, outTrans;
            XMMatrixDecompose(&outScale, &outRot, &outTrans, XMMatrixInverse(nullptr, lookAtMatrix));
            XMVECTOR currentRotation = XMLoadFloat4(&mainCamera.transform->localRotation);
            const float rotationAlpha = ResolveFollowAlpha(ctrl.rotationSmoothness, dt);
            XMVECTOR finalRotation = XMQuaternionSlerp(currentRotation, XMQuaternionNormalize(outRot), rotationAlpha);
            XMStoreFloat4(&mainCamera.transform->localRotation, XMQuaternionNormalize(finalRotation));

            // TransformSystem に再計算を要求します。
            mainCamera.transform->isDirty = true;
            return;
        }
    }
}
