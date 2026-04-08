#include "AnimatorSystem.h"

#include "AnimatorComponent.h"
#include "AnimatorRuntime.h"
#include "AnimatorService.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

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

    static void UpdateOffsetBlending(AnimatorRuntimeEntry& runtime, float dt)
    {
        if (!runtime.useOffsetBlending) return;
        runtime.offsetBlendTimer += dt;
        if (runtime.offsetBlendTimer >= runtime.offsetBlendDuration) {
            runtime.useOffsetBlending = false;
            runtime.offsetBlendTimer = 0.0f;
        }
    }

    static void ApplyOffsetBlending(AnimatorRuntimeEntry& runtime, std::vector<Model::NodePose>& poses)
    {
        if (!runtime.useOffsetBlending || runtime.offsetBlendDuration <= 0.0f) return;

        float t = runtime.offsetBlendTimer / runtime.offsetBlendDuration;
        t = (std::clamp)(t, 0.0f, 1.0f);
        const float decay = 1.0f - (1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t));

        for (size_t i = 0; i < poses.size(); ++i) {
            XMVECTOR p = XMLoadFloat3(&poses[i].position) + XMLoadFloat3(&runtime.blendOffsets[i].position) * decay;
            XMStoreFloat3(&poses[i].position, p);

            XMVECTOR qBase = XMLoadFloat4(&poses[i].rotation);
            XMVECTOR qOff = XMLoadFloat4(&runtime.blendOffsets[i].rotation);
            XMVECTOR q = XMQuaternionMultiply(qBase, XMQuaternionSlerp(XMQuaternionIdentity(), qOff, decay));
            XMStoreFloat4(&poses[i].rotation, q);

            XMVECTOR s = XMLoadFloat3(&poses[i].scale) + XMLoadFloat3(&runtime.blendOffsets[i].scale) * decay;
            XMStoreFloat3(&poses[i].scale, s);
        }
    }

    static void UpdateLayer(Model* model, AnimatorComponent::LayerState& layer, float dt, bool autoAdvance)
    {
        if (!model || layer.currentAnimIndex < 0 || layer.currentAnimIndex >= static_cast<int>(model->GetAnimations().size())) {
            return;
        }

        const auto& anim = model->GetAnimations()[layer.currentAnimIndex];
        const float maxTime = anim.secondsLength;
        if (autoAdvance) {
            layer.currentTime += dt * layer.currentSpeed;
            if (layer.currentTime >= maxTime) {
                layer.currentTime = layer.isLoop ? std::fmod(layer.currentTime, maxTime) : maxTime;
            }
        }

        if (layer.isBlending) {
            layer.blendTimer += dt;
            if (layer.blendTimer >= layer.blendDuration) {
                layer.isBlending = false;
                layer.blendTimer = 0.0f;
            }
        }
    }

    static void ComputeLayerPose(Model* model, int rootNodeIndex, const AnimatorComponent::LayerState& layer, std::vector<Model::NodePose>& out)
    {
        FillBindPose(out, model);
        if (layer.currentAnimIndex >= 0 && layer.currentAnimIndex < static_cast<int>(model->GetAnimations().size())) {
            model->ComputeAnimation(layer.currentAnimIndex, layer.currentTime, out);
        }
        ForceRootResetXZ(out, rootNodeIndex);
    }

    static void BlendPoses(const std::vector<Model::NodePose>& src, const std::vector<Model::NodePose>& dst, float t, int rootNodeIndex, std::vector<Model::NodePose>& out)
    {
        const size_t count = src.size();
        out.resize(count);
        for (size_t i = 0; i < count; ++i) {
            XMVECTOR p = XMVectorLerp(XMLoadFloat3(&src[i].position), XMLoadFloat3(&dst[i].position), t);
            if (static_cast<int>(i) == rootNodeIndex) {
                p = XMVectorSetY(p, dst[i].position.y);
            }
            XMStoreFloat3(&out[i].position, p);
            XMStoreFloat4(&out[i].rotation, XMQuaternionSlerp(XMLoadFloat4(&src[i].rotation), XMLoadFloat4(&dst[i].rotation), t));
            XMStoreFloat3(&out[i].scale, XMVectorLerp(XMLoadFloat3(&src[i].scale), XMLoadFloat3(&dst[i].scale), t));
        }
    }

    static XMFLOAT3 SampleRootPos(Model* model, int rootNodeIndex, int animIndex, float time)
    {
        if (!model || animIndex < 0 || rootNodeIndex < 0 || rootNodeIndex >= static_cast<int>(model->GetNodes().size())) {
            return { 0.0f, 0.0f, 0.0f };
        }

        const auto& anim = model->GetAnimations()[animIndex];
        const float safeTime = (std::clamp)(time, 0.0f, anim.secondsLength);
        Model::NodePose pose{};
        pose.position = model->GetNodes()[rootNodeIndex].position;
        pose.rotation = { 0, 0, 0, 1 };
        pose.scale = { 1, 1, 1 };
        model->ComputeAnimation(animIndex, rootNodeIndex, safeTime, pose);
        return pose.position;
    }

    static void ComputeRootMotion(AnimatorComponent& animator, AnimatorRuntimeEntry& runtime, const TransformComponent& transform, const AnimatorComponent::LayerState& layer, float prevTime, float currTime)
    {
        animator.rootMotionDelta = { 0.0f, 0.0f, 0.0f };
        if (!animator.enableRootMotion || !runtime.modelRef || layer.currentAnimIndex < 0) {
            return;
        }

        XMVECTOR delta = XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, currTime)) -
                         XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, prevTime));

        if (currTime < prevTime) {
            const float duration = runtime.modelRef->GetAnimations()[layer.currentAnimIndex].secondsLength;
            delta =
                (XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, duration)) -
                 XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, prevTime))) +
                (XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, currTime)) -
                 XMLoadFloat3(&SampleRootPos(runtime.modelRef, runtime.rootNodeIndex, layer.currentAnimIndex, 0.0f)));
        }

        delta = XMVector3Rotate(delta, XMLoadFloat4(&transform.worldRotation));
        delta *= transform.worldScale.x * animator.rootMotionScale;
        if (!animator.bakeRootMotionY) {
            delta = XMVectorSetY(delta, 0.0f);
        }

        XMStoreFloat3(&animator.rootMotionDelta, delta);
        if (std::isnan(animator.rootMotionDelta.x) || std::isnan(animator.rootMotionDelta.z)) {
            animator.rootMotionDelta = { 0.0f, 0.0f, 0.0f };
        }
    }
}

