#include "AnimatorSystem.h"

#include "AnimatorComponent.h"
#include "AnimatorRuntime.h"
#include "AnimatorService.h"
#include "Component/MeshComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <vector>

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

    static void LoadRuntimeBindPose(std::vector<Model::NodePose>& poses, const AnimatorRuntimeEntry& runtime, const Model* model)
    {
        if (!runtime.bindPoses.empty()) {
            poses = runtime.bindPoses;
            return;
        }

        FillBindPose(poses, model);
    }

    static void ForceRootMotionReset(
        std::vector<Model::NodePose>& poses,
        int rootMotionNodeIndex,
        const XMFLOAT3& bindPosition,
        bool bakeY)
    {
        if (rootMotionNodeIndex < 0 || rootMotionNodeIndex >= static_cast<int>(poses.size())) {
            return;
        }

        poses[rootMotionNodeIndex].position.x = bindPosition.x;
        poses[rootMotionNodeIndex].position.z = bindPosition.z;
        if (bakeY) {
            poses[rootMotionNodeIndex].position.y = bindPosition.y;
        }
    }

    static void ResetRuntimeRootMotionNodes(std::vector<Model::NodePose>& poses, const AnimatorRuntimeEntry& runtime, bool bakeY)
    {
        ForceRootMotionReset(poses, runtime.rootNodeIndex, runtime.rootNodeBindPosition, bakeY);
        if (runtime.rootMotionNodeIndex != runtime.rootNodeIndex) {
            ForceRootMotionReset(poses, runtime.rootMotionNodeIndex, runtime.rootMotionBindPosition, bakeY);
        }
    }

    static void UpdateLayer(Model* model, AnimatorComponent::LayerState& layer, float dt, bool autoAdvance)
    {
        if (!model || layer.currentAnimIndex < 0 || layer.currentAnimIndex >= static_cast<int>(model->GetAnimations().size())) {
            return;
        }

        const auto& anim = model->GetAnimations()[layer.currentAnimIndex];
        const float maxTime = anim.secondsLength;
        if (maxTime <= 0.0f) {
            layer.currentTime = 0.0f;
            return;
        }

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

    static float SmoothStep01(float t)
    {
        t = (std::clamp)(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    static XMVECTOR LoadUnitQuaternionOrIdentity(const XMFLOAT4& value)
    {
        XMVECTOR q = XMLoadFloat4(&value);
        float lengthSq = 0.0f;
        XMStoreFloat(&lengthSq, XMVector4LengthSq(q));
        if (lengthSq <= 0.00000001f) {
            return XMQuaternionIdentity();
        }

        return XMQuaternionNormalize(q);
    }

    static XMVECTOR BlendQuaternionShortestPath(const XMFLOAT4& src, const XMFLOAT4& dst, float t)
    {
        XMVECTOR q0 = LoadUnitQuaternionOrIdentity(src);
        XMVECTOR q1 = LoadUnitQuaternionOrIdentity(dst);

        float dot = 0.0f;
        XMStoreFloat(&dot, XMVector4Dot(q0, q1));
        if (dot < 0.0f) {
            q1 = XMVectorNegate(q1);
            dot = -dot;
        }

        XMVECTOR q = dot > 0.9995f
            ? XMQuaternionNormalize(XMVectorLerp(q0, q1, t))
            : XMQuaternionSlerp(q0, q1, t);

        return XMQuaternionNormalize(q);
    }

    static void BlendPoseHierarchy(
        const std::vector<Model::NodePose>& src,
        const std::vector<Model::NodePose>& dst,
        const std::vector<Model::Node>& nodes,
        float t,
        std::vector<Model::NodePose>& out)
    {
        if (src.size() != dst.size() || src.size() != nodes.size()) {
            out = dst;
            return;
        }

        const size_t count = src.size();
        const float blendT = SmoothStep01(t);

        std::vector<Model::NodePose> tmp(count);

        for (size_t i = 0; i < count; ++i) {
            const Model::NodePose& s = src[i];
            const Model::NodePose& d = dst[i];

            const XMVECTOR srcPos = XMLoadFloat3(&s.position);
            const XMVECTOR dstPos = XMLoadFloat3(&d.position);
            XMStoreFloat3(&tmp[i].position, XMVectorLerp(srcPos, dstPos, blendT));

            const XMVECTOR rotation = BlendQuaternionShortestPath(s.rotation, d.rotation, blendT);
            XMStoreFloat4(&tmp[i].rotation, rotation);

            const XMVECTOR srcScale = XMLoadFloat3(&s.scale);
            const XMVECTOR dstScale = XMLoadFloat3(&d.scale);
            XMStoreFloat3(&tmp[i].scale, XMVectorLerp(srcScale, dstScale, blendT));
        }

        out = std::move(tmp);
    }

    static void ComputeLayerPose(
        Model* model,
        AnimatorRuntimeEntry& runtime,
        bool bakeRootMotionY,
        const AnimatorComponent::LayerState& layer,
        const std::vector<Model::NodePose>& blendFromPoses,
        std::vector<Model::NodePose>& out)
    {
        const bool useBlendFromAsBase =
            layer.isBlending &&
            layer.prevAnimIndex >= 0 &&
            layer.prevAnimIndex < static_cast<int>(model->GetAnimations().size()) &&
            blendFromPoses.size() == runtime.bindPoses.size() &&
            !blendFromPoses.empty();

        // Seed 'out' with the previous pose during a blend so that bones
        // without a fresh keyframe value at this time stay at their last
        // animated value instead of snapping to bind (T) pose.
        if (useBlendFromAsBase) {
            out = blendFromPoses;
        }
        else {
            LoadRuntimeBindPose(out, runtime, model);
        }

        if (layer.currentAnimIndex >= 0 && layer.currentAnimIndex < static_cast<int>(model->GetAnimations().size())) {
            model->ComputeAnimation(layer.currentAnimIndex, layer.currentTime, out);
        }

        ResetRuntimeRootMotionNodes(out, runtime, bakeRootMotionY);

        if (useBlendFromAsBase) {
            if (blendFromPoses.size() != out.size()) {
                return;
            }

            runtime.tempPoses = blendFromPoses;
            ResetRuntimeRootMotionNodes(runtime.tempPoses, runtime, bakeRootMotionY);

            const float t = layer.blendDuration > 0.0f
                ? (std::clamp)(layer.blendTimer / layer.blendDuration, 0.0f, 1.0f)
                : 1.0f;

            BlendPoseHierarchy(runtime.tempPoses, out, model->GetNodes(), t, out);
        }
    }
}

void AnimatorSystem::Update(Registry& registry, float dt)
{
    AnimatorService::Instance().SetRegistry(&registry);

    AnimatorRuntimeRegistry& runtimeRegistry = AnimatorService::Instance().GetRuntimeRegistry();

    Query<MeshComponent, AnimatorComponent> query(registry);

    query.ForEachWithEntity([&](EntityID entity, MeshComponent& mesh, AnimatorComponent& animator) {
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

        if (auto* playback = registry.GetComponent<PlaybackComponent>(entity)) {
            animator.baseLayer.currentTime = playback->currentSeconds;
            animator.baseLayer.currentSpeed = playback->playSpeed;
            animator.baseLayer.isLoop = playback->looping;
            UpdateLayer(model, animator.baseLayer, updateDt, false);
        }
        else {
            UpdateLayer(model, animator.baseLayer, updateDt, true);
        }

        if (animator.actionLayer.currentAnimIndex >= 0 && animator.actionLayer.weight > 0.0f) {
            UpdateLayer(model, animator.actionLayer, updateDt, true);
        }

        const bool hasAction = (animator.actionLayer.currentAnimIndex >= 0 && animator.actionLayer.weight > 0.0f);

        if (animator.baseLayer.currentAnimIndex >= 0) {
            ComputeLayerPose(model, runtime, animator.bakeRootMotionY, animator.baseLayer, runtime.baseBlendFromPoses, runtime.basePoses);
        }
        else {
            LoadRuntimeBindPose(runtime.basePoses, runtime, model);
        }

        if (hasAction) {
            ComputeLayerPose(model, runtime, animator.bakeRootMotionY, animator.actionLayer, runtime.actionBlendFromPoses, runtime.actionPoses);
        }

        const size_t count = runtime.finalPoses.size();
        if (runtime.basePoses.size() != count) {
            LoadRuntimeBindPose(runtime.finalPoses, runtime, model);
        }
        else {
            for (size_t i = 0; i < count; ++i) {
                runtime.finalPoses[i] = runtime.basePoses[i];

                if (hasAction && i < runtime.isUpperBody.size()) {
                    if (animator.actionLayer.isFullBody || runtime.isUpperBody[i]) {
                        runtime.finalPoses[i] = runtime.actionPoses[i];
                    }
                }
            }
        }

        ResetRuntimeRootMotionNodes(runtime.finalPoses, runtime, animator.bakeRootMotionY);
        model->SetNodePoses(runtime.finalPoses);
        runtime.hasValidFinalPose = true;
    });
}
