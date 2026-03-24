#include "AnimatorComponent.h"
#include "Actor/Actor.h"
#include "Model/Model.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <stack>
#include "easing.h" 
#include "IAnimationDriver.h"

using namespace DirectX;

static void FillBindPose(std::vector<Model::NodePose>& poses, const Model* model) {
    if (!model) return;
    const auto& nodes = model->GetNodes();
    poses.resize(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
        poses[i].position = nodes[i].position;
        poses[i].rotation = nodes[i].rotation;
        poses[i].scale = nodes[i].scale;
    }
}


std::vector<std::string> AnimatorComponent::GetAnimationNameList() const
{
    std::vector<std::string> names;

    ::Model* targetModel = modelRef;

    if (!targetModel)
    {
        auto* self = const_cast<AnimatorComponent*>(this);
        if (auto owner = self->GetActor())
        {
            targetModel = owner->GetModelRaw();
        }
    }

    if (!targetModel) return names;

    if (targetModel->GetAnimations().empty()) return names;

    for (const auto& anim : targetModel->GetAnimations())
    {
        names.push_back(anim.name);
    }

    return names;
}

int AnimatorComponent::GetAnimationIndexByName(const std::string& name) const
{
    if (!modelRef) return -1;

    auto it = animNameCache.find(name);
    if (it != animNameCache.end())
    {
        return it->second;
    }

    const auto& anims = modelRef->GetAnimations();
    for (int i = 0; i < (int)anims.size(); ++i)
    {
        if (anims[i].name == name)
        {
            animNameCache[name] = i;
            return i;
        }
    }

    return -1;
}

void AnimatorComponent::Start()
{
    if (auto owner = GetActor()) {
        if (auto model = owner->GetModelRaw()) {
            modelRef = model;

            rootNodeIndex = 1;

            pelvisNodeIndex = model->GetNodeIndex("pelvis");
            spineNodeIndex = model->GetNodeIndex("spine_01");

            BuildBoneMask();

            size_t count = model->GetNodes().size();
            basePoses.resize(count);
            actionPoses.resize(count);
            tempPoses.resize(count);
            finalPoses.resize(count);

            blendOffsets.resize(count);

            FillBindPose(finalPoses, modelRef);
        }
    }
}

void AnimatorComponent::BuildBoneMask()
{
    if (!modelRef) return;
    const auto& nodes = modelRef->GetNodes();
    isUpperBody.assign(nodes.size(), false);

    if (spineNodeIndex < 0) return;

    std::stack<int> stack;
    stack.push(spineNodeIndex);

    while (!stack.empty()) {
        int idx = stack.top();
        stack.pop();
        isUpperBody[idx] = true;
        const auto& node = nodes[idx];
        for (auto* child : node.children) {
            int childIdx = (int)(child - &nodes[0]);
            if (childIdx >= 0 && childIdx < (int)nodes.size()) {
                stack.push(childIdx);
            }
        }
    }
}

void AnimatorComponent::ForceRootResetXZ(std::vector<Model::NodePose>& poses)
{
    if (rootNodeIndex >= 0 && rootNodeIndex < (int)poses.size()) {
        poses[rootNodeIndex].position.x = 0.0f;
        poses[rootNodeIndex].position.y = 0.0f;
        poses[rootNodeIndex].position.z = 0.0f;


    }
}

void AnimatorComponent::CaptureTransitionOffsets(const std::vector<Model::NodePose>& currentPose, int nextAnimIndex)
{
    if (!modelRef) return;

    FillBindPose(tempPoses, modelRef);
    modelRef->ComputeAnimation(nextAnimIndex, 0.0f, tempPoses);
    ForceRootResetXZ(tempPoses);

    size_t count = currentPose.size();
    for (size_t i = 0; i < count; ++i) {
        XMVECTOR P_Curr = XMLoadFloat3(&currentPose[i].position);
        XMVECTOR P_Next = XMLoadFloat3(&tempPoses[i].position);
        XMVECTOR P_Diff = P_Curr - P_Next;
        P_Diff = XMVectorSetY(P_Diff, 0.0f);
        XMStoreFloat3(&blendOffsets[i].position, P_Diff);

        XMVECTOR Q_Curr = XMLoadFloat4(&currentPose[i].rotation);
        XMVECTOR Q_Next = XMLoadFloat4(&tempPoses[i].rotation);
        XMVECTOR Q_InvNext = XMQuaternionInverse(Q_Next);
        XMVECTOR Q_Diff = XMQuaternionMultiply(Q_InvNext, Q_Curr);

        if (i == rootNodeIndex) {
            Q_Diff = XMQuaternionIdentity();
        }

        XMStoreFloat4(&blendOffsets[i].rotation, Q_Diff);

        XMVECTOR S_Curr = XMLoadFloat3(&currentPose[i].scale);
        XMVECTOR S_Next = XMLoadFloat3(&tempPoses[i].scale);
        XMStoreFloat3(&blendOffsets[i].scale, S_Curr - S_Next);
    }
}

