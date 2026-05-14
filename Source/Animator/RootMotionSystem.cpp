#include "RootMotionSystem.h"

#include "AnimatorComponent.h"
#include "AnimatorRuntime.h"
#include "AnimatorService.h"
#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"
#include "Component/PhysicsComponent.h"
#include "Component/TransformComponent.h"
#include "Entity/Entity.h"
#include "Physics/PhysicsManager.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include <DirectXMath.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace DirectX;

namespace
{
// AddUniqueNodeIndex は入力内容を検証し、対象コレクションへ新しい要素として追加する。
    static void AddUniqueNodeIndex(std::vector<int>& indices, int nodeIndex)
    {
        if (nodeIndex < 0) {
            return;
        }

        if (std::find(indices.begin(), indices.end(), nodeIndex) == indices.end()) {
            indices.push_back(nodeIndex);
        }
    }

    static float ComputeFullClipRootMotionLengthSq(
        const AnimatorRuntimeEntry& runtime,
        int nodeIndex,
        int animIndex,
        float duration,
        bool bakeY);

    static int ResolveRootMotionNodeIndex(
        const AnimatorRuntimeEntry& runtime,
        int animIndex,
        float duration,
        bool bakeY)
    {
        constexpr float thresholdSq = 0.000001f;
        std::vector<int> candidates;
        AddUniqueNodeIndex(candidates, runtime.rootNodeIndex);
        AddUniqueNodeIndex(candidates, runtime.rootMotionNodeIndex);

        int bestNodeIndex = -1;
        float bestLengthSq = thresholdSq;
        for (int candidate : candidates) {
            const float lengthSq = ComputeFullClipRootMotionLengthSq(
                runtime,
                candidate,
                animIndex,
                duration,
                bakeY);
            if (lengthSq > bestLengthSq) {
                bestNodeIndex = candidate;
                bestLengthSq = lengthSq;
            }
        }

        return bestNodeIndex;
    }

    static XMFLOAT3 SamplePosition(
        const Model::NodeAnim& nodeAnim,
        const XMFLOAT3& fallback,
        float time,
        float duration)
    {
        const auto& keyframes = nodeAnim.positionKeyframes;
        if (keyframes.empty()) {
            return fallback;
        }

        if (keyframes.size() == 1) {
            return keyframes.front().value;
        }

        const float safeTime = (std::clamp)(time, 0.0f, duration);
        if (safeTime <= keyframes.front().seconds) {
            return keyframes.front().value;
        }

        if (safeTime >= keyframes.back().seconds) {
            return keyframes.back().value;
        }

        for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
            const auto& k0 = keyframes[i];
            const auto& k1 = keyframes[i + 1];
            if (safeTime >= k0.seconds && safeTime <= k1.seconds) {
                const float span = k1.seconds - k0.seconds;
                const float rate = span > 0.000001f ? (safeTime - k0.seconds) / span : 1.0f;
                XMFLOAT3 result{};
                XMStoreFloat3(&result, XMVectorLerp(XMLoadFloat3(&k0.value), XMLoadFloat3(&k1.value), rate));
                return result;
            }
        }

