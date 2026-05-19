// PlayerEditor の中核パネル。ライフサイクル、上位 Draw / DockSpace、ツールバー、
// プレビューエンティティ / モデル設定、プレハブ保存の委譲を扱う。用途別の
// 描画処理は兄弟ファイルへ分割している:
//   PlayerEditorViewportPanel.cpp     — 3D ビューポート + カメラ補助
//   PlayerEditorSkeletonPanel.cpp     — ボーンツリー、ソケット、永続コライダー
//   PlayerEditorStateMachinePanel.cpp — ステートマシン UI + プリセット
//   PlayerEditorTimelinePanel.cpp     — タイムライン UI + 再生
//   PlayerEditorInspectorPanel.cpp    — プロパティ / アニメータ / 入力パネル
// 共有 static は PlayerEditorPanelInternal.{h,cpp} に置く。
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <unordered_map>

#include <imgui.h>
#include <imgui_internal.h>

#include "Animator/HumanoidRetargeter.h"
#include "Console/Logger.h"
#include "Icon/IconsFontAwesome7.h"
#include "PlayerEditorPanelInternal.h"
#include "PlayerEditorSession.h"
#include "EditorTheme.h"
#include "Component/ColliderComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/RetargetedAnimationComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Input/InputContextComponent.h"
#include "Archetype/Archetype.h"
#include "Type/TypeInfo.h"
#include "Component/ComponentSignature.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "System/PathResolver.h"
#include "System/ResourceManager.h"
#include "System/UndoSystem.h"

using namespace PlayerEditorInternal;

namespace
{
    constexpr const char* kDefaultMixamoReferenceTPosePath =
        "Data/Model/Actor/MixamoReference/MixamoCharacter_TPose.fbx";

    std::string BuildUniqueRetargetedAnimationName(const Model& model, const std::string& sourcePath, const Model::Animation& animation)
    {
        const std::string fileStem = std::filesystem::path(sourcePath).stem().string();
        std::string baseName = animation.name.empty() ? fileStem : animation.name;
        if (!fileStem.empty() && baseName.find(fileStem) == std::string::npos) {
            baseName = fileStem + "_" + baseName;
        }

        std::string uniqueName = baseName;
        int suffix = 1;
        while (model.GetAnimationIndex(uniqueName.c_str()) >= 0) {
            uniqueName = baseName + "_" + std::to_string(suffix++);
        }
        return uniqueName;
    }

    void LogRetargetWarnings(const std::vector<std::string>& warnings)
    {
        for (const std::string& warning : warnings) {
            LOG_WARN("[PlayerEditor][Retarget] %s", warning.c_str());
        }
    }

    std::shared_ptr<Model> LoadMixamoReferenceTPose()
    {
        const std::string resolvedReferencePath = PathResolver::Resolve(kDefaultMixamoReferenceTPosePath);
        if (!std::filesystem::exists(resolvedReferencePath)) {
            LOG_WARN("[PlayerEditor][Retarget] Reference T-pose model not found: %s", kDefaultMixamoReferenceTPosePath);
            return nullptr;
        }

        std::shared_ptr<Model> reference =
            ResourceManager::Instance().CreateModelInstance(kDefaultMixamoReferenceTPosePath, 1.0f, true);
        if (!reference || reference->GetNodes().empty()) {
            LOG_WARN("[PlayerEditor][Retarget] Failed to load reference T-pose model: %s", kDefaultMixamoReferenceTPosePath);
            return nullptr;
        }

        const HumanoidRetarget::HumanoidProfile profile = HumanoidRetarget::AnalyzeSkeleton(*reference);
        if (!profile.valid) {
            LOG_WARN("[PlayerEditor][Retarget] Reference T-pose model is not a supported humanoid: %s", kDefaultMixamoReferenceTPosePath);
            LogRetargetWarnings(profile.warnings);
            return nullptr;
        }
        return reference;
    }

