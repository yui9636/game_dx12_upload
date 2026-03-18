#pragma once
#include "Component/Component.h"
#include <string>
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <map> // mapが抜けていたので追加
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
    // 再生制御 API
    // -------------------------------------------------------
    void PlayBase(int animIndex, bool loop = true, float blendTime = 0.2f, float speed = 1.0f);
    // アクションレイヤー (攻撃用)
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

    // ★修正: ::Model::NodePose に統一して曖昧さを回避
    void ComputeLayerPose(AnimLayer& layer, std::vector<::Model::NodePose>& outPoses);

    void CaptureTransitionOffsets(const std::vector<::Model::NodePose>& currentFinalPose, int nextAnimIndex);
    void UpdateOffsetBlending(float dt);
    void ApplyOffsetBlending(std::vector<::Model::NodePose>& poses);

    void BuildBoneMask();
    void ComputeRootMotion(const AnimLayer& layer, float prevT, float currentT);
    void ForceRootResetXZ(std::vector<::Model::NodePose>& poses);

    // ★修正: 引数すべてに ::Model::NodePose を適用
    void BlendPoses(const std::vector<::Model::NodePose>& src, const std::vector<::Model::NodePose>& dst, float t, std::vector<::Model::NodePose>& out);

    DirectX::XMFLOAT3 SampleRootPos(int animIndex, float time);

private:
    // ★修正: ここも ::Model に
    ::Model* modelRef = nullptr;

    AnimLayer baseLayer;   // 下半身 (移動)
    AnimLayer actionLayer; // 上半身 (攻撃)

    std::vector<bool> isUpperBody;
    int rootNodeIndex = -1;
    int pelvisNodeIndex = -1;
    int spineNodeIndex = -1;

    // ★修正: std::vector<::Model::NodePose> に統一
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
    // ■ シーケンサー連携用 API
    std::vector<std::string> GetAnimationNameList() const;
    int GetAnimationIndexByName(const std::string& name) const;

    void SetDriver(IAnimationDriver* driver) { currentDriver = driver; }

private:
    IAnimationDriver* currentDriver = nullptr;
    mutable std::map<std::string, int> animNameCache;
};