void AnimatorComponent::UpdateOffsetBlending(float dt)
{
    if (!useOffsetBlending) return;

    offsetBlendTimer += dt;
    if (offsetBlendTimer >= offsetBlendDuration) {
        useOffsetBlending = false;
        offsetBlendTimer = 0.0f;
    }
}

void AnimatorComponent::ApplyOffsetBlending(std::vector<Model::NodePose>& poses)
{
    if (!useOffsetBlending) return;

    float t = offsetBlendTimer / offsetBlendDuration;
    if (t > 1.0f) t = 1.0f;

    float decay = 1.0f - Easing::easeOutCubic(t);

    size_t count = poses.size();
    for (size_t i = 0; i < count; ++i) {
        XMVECTOR P_Base = XMLoadFloat3(&poses[i].position);
        XMVECTOR P_Off = XMLoadFloat3(&blendOffsets[i].position);
        P_Base += P_Off * decay;
        XMStoreFloat3(&poses[i].position, P_Base);

        XMVECTOR Q_Base = XMLoadFloat4(&poses[i].rotation);
        XMVECTOR Q_Off = XMLoadFloat4(&blendOffsets[i].rotation);
        XMVECTOR Q_Identity = XMQuaternionIdentity();

        XMVECTOR Q_DecayedOff = XMQuaternionSlerp(Q_Identity, Q_Off, decay);
        XMVECTOR Q_Final = XMQuaternionMultiply(Q_Base, Q_DecayedOff);
        XMStoreFloat4(&poses[i].rotation, Q_Final);

        XMVECTOR S_Base = XMLoadFloat3(&poses[i].scale);
        XMVECTOR S_Off = XMLoadFloat3(&blendOffsets[i].scale);
        S_Base += S_Off * decay;
        XMStoreFloat3(&poses[i].scale, S_Base);
    }
}

void AnimatorComponent::PlayBase(int animIndex, bool loop, float blendTime, float speed)
{
    if (!modelRef) return;
    if (animIndex < 0) return;

    auto& L = baseLayer;
    if (L.currentAnimIndex == animIndex) {
        L.isLoop = loop;
        L.currentSpeed = speed;
        return;
    }

    if (L.currentAnimIndex >= 0 && blendTime > 0.0f) {
        CaptureTransitionOffsets(finalPoses, animIndex);

        useOffsetBlending = true;
        offsetBlendDuration = blendTime;
        offsetBlendTimer = 0.0f;
    }
    else {
        useOffsetBlending = false;
    }

    L.currentAnimIndex = animIndex;
    L.isLoop = loop;
    L.currentSpeed = speed;
    L.currentTime = 0.0f;
    L.weight = 1.0f;
}

void AnimatorComponent::PlayAction(int animIndex, bool loop, float blendTime, bool isFullBody)
{
    if (!modelRef) return;
    const auto& anims = modelRef->GetAnimations();
    if (animIndex < 0 || animIndex >= (int)anims.size()) return;

    auto& L = actionLayer;

    bool isAlreadyPlaying = (L.currentAnimIndex >= 0 && L.weight > 0.01f);

    if (isAlreadyPlaying && blendTime > 0.0f) {
        L.prevAnimIndex = L.currentAnimIndex;
        L.prevAnimTime = L.currentTime;
        L.isBlending = true;
        L.blendDuration = blendTime;
        L.blendTimer = 0.0f;
    }
    else {
        L.isBlending = false;
    }

    L.currentAnimIndex = animIndex;
    L.isLoop = loop;
    L.currentSpeed = 1.0f;
    L.currentTime = 0.0f;
    L.weight = 1.0f;
    L.isFullBody = isFullBody;

    prevActionTime = 0.0f;
}

void AnimatorComponent::StopAction(float blendTime)
{
    actionLayer.weight = 0.0f;
    actionLayer.currentAnimIndex = -1;
}