    std::string NormalizeAnimationNodeName(const std::string& name)
    {
        std::string lowered;
        lowered.reserve(name.size());
        for (char c : name) {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }

        const auto hasSidePrefix = [&](char side) {
            return lowered.size() >= 2 &&
                lowered[0] == side &&
                !std::isalnum(static_cast<unsigned char>(lowered[1]));
        };
        const auto hasSideSuffix = [&](char side) {
            return lowered.size() >= 2 &&
                lowered.back() == side &&
                !std::isalnum(static_cast<unsigned char>(lowered[lowered.size() - 2]));
        };

        const bool leftPrefix = hasSidePrefix('l');
        const bool leftSuffix = hasSideSuffix('l');
        const bool rightPrefix = hasSidePrefix('r');
        const bool rightSuffix = hasSideSuffix('r');
        const bool leftSeparated = leftPrefix || leftSuffix;
        const bool rightSeparated = rightPrefix || rightSuffix;

        std::string normalized;
        normalized.reserve(lowered.size());
        for (char c : lowered) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc)) {
                normalized.push_back(static_cast<char>(uc));
            }
        }

        const auto replaceAll = [&](const char* from, const char* to) {
            std::string::size_type pos = 0;
            const size_t fromLen = std::char_traits<char>::length(from);
            const size_t toLen = std::char_traits<char>::length(to);
            while ((pos = normalized.find(from, pos)) != std::string::npos) {
                normalized.replace(pos, fromLen, to);
                pos += toLen;
            }
        };

        replaceAll("mixamorig", "");
        replaceAll("bip001", "");
        replaceAll("upperleg", "upleg");
        replaceAll("lowerleg", "leg");

        if (leftSeparated && normalized.size() > 1) {
            if (leftPrefix && normalized.front() == 'l') {
                normalized.erase(normalized.begin());
            }
            if (leftSuffix && !normalized.empty() && normalized.back() == 'l') {
                normalized.pop_back();
            }
            if (normalized.find("left") == std::string::npos) {
                normalized = "left" + normalized;
            }
        }
        else if (rightSeparated && normalized.size() > 1) {
            if (rightPrefix && normalized.front() == 'r') {
                normalized.erase(normalized.begin());
            }
            if (rightSuffix && !normalized.empty() && normalized.back() == 'r') {
                normalized.pop_back();
            }
            if (normalized.find("right") == std::string::npos) {
                normalized = "right" + normalized;
            }
        }

        return normalized;
    }

    void FillBindNodeAnimation(const Model::Node& node, float length, Model::NodeAnim& out)
    {
        const float endTime = (std::max)(length, 1.0f / 60.0f);
        out.positionKeyframes = {
            Model::VectorKeyframe{ 0.0f, node.position },
            Model::VectorKeyframe{ endTime, node.position }
        };
        out.rotationKeyframes = {
            Model::QuaternionKeyframe{ 0.0f, node.rotation },
            Model::QuaternionKeyframe{ endTime, node.rotation }
        };
        out.scaleKeyframes = {
            Model::VectorKeyframe{ 0.0f, node.scale },
            Model::VectorKeyframe{ endTime, node.scale }
        };
    }

    void EnsureNodeAnimationHasKeys(const Model::Node& node, float length, Model::NodeAnim& anim)
    {
        const float endTime = (std::max)(length, 1.0f / 60.0f);
        if (anim.positionKeyframes.empty()) {
            anim.positionKeyframes.push_back(Model::VectorKeyframe{ 0.0f, node.position });
        }
        if (anim.positionKeyframes.size() == 1) {
            anim.positionKeyframes.push_back(Model::VectorKeyframe{ endTime, anim.positionKeyframes.front().value });
        }
        if (anim.rotationKeyframes.empty()) {
            anim.rotationKeyframes.push_back(Model::QuaternionKeyframe{ 0.0f, node.rotation });
        }
        if (anim.rotationKeyframes.size() == 1) {
            anim.rotationKeyframes.push_back(Model::QuaternionKeyframe{ endTime, anim.rotationKeyframes.front().value });
        }
        if (anim.scaleKeyframes.empty()) {
            anim.scaleKeyframes.push_back(Model::VectorKeyframe{ 0.0f, node.scale });
        }
        if (anim.scaleKeyframes.size() == 1) {
            anim.scaleKeyframes.push_back(Model::VectorKeyframe{ endTime, anim.scaleKeyframes.front().value });
        }
    }

    // Returns 0..1 indicating how much of the source skeleton (by normalized node name)
    // exists in the target skeleton. >= 0.7 means the rigs are effectively the same
    // family (Mixamo->Mixamo, same VRM, etc.) and direct local-rotation copy is the
    // safest and most accurate transfer.
    float ComputeSkeletonNameOverlapRatio(const Model& source, const Model& target)
    {
        const auto& srcNodes = source.GetNodes();
        const auto& tgtNodes = target.GetNodes();
        if (srcNodes.empty() || tgtNodes.empty()) {
            return 0.0f;
        }

        std::unordered_map<std::string, bool> targetNames;
        targetNames.reserve(tgtNodes.size());
        for (const auto& n : tgtNodes) {
            std::string norm = NormalizeAnimationNodeName(n.name);
            if (!norm.empty()) {
                targetNames.emplace(std::move(norm), true);
            }
        }

        int matched = 0;
        int sourceCounted = 0;
        for (const auto& n : srcNodes) {
            std::string norm = NormalizeAnimationNodeName(n.name);
            if (norm.empty()) {
                continue;
            }
            ++sourceCounted;
            if (targetNames.find(norm) != targetNames.end()) {
                ++matched;
            }
        }
        if (sourceCounted <= 0) {
            return 0.0f;
        }
        return static_cast<float>(matched) / static_cast<float>(sourceCounted);
    }

    // For same-skeleton retargeting we copy local rotations verbatim, but Hips position
    // keyframes describe absolute root motion. If the rigs differ in height, the motion
    // path will be off. Scale Hips position keyframes around the bind position so that
    // movement deltas are proportional to the target's height.
    void ScaleHipsRootMotion(
        const Model& source,
        const Model& target,
        Model::Animation& animation)
    {
        const HumanoidRetarget::HumanoidProfile srcProfile = HumanoidRetarget::AnalyzeSkeleton(source);
        const HumanoidRetarget::HumanoidProfile tgtProfile = HumanoidRetarget::AnalyzeSkeleton(target);
        if (!tgtProfile.valid || srcProfile.height <= 0.001f) {
            return;
        }

        const int targetHipsNode = tgtProfile.Get(HumanoidRetarget::BoneSlot::Hips);
        if (targetHipsNode < 0 || targetHipsNode >= static_cast<int>(animation.nodeAnims.size())) {
            return;
        }

        const auto& targetNodes = target.GetNodes();
        if (targetHipsNode >= static_cast<int>(targetNodes.size())) {
            return;
        }

        const int sourceHipsNode = srcProfile.Get(HumanoidRetarget::BoneSlot::Hips);
        const auto& sourceNodes = source.GetNodes();
        if (sourceHipsNode < 0 || sourceHipsNode >= static_cast<int>(sourceNodes.size())) {
            return;
        }

        const float scale = tgtProfile.height / srcProfile.height;
        const DirectX::XMFLOAT3 tgtBindPos = targetNodes[static_cast<size_t>(targetHipsNode)].position;
        const DirectX::XMFLOAT3 srcBindPos = sourceNodes[static_cast<size_t>(sourceHipsNode)].position;
        auto& hipsAnim = animation.nodeAnims[static_cast<size_t>(targetHipsNode)];
        for (auto& kf : hipsAnim.positionKeyframes) {
            kf.value.x = tgtBindPos.x + (kf.value.x - srcBindPos.x) * scale;
            kf.value.y = tgtBindPos.y + (kf.value.y - srcBindPos.y) * scale;
            kf.value.z = tgtBindPos.z + (kf.value.z - srcBindPos.z) * scale;
        }
    }

    // Bind-relative LOCAL rotation transfer (Mecanim Humanoid-style).
    // For each target bone matched to a source bone by normalized name, we treat the
    // source's local rotation as a bind-relative DELTA applied in parent's frame:
    //     delta = inv(R_src_bind_local) * R_src_anim_local
    //     R_tgt_anim_local = R_tgt_bind_local * delta
    // This preserves target's natural rest pose orientation while transferring exactly
    // the rotation the source bone applied, which works correctly even when the two
    // skeletons have slightly different bind orientations / bone roll for the same bone.
    bool TryBakeBindRelativeLocalRetarget(
        const Model& source,
        int sourceAnimationIndex,
        const Model& target,
        Model::Animation& outAnimation,
        int* outMatchedNodeCount)
    {
        using namespace DirectX;

        const auto& sourceAnimations = source.GetAnimations();
        const auto& sourceNodes = source.GetNodes();
        const auto& targetNodes = target.GetNodes();
        if (sourceAnimationIndex < 0 ||
            sourceAnimationIndex >= static_cast<int>(sourceAnimations.size()) ||
            sourceNodes.empty() ||
            targetNodes.empty()) {
            return false;
        }

        const Model::Animation& sourceAnimation = sourceAnimations[sourceAnimationIndex];
        std::unordered_map<std::string, int> sourceNodeByName;
        sourceNodeByName.reserve(sourceNodes.size());
        for (int nodeIndex = 0; nodeIndex < static_cast<int>(sourceNodes.size()); ++nodeIndex) {
            std::string normalized = NormalizeAnimationNodeName(sourceNodes[nodeIndex].name);
            if (!normalized.empty()) {
                sourceNodeByName.emplace(std::move(normalized), nodeIndex);
            }
        }

        outAnimation = Model::Animation{};
        outAnimation.name = sourceAnimation.name;
        outAnimation.secondsLength = (std::max)(sourceAnimation.secondsLength, 1.0f / 60.0f);
        outAnimation.nodeAnims.resize(targetNodes.size());

        const auto quatMul = [](XMVECTOR a, XMVECTOR b) {
            // Row-vector convention: applying (a*b) to v means first a then b.
            return XMQuaternionMultiply(a, b);
        };

        const HumanoidRetarget::HumanoidProfile targetProfile = HumanoidRetarget::AnalyzeSkeleton(target);
        const int targetHipsNode = targetProfile.valid ? targetProfile.Get(HumanoidRetarget::BoneSlot::Hips) : -1;

        int matchedNodeCount = 0;
        for (int targetNodeIndex = 0; targetNodeIndex < static_cast<int>(targetNodes.size()); ++targetNodeIndex) {
            Model::NodeAnim& targetAnim = outAnimation.nodeAnims[targetNodeIndex];
            FillBindNodeAnimation(targetNodes[targetNodeIndex], outAnimation.secondsLength, targetAnim);

            const std::string normalizedTargetName = NormalizeAnimationNodeName(targetNodes[targetNodeIndex].name);
            const auto it = sourceNodeByName.find(normalizedTargetName);
            if (it == sourceNodeByName.end()) {
                continue;
            }
            const int sourceNodeIndex = it->second;
            if (sourceNodeIndex < 0 || sourceNodeIndex >= static_cast<int>(sourceAnimation.nodeAnims.size())) {
                continue;
            }

            const Model::NodeAnim& srcAnim = sourceAnimation.nodeAnims[sourceNodeIndex];
            const Model::Node& srcNode = sourceNodes[static_cast<size_t>(sourceNodeIndex)];
            const Model::Node& tgtNode = targetNodes[static_cast<size_t>(targetNodeIndex)];
            const XMVECTOR srcBindLocal = XMQuaternionNormalize(XMLoadFloat4(&srcNode.rotation));
            const XMVECTOR srcBindInv = XMQuaternionInverse(srcBindLocal);
            const XMVECTOR tgtBindLocal = XMQuaternionNormalize(XMLoadFloat4(&tgtNode.rotation));

            // Rotation: bind-relative delta application.
            if (!srcAnim.rotationKeyframes.empty()) {
                targetAnim.rotationKeyframes.clear();
                targetAnim.rotationKeyframes.reserve(srcAnim.rotationKeyframes.size());
                for (const auto& kf : srcAnim.rotationKeyframes) {
                    const XMVECTOR srcAnimLocal = XMQuaternionNormalize(XMLoadFloat4(&kf.value));
                    const XMVECTOR delta = quatMul(srcBindInv, srcAnimLocal);
                    const XMVECTOR tgtAnimLocal = XMQuaternionNormalize(quatMul(tgtBindLocal, delta));
                    Model::QuaternionKeyframe outKf{};
                    outKf.seconds = kf.seconds;
                    XMStoreFloat4(&outKf.value, tgtAnimLocal);
                    targetAnim.rotationKeyframes.push_back(outKf);
                }
            }

            // Position: only transfer for Hips (root motion). Other bones keep bind.
            // We rebase by source bind + scale by height ratio in ScaleHipsRootMotion later.
            const bool isHips = (targetHipsNode == targetNodeIndex);
            if (isHips && !srcAnim.positionKeyframes.empty()) {
                targetAnim.positionKeyframes = srcAnim.positionKeyframes;
            }
            // Scale: leave at target bind.

            EnsureNodeAnimationHasKeys(tgtNode, outAnimation.secondsLength, targetAnim);
            ++matchedNodeCount;
        }

        if (outMatchedNodeCount) {
            *outMatchedNodeCount = matchedNodeCount;
        }

        // Validate: require at least 8 matched nodes (Hips + spine + 4 limb roots minimum).
        return matchedNodeCount >= 8;
    }

    bool TryBakeAnimationByMatchingNodeNames(
        const Model& source,
        int sourceAnimationIndex,
        const Model& target,
        Model::Animation& outAnimation)
    {
        const auto& sourceAnimations = source.GetAnimations();
        const auto& sourceNodes = source.GetNodes();
        const auto& targetNodes = target.GetNodes();
        if (sourceAnimationIndex < 0 ||
            sourceAnimationIndex >= static_cast<int>(sourceAnimations.size()) ||
            sourceNodes.empty() ||
            targetNodes.empty()) {
            return false;
        }

        const Model::Animation& sourceAnimation = sourceAnimations[sourceAnimationIndex];
        std::unordered_map<std::string, int> sourceNodeByName;
        sourceNodeByName.reserve(sourceNodes.size());
        for (int nodeIndex = 0; nodeIndex < static_cast<int>(sourceNodes.size()); ++nodeIndex) {
            std::string normalized = NormalizeAnimationNodeName(sourceNodes[nodeIndex].name);
            if (!normalized.empty()) {
                sourceNodeByName.emplace(std::move(normalized), nodeIndex);
            }
        }

        outAnimation = Model::Animation{};
        outAnimation.name = sourceAnimation.name;
        outAnimation.secondsLength = (std::max)(sourceAnimation.secondsLength, 1.0f / 60.0f);
        outAnimation.nodeAnims.resize(targetNodes.size());

        int matchedNodeCount = 0;
        std::array<bool, HumanoidRetarget::kBoneSlotCount> matchedSlots{};
        const HumanoidRetarget::HumanoidProfile targetProfile = HumanoidRetarget::AnalyzeSkeleton(target);

        for (int targetNodeIndex = 0; targetNodeIndex < static_cast<int>(targetNodes.size()); ++targetNodeIndex) {
            Model::NodeAnim& targetAnim = outAnimation.nodeAnims[targetNodeIndex];
            FillBindNodeAnimation(targetNodes[targetNodeIndex], outAnimation.secondsLength, targetAnim);

            const std::string normalizedTargetName = NormalizeAnimationNodeName(targetNodes[targetNodeIndex].name);
            const auto sourceIt = sourceNodeByName.find(normalizedTargetName);
            if (sourceIt == sourceNodeByName.end()) {
                continue;
            }

            const int sourceNodeIndex = sourceIt->second;
            if (sourceNodeIndex < 0 || sourceNodeIndex >= static_cast<int>(sourceAnimation.nodeAnims.size())) {
                continue;
            }

            targetAnim = sourceAnimation.nodeAnims[sourceNodeIndex];
            EnsureNodeAnimationHasKeys(targetNodes[targetNodeIndex], outAnimation.secondsLength, targetAnim);
            ++matchedNodeCount;

            if (targetProfile.valid) {
                for (size_t slotIndex = 0; slotIndex < HumanoidRetarget::kBoneSlotCount; ++slotIndex) {
                    if (targetProfile.slots[slotIndex].nodeIndex == targetNodeIndex) {
                        matchedSlots[slotIndex] = true;
                    }
                }
            }
        }

        int matchedRequiredSlots = 0;
        constexpr HumanoidRetarget::BoneSlot kRequiredSlots[] = {
            HumanoidRetarget::BoneSlot::Hips,
            HumanoidRetarget::BoneSlot::Spine,
            HumanoidRetarget::BoneSlot::Head,
            HumanoidRetarget::BoneSlot::LeftUpperArm,
            HumanoidRetarget::BoneSlot::LeftLowerArm,
            HumanoidRetarget::BoneSlot::LeftHand,
            HumanoidRetarget::BoneSlot::RightUpperArm,
            HumanoidRetarget::BoneSlot::RightLowerArm,
            HumanoidRetarget::BoneSlot::RightHand,
            HumanoidRetarget::BoneSlot::LeftUpperLeg,
            HumanoidRetarget::BoneSlot::LeftLowerLeg,
            HumanoidRetarget::BoneSlot::LeftFoot,
            HumanoidRetarget::BoneSlot::RightUpperLeg,
            HumanoidRetarget::BoneSlot::RightLowerLeg,
            HumanoidRetarget::BoneSlot::RightFoot
        };
        for (HumanoidRetarget::BoneSlot slot : kRequiredSlots) {
            if (matchedSlots[static_cast<size_t>(slot)]) {
                ++matchedRequiredSlots;
            }
        }

        const int minNodeMatches = (std::max)(8, static_cast<int>(targetNodes.size() / 5));
        return matchedRequiredSlots >= 10 || matchedNodeCount >= minNodeMatches;
    }
}

