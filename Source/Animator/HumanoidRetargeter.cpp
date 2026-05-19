#include "Animator/HumanoidRetargeter.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>

using namespace DirectX;

namespace HumanoidRetarget
{
namespace
{
    struct SlotRule
    {
        BoneSlot slot;
        const char* requiredA;
        const char* requiredB;
        const char* rejectA;
        const char* rejectB;
    };

    constexpr SlotRule kRules[] = {
        { BoneSlot::Root,          "root",     "",        "thumb",  "index" },
        { BoneSlot::Hips,          "hip",      "",        "twist",  "helper" },
        { BoneSlot::Hips,          "pelvis",   "",        "twist",  "helper" },
        { BoneSlot::Spine,         "spine",    "",        "twist",  "helper" },
        { BoneSlot::Chest,         "chest",    "",        "twist",  "helper" },
        { BoneSlot::Chest,         "spine2",   "",        "twist",  "helper" },
        { BoneSlot::Chest,         "spine03",  "",        "twist",  "helper" },
        { BoneSlot::Neck,          "neck",     "",        "twist",  "helper" },
        { BoneSlot::Head,          "head",     "",        "twist",  "helper" },

        { BoneSlot::LeftShoulder,  "left",     "shoulder","twist",  "helper" },
        { BoneSlot::LeftShoulder,  "l",        "clavicle","twist",  "helper" },
        { BoneSlot::LeftUpperArm,  "left",     "upperarm","twist",  "helper" },
        { BoneSlot::LeftUpperArm,  "left",     "arm",     "forearm","lower" },
        { BoneSlot::LeftLowerArm,  "left",     "forearm", "twist",  "helper" },
        { BoneSlot::LeftLowerArm,  "left",     "lowerarm","twist",  "helper" },
        { BoneSlot::LeftHand,      "left",     "hand",    "thumb",  "index" },

        { BoneSlot::RightShoulder, "right",    "shoulder","twist",  "helper" },
        { BoneSlot::RightShoulder, "r",        "clavicle","twist",  "helper" },
        { BoneSlot::RightUpperArm, "right",    "upperarm","twist",  "helper" },
        { BoneSlot::RightUpperArm, "right",    "arm",     "forearm","lower" },
        { BoneSlot::RightLowerArm, "right",    "forearm", "twist",  "helper" },
        { BoneSlot::RightLowerArm, "right",    "lowerarm","twist",  "helper" },
        { BoneSlot::RightHand,     "right",    "hand",    "thumb",  "index" },

        { BoneSlot::LeftUpperLeg,  "left",     "upleg",   "twist",  "helper" },
        { BoneSlot::LeftUpperLeg,  "left",     "thigh",   "twist",  "helper" },
        { BoneSlot::LeftLowerLeg,  "left",     "leg",     "upleg",  "thigh" },
        { BoneSlot::LeftLowerLeg,  "left",     "calf",    "twist",  "helper" },
        { BoneSlot::LeftFoot,      "left",     "foot",    "toe",    "ik" },
        { BoneSlot::LeftToes,      "left",     "toe",     "ik",     "helper" },

        { BoneSlot::RightUpperLeg, "right",    "upleg",   "twist",  "helper" },
        { BoneSlot::RightUpperLeg, "right",    "thigh",   "twist",  "helper" },
        { BoneSlot::RightLowerLeg, "right",    "leg",     "upleg",  "thigh" },
        { BoneSlot::RightLowerLeg, "right",    "calf",    "twist",  "helper" },
        { BoneSlot::RightFoot,     "right",    "foot",    "toe",    "ik" },
        { BoneSlot::RightToes,     "right",    "toe",     "ik",     "helper" },
    };