void AnimatorComponent::SetActionTime(float time)
{
    if (!modelRef || actionLayer.currentAnimIndex < 0) return;
    prevActionTime = actionLayer.currentTime;
    actionLayer.currentTime = time;
    actionLayer.isBlending = false;
}
//void AnimatorComponent::Update(float dt)
//{
//    if (!modelRef) {
//        if (auto owner = GetActor()) {
//            if (auto m = owner->GetModelRaw()) {
//                modelRef = m;
//                Start();
//            }
//        }
//        if (!modelRef) return;
//    }
//
//    rootMotionDelta = { 0,0,0 };
//
// 
//    float dt = dt;
//
//    if (currentDriver)
//    {
//        if (!currentDriver->AllowInternalUpdate())
//        {
//            dt = 0.0f;
//        }
//
//        int overrideIdx = currentDriver->GetOverrideAnimationIndex();
//        int animCount = (int)modelRef->GetAnimations().size();
//
//        if (overrideIdx >= 0 && overrideIdx < animCount)
//        {
//            if (actionLayer.currentAnimIndex != overrideIdx)
//            {
//                PlayAction(overrideIdx, true, 0.0f);
//            }
//        }
//
// 
//        float driverTime = currentDriver->GetTime();
//        SetActionTime(driverTime);
//    }
//
// 
//
//    float localPrevBaseTime = baseLayer.currentTime;
//
//    UpdateLayer(baseLayer, dt, true);
//    UpdateOffsetBlending(dt);
//
//    if (actionLayer.currentAnimIndex >= 0 && actionLayer.weight > 0.0f) {
//        UpdateLayer(actionLayer, dt, true);
//    }
//
//    bool hasAction = (actionLayer.currentAnimIndex >= 0 && actionLayer.weight > 0.0f);
//
//    if (hasAction && actionLayer.isFullBody) {
//        if (dt > 0.0001f) {
//            ComputeRootMotion(actionLayer, this->prevActionTime, actionLayer.currentTime);
//        }
//        this->prevActionTime = actionLayer.currentTime;
//    }
//    else {
//        ComputeRootMotion(baseLayer, localPrevBaseTime, baseLayer.currentTime);
//    }
//
//    if (baseLayer.currentAnimIndex >= 0) {
//        ComputeLayerPose(baseLayer, basePoses);
//    }
//    else {
//        FillBindPose(basePoses, modelRef);
//    }
//
//    if (hasAction) {
//        FillBindPose(actionPoses, modelRef);
//        if (actionLayer.currentAnimIndex >= 0 && actionLayer.currentAnimIndex < (int)modelRef->GetAnimations().size()) {
//            modelRef->ComputeAnimation(actionLayer.currentAnimIndex, actionLayer.currentTime, actionPoses);
//        }
//        ForceRootResetXZ(actionPoses);
//
//        if (actionLayer.isBlending && actionLayer.prevAnimIndex >= 0) {
//            if (actionLayer.prevAnimIndex < (int)modelRef->GetAnimations().size()) {
//                FillBindPose(tempPoses, modelRef);
//                modelRef->ComputeAnimation(actionLayer.prevAnimIndex, actionLayer.prevAnimTime, tempPoses);
//                ForceRootResetXZ(tempPoses);
//
//                float t = (actionLayer.blendDuration > 0.0f) ? (actionLayer.blendTimer / actionLayer.blendDuration) : 1.0f;
//                if (t > 1.0f) t = 1.0f;
//                BlendPoses(tempPoses, actionPoses, t, actionPoses);
//            }
//        }
//    }
//
//    size_t count = finalPoses.size();
//    bool isSizeValid = (basePoses.size() == count);
//    if (hasAction && actionPoses.size() != count) isSizeValid = false;
//
//    if (isSizeValid)
//    {
//        for (size_t i = 0; i < count; ++i) {
//            finalPoses[i] = basePoses[i];
//            if (hasAction) {
//                if (actionLayer.isFullBody || isUpperBody[i]) {
//                    finalPoses[i] = actionPoses[i];
//                }
//            }
//        }
//    }
//
//    ApplyOffsetBlending(finalPoses);
//    ForceRootResetXZ(finalPoses);
//
//    modelRef->SetNodePoses(finalPoses);
//}