// ライフサイクル / プレビューエンティティ / 外部選択（セッション補助へ委譲）
void PlayerEditorPanel::Suspend()
{
    PlayerEditorSession::Suspend(*this);
}

void PlayerEditorPanel::DestroyOwnedPreviewEntity()
{
    PlayerEditorSession::DestroyOwnedPreviewEntity(*this);
}

void PlayerEditorPanel::SetSharedSceneCamera(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, float fovY)
{
    m_sharedSceneCameraPosition = position;
    m_sharedSceneCameraDirection = direction;
    m_sharedSceneCameraFovY = fovY;
}

bool PlayerEditorPanel::ConsumePendingCameraFit(
    DirectX::XMFLOAT3& outTarget,
    float& outRadius,
    DirectX::XMFLOAT3* outForward,
    float* outDistance)
{
    if (!m_hasPendingCameraFit) {
        return false;
    }

    outTarget = m_pendingCameraFitTarget;
    outRadius = m_pendingCameraFitRadius;
    if (outForward) {
        *outForward = m_pendingCameraFitForward;
    }
    if (outDistance) {
        *outDistance = m_pendingCameraFitDistance;
    }
    if (m_pendingCameraFitDistance > 0.0f) {
        m_vpCameraDist = m_pendingCameraFitDistance;
    }
    // 自動 Fit でパンオフセットを必ずリセット
    m_vpCameraPanOffset = { 0.0f, 0.0f, 0.0f };
    // 軌道中心も pending から確定
    m_orbitCenter = m_pendingCameraFitTarget;
    m_orbitCenterValid = true;
    const float forwardLenSq =
        m_pendingCameraFitForward.x * m_pendingCameraFitForward.x +
        m_pendingCameraFitForward.y * m_pendingCameraFitForward.y +
        m_pendingCameraFitForward.z * m_pendingCameraFitForward.z;
    if (forwardLenSq > 0.0001f) {
        const float invLen = 1.0f / std::sqrt(forwardLenSq);
        const float fx = m_pendingCameraFitForward.x * invLen;
        const float fy = std::clamp(m_pendingCameraFitForward.y * invLen, -0.98f, 0.98f);
        const float fz = m_pendingCameraFitForward.z * invLen;
        m_vpCameraYaw = std::atan2(fx, fz);
        m_vpCameraPitch = std::asin(fy);
    }
    m_hasPendingCameraFit = false;
    return true;
}

