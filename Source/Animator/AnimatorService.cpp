#include "AnimatorService.h"

#include "AnimatorComponent.h"
#include "AnimatorRuntime.h"
#include "Component/MeshComponent.h"
#include "Registry/Registry.h"

namespace
{
    static void FillBindPose(std::vector<Model::NodePose>& poses, const Model* model)
    {
        if (!model) return;
        const auto& nodes = model->GetNodes();
        poses.resize(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            poses[i].position = nodes[i].position;
            poses[i].rotation = nodes[i].rotation;
            poses[i].scale = nodes[i].scale;
        }
    }

    static void ForceRootResetXZ(std::vector<Model::NodePose>& poses, int rootNodeIndex)
    {
        if (rootNodeIndex >= 0 && rootNodeIndex < static_cast<int>(poses.size())) {
            poses[rootNodeIndex].position.x = 0.0f;
            poses[rootNodeIndex].position.y = 0.0f;
            poses[rootNodeIndex].position.z = 0.0f;
        }
    }

    static void CaptureTransitionOffsets(AnimatorRuntimeEntry& runtime, const std::vector<Model::NodePose>& currentPose, int nextAnimIndex)
    {
        if (!runtime.modelRef) return;

        FillBindPose(runtime.tempPoses, runtime.modelRef);
        runtime.modelRef->ComputeAnimation(nextAnimIndex, 0.0f, runtime.tempPoses);
        ForceRootResetXZ(runtime.tempPoses, runtime.rootNodeIndex);

        const size_t count = currentPose.size();
        for (size_t i = 0; i < count; ++i) {
            const DirectX::XMVECTOR pCurr = DirectX::XMLoadFloat3(&currentPose[i].position);
            const DirectX::XMVECTOR pNext = DirectX::XMLoadFloat3(&runtime.tempPoses[i].position);
            DirectX::XMVECTOR pDiff = DirectX::XMVectorSubtract(pCurr, pNext);
            pDiff = DirectX::XMVectorSetY(pDiff, 0.0f);
            DirectX::XMStoreFloat3(&runtime.blendOffsets[i].position, pDiff);

            const DirectX::XMVECTOR qCurr = DirectX::XMLoadFloat4(&currentPose[i].rotation);
            const DirectX::XMVECTOR qNext = DirectX::XMLoadFloat4(&runtime.tempPoses[i].rotation);
            DirectX::XMVECTOR qDiff = DirectX::XMQuaternionMultiply(DirectX::XMQuaternionInverse(qNext), qCurr);
            if (static_cast<int>(i) == runtime.rootNodeIndex) {
                qDiff = DirectX::XMQuaternionIdentity();
            }
            DirectX::XMStoreFloat4(&runtime.blendOffsets[i].rotation, qDiff);

            const DirectX::XMVECTOR sCurr = DirectX::XMLoadFloat3(&currentPose[i].scale);
            const DirectX::XMVECTOR sNext = DirectX::XMLoadFloat3(&runtime.tempPoses[i].scale);
            DirectX::XMStoreFloat3(&runtime.blendOffsets[i].scale, DirectX::XMVectorSubtract(sCurr, sNext));
        }
    }
}

AnimatorService& AnimatorService::Instance()
{
    static AnimatorService instance;
    return instance;
}

AnimatorService::AnimatorService()
{
    m_runtimeRegistry = new AnimatorRuntimeRegistry();
}

void AnimatorService::SetRegistry(Registry* registry)
{
    m_registry = registry;
}

void AnimatorService::EnsureAnimator(EntityID entity)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }
    if (!m_registry->GetComponent<AnimatorComponent>(entity)) {
        m_registry->AddComponent(entity, AnimatorComponent{});
    }
}

void AnimatorService::RemoveAnimator(EntityID entity)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }
    if (m_registry->GetComponent<AnimatorComponent>(entity)) {
        m_registry->RemoveComponent<AnimatorComponent>(entity);
    }
    m_runtimeRegistry->Remove(entity);
}