    std::string NormalizeName(const std::string& name)
    {
        std::string lowered;
        lowered.reserve(name.size());
        for (char c : name) {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }

        const auto hasSeparatedSidePrefix = [&](char side) {
            return lowered.size() >= 2 &&
                lowered[0] == side &&
                !std::isalnum(static_cast<unsigned char>(lowered[1]));
        };
        const auto hasSeparatedSideSuffix = [&](char side) {
            return lowered.size() >= 2 &&
                lowered.back() == side &&
                !std::isalnum(static_cast<unsigned char>(lowered[lowered.size() - 2]));
        };

        const bool leftPrefix = hasSeparatedSidePrefix('l');
        const bool leftSuffix = hasSeparatedSideSuffix('l');
        const bool rightPrefix = hasSeparatedSidePrefix('r');
        const bool rightSuffix = hasSeparatedSideSuffix('r');
        const bool leftSeparated = leftPrefix || leftSuffix;
        const bool rightSeparated = rightPrefix || rightSuffix;

        std::string out;
        out.reserve(name.size());
        for (char c : lowered) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc)) {
                out.push_back(static_cast<char>(uc));
            }
        }

        if (leftSeparated && out.size() > 1) {
            if (leftPrefix && out.front() == 'l') {
                out.erase(out.begin());
            }
            if (leftSuffix && !out.empty() && out.back() == 'l') {
                out.pop_back();
            }
            if (out.find("left") == std::string::npos) {
                out = "left" + out;
            }
        }
        else if (rightSeparated && out.size() > 1) {
            if (rightPrefix && out.front() == 'r') {
                out.erase(out.begin());
            }
            if (rightSuffix && !out.empty() && out.back() == 'r') {
                out.pop_back();
            }
            if (out.find("right") == std::string::npos) {
                out = "right" + out;
            }
        }

        const auto replaceAll = [&](const char* from, const char* to) {
            std::string::size_type pos = 0;
            const size_t fromLen = std::char_traits<char>::length(from);
            const size_t toLen = std::char_traits<char>::length(to);
            while ((pos = out.find(from, pos)) != std::string::npos) {
                out.replace(pos, fromLen, to);
                pos += toLen;
            }
        };

        replaceAll("mixamorig", "");
        replaceAll("bip001", "");
        replaceAll("lefthand", "lefthand");
        replaceAll("righthand", "righthand");
        replaceAll("upperleg", "upleg");
        replaceAll("lowerleg", "leg");
        return out;
    }

    bool Contains(const std::string& text, const char* needle)
    {
        return !needle || needle[0] == '\0' || text.find(needle) != std::string::npos;
    }

    bool ContainsSideRequired(const std::string& text, const char* marker)
    {
        if (!marker || marker[0] == '\0') {
            return true;
        }
        if (std::strcmp(marker, "l") == 0) {
            return text.rfind("l", 0) == 0 || text.find("left") != std::string::npos;
        }
        if (std::strcmp(marker, "r") == 0) {
            return text.rfind("r", 0) == 0 || text.find("right") != std::string::npos;
        }
        return Contains(text, marker);
    }

    bool HasLeftMarker(const std::string& text)
    {
        return Contains(text, "left") || text.rfind("l", 0) == 0;
    }

    bool HasRightMarker(const std::string& text)
    {
        return Contains(text, "right") || text.rfind("r", 0) == 0;
    }

    bool SlotWantsLeft(BoneSlot slot)
    {
        switch (slot) {
        case BoneSlot::LeftShoulder:
        case BoneSlot::LeftUpperArm:
        case BoneSlot::LeftLowerArm:
        case BoneSlot::LeftHand:
        case BoneSlot::LeftUpperLeg:
        case BoneSlot::LeftLowerLeg:
        case BoneSlot::LeftFoot:
        case BoneSlot::LeftToes:
            return true;
        default:
            return false;
        }
    }

    bool SlotWantsRight(BoneSlot slot)
    {
        switch (slot) {
        case BoneSlot::RightShoulder:
        case BoneSlot::RightUpperArm:
        case BoneSlot::RightLowerArm:
        case BoneSlot::RightHand:
        case BoneSlot::RightUpperLeg:
        case BoneSlot::RightLowerLeg:
        case BoneSlot::RightFoot:
        case BoneSlot::RightToes:
            return true;
        default:
            return false;
        }
    }

    float ScoreName(const std::string& normalized, const SlotRule& rule)
    {
        if (!ContainsSideRequired(normalized, rule.requiredA) || !Contains(normalized, rule.requiredB)) {
            return 0.0f;
        }
        if (Contains(normalized, rule.rejectA) || Contains(normalized, rule.rejectB)) {
            return 0.0f;
        }
        if (SlotWantsLeft(rule.slot) && HasRightMarker(normalized)) {
            return 0.0f;
        }
        if (SlotWantsRight(rule.slot) && HasLeftMarker(normalized)) {
            return 0.0f;
        }

        float score = 0.55f;
        if (SlotWantsLeft(rule.slot) && HasLeftMarker(normalized)) score += 0.25f;
        if (SlotWantsRight(rule.slot) && HasRightMarker(normalized)) score += 0.25f;
        if (normalized == rule.requiredA || normalized == rule.requiredB) score += 0.1f;
        return (std::min)(score, 1.0f);
    }

    XMVECTOR LoadQuat(const XMFLOAT4& q)
    {
        XMVECTOR value = XMLoadFloat4(&q);
        float lenSq = 0.0f;
        XMStoreFloat(&lenSq, XMVector4LengthSq(value));
        if (lenSq <= 0.0000001f) {
            return XMQuaternionIdentity();
        }
        return XMQuaternionNormalize(value);
    }

    XMFLOAT4 StoreQuat(XMVECTOR q)
    {
        XMFLOAT4 out{};
        XMStoreFloat4(&out, XMQuaternionNormalize(q));
        return out;
    }

    void FillBindPose(const Model& model, std::vector<Model::NodePose>& poses)
    {
        const auto& nodes = model.GetNodes();
        poses.resize(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            poses[i].position = nodes[i].position;
            poses[i].rotation = nodes[i].rotation;
            poses[i].scale = nodes[i].scale;
        }
    }

    XMMATRIX LocalMatrix(const Model::NodePose& pose)
    {
        return XMMatrixScaling(pose.scale.x, pose.scale.y, pose.scale.z)
            * XMMatrixRotationQuaternion(LoadQuat(pose.rotation))
            * XMMatrixTranslation(pose.position.x, pose.position.y, pose.position.z);
    }

    void BuildGlobalMatrices(
        const Model& model,
        const std::vector<Model::NodePose>& poses,
        std::vector<XMFLOAT4X4>& globals)
    {
        const auto& nodes = model.GetNodes();
        globals.resize(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            XMMATRIX parent = XMMatrixIdentity();
            if (nodes[i].parentIndex >= 0 && static_cast<size_t>(nodes[i].parentIndex) < globals.size()) {
                parent = XMLoadFloat4x4(&globals[static_cast<size_t>(nodes[i].parentIndex)]);
            }
            XMMATRIX global = LocalMatrix(poses[i]) * parent;
            XMStoreFloat4x4(&globals[i], global);
        }
    }

    float DistanceBetweenSlots(const Model& model, const HumanoidProfile& profile, BoneSlot a, BoneSlot b)
    {
        const int ia = profile.Get(a);
        const int ib = profile.Get(b);
        if (ia < 0 || ib < 0) {
            return 0.0f;
        }

        std::vector<Model::NodePose> bind;
        FillBindPose(model, bind);
        std::vector<XMFLOAT4X4> globals;
        BuildGlobalMatrices(model, bind, globals);

        const XMMATRIX ma = XMLoadFloat4x4(&globals[ia]);
        const XMMATRIX mb = XMLoadFloat4x4(&globals[ib]);
        XMFLOAT3 pa{};
        XMFLOAT3 pb{};
        XMStoreFloat3(&pa, ma.r[3]);
        XMStoreFloat3(&pb, mb.r[3]);
        const float dx = pa.x - pb.x;
        const float dy = pa.y - pb.y;
        const float dz = pa.z - pb.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    void PushPoseKey(Model::NodeAnim& anim, float time, const Model::NodePose& pose, bool keepScale)
    {
        anim.positionKeyframes.push_back(Model::VectorKeyframe{ time, pose.position });
        anim.rotationKeyframes.push_back(Model::QuaternionKeyframe{ time, pose.rotation });
        anim.scaleKeyframes.push_back(Model::VectorKeyframe{ time, keepScale ? pose.scale : pose.scale });
    }

    bool IsRootLike(BoneSlot slot)
    {
        return slot == BoneSlot::Root || slot == BoneSlot::Hips;
    }
}