void AnimatorComponent::Update(float dt)
{
    if (!modelRef) {
        if (auto owner = GetActor()) {
            if (auto m = owner->GetModelRaw()) {
                modelRef = m;
                Start();
            }
        }
        if (!modelRef) return;
    }

    rootMotionDelta = { 0,0,0 };



    if (currentDriver)
    {
        if (!currentDriver->AllowInternalUpdate())
        {
            dt = 0.0f;
        }

        int overrideIdx = currentDriver->GetOverrideAnimationIndex();
        int animCount = (int)modelRef->GetAnimations().size();

        if (overrideIdx >= 0 && overrideIdx < animCount)
        {
            if (actionLayer.currentAnimIndex != overrideIdx)
            {
                PlayAction(overrideIdx, true, 0.0f);
            }
        }

        float driverTime = currentDriver->GetTime();

        if (overrideIdx >= 0 && overrideIdx < animCount)
        {
            float duration = modelRef->GetAnimations()[overrideIdx].secondsLength;

            if (!currentDriver->IsLoop())
            {
                if (driverTime > duration)
                {
                    driverTime = duration;
                }
            }
        }

        SetActionTime(driverTime);
    }


    float localPrevBaseTime = baseLayer.currentTime;

    UpdateLayer(baseLayer, dt, true);
    UpdateOffsetBlending(dt);

    if (actionLayer.currentAnimIndex >= 0 && actionLayer.weight > 0.0f) {
        UpdateLayer(actionLayer, dt, true);
    }

    bool hasAction = (actionLayer.currentAnimIndex >= 0 && actionLayer.weight > 0.0f);

    if (hasAction && actionLayer.isFullBody) {
        if (dt > 0.0001f) {
            ComputeRootMotion(actionLayer, this->prevActionTime, actionLayer.currentTime);
        }
        this->prevActionTime = actionLayer.currentTime;
    }
    else {
        ComputeRootMotion(baseLayer, localPrevBaseTime, baseLayer.currentTime);
    }

    if (baseLayer.currentAnimIndex >= 0) {
        ComputeLayerPose(baseLayer, basePoses);
    }
    else {
        FillBindPose(basePoses, modelRef);
    }

    if (hasAction) {
        FillBindPose(actionPoses, modelRef);
        if (actionLayer.currentAnimIndex >= 0 && actionLayer.currentAnimIndex < (int)modelRef->GetAnimations().size()) {
            modelRef->ComputeAnimation(actionLayer.currentAnimIndex, actionLayer.currentTime, actionPoses);
        }
        ForceRootResetXZ(actionPoses);

        if (actionLayer.isBlending && actionLayer.prevAnimIndex >= 0) {
            if (actionLayer.prevAnimIndex < (int)modelRef->GetAnimations().size()) {
                FillBindPose(tempPoses, modelRef);
                modelRef->ComputeAnimation(actionLayer.prevAnimIndex, actionLayer.prevAnimTime, tempPoses);
                ForceRootResetXZ(tempPoses);

                float t = (actionLayer.blendDuration > 0.0f) ? (actionLayer.blendTimer / actionLayer.blendDuration) : 1.0f;
                if (t > 1.0f) t = 1.0f;
                BlendPoses(tempPoses, actionPoses, t, actionPoses);
            }
        }
    }

    size_t count = finalPoses.size();
    bool isSizeValid = (basePoses.size() == count);
    if (hasAction && actionPoses.size() != count) isSizeValid = false;

    if (isSizeValid)
    {
        for (size_t i = 0; i < count; ++i) {
            finalPoses[i] = basePoses[i];
            if (hasAction) {
                if (actionLayer.isFullBody || isUpperBody[i]) {
                    finalPoses[i] = actionPoses[i];
                }
            }
        }
    }

    ApplyOffsetBlending(finalPoses);
    ForceRootResetXZ(finalPoses);

    modelRef->SetNodePoses(finalPoses);
}


void AnimatorComponent::UpdateLayer(AnimLayer& L, float dt, bool autoAdvance)
{
    if (L.currentAnimIndex < 0) return;
    const auto& anim = modelRef->GetAnimations()[L.currentAnimIndex];
    float maxTime = anim.secondsLength;

    if (autoAdvance) {
        L.currentTime += dt * L.currentSpeed;
        if (L.currentTime >= maxTime) {
            if (L.isLoop) L.currentTime = std::fmod(L.currentTime, maxTime);
            else L.currentTime = maxTime;
        }
    }

    if (L.isBlending) {
        L.blendTimer += dt;
        if (L.blendTimer >= L.blendDuration) {
            L.isBlending = false;
            L.blendTimer = 0.0f;
        }
    }
}