void PlayerEditorPanel::EnsureOwnedPreviewEntity()
{
    PlayerEditorSession::EnsureOwnedPreviewEntity(*this);
}

void PlayerEditorPanel::SetPreviewEntity(EntityID entity)
{
    PlayerEditorSession::SetPreviewEntity(*this, entity);
}

void PlayerEditorPanel::SyncExternalSelection(EntityID entity, const std::string& modelPath)
{
    PlayerEditorSession::SyncExternalSelection(*this, entity, modelPath);
}

bool PlayerEditorPanel::DrawToolbarButton(const char* label, bool enabled)
{
    if (!enabled) {
        ImGui::BeginDisabled();
    }
    const bool pressed = ImGui::Button(label);
    if (!enabled) {
        ImGui::EndDisabled();
    }
    return enabled && pressed;
}

void PlayerEditorPanel::ResetSelectionState()
{
    m_selectionCtx = SelectionContext::None;
    m_selectedTrackId = -1;
    m_selectedItemIdx = -1;
    m_selectedNodeId = 0;
    m_selectedTransitionId = 0;
    m_selectedBoneIndex = -1;
    m_hoveredBoneIndex = -1;
    m_selectedBoneName.clear();
    m_selectedSocketIdx = -1;
    m_selectedColliderIdx = -1;
    m_playheadFrame = 0;
    m_isPlaying = false;
}

bool PlayerEditorPanel::HasOpenModel() const
{
    return m_model != nullptr;
}

bool PlayerEditorPanel::HasAnyDirtyDocument() const
{
    return m_modelAnimationDirty || m_timelineDirty || m_stateMachineDirty || m_socketDirty || m_colliderDirty || m_inputMappingTab.IsDirty();
}

bool PlayerEditorPanel::HasSelectedEntityContext() const
{
    return !Entity::IsNull(m_selectedEntity) && !m_selectedEntityModelPath.empty();
}

bool PlayerEditorPanel::CanUsePreviewEntity() const
{
    return m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity);
}

bool PlayerEditorPanel::OpenModelFromPath(const std::string& path)
{
    return PlayerEditorSession::OpenModelFromPath(*this, path);
}

bool PlayerEditorPanel::OpenAnimationFromPath(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    if (!m_model) {
        LOG_WARN("[PlayerEditor][Retarget] Target player model is not loaded.");
        return false;
    }

    Model* targetModel = m_ownedModel ? m_ownedModel.get() : const_cast<Model*>(m_model);
    if (!targetModel) {
        LOG_WARN("[PlayerEditor][Retarget] Target player model is not editable.");
        return false;
    }

    std::shared_ptr<Model> sourceModel = ResourceManager::Instance().CreateModelInstance(path, 1.0f, true);
    if (!sourceModel) {
        LOG_ERROR("[PlayerEditor][Retarget] Failed to load animation source: %s", path.c_str());
        return false;
    }

    const auto& sourceAnimations = sourceModel->GetAnimations();
    if (sourceAnimations.empty()) {
        LOG_WARN("[PlayerEditor][Retarget] Animation source has no clips: %s", path.c_str());
        return false;
    }

    const HumanoidRetarget::HumanoidProfile targetProfile = HumanoidRetarget::AnalyzeSkeleton(*targetModel);
    if (!targetProfile.valid) {
        LOG_ERROR("[PlayerEditor][Retarget] Target model is not a supported humanoid.");
        LogRetargetWarnings(targetProfile.warnings);
        return false;
    }

    HumanoidRetarget::RetargetOptions options{};
    options.sampleRate = 60.0f;
    options.transferRootMotion = true;
    options.keepTargetScale = true;

    // Path priority:
    //   1. Name overlap >= 70%: name-based bind-relative LOCAL transfer. Preserves
    //      finger/twist bones and other non-humanoid bones via direct name matching.
    //      Best path for same-rig retargets (Mixamo->Mixamo with different proportions).
    //   2. Reference T-pose limb calibration. The neutral Mixamo skeleton is used only
    //      to correct arm rest-pose direction before local bind-relative transfer.
    //   3. Slot-based bind-relative LOCAL transfer (HumanoidRetarget::
    //      BakeAnimationLocalBindRelative). Pairs bones via humanoid slot identity, not
    //      by name. Works across naming conventions and across T-pose vs A-pose targets
    //      because target's own bind orientation is preserved.
    //   4. World-delta retarget fallback (HumanoidRetarget::BakeAnimation) - accumulates
    //      global rotations; useful when bind orientations are mismatched in ways the
    //      local-bind-relative path can't capture.
    //   5. Last-resort direct local-rotation copy.
    const float nameOverlap = ComputeSkeletonNameOverlapRatio(*sourceModel, *targetModel);
    const HumanoidRetarget::HumanoidProfile sourceProfile = HumanoidRetarget::AnalyzeSkeleton(*sourceModel);
    std::shared_ptr<Model> referenceTPoseModel = LoadMixamoReferenceTPose();
    const float referenceOverlap = referenceTPoseModel
        ? ComputeSkeletonNameOverlapRatio(*sourceModel, *referenceTPoseModel)
        : 0.0f;
    const bool useNameBindRelative = nameOverlap >= 0.7f;
    const bool useReferenceTPose =
        !useNameBindRelative &&
        referenceTPoseModel &&
        sourceProfile.valid &&
        targetProfile.valid &&
        referenceOverlap >= 0.55f;
    const bool useSlotBindRelative = sourceProfile.valid && targetProfile.valid;
    const char* pathName =
        useNameBindRelative ? "name bind-relative local"
        : useReferenceTPose ? "reference T-pose limb calibration"
        : useSlotBindRelative ? "slot bind-relative local"
        : "world-delta slot-matched";
    LOG_INFO("[PlayerEditor][Retarget] Source bones: %zu, target bones: %zu, name overlap %.0f%%, reference overlap %.0f%%, path: %s",
        sourceModel->GetNodes().size(),
        targetModel->GetNodes().size(),
        nameOverlap * 100.0f,
        referenceOverlap * 100.0f,
        pathName);

    int importedCount = 0;
    int lastImportedIndex = -1;
    std::vector<Model::Animation> importedAnimations;
    for (int animationIndex = 0; animationIndex < static_cast<int>(sourceAnimations.size()); ++animationIndex) {
        HumanoidRetarget::RetargetResult result{};
        Model::Animation importedAnimation{};
        bool accepted = false;

        if (useNameBindRelative) {
            int matched = 0;
            if (TryBakeBindRelativeLocalRetarget(*sourceModel, animationIndex, *targetModel, importedAnimation, &matched)) {
                ScaleHipsRootMotion(*sourceModel, *targetModel, importedAnimation);
                if (animationIndex == 0) {
                    LOG_INFO("[PlayerEditor][Retarget] Name bind-relative matched %d/%zu target bones.",
                        matched, targetModel->GetNodes().size());
                }
                accepted = true;
            }
        }

        if (!accepted && useReferenceTPose) {
            result = HumanoidRetarget::BakeAnimationWithReferenceTPose(
                *sourceModel,
                animationIndex,
                *targetModel,
                *referenceTPoseModel,
                options);
            if (result.success) {
                importedAnimation = std::move(result.animation);
                if (animationIndex == 0) {
                    LOG_INFO("[PlayerEditor][Retarget] Reference T-pose model: %s", kDefaultMixamoReferenceTPosePath);
                }
                accepted = true;
            }
        }

        if (!accepted && useSlotBindRelative) {
            result = HumanoidRetarget::BakeAnimationLocalBindRelative(*sourceModel, animationIndex, *targetModel, options);
            if (result.success) {
                importedAnimation = std::move(result.animation);
                accepted = true;
            }
        }

        if (!accepted) {
            result = HumanoidRetarget::BakeAnimation(*sourceModel, animationIndex, *targetModel, options);
            if (result.success) {
                importedAnimation = std::move(result.animation);
                accepted = true;
            }
            else if (TryBakeAnimationByMatchingNodeNames(*sourceModel, animationIndex, *targetModel, importedAnimation)) {
                accepted = true;
            }
        }

        if (!accepted) {
            LOG_WARN("[PlayerEditor][Retarget] Rejected clip [%d] %s", animationIndex, sourceAnimations[animationIndex].name.c_str());
            LogRetargetWarnings(result.warnings);
            continue;
        }

        importedAnimation.name = BuildUniqueRetargetedAnimationName(*targetModel, path, importedAnimation);
        lastImportedIndex = targetModel->AddAnimation(importedAnimation);
        importedAnimations.push_back(importedAnimation);
        ++importedCount;
    }

    if (importedCount <= 0) {
        LOG_ERROR("[PlayerEditor][Retarget] No compatible humanoid animation was imported: %s", path.c_str());
        return false;
    }

    EnsureOwnedPreviewEntity();
    if (CanUsePreviewEntity()) {
        if (auto* retargeted = m_registry->GetComponent<RetargetedAnimationComponent>(m_previewEntity)) {
            retargeted->animations.insert(retargeted->animations.end(), importedAnimations.begin(), importedAnimations.end());
        }
        else {
            RetargetedAnimationComponent component{};
            component.animations = importedAnimations;
            m_registry->AddComponent(m_previewEntity, std::move(component));
        }
    }
    else {
        LOG_WARN("[PlayerEditor][Retarget] Imported animations cannot be saved until a preview entity exists.");
    }

    m_selectedAnimIndex = lastImportedIndex;
    m_modelAnimationDirty = true;
    m_playheadFrame = 0;
    m_isPlaying = false;
    PlayerEditorSession::SyncTimelineAssetSelection(*this);
    RebuildPreviewTimelineRuntimeData();
    StartSelectedAnimationPreview();

    return true;
}