        return keyframes.back().value;
    }

    static XMFLOAT4 SampleRotation(
        const Model::NodeAnim& nodeAnim,
        const XMFLOAT4& fallback,
        float time,
        float duration)
    {
        const auto& keyframes = nodeAnim.rotationKeyframes;
        if (keyframes.empty()) {
            return fallback;
        }

        if (keyframes.size() == 1) {
            return keyframes.front().value;
        }

        const float safeTime = (std::clamp)(time, 0.0f, duration);
        if (safeTime <= keyframes.front().seconds) {
            return keyframes.front().value;
        }

        if (safeTime >= keyframes.back().seconds) {
            return keyframes.back().value;
        }

        for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
            const auto& k0 = keyframes[i];
            const auto& k1 = keyframes[i + 1];
            if (safeTime >= k0.seconds && safeTime <= k1.seconds) {
                const float span = k1.seconds - k0.seconds;
                const float rate = span > 0.000001f ? (safeTime - k0.seconds) / span : 1.0f;
                XMFLOAT4 result{};
                XMStoreFloat4(&result, XMQuaternionSlerp(XMLoadFloat4(&k0.value), XMLoadFloat4(&k1.value), rate));
                return result;
            }
        }

        return keyframes.back().value;
    }

    static XMFLOAT3 SampleScale(
        const Model::NodeAnim& nodeAnim,
        const XMFLOAT3& fallback,
        float time,
        float duration)
    {
        const auto& keyframes = nodeAnim.scaleKeyframes;
        if (keyframes.empty()) {
            return fallback;
        }

        if (keyframes.size() == 1) {
            return keyframes.front().value;
        }

        const float safeTime = (std::clamp)(time, 0.0f, duration);
        if (safeTime <= keyframes.front().seconds) {
            return keyframes.front().value;
        }

        if (safeTime >= keyframes.back().seconds) {
            return keyframes.back().value;
        }

        for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
            const auto& k0 = keyframes[i];
            const auto& k1 = keyframes[i + 1];
            if (safeTime >= k0.seconds && safeTime <= k1.seconds) {
                const float span = k1.seconds - k0.seconds;
                const float rate = span > 0.000001f ? (safeTime - k0.seconds) / span : 1.0f;
                XMFLOAT3 result{};
                XMStoreFloat3(&result, XMVectorLerp(XMLoadFloat3(&k0.value), XMLoadFloat3(&k1.value), rate));
                return result;
            }
        }

        return keyframes.back().value;
    }

    static XMMATRIX SampleNodeLocalTransform(
        const AnimatorRuntimeEntry& runtime,
        int nodeIndex,
        int animIndex,
        float time)
    {
        Model* model = runtime.modelRef;
        const auto& nodes = model->GetNodes();
        const auto& anim = model->GetAnimations()[animIndex];
        const auto& node = nodes[nodeIndex];

        XMFLOAT3 position = node.position;
        XMFLOAT4 rotation = node.rotation;
        XMFLOAT3 scale = node.scale;
        if (nodeIndex >= 0 && nodeIndex < static_cast<int>(runtime.bindPoses.size())) {
            position = runtime.bindPoses[nodeIndex].position;
            rotation = runtime.bindPoses[nodeIndex].rotation;
            scale = runtime.bindPoses[nodeIndex].scale;
        }

        if (nodeIndex >= 0 && nodeIndex < static_cast<int>(anim.nodeAnims.size())) {
            const Model::NodeAnim& nodeAnim = anim.nodeAnims[nodeIndex];
            position = SamplePosition(nodeAnim, position, time, anim.secondsLength);
            rotation = SampleRotation(nodeAnim, rotation, time, anim.secondsLength);
            scale = SampleScale(nodeAnim, scale, time, anim.secondsLength);
        }

        return XMMatrixScaling(scale.x, scale.y, scale.z) *
            XMMatrixRotationQuaternion(XMLoadFloat4(&rotation)) *
            XMMatrixTranslation(position.x, position.y, position.z);
    }

    static XMVECTOR LoadRootMotionPosition(
        const AnimatorRuntimeEntry& runtime,
        int nodeIndex,
        int animIndex,
        float time)
    {
        Model* model = runtime.modelRef;
        if (!model || animIndex < 0 || animIndex >= static_cast<int>(model->GetAnimations().size()) ||
            nodeIndex < 0 || nodeIndex >= static_cast<int>(model->GetNodes().size()))
        {
            return XMVectorZero();
        }

        std::vector<int> chain;
        const auto& nodes = model->GetNodes();
        int current = nodeIndex;
        while (current >= 0 && current < static_cast<int>(nodes.size())) {
            chain.push_back(current);
            current = nodes[current].parentIndex;
        }

        XMMATRIX global = XMMatrixIdentity();
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const XMMATRIX local = SampleNodeLocalTransform(runtime, *it, animIndex, time);
            global = local * global;
        }

        XMVECTOR scale{};
        XMVECTOR rotation{};
        XMVECTOR translation{};
        if (!XMMatrixDecompose(&scale, &rotation, &translation, global)) {
            return XMVectorZero();
        }

        return translation;
    }

    static XMVECTOR ComputeRootMotionLocalDelta(
        const AnimatorRuntimeEntry& runtime,
        int nodeIndex,
        int animIndex,
        float prevTime,
        float currTime,
        float duration)
    {
        if (currTime < prevTime) {
            return
                (LoadRootMotionPosition(runtime, nodeIndex, animIndex, duration) -
                    LoadRootMotionPosition(runtime, nodeIndex, animIndex, prevTime)) +
                (LoadRootMotionPosition(runtime, nodeIndex, animIndex, currTime) -
                    LoadRootMotionPosition(runtime, nodeIndex, animIndex, 0.0f));
        }

        return LoadRootMotionPosition(runtime, nodeIndex, animIndex, currTime) -
            LoadRootMotionPosition(runtime, nodeIndex, animIndex, prevTime);
    }

    static bool IsNearlyZeroRootMotionDelta(const XMFLOAT3& delta)
    {
        constexpr float epsilon = 0.000001f;
        return std::fabs(delta.x) <= epsilon &&
            std::fabs(delta.y) <= epsilon &&
            std::fabs(delta.z) <= epsilon;
    }

    static bool IsValidRootMotionDelta(const XMFLOAT3& delta)
    {
        return std::isfinite(delta.x) &&
            std::isfinite(delta.y) &&
            std::isfinite(delta.z);
    }

    static XMVECTOR ConvertAnimationDeltaToModelDelta(
        const AnimatorRuntimeEntry& runtime,
        XMVECTOR delta)
    {
        const float modelScale = runtime.modelRef ? runtime.modelRef->GetScaling() : 1.0f;
        delta = XMVectorSet(-XMVectorGetX(delta), XMVectorGetY(delta), XMVectorGetZ(delta), 0.0f);
        return delta * modelScale;
    }

    static float ComputeFullClipRootMotionLengthSq(
        const AnimatorRuntimeEntry& runtime,
        int nodeIndex,
        int animIndex,
        float duration,
        bool bakeY)
    {
        XMVECTOR fullClipDelta = ComputeRootMotionLocalDelta(
            runtime,
            nodeIndex,
            animIndex,
            0.0f,
            duration,
            duration);

        fullClipDelta = ConvertAnimationDeltaToModelDelta(runtime, fullClipDelta);
        if (!bakeY) {
            fullClipDelta = XMVectorSetY(fullClipDelta, 0.0f);
        }

        return XMVectorGetX(XMVector3LengthSq(fullClipDelta));
    }

    static EntityID GetParentEntity(Registry& registry, EntityID entity)
    {
        if (HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            return hierarchy->parent;
        }

        if (TransformComponent* transform = registry.GetComponent<TransformComponent>(entity)) {
            return transform->parent == 0 ? Entity::NULL_ID : transform->parent;
        }

        return Entity::NULL_ID;
    }

    static XMVECTOR ConvertWorldPositionToLocalPosition(
        Registry& registry,
        EntityID entity,
        XMVECTOR worldPosition)
    {
        const EntityID parent = GetParentEntity(registry, entity);
        if (Entity::IsNull(parent)) {
            return worldPosition;
        }

        TransformComponent* parentTransform = registry.GetComponent<TransformComponent>(parent);
        if (!parentTransform) {
            return worldPosition;
        }

        const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
        const XMMATRIX inverseParentWorld = XMMatrixInverse(nullptr, parentWorld);

        return XMVector3TransformCoord(worldPosition, inverseParentWorld);
    }

    static XMVECTOR ConvertWorldDeltaToLocalDelta(
        Registry& registry,
        EntityID entity,
        XMVECTOR worldDelta)
    {
        const EntityID parent = GetParentEntity(registry, entity);
        if (Entity::IsNull(parent)) {
            return worldDelta;
        }

        TransformComponent* parentTransform = registry.GetComponent<TransformComponent>(parent);
        if (!parentTransform) {
            return worldDelta;
        }

        const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
        const XMMATRIX inverseParentWorld = XMMatrixInverse(nullptr, parentWorld);

        return XMVector3TransformNormal(worldDelta, inverseParentWorld);
    }

    static void SyncRootMotionState(
        const AnimatorComponent::LayerState& layer,
        int& prevAnimIndex,
        float& prevTime)
    {
        prevAnimIndex = layer.currentAnimIndex;
        prevTime = layer.currentTime;
    }

    static void ComputeLayerRootMotion(
        AnimatorComponent& animator,
        AnimatorRuntimeEntry& runtime,
        const TransformComponent& transform,
        const AnimatorComponent::LayerState& layer,
        int& prevAnimIndex,
        float& prevTime)
    {
        animator.rootMotionDelta = { 0.0f, 0.0f, 0.0f };

        if (!animator.enableRootMotion || !runtime.modelRef || layer.currentAnimIndex < 0) {
            SyncRootMotionState(layer, prevAnimIndex, prevTime);
            return;
        }

        const auto& animations = runtime.modelRef->GetAnimations();
        if (layer.currentAnimIndex >= static_cast<int>(animations.size())) {
            SyncRootMotionState(layer, prevAnimIndex, prevTime);
            return;
        }

        const float duration = animations[layer.currentAnimIndex].secondsLength;
        if (duration <= 0.0f) {
            SyncRootMotionState(layer, prevAnimIndex, prevTime);
            return;
        }

        const int nodeIndex = ResolveRootMotionNodeIndex(
            runtime,
            layer.currentAnimIndex,
            duration,
            animator.bakeRootMotionY);
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(runtime.modelRef->GetNodes().size())) {
            SyncRootMotionState(layer, prevAnimIndex, prevTime);
            return;
        }

        if (prevAnimIndex != layer.currentAnimIndex) {
            SyncRootMotionState(layer, prevAnimIndex, prevTime);
            return;
        }

        XMVECTOR delta = ComputeRootMotionLocalDelta(
            runtime,
            nodeIndex,
            layer.currentAnimIndex,
            prevTime,
            layer.currentTime,
            duration);

        delta = ConvertAnimationDeltaToModelDelta(runtime, delta);
        delta = XMVector3Rotate(delta, XMLoadFloat4(&transform.worldRotation));
        delta *= transform.worldScale.x * animator.rootMotionScale;

        if (!animator.bakeRootMotionY) {
            delta = XMVectorSetY(delta, 0.0f);
        }

        XMStoreFloat3(&animator.rootMotionDelta, delta);
        if (!IsValidRootMotionDelta(animator.rootMotionDelta)) {
            animator.rootMotionDelta = { 0.0f, 0.0f, 0.0f };
        }

        SyncRootMotionState(layer, prevAnimIndex, prevTime);
    }

    static void ApplyRootMotionToMotionOwner(
        Registry& registry,
        EntityID entity,
        TransformComponent& transform,
        const AnimatorComponent& animator)
    {
        if (!animator.enableRootMotion) {
            return;
        }

        if (IsNearlyZeroRootMotionDelta(animator.rootMotionDelta)) {
            return;
        }

        PhysicsComponent* physics = registry.GetComponent<PhysicsComponent>(entity);
        if (physics && !physics->bodyID.IsInvalid()) {
            JPH::BodyInterface& bodyInterface = PhysicsManager::Instance().GetBodyInterface();

            const JPH::RVec3 currentBodyPosition = bodyInterface.GetPosition(physics->bodyID);
            const JPH::Quat currentBodyRotation = bodyInterface.GetRotation(physics->bodyID);

            const JPH::RVec3 nextBodyPosition(
                static_cast<JPH::Real>(currentBodyPosition.GetX() + animator.rootMotionDelta.x),
                static_cast<JPH::Real>(currentBodyPosition.GetY() + animator.rootMotionDelta.y),
                static_cast<JPH::Real>(currentBodyPosition.GetZ() + animator.rootMotionDelta.z));

            bodyInterface.SetPositionAndRotation(
                physics->bodyID,
                nextBodyPosition,
                currentBodyRotation,
                JPH::EActivation::Activate);

            const XMVECTOR nextWorldPosition = XMVectorSet(
                static_cast<float>(nextBodyPosition.GetX()),
                static_cast<float>(nextBodyPosition.GetY()),
                static_cast<float>(nextBodyPosition.GetZ()),
                1.0f);

            const XMVECTOR nextLocalPosition = ConvertWorldPositionToLocalPosition(
                registry,
                entity,
                nextWorldPosition);

            XMStoreFloat3(&transform.worldPosition, nextWorldPosition);
            XMStoreFloat3(&transform.localPosition, nextLocalPosition);
            transform.isDirty = true;
            return;
        }

        const XMVECTOR worldDelta = XMLoadFloat3(&animator.rootMotionDelta);
        const XMVECTOR localDelta = ConvertWorldDeltaToLocalDelta(registry, entity, worldDelta);

        XMVECTOR localPosition = XMLoadFloat3(&transform.localPosition);
        localPosition += localDelta;

        XMStoreFloat3(&transform.localPosition, localPosition);
        transform.isDirty = true;
    }
}