void AnimatorComponent::ComputeLayerPose(AnimLayer& L, std::vector<Model::NodePose>& out)
{
    FillBindPose(out, modelRef);
    modelRef->ComputeAnimation(L.currentAnimIndex, L.currentTime, out);
    ForceRootResetXZ(out);
}

void AnimatorComponent::BlendPoses(const std::vector<Model::NodePose>& src, const std::vector<Model::NodePose>& dst, float t, std::vector<Model::NodePose>& out)
{
    size_t count = src.size();
    for (size_t i = 0; i < count; ++i) {
        XMVECTOR P = XMVectorLerp(XMLoadFloat3(&src[i].position), XMLoadFloat3(&dst[i].position), t);
        if (i == (size_t)rootNodeIndex) {
            float dstY = dst[i].position.y;
            P = XMVectorSetY(P, dstY);
        }
        XMVECTOR R = XMQuaternionSlerp(XMLoadFloat4(&src[i].rotation), XMLoadFloat4(&dst[i].rotation), t);
        XMVECTOR S = XMVectorLerp(XMLoadFloat3(&src[i].scale), XMLoadFloat3(&dst[i].scale), t);
        XMStoreFloat3(&out[i].position, P);
        XMStoreFloat4(&out[i].rotation, R);
        XMStoreFloat3(&out[i].scale, S);
    }
}

DirectX::XMFLOAT3 AnimatorComponent::SampleRootPos(int animIndex, float time)
{
    if (!modelRef || animIndex < 0 || rootNodeIndex < 0) return { 0,0,0 };

    const auto& anim = modelRef->GetAnimations()[animIndex];

    float safeTime = time;
    if (safeTime < 0.0f)
    {
        safeTime = 0.0f;
    }
    else if (safeTime > anim.secondsLength)
    {
        safeTime = anim.secondsLength;
    }

    Model::NodePose pose = {};
    pose.scale = { 1,1,1 };
    pose.rotation = { 0,0,0,1 };

    if (rootNodeIndex < (int)modelRef->GetNodes().size()) {
        pose.position = modelRef->GetNodes()[rootNodeIndex].position;
    }

    modelRef->ComputeAnimation(animIndex, rootNodeIndex, safeTime, pose);

    return pose.position;
}

void AnimatorComponent::ComputeRootMotion(const AnimLayer& layer, float prevTime, float currTime)
{
    if (!enableRootMotion || layer.currentAnimIndex < 0) return;

    XMVECTOR P_Prev = XMLoadFloat3(&SampleRootPos(layer.currentAnimIndex, prevTime));
    XMVECTOR P_Curr = XMLoadFloat3(&SampleRootPos(layer.currentAnimIndex, currTime));
    XMVECTOR V_Delta = P_Curr - P_Prev;

    if (currTime < prevTime)
    {
        float duration = modelRef->GetAnimations()[layer.currentAnimIndex].secondsLength;
        XMVECTOR P_End = XMLoadFloat3(&SampleRootPos(layer.currentAnimIndex, duration));
        XMVECTOR P_Start = XMLoadFloat3(&SampleRootPos(layer.currentAnimIndex, 0.0f));

        V_Delta = (P_End - P_Prev) + (P_Curr - P_Start);
    }

    if (auto owner = GetActor())
    {
        XMVECTOR Q_Actor = XMLoadFloat4(&owner->GetRotation());
        V_Delta = XMVector3Rotate(V_Delta, Q_Actor);

        float scale = owner->GetScale().x;
        V_Delta *= scale * RootMotionScale;
    }

    if (!bakeRootMotionY)
    {
        V_Delta = XMVectorSetY(V_Delta, 0.0f);
    }

    XMFLOAT3 delta;
    XMStoreFloat3(&delta, V_Delta);

    if (std::isnan(delta.x) || std::isnan(delta.z)) {
        rootMotionDelta = { 0,0,0 };
    }
    else {
        rootMotionDelta = delta;
    }
}

void AnimatorComponent::OnGUI()
{
    
        ImGui::Text("Base   Anim: %d", baseLayer.currentAnimIndex);
        ImGui::Text("Offset Blend: %s (%.2f / %.2f)", useOffsetBlending ? "ON" : "OFF", offsetBlendTimer, offsetBlendDuration);

        ImGui::Text("Root Delta: (%.4f, %.4f, %.4f)",
            rootMotionDelta.x, rootMotionDelta.y, rootMotionDelta.z);

    
}