void PlayerEditorPanel::ApplyEditorBindingsToPreviewEntity()
{
    PlayerEditorSession::ApplyEditorBindingsToPreviewEntity(*this);
}

void PlayerEditorPanel::RebuildPreviewTimelineRuntimeData()
{
    PlayerEditorSession::RebuildPreviewTimelineRuntimeData(*this);
}

void PlayerEditorPanel::SyncPreviewTimelinePlayback(bool syncPreviewState)
{
    if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
        float fps = m_timelineAsset.fps > 0.0f ? m_timelineAsset.fps : 60.0f;
        const float clipLength = GetTimelinePlaybackDurationSeconds();
        int frameMin = 0;
        int frameMax = GetTimelineFrameLimit();

        if (TimelineComponent* timeline = m_registry->GetComponent<TimelineComponent>(m_previewEntity)) {
            if (timeline->fps > 0.0f) {
                fps = timeline->fps;
            }
            frameMin = timeline->frameMin;
        }

        if (frameMax > frameMin) {
            if (m_playheadFrame < frameMin) {
                m_playheadFrame = frameMin;
            }
            if (m_playheadFrame > frameMax) {
                m_playheadFrame = frameMax;
            }
        }

        const float timeSeconds = fps > 0.0f
            ? static_cast<float>(m_playheadFrame) / fps
            : 0.0f;

        if (PlaybackComponent* playback = m_registry->GetComponent<PlaybackComponent>(m_previewEntity)) {
            playback->currentSeconds = timeSeconds;
            if (clipLength > 0.0f) {
                playback->clipLength = clipLength;
            }
            playback->playing = m_isPlaying;
            if (m_previewState.IsActive()) {
                playback->looping = m_previewState.GetDriver()->IsLoop();
            }
            playback->stopAtEnd = !playback->looping;
            playback->finished = false;
        }

        if (TimelineComponent* timeline = m_registry->GetComponent<TimelineComponent>(m_previewEntity)) {
            timeline->frameMin = frameMin;
            timeline->frameMax = frameMax;
            timeline->previousFrame = timeline->currentFrame;
            timeline->currentFrame = m_playheadFrame;
            timeline->playing = m_isPlaying;
            if (clipLength > 0.0f) {
                timeline->clipLengthSec = clipLength;
            }
        }

        if (ColliderComponent* collider = m_registry->GetComponent<ColliderComponent>(m_previewEntity)) {
            collider->enabled = true;
            collider->drawGizmo = true;
        }
    }

    if (syncPreviewState) {
        PlayerEditorSession::SyncPreviewTimelinePlayback(*this);
    }
}

bool PlayerEditorPanel::SavePrefabDocument(bool saveAs)
{
    return PlayerEditorSession::SavePrefabDocument(*this, saveAs);
}

void PlayerEditorPanel::ImportFromSelectedEntity()
{
    PlayerEditorSession::ImportFromSelectedEntity(*this);
}

void PlayerEditorPanel::SetModel(const Model* model)
{
    if (m_model == model) return;
    Suspend();
    m_ownedModel.reset();
    m_model = model;
    m_modelAnimationDirty = false;
    ResetSelectionState();
    m_orbitCenterValid = false;  // モデル変更で軌道中心リセット
    if (m_model) {
        // 読込直後はバウンドが未確定なことがあるので数フレーム再 Fit
        m_autoFitCountdown = 8;
    }
}
// コンパクトな縦スタックボタン (アイコン 14px + 8pt ラベル、30px 高)
bool PlayerEditorPanel::DrawToolbarInlineButton(const char* icon, const char* label, const char* id,
                                                  bool active, bool enabled, bool dirtyDot)
{
    constexpr float kIconSize = 14.0f;
    constexpr float kBtnH     = 30.0f;
    constexpr float kPadX     = 5.0f;
    constexpr float kMinW     = 30.0f;

    if (!enabled) ImGui::BeginDisabled();
    ImGui::PushID(id);

    const ImVec2 lblSize  = (label && label[0]) ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
    const float btnW = (std::max)(kMinW, kPadX * 2.0f + lblSize.x);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.173f, 0.173f, 0.173f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.000f, 0.439f, 0.878f, 1.0f));
    const bool pressed = ImGui::Button("##b", ImVec2(btnW, kBtnH));
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopStyleColor(3);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (active) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + btnW, cursor.y + kBtnH),
                          IM_COL32(0, 112, 224, 255));
    } else if (hovered) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + btnW, cursor.y + kBtnH),
                          IM_COL32(44, 44, 44, 255));
    }
    const ImU32 textColor = active ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255);
    if (icon && icon[0]) {
        ImFont* font = ImGui::GetFont();
        const ImVec2 iconDim = font->CalcTextSizeA(kIconSize, FLT_MAX, 0.0f, icon);
        dl->AddText(font, kIconSize,
            ImVec2(cursor.x + (btnW - iconDim.x) * 0.5f, cursor.y + 2.0f),
            textColor, icon);
    }
    if (label && label[0]) {
        dl->AddText(ImVec2(cursor.x + (btnW - lblSize.x) * 0.5f, cursor.y + kBtnH - lblSize.y - 2.0f),
                    textColor, label);
    }
    if (dirtyDot) {
        dl->AddCircleFilled(ImVec2(cursor.x + btnW - 4.0f, cursor.y + 4.0f),
                            2.5f, IM_COL32(255, 165, 50, 255), 8);
    }

    ImGui::PopID();
    if (!enabled) ImGui::EndDisabled();
    return enabled && pressed;
}