void RootMotionSystem::Update(Registry& registry, float dt)
{
    AnimatorService::Instance().SetRegistry(&registry);
    AnimatorRuntimeRegistry& runtimeRegistry = AnimatorService::Instance().GetRuntimeRegistry();

    Query<MeshComponent, TransformComponent, AnimatorComponent> query(registry);
    query.ForEachWithEntity([&](EntityID entity, MeshComponent& mesh, TransformComponent& transform, AnimatorComponent& animator) {
        if (!mesh.model) {
            runtimeRegistry.Remove(entity);
            return;
        }

        AnimatorRuntimeEntry& runtime = runtimeRegistry.Ensure(entity, mesh.model.get());

        const bool hasFullBodyAction =
            animator.actionLayer.currentAnimIndex >= 0 &&
            animator.actionLayer.weight > 0.0f &&
            animator.actionLayer.isFullBody;

        if (!animator.enableRootMotion || dt <= 0.0001f) {
            animator.rootMotionDelta = { 0.0f, 0.0f, 0.0f };
            if (hasFullBodyAction) {
                SyncRootMotionState(
                    animator.actionLayer,
                    runtime.prevActionRootMotionAnimIndex,
                    runtime.prevActionRootMotionTime);
                SyncRootMotionState(
                    animator.baseLayer,
                    runtime.prevBaseRootMotionAnimIndex,
                    runtime.prevBaseRootMotionTime);
            }
            else {
                SyncRootMotionState(
                    animator.baseLayer,
                    runtime.prevBaseRootMotionAnimIndex,
                    runtime.prevBaseRootMotionTime);
                runtime.prevActionRootMotionAnimIndex = -1;
                runtime.prevActionRootMotionTime = 0.0f;
            }
            return;
        }

        if (hasFullBodyAction) {
            ComputeLayerRootMotion(
                animator,
                runtime,
                transform,
                animator.actionLayer,
                runtime.prevActionRootMotionAnimIndex,
                runtime.prevActionRootMotionTime);
            SyncRootMotionState(
                animator.baseLayer,
                runtime.prevBaseRootMotionAnimIndex,
                runtime.prevBaseRootMotionTime);
        }
        else {
            ComputeLayerRootMotion(
                animator,
                runtime,
                transform,
                animator.baseLayer,
                runtime.prevBaseRootMotionAnimIndex,
                runtime.prevBaseRootMotionTime);

            runtime.prevActionRootMotionAnimIndex = -1;
            runtime.prevActionRootMotionTime = 0.0f;
        }

        ApplyRootMotionToMotionOwner(registry, entity, transform, animator);
    });
}