const char* ToString(BoneSlot slot)
{
    switch (slot) {
    case BoneSlot::Root: return "Root";
    case BoneSlot::Hips: return "Hips";
    case BoneSlot::Spine: return "Spine";
    case BoneSlot::Chest: return "Chest";
    case BoneSlot::Neck: return "Neck";
    case BoneSlot::Head: return "Head";
    case BoneSlot::LeftShoulder: return "LeftShoulder";
    case BoneSlot::LeftUpperArm: return "LeftUpperArm";
    case BoneSlot::LeftLowerArm: return "LeftLowerArm";
    case BoneSlot::LeftHand: return "LeftHand";
    case BoneSlot::RightShoulder: return "RightShoulder";
    case BoneSlot::RightUpperArm: return "RightUpperArm";
    case BoneSlot::RightLowerArm: return "RightLowerArm";
    case BoneSlot::RightHand: return "RightHand";
    case BoneSlot::LeftUpperLeg: return "LeftUpperLeg";
    case BoneSlot::LeftLowerLeg: return "LeftLowerLeg";
    case BoneSlot::LeftFoot: return "LeftFoot";
    case BoneSlot::LeftToes: return "LeftToes";
    case BoneSlot::RightUpperLeg: return "RightUpperLeg";
    case BoneSlot::RightLowerLeg: return "RightLowerLeg";
    case BoneSlot::RightFoot: return "RightFoot";
    case BoneSlot::RightToes: return "RightToes";
    default: return "Unknown";
    }
}