void AnimatorService::PlayBase(EntityID entity, int animIndex, bool loop, float blendTime, float speed)
{
    EnsureAnimator(entity);
    AnimatorComponent* animator = GetAnimator(entity);
    if (!animator) return;

    Model* model = GetModel(entity);
    if (!model || animIndex < 0 || animIndex >= static_cast<int>(model->GetAnimations().size())) {
        return;
    }

    auto& layer = animator->baseLayer;
    if (layer.currentAnimIndex == animIndex) {
        layer.isLoop = loop;
        layer.currentSpeed = speed;
        return;
    }

    if (auto* runtime = m_runtimeRegistry->Find(entity); runtime && layer.currentAnimIndex >= 0 && blendTime > 0.0f && !runtime->finalPoses.empty()) {
        CaptureTransitionOffsets(*runtime, runtime->finalPoses, animIndex);
        runtime->useOffsetBlending = true;
        runtime->offsetBlendDuration = blendTime;
        runtime->offsetBlendTimer = 0.0f;
    }

    layer.currentAnimIndex = animIndex;
    layer.currentTime = 0.0f;
    layer.currentSpeed = speed;
    layer.isLoop = loop;
    layer.weight = 1.0f;
}

void AnimatorService::PlayAction(EntityID entity, int animIndex, bool loop, float blendTime, bool isFullBody)
{
    EnsureAnimator(entity);
    AnimatorComponent* animator = GetAnimator(entity);
    Model* model = GetModel(entity);
    if (!animator || !model || animIndex < 0 || animIndex >= static_cast<int>(model->GetAnimations().size())) {
        return;
    }

    auto& layer = animator->actionLayer;
    if (layer.currentAnimIndex >= 0 && layer.weight > 0.01f && blendTime > 0.0f) {
        layer.prevAnimIndex = layer.currentAnimIndex;
        layer.prevAnimTime = layer.currentTime;
        layer.isBlending = true;
        layer.blendDuration = blendTime;
        layer.blendTimer = 0.0f;
    } else {
        layer.isBlending = false;
    }

    layer.currentAnimIndex = animIndex;
    layer.currentTime = 0.0f;
    layer.currentSpeed = 1.0f;
    layer.isLoop = loop;
    layer.weight = 1.0f;
    layer.isFullBody = isFullBody;

    if (auto* runtime = m_runtimeRegistry->Find(entity)) {
        runtime->prevActionTime = 0.0f;
    }
}

void AnimatorService::StopAction(EntityID entity, float)
{
    if (auto* animator = GetAnimator(entity)) {
        animator->actionLayer.weight = 0.0f;
        animator->actionLayer.currentAnimIndex = -1;
        animator->actionLayer.currentTime = 0.0f;
        animator->actionLayer.isBlending = false;
    }
}

void AnimatorService::SetActionTime(EntityID entity, float time)
{
    if (auto* animator = GetAnimator(entity)) {
        animator->actionLayer.currentTime = time;
        animator->actionLayer.isBlending = false;
    }
}

void AnimatorService::SetDriver(EntityID entity, float time, int overrideAnimIndex, bool loop, bool allowInternalUpdate)
{
    EnsureAnimator(entity);
    if (auto* animator = GetAnimator(entity)) {
        animator->driverConnected = true;
        animator->driverTime = time;
        animator->driverOverrideAnimIndex = overrideAnimIndex;
        animator->driverLoop = loop;
        animator->driverAllowInternalUpdate = allowInternalUpdate;
    }
}

void AnimatorService::ClearDriver(EntityID entity)
{
    if (auto* animator = GetAnimator(entity)) {
        animator->driverConnected = false;
        animator->driverTime = 0.0f;
        animator->driverOverrideAnimIndex = -1;
        animator->driverLoop = false;
        animator->driverAllowInternalUpdate = false;
    }
}

std::vector<std::string> AnimatorService::GetAnimationNameList(EntityID entity) const
{
    std::vector<std::string> names;
    Model* model = GetModel(entity);
    if (!model) return names;
    for (const auto& anim : model->GetAnimations()) {
        names.push_back(anim.name);
    }
    return names;
}

int AnimatorService::GetAnimationIndexByName(EntityID entity, const std::string& name) const
{
    Model* model = GetModel(entity);
    if (!model) return -1;
    return model->GetAnimationIndex(name.c_str());
}

DirectX::XMFLOAT3 AnimatorService::GetRootMotionDelta(EntityID entity) const
{
    if (const AnimatorComponent* animator = GetAnimator(entity)) {
        return animator->rootMotionDelta;
    }
    return { 0.0f, 0.0f, 0.0f };
}

AnimatorComponent* AnimatorService::GetAnimator(EntityID entity) const
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return nullptr;
    }
    return m_registry->GetComponent<AnimatorComponent>(entity);
}

Model* AnimatorService::GetModel(EntityID entity) const
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return nullptr;
    }
    if (auto* mesh = m_registry->GetComponent<MeshComponent>(entity); mesh && mesh->model) {
        return mesh->model.get();
    }
    return nullptr;
}
