#include "AnimatorService.h"

#include "AnimatorComponent.h"
#include "AnimatorRuntime.h"
#include "Component/MeshComponent.h"
#include "Registry/Registry.h"
// AnimatorService::Instance はこのモジュールの実行時処理を構成する補助処理を行う。

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

    AnimatorRuntimeEntry* runtime = m_runtimeRegistry->Find(entity);
    if (runtime) {
        runtime->useOffsetBlending = false;
        runtime->offsetBlendDuration = 0.0f;
        runtime->offsetBlendTimer = 0.0f;
    }

    if (runtime &&
        runtime->hasValidFinalPose &&
        layer.currentAnimIndex >= 0 &&
        layer.currentAnimIndex < static_cast<int>(model->GetAnimations().size()) &&
        blendTime > 0.0f) {
        layer.prevAnimIndex = layer.currentAnimIndex;
        layer.prevAnimTime = layer.currentTime;
        layer.blendDuration = blendTime;
        layer.blendTimer = 0.0f;
        layer.isBlending = true;

        model->GetNodePoses(runtime->baseBlendFromPoses);
    }
    else {
        layer.prevAnimIndex = -1;
        layer.prevAnimTime = 0.0f;
        layer.blendDuration = 0.0f;
        layer.blendTimer = 0.0f;
        layer.isBlending = false;
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

        if (auto* runtime = m_runtimeRegistry->Find(entity); runtime && runtime->hasValidFinalPose) {
            model->GetNodePoses(runtime->actionBlendFromPoses);
        }
        else {
            layer.isBlending = false;
            layer.blendDuration = 0.0f;
            layer.blendTimer = 0.0f;
            layer.prevAnimIndex = -1;
            layer.prevAnimTime = 0.0f;
        }
    }
    else {
        layer.prevAnimIndex = -1;
        layer.prevAnimTime = 0.0f;
        layer.blendDuration = 0.0f;
        layer.blendTimer = 0.0f;
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
        runtime->prevActionRootMotionAnimIndex = -1;
        runtime->prevActionRootMotionTime = 0.0f;
    }
}

void AnimatorService::StopAction(EntityID entity, float)
{
    if (auto* animator = GetAnimator(entity)) {
        animator->actionLayer.weight = 0.0f;
        animator->actionLayer.currentAnimIndex = -1;
        animator->actionLayer.currentTime = 0.0f;
        animator->actionLayer.prevAnimIndex = -1;
        animator->actionLayer.prevAnimTime = 0.0f;
        animator->actionLayer.blendDuration = 0.0f;
        animator->actionLayer.blendTimer = 0.0f;
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