void PlayerEditorPanel::DrawTopBar()
{
    constexpr float kBarH = 34.0f;
    constexpr float kGap  = 2.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.122f, 0.122f, 0.122f, 1.0f));
    ImGui::BeginChild("##PETopBar", ImVec2(0.0f, kBarH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 barOrigin = ImGui::GetCursorScreenPos();
    const float barWidth = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kGap, 0.0f));

    const auto sep = [&]() {
        ImGui::SameLine(0.0f, 4.0f);
        const ImVec2 sp = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sp.x, sp.y + 4.0f),
                    ImVec2(sp.x, sp.y + kBarH - 8.0f),
                    IM_COL32(56, 56, 56, 255), 1.0f);
        ImGui::Dummy(ImVec2(1.0f, kBarH - 6.0f));
        ImGui::SameLine(0.0f, 4.0f);
    };

    // G1: Menu (☰)
    if (DrawToolbarInlineButton(ICON_FA_BARS, nullptr, "##tbMenu", false, true, false)) {
        ImGui::OpenPopup("##PEMenuPopup");
    }
    if (ImGui::BeginPopup("##PEMenuPopup")) {
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open...", "Ctrl+O")) {
            char pathBuffer[MAX_PATH] = {};
            if (!m_currentModelPath.empty()) strcpy_s(pathBuffer, m_currentModelPath.c_str());
            if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kModelFileFilter, "Open Player Source") == DialogResult::OK) {
                OpenModelFromPath(pathBuffer);
            }
        }
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Animation...", nullptr, false, HasOpenModel())) {
            char pathBuffer[MAX_PATH] = {};
            if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kAnimationFileFilter, "Open Animation Source") == DialogResult::OK) {
                OpenAnimationFromPath(pathBuffer);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save", "Ctrl+S", false, CanUsePreviewEntity())) SavePrefabDocument(false);
        if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Save As...", "Ctrl+Shift+S", false, CanUsePreviewEntity())) SavePrefabDocument(true);
        ImGui::EndPopup();
    }
    sep();

    // G2: Open のみ (Save は ☰ メニューへ集約)
    {
        if (DrawToolbarInlineButton(ICON_FA_FOLDER_OPEN, "Open", "##tbOpen", false, true, false)) {
            char pathBuffer[MAX_PATH] = {};
            if (!m_currentModelPath.empty()) strcpy_s(pathBuffer, m_currentModelPath.c_str());
            if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kModelFileFilter, "Open Player Source") == DialogResult::OK) {
                OpenModelFromPath(pathBuffer);
            }
        }
        ImGui::SameLine(0.0f, kGap);
        if (DrawToolbarInlineButton(ICON_FA_FOLDER_OPEN, "Open Animation", "##tbOpenAnim", false, HasOpenModel(), false)) {
            char pathBuffer[MAX_PATH] = {};
            if (Dialog::OpenFileName(pathBuffer, MAX_PATH, kAnimationFileFilter, "Open Animation Source") == DialogResult::OK) {
                OpenAnimationFromPath(pathBuffer);
            }
        }
    }
    sep();

    // G3: Mode (Player / Enemy / NPC) — 単一ボタンで dropdown
    {
        const char* modeLabels[] = { "Player", "Enemy", "NPC" };
        const char* curLabel = modeLabels[(int)m_actorEditorMode];
        if (DrawToolbarInlineButton(ICON_FA_USER, curLabel, "##tbMode", false, true, false)) {
            ImGui::OpenPopup("##PEModePopup");
        }
        if (ImGui::BeginPopup("##PEModePopup")) {
            for (int i = 0; i < 3; ++i) {
                if (ImGui::MenuItem(modeLabels[i], nullptr, (int)m_actorEditorMode == i)) {
                    if (m_actorEditorMode != (ActorEditorMode)i) {
                        m_actorEditorMode = (ActorEditorMode)i;
                        m_inlineBtExpanded = false;
                        m_inlineBtLoaded = false;
                    }
                }
            }
            ImGui::EndPopup();
        }
    }
    sep();

    // G4: Play / Edit (モード切替時は Fit 再トリガー + SceneView コンテキスト切替)
    {
        auto setSceneViewInputEnabled = [&](bool enabled) {
            if (!m_registry) return;
            Signature ctxSig = CreateSignature<InputContextComponent>();
            for (auto* arch : m_registry->GetAllArchetypes()) {
                if (!SignatureMatches(arch->GetSignature(), ctxSig)) continue;
                auto* col = arch->GetColumn(TypeManager::GetComponentTypeID<InputContextComponent>());
                if (!col) continue;
                for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                    auto& ctx = *static_cast<InputContextComponent*>(col->Get(i));
                    if (ctx.priority == InputContextPriority::SceneView) {
                        ctx.enabled = enabled;
                    }
                }
            }
        };

        const bool isTest = m_viewMode == PlayerEditorViewMode::Test;
        if (DrawToolbarInlineButton(ICON_FA_PLAY, "Test", "##tbTest", isTest, true, false)) {
            if (!isTest) {
                m_viewMode = PlayerEditorViewMode::Test;
                m_autoFitCountdown = 8;
                setSceneViewInputEnabled(false);  // RuntimeGameplay へ入力を流す
                // PreviewState が active だと TimelineDriver が毎フレーム animator を上書きし、
                // StateMachineSystem の PlayBase が無効化される。Test 中は必ず exit。
                if (m_previewState.IsActive()) {
                    m_previewState.ExitPreview();
                }
                m_isPlaying = false;
                // 既定ステートへ即遷移できるよう state machine 状態を初期化
                if (CanUsePreviewEntity()) {
                    if (auto* params = m_registry->GetComponent<StateMachineParamsComponent>(m_previewEntity)) {
                        params->currentStateId = 0;
                        params->animFinished = false;
                        params->stateTimer = 0.0f;
                    }
                    PlayerRuntimeSetup::ResetPlayerRuntimeState(*m_registry, m_previewEntity);
                }
            }
        }
        ImGui::SameLine(0.0f, kGap);
        if (DrawToolbarInlineButton(ICON_FA_PEN, "Edit", "##tbEdit", !isTest, true, false)) {
            if (isTest) {
                m_viewMode = PlayerEditorViewMode::Edit;
                m_autoFitCountdown = 8;
                setSceneViewInputEnabled(true);  // 通常編集用入力に戻す
            }
        }
    }
    sep();

    // G5: Full Player (Fit は自動化、ボタン廃止)
    {
        const bool canFull = (m_actorEditorMode == ActorEditorMode::Player) && CanUsePreviewEntity();
        if (DrawToolbarInlineButton(ICON_FA_BOLT, "Full", "##tbFull", false, canFull, false)) ApplyFullPlayerPreset();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    dl->AddLine(ImVec2(barOrigin.x, barOrigin.y + kBarH - 0.5f),
                ImVec2(barOrigin.x + barWidth, barOrigin.y + kBarH - 0.5f),
                IM_COL32(13, 13, 13, 255), 1.0f);
}