void AnimatorSystem::Update(Registry& registry, float dt)
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
        Model* model = runtime.modelRef;
        if (!model) {
            return;
        }

        float updateDt = dt;
        if (animator.driverConnected) {
            if (!animator.driverAllowInternalUpdate) {
                updateDt = 0.0f;
            }

            const int overrideIdx = animator.driverOverrideAnimIndex;
            const int animCount = static_cast<int>(model->GetAnimations().size());
            if (overrideIdx >= 0 && overrideIdx < animCount) {
                if (animator.actionLayer.currentAnimIndex != overrideIdx) {
                    AnimatorService::Instance().PlayAction(entity, overrideIdx, animator.driverLoop, 0.0f, true);
                }
                float driverTime = animator.driverTime;
                if (!animator.driverLoop) {
                    driverTime = (std::min)(driverTime, model->GetAnimations()[overrideIdx].secondsLength);
                }
                animator.actionLayer.currentTime = driverTime;
                animator.actionLayer.isLoop = animator.driverLoop;
                animator.actionLayer.isBlending = false;
            }
        }

        float prevBaseTime = animator.baseLayer.currentTime;
        if (auto* playback = registry.GetComponent<PlaybackComponent>(entity)) {
            animator.baseLayer.currentTime = playback->currentSeconds;
            animator.baseLayer.currentSpeed = playback->playSpeed;
            animator.baseLayer.isLoop = playback->looping;
        } else {
            UpdateLayer(model, animator.baseLayer, updateDt, true);
        }

        UpdateOffsetBlending(runtime, updateDt);

        if (animator.actionLayer.currentAnimIndex >= 0 && animator.actionLayer.weight > 0.0f) {
            UpdateLayer(model, animator.actionLayer, updateDt, true);
        }

        const bool hasAction = (animator.actionLayer.currentAnimIndex >= 0 && animator.actionLayer.weight > 0.0f);
        if (hasAction && animator.actionLayer.isFullBody) {
            if (updateDt > 0.0001f) {
                ComputeRootMotion(animator, runtime, transform, animator.actionLayer, runtime.prevActionTime, animator.actionLayer.currentTime);
            }
            runtime.prevActionTime = animator.actionLayer.currentTime;
        } else {
            ComputeRootMotion(animator, runtime, transform, animator.baseLayer, prevBaseTime, animator.baseLayer.currentTime);
        }

        if (animator.baseLayer.currentAnimIndex >= 0) {
            ComputeLayerPose(model, runtime.rootNodeIndex, animator.baseLayer, runtime.basePoses);
        } else {
            FillBindPose(runtime.basePoses, model);
        }

        if (hasAction) {
            FillBindPose(runtime.actionPoses, model);
            if (animator.actionLayer.currentAnimIndex >= 0 && animator.actionLayer.currentAnimIndex < static_cast<int>(model->GetAnimations().size())) {
                model->ComputeAnimation(animator.actionLayer.currentAnimIndex, animator.actionLayer.currentTime, runtime.actionPoses);
            }
            ForceRootResetXZ(runtime.actionPoses, runtime.rootNodeIndex);

            if (animator.actionLayer.isBlending && animator.actionLayer.prevAnimIndex >= 0 &&
                animator.actionLayer.prevAnimIndex < static_cast<int>(model->GetAnimations().size())) {
                FillBindPose(runtime.tempPoses, model);
                model->ComputeAnimation(animator.actionLayer.prevAnimIndex, animator.actionLayer.prevAnimTime, runtime.tempPoses);
                ForceRootResetXZ(runtime.tempPoses, runtime.rootNodeIndex);
                const float t = animator.actionLayer.blendDuration > 0.0f
                    ? (std::min)(1.0f, animator.actionLayer.blendTimer / animator.actionLayer.blendDuration)
                    : 1.0f;
                BlendPoses(runtime.tempPoses, runtime.actionPoses, t, runtime.rootNodeIndex, runtime.actionPoses);
            }
        }

        const size_t count = runtime.finalPoses.size();
        if (runtime.basePoses.size() != count) {
            FillBindPose(runtime.finalPoses, model);
        } else {
            for (size_t i = 0; i < count; ++i) {
                runtime.finalPoses[i] = runtime.basePoses[i];
                if (hasAction && i < runtime.isUpperBody.size()) {
                    if (animator.actionLayer.isFullBody || runtime.isUpperBody[i]) {
                        runtime.finalPoses[i] = runtime.actionPoses[i];
                    }
                }
            }
        }

        ApplyOffsetBlending(runtime, runtime.finalPoses);
        ForceRootResetXZ(runtime.finalPoses, runtime.rootNodeIndex);
        model->SetNodePoses(runtime.finalPoses);
    });
}