HumanoidProfile AnalyzeSkeleton(const Model& model)
{
    HumanoidProfile profile{};
    const auto& nodes = model.GetNodes();

    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
        const std::string normalized = NormalizeName(nodes[nodeIndex].name);
        for (const SlotRule& rule : kRules) {
            const float score = ScoreName(normalized, rule);
            if (score <= 0.0f) {
                continue;
            }
            SlotMatch& match = profile.slots[static_cast<size_t>(rule.slot)];
            if (score > match.confidence) {
                match.nodeIndex = static_cast<int>(nodeIndex);
                match.confidence = score;
            }
        }
    }

    const BoneSlot required[] = {
        BoneSlot::Hips,
        BoneSlot::Spine,
        BoneSlot::Head,
        BoneSlot::LeftUpperArm,
        BoneSlot::LeftLowerArm,
        BoneSlot::LeftHand,
        BoneSlot::RightUpperArm,
        BoneSlot::RightLowerArm,
        BoneSlot::RightHand,
        BoneSlot::LeftUpperLeg,
        BoneSlot::LeftLowerLeg,
        BoneSlot::LeftFoot,
        BoneSlot::RightUpperLeg,
        BoneSlot::RightLowerLeg,
        BoneSlot::RightFoot,
    };

    profile.valid = true;
    for (BoneSlot slot : required) {
        if (profile.Get(slot) < 0) {
            profile.valid = false;
            profile.warnings.push_back(std::string("Missing required humanoid bone: ") + ToString(slot));
        }
    }

    profile.height =
        DistanceBetweenSlots(model, profile, BoneSlot::Hips, BoneSlot::Head) +
        (std::max)(
            DistanceBetweenSlots(model, profile, BoneSlot::LeftUpperLeg, BoneSlot::LeftFoot),
            DistanceBetweenSlots(model, profile, BoneSlot::RightUpperLeg, BoneSlot::RightFoot));
    if (profile.height <= 0.001f) {
        profile.height = 1.0f;
    }

    return profile;
}