void PlayerEditorPanel::DrawLeftSidebar(float w)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##PELeftSidebar", ImVec2(w, 0.0f), true);
    ImGui::PopStyleVar();

    if (ImGui::BeginTabBar("##LSTabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        if (ImGui::BeginTabItem(ICON_FA_BONE " Skeleton Tree")) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
            ImGui::BeginChild("##LSBoneInner", ImVec2(0.0f, 0.0f), false);
            DrawSkeletonPanel();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(ICON_FA_FOLDER_TREE " Asset Browser")) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
            ImGui::BeginChild("##LSAnimInner", ImVec2(0.0f, 0.0f), false);
            DrawAnimatorPanel();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void PlayerEditorPanel::DrawWorkbench()
{
    // Workspace タブが既に選択 UI を担うため、ここでは内部 TabBar は出さない。
    // m_workbenchActiveTab で直接ディスパッチ。
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
    ImGui::BeginChild("##PEWorkbench", ImVec2(0.0f, 0.0f), true);

    switch (m_workbenchActiveTab) {
    case 0: DrawStateMachinePanel(); break;
    case 1: DrawTimelinePanel(); break;
    case 2:
        if (HasOpenModel() && CanUsePreviewEntity()) {
            DrawPersistentColliderSection();
        } else {
            ImGui::TextDisabled("Open a model to edit colliders.");
        }
        break;
    case 3: DrawInputPanel(); break;
    default: break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void PlayerEditorPanel::DrawRightInspector(float w)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##PERightInspector", ImVec2(w, 0.0f), true);
    ImGui::PopStyleVar();

    const bool physicsActive = m_workbenchOpen && m_workbenchActiveTab == 2;

    // 右パネル内容: Physics タブだけ Colliders UI、他は Details (選択依存)
    const char* tabIcon = ICON_FA_SLIDERS;
    const char* tabLabel = "Details";
    if (physicsActive) { tabIcon = ICON_FA_SHIELD; tabLabel = "Physics"; }

    char tabTitle[64];
    snprintf(tabTitle, sizeof(tabTitle), "%s %s", tabIcon, tabLabel);

    if (ImGui::BeginTabBar("##RITabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        if (ImGui::BeginTabItem(tabTitle)) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
            ImGui::BeginChild("##RIContent", ImVec2(0.0f, 0.0f), false);

            if (physicsActive) {
                if (!HasOpenModel() || !CanUsePreviewEntity()) {
                    ImGui::TextDisabled("Open a model to edit colliders.");
                } else {
                    if (ImGui::Button(ICON_FA_PLUS " Collider", ImVec2(-1.0f, 0.0f))) {
                        AddPersistentCollider(ColliderAttribute::Body);
                    }
                    ImGui::Separator();
                    const float availY = ImGui::GetContentRegionAvail().y;
                    const float listH = (std::max)(80.0f, availY * 0.35f);
                    ImGui::BeginChild("##RIPhysList", ImVec2(0.0f, listH), true);
                    DrawPersistentColliderSection();
                    ImGui::EndChild();
                    ImGui::Separator();
                    ImGui::BeginChild("##RIPhysInsp", ImVec2(0.0f, 0.0f), false);
                    if (m_selectionCtx == SelectionContext::PersistentCollider) {
                        DrawPersistentColliderInspector();
                    } else {
                        ImGui::TextDisabled("Select a collider above to edit.");
                    }
                    ImGui::EndChild();
                }
            } else {
                // Viewport / State Machine / Input: 選択依存の Details
                if (m_selectionCtx == SelectionContext::None) {
                    ImGui::TextDisabled("Select an item to view details.");
                } else {
                    DrawPropertiesPanel();
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void PlayerEditorPanel::DrawStatusBar()
{
    constexpr float kStatusH = 22.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.122f, 0.122f, 0.122f, 1.0f));
    ImGui::BeginChild("##PEStatus", ImVec2(0.0f, kStatusH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    dl->AddLine(p, ImVec2(p.x + w, p.y), IM_COL32(13, 13, 13, 255), 1.0f);

    ImGui::AlignTextToFramePadding();
    if (HasAnyDirtyDocument()) {
        ImGui::TextColored(ImVec4(0.040f, 0.450f, 0.900f, 1.0f), ICON_FA_CIRCLE_DOT " Modified");
    } else {
        ImGui::TextDisabled(ICON_FA_CHECK " Clean");
    }
    ImGui::SameLine(0.0f, 16.0f);
    if (!m_currentModelPath.empty()) ImGui::TextDisabled("%s", m_currentModelPath.c_str());
    ImGui::EndChild();
}

void PlayerEditorPanel::HandleGlobalShortcuts()
{
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyCtrl) return;
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (CanUsePreviewEntity()) SavePrefabDocument(io.KeyShift);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && m_registry) {
        if (io.KeyShift) UndoSystem::Instance().Redo(*m_registry);
        else UndoSystem::Instance().Undo(*m_registry);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && m_registry) {
        UndoSystem::Instance().Redo(*m_registry);
    }
}

void PlayerEditorPanel::DrawMainWorkspace()
{
    PushPlayerEditorPanelStyle();
    PushPlayerEditorPanelSizeStyle();

    DrawTopBar();
    HandleGlobalShortcuts();

    // 読込直後のバウンド未確定を吸収する自動 Fit
    if (m_autoFitCountdown > 0 && m_model) {
        RequestCameraFit();
        --m_autoFitCountdown;
    }

    constexpr float kLeftW   = 240.0f;
    constexpr float kRightW  = 300.0f;
    constexpr float kStatusH = 22.0f;
    constexpr float kGap     = 1.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kGap, kGap));

    ImGui::BeginGroup();
    DrawLeftSidebar(kLeftW);
    ImGui::SameLine(0.0f, kGap);

    const float centerW = ImGui::GetContentRegionAvail().x - kRightW - kGap;

    // 中央列の絶対座標を記録 (PiP オーバーレイ用)
    const ImVec2 centerOrigin = ImGui::GetCursorScreenPos();
    const float centerH = ImGui::GetContentRegionAvail().y - kStatusH - kGap;

    ImGui::BeginChild("##PECenter", ImVec2(centerW, centerH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // UE Persona 風: 中央列の上にタブ列
    DrawWorkspaceTabBar();

    // 下部に Timeline ストリップを常時固定
    constexpr float kTimelineStripH = 200.0f;
    const float centerInnerH = ImGui::GetContentRegionAvail().y;
    const float upperH = (std::max)(120.0f, centerInnerH - kTimelineStripH - kGap);

    // 中央 = State Machine / Input タブの時はツール全画面、それ以外は viewport
    const bool stateMachineActive = m_workbenchOpen && m_workbenchActiveTab == 0;
    const bool inputActive        = m_workbenchOpen && m_workbenchActiveTab == 3;
    const bool toolFullScreen     = stateMachineActive || inputActive;

    if (toolFullScreen) {
        // ステートマシン / Input: 中央をツールがフル占有 (viewport 非表示)
        ImGui::BeginChild("##PEToolHost", ImVec2(0.0f, upperH), false);
        DrawWorkbench();
        ImGui::EndChild();
    } else {
        // Viewport / Physics タブ: 中央は viewport
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.059f, 0.059f, 0.059f, 1.0f));
        ImGui::BeginChild("##PEViewportHost", ImVec2(0.0f, upperH), true);
        DrawViewportSurface();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    // 下半分: Timeline ストリップ (常時固定)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.082f, 0.082f, 0.082f, 1.0f));
    ImGui::BeginChild("##PETimelineStrip", ImVec2(0.0f, kTimelineStripH), true);
    DrawTimelinePanel();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::EndChild();

    ImGui::SameLine(0.0f, kGap);
    DrawRightInspector(kRightW);
    ImGui::EndGroup();

    DrawStatusBar();

    // State Machine タブは PiP viewport を出さない (グラフ編集に専念)。
    // 必要なら Viewport タブに切り替えてモデルを確認する。
    (void)centerOrigin; (void)centerW;  // 旧 PiP 用に取っていた変数

    ImGui::PopStyleVar();
    PopPlayerEditorPanelSizeStyle();
    PopPlayerEditorPanelStyle();
}

// 中央列上部のタブ列 (Viewport / State Machine / Hit / Input)
// Timeline はタブから除外し、常時下部にドックする (UE Persona の Notify Track 流儀)。
void PlayerEditorPanel::DrawWorkspaceTabBar()
{
    constexpr float kTabBarH = 28.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.082f, 0.082f, 0.082f, 1.0f));
    ImGui::BeginChild("##PEWorkspaceTabs", ImVec2(0.0f, kTabBarH), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 barOrigin = ImGui::GetCursorScreenPos();
    const float barWidth = ImGui::GetContentRegionAvail().x;

    // 0=Viewport, 1=State (m_workbenchActiveTab=0), 2=Hit (=2), 3=Input (=3)
    // (m_workbenchActiveTab=1 は Timeline で旧 tab 用、今は使わない)
    struct TabDef { const char* label; int tabIdx; bool isViewport; };
    const TabDef tabs[4] = {
        { "Viewport",      -1, true  },
        { "State Machine",  0, false },
        { "Physics",        2, false },
        { "Input",          3, false },
    };

    int activeIdx = 0;
    if (m_workbenchOpen) {
        for (int i = 1; i < 4; ++i) if (tabs[i].tabIdx == m_workbenchActiveTab) { activeIdx = i; break; }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    for (int i = 0; i < 4; ++i) {
        const bool active = (i == activeIdx);
        const ImVec2 sz = ImGui::CalcTextSize(tabs[i].label);
        const float btnW = sz.x + 20.0f;
        const float btnH = kTabBarH - 1.0f;
        const ImVec2 p = ImGui::GetCursorScreenPos();

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.173f, 0.173f, 0.173f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.220f, 0.220f, 0.220f, 1.0f));
        if (ImGui::Button("##t", ImVec2(btnW, btnH))) {
            if (tabs[i].isViewport) {
                m_workbenchOpen = false;
            } else {
                m_workbenchOpen = true;
                m_workbenchActiveTab = tabs[i].tabIdx;
            }
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        ImGui::PopID();

        if (active) {
            dl->AddRectFilled(p, ImVec2(p.x + btnW, p.y + btnH), IM_COL32(36, 36, 36, 255));
            dl->AddRectFilled(ImVec2(p.x, p.y + btnH - 2.0f),
                              ImVec2(p.x + btnW, p.y + btnH),
                              IM_COL32(0, 112, 224, 255));
        } else if (hovered) {
            dl->AddRectFilled(p, ImVec2(p.x + btnW, p.y + btnH), IM_COL32(44, 44, 44, 255));
        }
        dl->AddText(ImVec2(p.x + 10.0f, p.y + (btnH - sz.y) * 0.5f),
                    active ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255),
                    tabs[i].label);
        ImGui::SameLine(0.0f, 0.0f);
    }

    if (activeIdx != 0) {
        char crumb[96];
        snprintf(crumb, sizeof(crumb), "  Viewport  >  %s", tabs[activeIdx].label);
        const ImVec2 sz = ImGui::CalcTextSize(crumb);
        const float crumbX = barOrigin.x + barWidth - sz.x - 12.0f;
        dl->AddText(ImVec2(crumbX, barOrigin.y + (kTabBarH - sz.y) * 0.5f),
                    IM_COL32(140, 140, 140, 255), crumb);
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    dl->AddLine(ImVec2(barOrigin.x, barOrigin.y + kTabBarH - 0.5f),
                ImVec2(barOrigin.x + barWidth, barOrigin.y + kTabBarH - 0.5f),
                IM_COL32(13, 13, 13, 255), 1.0f);
}

void PlayerEditorPanel::RequestCameraFit()
{
    if (!m_model) {
        return;
    }

    const auto bounds = m_model->GetWorldBounds();
    const DirectX::XMFLOAT3 ex = bounds.Extents;
    // バウンディング球半径。1.0 にクランプせず実寸を尊重 (小型モデル対応)
    const float radius = (std::max)(std::sqrt(ex.x * ex.x + ex.y * ex.y + ex.z * ex.z), 0.05f);
    // 水平視 + 1.2 倍マージン、min distance 0.8 (近接クリップ回避)
    const float pitch = 0.0f;
    const float yaw = 0.0f;
    const float halfFov = 0.785398f * 0.5f;
    const float distance = (std::max)((radius / (std::max)(std::sin(halfFov), 0.001f)) * 1.2f, 0.8f);

    m_vpCameraYaw = yaw;
    m_vpCameraPitch = pitch;
    m_vpCameraDist = distance;

    m_pendingCameraFitTarget = bounds.Center;
    m_pendingCameraFitRadius = radius;
    m_pendingCameraFitForward = {
        std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::cos(yaw) * std::cos(pitch)
    };
    m_pendingCameraFitDistance = distance;
    m_hasPendingCameraFit = true;

    // 軌道中心をスナップショット (アニメ中は固定)
    m_orbitCenter = bounds.Center;
    m_orbitCenterValid = true;
}

void PlayerEditorPanel::ResetPreviewRuntime()
{
    m_playheadFrame = 0;
    m_isPlaying = false;
    if (m_previewState.IsActive()) {
        m_previewState.ExitPreview();
    }
    if (CanUsePreviewEntity()) {
        PlayerRuntimeSetup::ResetPlayerRuntimeState(*m_registry, m_previewEntity);
    }
    SyncPreviewTimelinePlayback();
    RequestCameraFit();
}

// 上位 Draw 入口と DockSpace の外枠
void PlayerEditorPanel::Draw(Registry* registry, bool* p_open, bool* outFocused)
{
    DrawInternal(registry, p_open, outFocused, HostMode::Window);
}

void PlayerEditorPanel::DrawWorkspace(Registry* registry, bool* outFocused)
{
    DrawInternal(registry, nullptr, outFocused, HostMode::Workspace);
}

void PlayerEditorPanel::DrawDetached(Registry* registry, bool* p_open, bool* outFocused)
{
    DrawInternal(registry, p_open, outFocused, HostMode::Detached);
}

void PlayerEditorPanel::DrawInternal(Registry* registry, bool* p_open, bool* outFocused, HostMode hostMode)
{
    m_registry = registry;
    if (!Entity::IsNull(m_previewEntity) && (!m_registry || !m_registry->IsAlive(m_previewEntity))) {
        m_previewEntity = Entity::NULL_ID;
        m_previewEntityOwned = false;
    }
    EnsureOwnedPreviewEntity();

    if (hostMode != m_lastHostMode) {
        m_needsLayoutRebuild = true;
        m_lastHostMode = hostMode;
    }

    if (hostMode == HostMode::Workspace || hostMode == HostMode::Detached) {
        if (hostMode == HostMode::Detached) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        const ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        const char* hostName = (hostMode == HostMode::Detached)
            ? "##PlayerEditorDetachedRoot"
            : "##PlayerEditorWorkspaceRoot";
        const bool hostOpen = ImGui::Begin(hostName, nullptr, hostFlags);
        ImGui::PopStyleVar(3);
        if (!hostOpen) {
            ImGui::End();
            if (outFocused) *outFocused = false;
            return;
        }

        if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        if (hostMode == HostMode::Detached && !DrawDetachedTopTabBar(p_open)) {
            ImGui::End();
            if (outFocused) *outFocused = false;
            return;
        }

        DrawMainWorkspace();

        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::Begin(ICON_FA_USER " Player Editor", p_open, hostFlags)) {
        ImGui::End();
        if (outFocused) *outFocused = false;
        return;
    }

    if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    DrawMainWorkspace();

    ImGui::End(); // ホストウィンドウ
}
// DockSpace レイアウト構築（UE Animation Editor 風）
void PlayerEditorPanel::BuildDockLayout(unsigned int dockspaceId)
{
    ImGuiID dockId = dockspaceId;
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImVec2(1500, 850));

    ImGuiID topId = dockId;

    ImGuiID bottomId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Down, 0.34f, &bottomId, &topId);

    ImGuiID skeletonId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Left, 0.21f, &skeletonId, &topId);

    ImGuiID rightColumnId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Right, 0.23f, &rightColumnId, &topId);

    ImGuiID viewportId = 0;
    ImGui::DockBuilderSplitNode(topId, ImGuiDir_Right, 0.34f, &viewportId, &topId);

    ImGuiID rightBottomId = 0;
    ImGui::DockBuilderSplitNode(rightColumnId, ImGuiDir_Down, 0.42f, &rightBottomId, &rightColumnId);

    ImGui::DockBuilderDockWindow(kPESkeletonTitle,     skeletonId);
    ImGui::DockBuilderDockWindow(kPEStateMachineTitle, topId);
    ImGui::DockBuilderDockWindow(kPEViewportTitle,     viewportId);
    ImGui::DockBuilderDockWindow(kPEPropertiesTitle,   rightColumnId);
    ImGui::DockBuilderDockWindow(kPEAnimatorTitle,     rightBottomId);
    ImGui::DockBuilderDockWindow(kPEInputTitle,        rightBottomId);
    ImGui::DockBuilderDockWindow(kPETimelineTitle,     bottomId);

    ImGui::DockBuilderFinish(dockId);
}
