#pragma once
#include "Component/Component.h"
#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <map>
#include "Model/Model.h" 

class IAnimationDriver;

class AnimatorComponent : public Component
{
public:
    const char* GetName() const override { return "Animator"; }

    void Start() override;
    void Update(float dt) override;
    void OnGUI() override;

    // -------------------------------------------------------
    // -------------------------------------------------------
    void PlayBase(int animIndex, bool loop = true, float blendTime = 0.2f, float speed = 1.0f);
    void PlayAction(int animIndex, bool loop = false, float blendTime = 0.1f, bool isFullBody = true);
    void StopAction(float blendTime = 0.2f);
    void SetActionTime(float time);

    int GetBaseAnimIndex() const { return baseLayer.currentAnimIndex; }
    int GetActionAnimIndex() const { return actionLayer.currentAnimIndex; }

    const DirectX::XMFLOAT3& GetRootMotionDelta() const { return rootMotionDelta; }
    void SetRootMotionEnabled(bool enabled) { enableRootMotion = enabled; }

private:
    struct AnimLayer {
        int   currentAnimIndex = -1;
        float currentTime = 0.0f;
        float currentSpeed = 1.0f;
        bool  isLoop = true;
        float weight = 0.0f;
        bool isFullBody = false;

        int   prevAnimIndex = -1;
        float prevAnimTime = 0.0f;
        float blendDuration = 0.0f;
        float blendTimer = 0.0f;
        bool  isBlending = false;
    };

    void UpdateLayer(AnimLayer& layer, float dt, bool autoAdvance);

    void ComputeLayerPose(AnimLayer& layer, std::vector<::Model::NodePose>& outPoses);

    void CaptureTransitionOffsets(const std::vector<::Model::NodePose>& currentFinalPose, int nextAnimIndex);
    void UpdateOffsetBlending(float dt);
    void ApplyOffsetBlending(std::vector<::Model::NodePose>& poses);

    void BuildBoneMask();
    void ComputeRootMotion(const AnimLayer& layer, float prevT, float currentT);
    void ForceRootResetXZ(std::vector<::Model::NodePose>& poses);

    void BlendPoses(const std::vector<::Model::NodePose>& src, const std::vector<::Model::NodePose>& dst, float t, std::vector<::Model::NodePose>& out);

    DirectX::XMFLOAT3 SampleRootPos(int animIndex, float time);

private:
    ::Model* modelRef = nullptr;

    AnimLayer baseLayer;
    AnimLayer actionLayer;

    std::vector<bool> isUpperBody;
    int rootNodeIndex = -1;
    int pelvisNodeIndex = -1;
    int spineNodeIndex = -1;

    std::vector<::Model::NodePose> basePoses;
    std::vector<::Model::NodePose> actionPoses;
    std::vector<::Model::NodePose> tempPoses;
    std::vector<::Model::NodePose> finalPoses;

    std::vector<::Model::NodePose> blendOffsets;
    float offsetBlendDuration = 0.0f;
    float offsetBlendTimer = 0.0f;
    bool  useOffsetBlending = false;

    bool enableRootMotion = true;
    bool bakeRootMotionY = false;
    float prevActionTime = 0.0f;

    float RootMotionScale = 0.1f;

    DirectX::XMFLOAT3 rootMotionDelta{ 0,0,0 };

public:
    std::vector<std::string> GetAnimationNameList() const;
    int GetAnimationIndexByName(const std::string& name) const;

    void SetDriver(IAnimationDriver* driver) { currentDriver = driver; }

private:
    IAnimationDriver* currentDriver = nullptr;
    mutable std::map<std::string, int> animNameCache;
};