RetargetResult BakeAnimation(
    const Model& source,
    int sourceAnimationIndex,
    const Model& target,
    const RetargetOptions& options)
{
    RetargetResult result{};
    result.sourceProfile = AnalyzeSkeleton(source);
    result.targetProfile = AnalyzeSkeleton(target);
    result.warnings.insert(result.warnings.end(), result.sourceProfile.warnings.begin(), result.sourceProfile.warnings.end());
    result.warnings.insert(result.warnings.end(), result.targetProfile.warnings.begin(), result.targetProfile.warnings.end());

    const auto& sourceAnimations = source.GetAnimations();
    if (sourceAnimationIndex < 0 || sourceAnimationIndex >= static_cast<int>(sourceAnimations.size())) {
        result.warnings.push_back("Invalid source animation index.");
        return result;
    }
    if (!result.sourceProfile.valid || !result.targetProfile.valid) {
        return result;
    }

    const Model::Animation& srcAnim = sourceAnimations[sourceAnimationIndex];
    const auto& targetNodes = target.GetNodes();

    result.animation.name = srcAnim.name + options.outputNameSuffix;
    result.animation.secondsLength = srcAnim.secondsLength;
    result.animation.nodeAnims.resize(targetNodes.size());

    std::vector<Model::NodePose> sourceBind;
    std::vector<Model::NodePose> targetBind;
    std::vector<Model::NodePose> sourcePose;
    std::vector<Model::NodePose> targetPose;
    FillBindPose(source, sourceBind);
    FillBindPose(target, targetBind);

    const float sampleRate = (std::max)(options.sampleRate, 1.0f);
    const int frameCount = (std::max)(1, static_cast<int>(std::ceil(srcAnim.secondsLength * sampleRate)));
    const float sourceToTargetScale = result.targetProfile.height / (std::max)(result.sourceProfile.height, 0.001f);

    for (int frame = 0; frame <= frameCount; ++frame) {
        const float t = frameCount > 0
            ? (srcAnim.secondsLength * static_cast<float>(frame) / static_cast<float>(frameCount))
            : 0.0f;

        sourcePose = sourceBind;
        targetPose = targetBind;
        for (size_t nodeIndex = 0; nodeIndex < sourcePose.size(); ++nodeIndex) {
            source.ComputeAnimation(sourceAnimationIndex, static_cast<int>(nodeIndex), t, sourcePose[nodeIndex]);
        }

        for (size_t slotIndex = 0; slotIndex < kBoneSlotCount; ++slotIndex) {
            const BoneSlot slot = static_cast<BoneSlot>(slotIndex);
            const int srcNode = result.sourceProfile.slots[slotIndex].nodeIndex;
            const int dstNode = result.targetProfile.slots[slotIndex].nodeIndex;
            if (srcNode < 0 || dstNode < 0 ||
                srcNode >= static_cast<int>(sourcePose.size()) ||
                dstNode >= static_cast<int>(targetPose.size())) {
                continue;
            }

            const XMVECTOR srcBindRot = LoadQuat(sourceBind[srcNode].rotation);
            const XMVECTOR srcAnimRot = LoadQuat(sourcePose[srcNode].rotation);
            const XMVECTOR dstBindRot = LoadQuat(targetBind[dstNode].rotation);
            const XMVECTOR delta = XMQuaternionMultiply(XMQuaternionInverse(srcBindRot), srcAnimRot);
            targetPose[dstNode].rotation = StoreQuat(XMQuaternionMultiply(dstBindRot, delta));

            if (options.transferRootMotion && IsRootLike(slot)) {
                XMVECTOR srcDelta = XMLoadFloat3(&sourcePose[srcNode].position) - XMLoadFloat3(&sourceBind[srcNode].position);
                srcDelta *= sourceToTargetScale;
                XMFLOAT3 dstPos{};
                XMStoreFloat3(&dstPos, XMLoadFloat3(&targetBind[dstNode].position) + srcDelta);
                targetPose[dstNode].position = dstPos;
            }
        }

        for (size_t nodeIndex = 0; nodeIndex < targetPose.size(); ++nodeIndex) {
            if (options.keepTargetScale) {
                targetPose[nodeIndex].scale = targetBind[nodeIndex].scale;
            }
            PushPoseKey(result.animation.nodeAnims[nodeIndex], t, targetPose[nodeIndex], options.keepTargetScale);
        }
    }

    result.success = true;
    return result;
}
}
