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

    bool IsAssimpFbxHelperNode(const std::string& name)
    {
        return name.find("$AssimpFbx$") != std::string::npos;
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

    XMMATRIX ExtractRotationMatrix(const XMFLOAT4X4& matrix)
    {
        XMVECTOR scale{};
        XMVECTOR rotation{};
        XMVECTOR translation{};
        if (!XMMatrixDecompose(&scale, &rotation, &translation, XMLoadFloat4x4(&matrix))) {
            return XMMatrixIdentity();
        }
        return XMMatrixRotationQuaternion(XMQuaternionNormalize(rotation));
    }

    XMVECTOR LoadTranslation(const XMFLOAT4X4& matrix)
    {
        return XMVectorSet(matrix._41, matrix._42, matrix._43, 0.0f);
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

    XMVECTOR RotationBetweenDirections(XMVECTOR fromDirection, XMVECTOR toDirection, float maxRadians)
    {
        fromDirection = XMVector3Normalize(fromDirection);
        toDirection = XMVector3Normalize(toDirection);

        float dot = 0.0f;
        XMStoreFloat(&dot, XMVector3Dot(fromDirection, toDirection));
        dot = std::clamp(dot, -1.0f, 1.0f);
        if (dot > 0.9995f) {
            return XMQuaternionIdentity();
        }

        XMVECTOR axis = XMVector3Cross(fromDirection, toDirection);
        float axisLenSq = 0.0f;
        XMStoreFloat(&axisLenSq, XMVector3LengthSq(axis));
        if (axisLenSq < 0.000001f) {
            axis = XMVector3Cross(fromDirection, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
            XMStoreFloat(&axisLenSq, XMVector3LengthSq(axis));
            if (axisLenSq < 0.000001f) {
                axis = XMVector3Cross(fromDirection, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
            }
        }
        axis = XMVector3Normalize(axis);

        const float angle = (std::min)(std::acos(dot), maxRadians);
        return XMQuaternionRotationNormal(axis, angle);
    }

    void ApplyWorldRotationToNode(
        const Model& target,
        std::vector<Model::NodePose>& targetPose,
        const std::vector<XMFLOAT4X4>& targetGlobals,
        int nodeIndex,
        XMVECTOR worldDeltaRotation)
    {
        const auto& nodes = target.GetNodes();
        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) {
            return;
        }

        const XMMATRIX currentGlobalRotation = ExtractRotationMatrix(targetGlobals[static_cast<size_t>(nodeIndex)]);
        const XMMATRIX desiredGlobalRotation =
            currentGlobalRotation * XMMatrixRotationQuaternion(XMQuaternionNormalize(worldDeltaRotation));

        XMMATRIX parentGlobalRotation = XMMatrixIdentity();
        const int parentIndex = nodes[static_cast<size_t>(nodeIndex)].parentIndex;
        if (parentIndex >= 0 && parentIndex < static_cast<int>(targetGlobals.size())) {
            parentGlobalRotation = ExtractRotationMatrix(targetGlobals[static_cast<size_t>(parentIndex)]);
        }

        const XMMATRIX localRotation = desiredGlobalRotation * XMMatrixInverse(nullptr, parentGlobalRotation);
        targetPose[static_cast<size_t>(nodeIndex)].rotation = StoreQuat(XMQuaternionRotationMatrix(localRotation));
    }

    void SolveArmIk(
        const Model& target,
        const HumanoidProfile& targetProfile,
        BoneSlot upperArmSlot,
        BoneSlot lowerArmSlot,
        BoneSlot handSlot,
        XMVECTOR desiredHandPosition,
        std::vector<Model::NodePose>& targetPose,
        const std::vector<XMFLOAT4X4>& targetBindGlobals,
        std::vector<XMFLOAT4X4>& targetGlobals)
    {
        const int upperArmNode = targetProfile.Get(upperArmSlot);
        const int lowerArmNode = targetProfile.Get(lowerArmSlot);
        const int handNode = targetProfile.Get(handSlot);
        if (upperArmNode < 0 || lowerArmNode < 0 || handNode < 0) {
            return;
        }

        XMVECTOR upperBind = LoadTranslation(targetBindGlobals[static_cast<size_t>(upperArmNode)]);
        XMVECTOR lowerBind = LoadTranslation(targetBindGlobals[static_cast<size_t>(lowerArmNode)]);
        XMVECTOR handBind = LoadTranslation(targetBindGlobals[static_cast<size_t>(handNode)]);
        float upperLen = 0.0f;
        float lowerLen = 0.0f;
        XMStoreFloat(&upperLen, XMVector3Length(lowerBind - upperBind));
        XMStoreFloat(&lowerLen, XMVector3Length(handBind - lowerBind));

        const float maxReach = (upperLen + lowerLen) * 0.98f;
        XMVECTOR upperCurrent = LoadTranslation(targetGlobals[static_cast<size_t>(upperArmNode)]);
        XMVECTOR desiredFromUpper = desiredHandPosition - upperCurrent;
        float desiredDistance = 0.0f;
        XMStoreFloat(&desiredDistance, XMVector3Length(desiredFromUpper));
        if (desiredDistance > maxReach && desiredDistance > 0.0001f) {
            desiredHandPosition = upperCurrent + XMVector3Normalize(desiredFromUpper) * maxReach;
        }

        constexpr int kIterations = 8;
        constexpr float kMaxStepRadians = XMConvertToRadians(18.0f);
        const int joints[] = { lowerArmNode, upperArmNode };
        for (int iteration = 0; iteration < kIterations; ++iteration) {
            BuildGlobalMatrices(target, targetPose, targetGlobals);

            XMVECTOR handPosition = LoadTranslation(targetGlobals[static_cast<size_t>(handNode)]);
            float error = 0.0f;
            XMStoreFloat(&error, XMVector3Length(desiredHandPosition - handPosition));
            if (error < 0.5f) {
                break;
            }

            for (int jointNode : joints) {
                const XMVECTOR jointPosition = LoadTranslation(targetGlobals[static_cast<size_t>(jointNode)]);
                handPosition = LoadTranslation(targetGlobals[static_cast<size_t>(handNode)]);
                const XMVECTOR currentVector = handPosition - jointPosition;
                const XMVECTOR desiredVector = desiredHandPosition - jointPosition;

                float currentLenSq = 0.0f;
                float desiredLenSq = 0.0f;
                XMStoreFloat(&currentLenSq, XMVector3LengthSq(currentVector));
                XMStoreFloat(&desiredLenSq, XMVector3LengthSq(desiredVector));
                if (currentLenSq < 0.0001f || desiredLenSq < 0.0001f) {
                    continue;
                }

                const XMVECTOR deltaRotation = RotationBetweenDirections(currentVector, desiredVector, kMaxStepRadians);
                ApplyWorldRotationToNode(target, targetPose, targetGlobals, jointNode, deltaRotation);
                BuildGlobalMatrices(target, targetPose, targetGlobals);
            }
        }
    }

    std::vector<int> CollectNodeChain(const Model& model, int ancestorNode, int descendantNode)
    {
        std::vector<int> reversed;
        if (ancestorNode < 0 || descendantNode < 0) {
            return {};
        }

        const auto& nodes = model.GetNodes();
        for (int node = descendantNode;
            node >= 0 && node < static_cast<int>(nodes.size());
            node = nodes[static_cast<size_t>(node)].parentIndex) {
            if (!IsAssimpFbxHelperNode(nodes[static_cast<size_t>(node)].name)) {
                reversed.push_back(node);
            }
            if (node == ancestorNode) {
                break;
            }
        }

        if (reversed.empty() || reversed.back() != ancestorNode) {
            return {};
        }

        std::reverse(reversed.begin(), reversed.end());
        return reversed;
    }

    void MapChainNodes(
        const Model& source,
        int sourceStart,
        int sourceEnd,
        const Model& target,
        int targetStart,
        int targetEnd,
        std::vector<int>& sourceNodeForTarget)
    {
        const std::vector<int> sourceChain = CollectNodeChain(source, sourceStart, sourceEnd);
        const std::vector<int> targetChain = CollectNodeChain(target, targetStart, targetEnd);
        if (sourceChain.empty() || targetChain.empty()) {
            return;
        }

        const float sourceMax = static_cast<float>((std::max)(sourceChain.size(), size_t{ 1 }) - 1);
        const float targetMax = static_cast<float>((std::max)(targetChain.size(), size_t{ 1 }) - 1);
        for (size_t targetIndex = 0; targetIndex < targetChain.size(); ++targetIndex) {
            const float normalized = targetMax > 0.0f
                ? static_cast<float>(targetIndex) / targetMax
                : 0.0f;
            const size_t sourceIndex = static_cast<size_t>((std::min)(
                sourceMax,
                std::round(normalized * sourceMax)));
            const int targetNode = targetChain[targetIndex];
            if (targetNode >= 0 && targetNode < static_cast<int>(sourceNodeForTarget.size())) {
                sourceNodeForTarget[static_cast<size_t>(targetNode)] = sourceChain[sourceIndex];
            }
        }
    }

    XMVECTOR SlotGlobalPosition(const std::vector<XMFLOAT4X4>& globals, const HumanoidProfile& profile, BoneSlot slot)
    {
        const int node = profile.Get(slot);
        if (node < 0 || node >= static_cast<int>(globals.size())) {
            return XMVectorZero();
        }
        const XMFLOAT4X4& matrix = globals[static_cast<size_t>(node)];
        const XMFLOAT3 position{ matrix._41, matrix._42, matrix._43 };
        return XMLoadFloat3(&position);
    }

    bool ShouldFlipSideMapping(
        const std::vector<XMFLOAT4X4>& sourceGlobals,
        const HumanoidProfile& sourceProfile,
        const std::vector<XMFLOAT4X4>& targetGlobals,
        const HumanoidProfile& targetProfile)
    {
        const XMVECTOR sourceCenter = SlotGlobalPosition(sourceGlobals, sourceProfile, BoneSlot::Hips);
        const XMVECTOR targetCenter = SlotGlobalPosition(targetGlobals, targetProfile, BoneSlot::Hips);
        XMVECTOR sourceLeft = SlotGlobalPosition(sourceGlobals, sourceProfile, BoneSlot::LeftHand) - sourceCenter;
        XMVECTOR sourceRight = SlotGlobalPosition(sourceGlobals, sourceProfile, BoneSlot::RightHand) - sourceCenter;
        XMVECTOR targetLeft = SlotGlobalPosition(targetGlobals, targetProfile, BoneSlot::LeftHand) - targetCenter;
        XMVECTOR targetRight = SlotGlobalPosition(targetGlobals, targetProfile, BoneSlot::RightHand) - targetCenter;

        sourceLeft = XMVector3Normalize(sourceLeft);
        sourceRight = XMVector3Normalize(sourceRight);
        targetLeft = XMVector3Normalize(targetLeft);
        targetRight = XMVector3Normalize(targetRight);

        float sameScore = 0.0f;
        float flippedScore = 0.0f;
        XMStoreFloat(&sameScore, XMVector3Dot(sourceLeft, targetLeft) + XMVector3Dot(sourceRight, targetRight));
        XMStoreFloat(&flippedScore, XMVector3Dot(sourceLeft, targetRight) + XMVector3Dot(sourceRight, targetLeft));
        return flippedScore > sameScore + 0.25f;
    }

    // Quaternion that rotates 'from' direction onto 'to' direction (shortest arc).
    XMVECTOR QuaternionFromTo(XMVECTOR from, XMVECTOR to)
    {
        from = XMVector3Normalize(from);
        to = XMVector3Normalize(to);
        float dot = 0.0f;
        XMStoreFloat(&dot, XMVector3Dot(from, to));
        if (dot > 0.99999f) {
            return XMQuaternionIdentity();
        }
        if (dot < -0.99999f) {
            XMVECTOR axis = XMVector3Cross(from, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
            float lenSq = 0.0f;
            XMStoreFloat(&lenSq, XMVector3LengthSq(axis));
            if (lenSq < 0.0001f) {
                axis = XMVector3Cross(from, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
            }
            return XMQuaternionRotationAxis(XMVector3Normalize(axis), XM_PI);
        }
        XMVECTOR axis = XMVector3Cross(from, to);
        XMVECTOR q = XMVectorSetW(axis, 1.0f + dot);
        return XMQuaternionNormalize(q);
    }

    // For each humanoid limb / spine bone, compute a per-bone "bind alignment" quaternion
    // expressed in the TARGET's parent-local frame. Applying this quaternion to the
    // target's bind LOCAL rotation rotates the bone so its world direction matches the
    // source's same-slot bone's world direction. This effectively converts the target's
    // A-pose bind into a T-pose-equivalent bind (per bone), so bind-relative motion
    // transfer no longer carries the source/target rest-pose offset.
    //
    // The alignment only changes the bone's DIRECTION (swing), not its twist around the
    // bone axis, so target's mesh skinning remains correct.
    std::vector<XMFLOAT4> ComputeBindAlignment(
        const Model& source,
        const Model& target,
        const HumanoidProfile& sourceProfile,
        const HumanoidProfile& targetProfile,
        const std::vector<XMFLOAT4X4>& sourceBindGlobals,
        const std::vector<XMFLOAT4X4>& targetBindGlobals)
    {
        const auto& targetNodes = target.GetNodes();
        std::vector<XMFLOAT4> calibration(targetNodes.size());
        const XMFLOAT4 kIdentityQ{ 0.0f, 0.0f, 0.0f, 1.0f };
        for (auto& q : calibration) {
            q = kIdentityQ;
        }

        // Chain pairs (from -> to). Only LIMB bones get calibration:
        //   - Spine/Chest/Neck/Head chains have near-identical rest pose between rigs.
        //     Calibrating them stacks tiny direction errors through the hierarchy and
        //     produces a runaway forward-lean of the whole body.
        //   - Hand/Toes are leaves with no clear "next bone" to define direction.
        //   - Shoulder calibration also tends to over-rotate the arm root, so skip.
        struct Chain { BoneSlot from; BoneSlot to; };
        const Chain chains[] = {
            { BoneSlot::LeftUpperArm, BoneSlot::LeftLowerArm },
            { BoneSlot::LeftLowerArm, BoneSlot::LeftHand },
            { BoneSlot::RightUpperArm, BoneSlot::RightLowerArm },
            { BoneSlot::RightLowerArm, BoneSlot::RightHand },
            { BoneSlot::LeftUpperLeg, BoneSlot::LeftLowerLeg },
            { BoneSlot::LeftLowerLeg, BoneSlot::LeftFoot },
            { BoneSlot::RightUpperLeg, BoneSlot::RightLowerLeg },
            { BoneSlot::RightLowerLeg, BoneSlot::RightFoot },
        };

        // Compute character "facing" difference quaternion from Hips world rotations.
        // Source's world directions are rotated by this to be expressed in target's
        // facing frame before being compared with target's world directions. This makes
        // calibration robust to source vs target characters facing different world axes.
        XMVECTOR qFacingDiff = XMQuaternionIdentity();
        {
            const int srcHipsIdx = sourceProfile.Get(BoneSlot::Hips);
            const int tgtHipsIdx = targetProfile.Get(BoneSlot::Hips);
            if (srcHipsIdx >= 0 && tgtHipsIdx >= 0 &&
                srcHipsIdx < static_cast<int>(sourceBindGlobals.size()) &&
                tgtHipsIdx < static_cast<int>(targetBindGlobals.size())) {
                const XMMATRIX rSrcHips = ExtractRotationMatrix(sourceBindGlobals[srcHipsIdx]);
                const XMMATRIX rTgtHips = ExtractRotationMatrix(targetBindGlobals[tgtHipsIdx]);
                const XMVECTOR qSrcHips = XMQuaternionRotationMatrix(rSrcHips);
                const XMVECTOR qTgtHips = XMQuaternionRotationMatrix(rTgtHips);
                qFacingDiff = XMQuaternionNormalize(
                    XMQuaternionMultiply(XMQuaternionInverse(qSrcHips), qTgtHips));
            }
        }

        const auto& targetNodesRef = targetNodes;
        for (const Chain& chain : chains) {
            const int srcFrom = sourceProfile.Get(chain.from);
            const int srcTo = sourceProfile.Get(chain.to);
            const int tgtFrom = targetProfile.Get(chain.from);
            const int tgtTo = targetProfile.Get(chain.to);
            if (srcFrom < 0 || srcTo < 0 || tgtFrom < 0 || tgtTo < 0) {
                continue;
            }
            if (srcFrom >= static_cast<int>(sourceBindGlobals.size()) ||
                srcTo >= static_cast<int>(sourceBindGlobals.size()) ||
                tgtFrom >= static_cast<int>(targetBindGlobals.size()) ||
                tgtTo >= static_cast<int>(targetBindGlobals.size())) {
                continue;
            }

            const XMVECTOR pSrcFrom = LoadTranslation(sourceBindGlobals[srcFrom]);
            const XMVECTOR pSrcTo = LoadTranslation(sourceBindGlobals[srcTo]);
            const XMVECTOR pTgtFrom = LoadTranslation(targetBindGlobals[tgtFrom]);
            const XMVECTOR pTgtTo = LoadTranslation(targetBindGlobals[tgtTo]);

            const XMVECTOR dSrcRaw = XMVectorSubtract(pSrcTo, pSrcFrom);
            const XMVECTOR dTgt = XMVectorSubtract(pTgtTo, pTgtFrom);
            float lsSrc = 0.0f, lsTgt = 0.0f;
            XMStoreFloat(&lsSrc, XMVector3LengthSq(dSrcRaw));
            XMStoreFloat(&lsTgt, XMVector3LengthSq(dTgt));
            if (lsSrc < 0.0001f || lsTgt < 0.0001f) {
                continue;
            }

            // Rotate source's world direction into target's facing frame, so the
            // comparison reflects anatomical sameness rather than world-axis sameness.
            const XMVECTOR dSrc = XMVector3Rotate(dSrcRaw, qFacingDiff);

            // World-space rotation that aligns target's bone direction onto source's.
            XMVECTOR qWorld = QuaternionFromTo(dTgt, dSrc);

            // Cap calibration angle. A very large correction usually indicates a bad
            // mapping or extreme axis mismatch; better to leave the bone at its bind.
            float qW = 0.0f;
            XMStoreFloat(&qW, XMVectorSplatW(qWorld));
            const float angle = 2.0f * std::acos(std::clamp(qW, -1.0f, 1.0f));
            constexpr float kMaxAngle = XMConvertToRadians(60.0f);
            if (angle > kMaxAngle) {
                const float ratio = kMaxAngle / angle;
                qWorld = XMQuaternionSlerp(XMQuaternionIdentity(), qWorld, ratio);
            }

            // Express qWorld in target's parent-local frame:
            //   q_parent_local = R_parent_world * q_world * inv(R_parent_world)
            // i.e., apply parent transform, then qWorld, then undo parent transform.
            XMVECTOR parentWorldQ = XMQuaternionIdentity();
            const int parentIndex = targetNodesRef[static_cast<size_t>(tgtFrom)].parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(targetBindGlobals.size())) {
                const XMMATRIX parentMat = ExtractRotationMatrix(targetBindGlobals[static_cast<size_t>(parentIndex)]);
                parentWorldQ = XMQuaternionRotationMatrix(parentMat);
            }
            const XMVECTOR parentWorldQInv = XMQuaternionInverse(parentWorldQ);
            const XMVECTOR qLocal = XMQuaternionNormalize(
                XMQuaternionMultiply(XMQuaternionMultiply(parentWorldQ, qWorld), parentWorldQInv));

            XMStoreFloat4(&calibration[static_cast<size_t>(tgtFrom)], qLocal);
        }

        return calibration;
    }

    BoneSlot OppositeSideSlot(BoneSlot slot)
    {
        switch (slot) {
        case BoneSlot::LeftShoulder: return BoneSlot::RightShoulder;
        case BoneSlot::LeftUpperArm: return BoneSlot::RightUpperArm;
        case BoneSlot::LeftLowerArm: return BoneSlot::RightLowerArm;
        case BoneSlot::LeftHand: return BoneSlot::RightHand;
        case BoneSlot::LeftUpperLeg: return BoneSlot::RightUpperLeg;
        case BoneSlot::LeftLowerLeg: return BoneSlot::RightLowerLeg;
        case BoneSlot::LeftFoot: return BoneSlot::RightFoot;
        case BoneSlot::LeftToes: return BoneSlot::RightToes;
        case BoneSlot::RightShoulder: return BoneSlot::LeftShoulder;
        case BoneSlot::RightUpperArm: return BoneSlot::LeftUpperArm;
        case BoneSlot::RightLowerArm: return BoneSlot::LeftLowerArm;
        case BoneSlot::RightHand: return BoneSlot::LeftHand;
        case BoneSlot::RightUpperLeg: return BoneSlot::LeftUpperLeg;
        case BoneSlot::RightLowerLeg: return BoneSlot::LeftLowerLeg;
        case BoneSlot::RightFoot: return BoneSlot::LeftFoot;
        case BoneSlot::RightToes: return BoneSlot::LeftToes;
        default: return slot;
        }
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
        if (IsAssimpFbxHelperNode(nodes[nodeIndex].name)) {
            continue;
        }

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

    std::vector<XMFLOAT4X4> sourceBindGlobals;
    std::vector<XMFLOAT4X4> targetBindGlobals;
    BuildGlobalMatrices(source, sourceBind, sourceBindGlobals);
    BuildGlobalMatrices(target, targetBind, targetBindGlobals);

    std::vector<int> targetNodeToSlot(targetNodes.size(), -1);
    std::vector<int> sourceNodeForTarget(targetNodes.size(), -1);
    for (size_t slotIndex = 0; slotIndex < kBoneSlotCount; ++slotIndex) {
        const int srcNode = result.sourceProfile.slots[slotIndex].nodeIndex;
        const int dstNode = result.targetProfile.slots[slotIndex].nodeIndex;
        if (srcNode >= 0 &&
            dstNode >= 0 &&
            srcNode < static_cast<int>(sourceBind.size()) &&
            dstNode < static_cast<int>(targetBind.size())) {
            targetNodeToSlot[static_cast<size_t>(dstNode)] = static_cast<int>(slotIndex);
            sourceNodeForTarget[static_cast<size_t>(dstNode)] = srcNode;
        }
    }
    const bool flipSideMapping = ShouldFlipSideMapping(
        sourceBindGlobals,
        result.sourceProfile,
        targetBindGlobals,
        result.targetProfile);

    for (size_t slotIndex = 0; slotIndex < kBoneSlotCount; ++slotIndex) {
        const BoneSlot targetSlot = static_cast<BoneSlot>(slotIndex);
        const BoneSlot sourceSlot = flipSideMapping ? OppositeSideSlot(targetSlot) : targetSlot;
        const int srcNode = result.sourceProfile.Get(sourceSlot);
        const int dstNode = result.targetProfile.Get(targetSlot);
        if (srcNode >= 0 &&
            dstNode >= 0 &&
            srcNode < static_cast<int>(sourceBind.size()) &&
            dstNode < static_cast<int>(targetBind.size())) {
            sourceNodeForTarget[static_cast<size_t>(dstNode)] = srcNode;
        }
    }

    MapChainNodes(
        source,
        result.sourceProfile.Get(BoneSlot::Spine),
        result.sourceProfile.Get(BoneSlot::Chest),
        target,
        result.targetProfile.Get(BoneSlot::Spine),
        result.targetProfile.Get(BoneSlot::Chest),
        sourceNodeForTarget);

    const float sampleRate = (std::max)(options.sampleRate, 1.0f);
    const int frameCount = (std::max)(1, static_cast<int>(std::ceil(srcAnim.secondsLength * sampleRate)));
    const float sourceToTargetScale = result.targetProfile.height / (std::max)(result.sourceProfile.height, 0.001f);

    std::vector<Model::NodePose> sourceRetargetPose;
    std::vector<XMFLOAT4X4> sourceAnimGlobals;
    std::vector<XMFLOAT4X4> targetCurrentGlobals(targetNodes.size());

    for (int frame = 0; frame <= frameCount; ++frame) {
        const float t = frameCount > 0
            ? (srcAnim.secondsLength * static_cast<float>(frame) / static_cast<float>(frameCount))
            : 0.0f;

        sourcePose = sourceBind;
        targetPose = targetBind;
        for (size_t nodeIndex = 0; nodeIndex < sourcePose.size(); ++nodeIndex) {
            source.ComputeAnimation(sourceAnimationIndex, static_cast<int>(nodeIndex), t, sourcePose[nodeIndex]);
        }
        sourceRetargetPose = sourcePose;
        if (options.zeroHipsRotation) {
            for (size_t slotIndex = 0; slotIndex < kBoneSlotCount; ++slotIndex) {
                const BoneSlot slot = static_cast<BoneSlot>(slotIndex);
                if (!IsRootLike(slot)) {
                    continue;
                }
                const int srcNode = result.sourceProfile.slots[slotIndex].nodeIndex;
                if (srcNode >= 0 && srcNode < static_cast<int>(sourceRetargetPose.size())) {
                    sourceRetargetPose[static_cast<size_t>(srcNode)].rotation =
                        sourceBind[static_cast<size_t>(srcNode)].rotation;
                }
            }
        }
        BuildGlobalMatrices(source, sourceRetargetPose, sourceAnimGlobals);

        for (size_t nodeIndex = 0; nodeIndex < targetNodes.size(); ++nodeIndex) {
            const int slotIndex = targetNodeToSlot[nodeIndex];
            const int mappedSourceNode = sourceNodeForTarget[nodeIndex];
            const bool rootLikeTarget = slotIndex >= 0 && IsRootLike(static_cast<BoneSlot>(slotIndex));
            if (mappedSourceNode >= 0) {
                const int srcNode = mappedSourceNode;
                if (srcNode >= 0 &&
                    srcNode < static_cast<int>(sourceAnimGlobals.size()) &&
                    nodeIndex < targetPose.size() &&
                    nodeIndex < targetBindGlobals.size()) {
                    if (!rootLikeTarget) {
                        const XMMATRIX sourceBindRotation = ExtractRotationMatrix(sourceBindGlobals[static_cast<size_t>(srcNode)]);
                        const XMMATRIX sourceAnimRotation = ExtractRotationMatrix(sourceAnimGlobals[static_cast<size_t>(srcNode)]);
                        const XMMATRIX targetBindRotation = ExtractRotationMatrix(targetBindGlobals[nodeIndex]);
                        const XMMATRIX sourceDelta = XMMatrixInverse(nullptr, sourceBindRotation) * sourceAnimRotation;
                        const XMMATRIX desiredTargetGlobal = targetBindRotation * sourceDelta;

                        XMMATRIX parentGlobalRotation = XMMatrixIdentity();
                        const int parentIndex = targetNodes[nodeIndex].parentIndex;
                        if (parentIndex >= 0 && parentIndex < static_cast<int>(targetCurrentGlobals.size())) {
                            parentGlobalRotation = ExtractRotationMatrix(targetCurrentGlobals[static_cast<size_t>(parentIndex)]);
                        }

                        const XMMATRIX localRotation = desiredTargetGlobal * XMMatrixInverse(nullptr, parentGlobalRotation);
                        targetPose[nodeIndex].rotation = StoreQuat(XMQuaternionRotationMatrix(localRotation));
                    }

                    if (options.transferRootMotion && rootLikeTarget) {
                        XMVECTOR srcDelta = XMLoadFloat3(&sourcePose[static_cast<size_t>(srcNode)].position) -
                            XMLoadFloat3(&sourceBind[static_cast<size_t>(srcNode)].position);
                        srcDelta *= sourceToTargetScale;
                        XMFLOAT3 dstPos{};
                        XMStoreFloat3(&dstPos, XMLoadFloat3(&targetBind[nodeIndex].position) + srcDelta);
                        targetPose[nodeIndex].position = dstPos;
                    }
                }
            }

            if (options.keepTargetScale) {
                targetPose[nodeIndex].scale = targetBind[nodeIndex].scale;
            }

            XMMATRIX parentGlobal = XMMatrixIdentity();
            const int parentIndex = targetNodes[nodeIndex].parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(targetCurrentGlobals.size())) {
                parentGlobal = XMLoadFloat4x4(&targetCurrentGlobals[static_cast<size_t>(parentIndex)]);
            }
            XMStoreFloat4x4(&targetCurrentGlobals[nodeIndex], LocalMatrix(targetPose[nodeIndex]) * parentGlobal);
        }

        const auto solveHand = [&](BoneSlot upperArm, BoneSlot lowerArm, BoneSlot hand) {
            const int targetHandNode = result.targetProfile.Get(hand);
            if (targetHandNode < 0 || targetHandNode >= static_cast<int>(sourceNodeForTarget.size())) {
                return;
            }

            const BoneSlot sourceHandSlot = flipSideMapping ? OppositeSideSlot(hand) : hand;
            const int sourceHandNode = result.sourceProfile.Get(sourceHandSlot);
            if (sourceHandNode < 0 ||
                sourceHandNode >= static_cast<int>(sourceAnimGlobals.size()) ||
                sourceHandNode >= static_cast<int>(sourceBindGlobals.size()) ||
                targetHandNode >= static_cast<int>(targetBindGlobals.size())) {
                return;
            }

            XMVECTOR sourceDelta = LoadTranslation(sourceAnimGlobals[static_cast<size_t>(sourceHandNode)]) -
                LoadTranslation(sourceBindGlobals[static_cast<size_t>(sourceHandNode)]);
            sourceDelta *= sourceToTargetScale;
            if (flipSideMapping) {
                sourceDelta = XMVectorSet(
                    -XMVectorGetX(sourceDelta),
                    XMVectorGetY(sourceDelta),
                    XMVectorGetZ(sourceDelta),
                    0.0f);
            }

            const XMVECTOR desiredHandPosition =
                LoadTranslation(targetBindGlobals[static_cast<size_t>(targetHandNode)]) + sourceDelta;
            SolveArmIk(
                target,
                result.targetProfile,
                upperArm,
                lowerArm,
                hand,
                desiredHandPosition,
                targetPose,
                targetBindGlobals,
                targetCurrentGlobals);
        };

        if (options.enableArmIk) {
            solveHand(BoneSlot::LeftUpperArm, BoneSlot::LeftLowerArm, BoneSlot::LeftHand);
            solveHand(BoneSlot::RightUpperArm, BoneSlot::RightLowerArm, BoneSlot::RightHand);
        }

        for (size_t nodeIndex = 0; nodeIndex < targetPose.size(); ++nodeIndex) {
            PushPoseKey(result.animation.nodeAnims[nodeIndex], t, targetPose[nodeIndex], options.keepTargetScale);
        }
    }

    result.success = true;
    return result;
}

namespace
{
RetargetResult BakeAnimationLocalBindRelativeCore(
    const Model& source,
    int sourceAnimationIndex,
    const Model& target,
    const Model* referenceTPose,
    const RetargetOptions& options)
{
    RetargetResult result{};
    result.sourceProfile = AnalyzeSkeleton(source);
    result.targetProfile = AnalyzeSkeleton(target);
    HumanoidProfile referenceProfile{};
    result.warnings.insert(result.warnings.end(), result.sourceProfile.warnings.begin(), result.sourceProfile.warnings.end());
    result.warnings.insert(result.warnings.end(), result.targetProfile.warnings.begin(), result.targetProfile.warnings.end());
    if (referenceTPose) {
        referenceProfile = AnalyzeSkeleton(*referenceTPose);
        result.warnings.insert(result.warnings.end(), referenceProfile.warnings.begin(), referenceProfile.warnings.end());
        if (!referenceProfile.valid) {
            result.warnings.push_back("Reference T-pose model is not a supported humanoid.");
            return result;
        }
    }

    const auto& sourceAnimations = source.GetAnimations();
    if (sourceAnimationIndex < 0 || sourceAnimationIndex >= static_cast<int>(sourceAnimations.size())) {
        result.warnings.push_back("Invalid source animation index.");
        return result;
    }
    if (!result.sourceProfile.valid || !result.targetProfile.valid) {
        return result;
    }

    const Model::Animation& srcAnim = sourceAnimations[sourceAnimationIndex];
    const auto& sourceNodes = source.GetNodes();
    const auto& targetNodes = target.GetNodes();
    const Model& calibrationModel = referenceTPose ? *referenceTPose : source;
    const HumanoidProfile& calibrationProfile = referenceTPose ? referenceProfile : result.sourceProfile;
    const auto& calibrationNodes = calibrationModel.GetNodes();

    result.animation.name = srcAnim.name + options.outputNameSuffix;
    result.animation.secondsLength = (std::max)(srcAnim.secondsLength, 1.0f / 60.0f);
    result.animation.nodeAnims.resize(targetNodes.size());

    // Build bind globals for the animated source, target, and optional neutral
    // T-pose reference. The reference is used only for target direction calibration;
    // animation deltas remain relative to the source FBX's own local bind axes.
    std::vector<Model::NodePose> sourceBind, targetBind, referenceBind;
    FillBindPose(source, sourceBind);
    FillBindPose(target, targetBind);
    if (referenceTPose) {
        FillBindPose(*referenceTPose, referenceBind);
    }
    std::vector<XMFLOAT4X4> sourceBindGlobals, targetBindGlobals, referenceBindGlobals;
    BuildGlobalMatrices(source, sourceBind, sourceBindGlobals);
    BuildGlobalMatrices(target, targetBind, targetBindGlobals);
    if (referenceTPose) {
        BuildGlobalMatrices(*referenceTPose, referenceBind, referenceBindGlobals);
    }
    const std::vector<XMFLOAT4X4>& calibrationBindGlobals =
        referenceTPose ? referenceBindGlobals : sourceBindGlobals;

    // Per-bone T-pose calibration. For limb root bones (UpperArm, UpperLeg), compute the
    // rotation in the target's parent-local frame that aligns the target's bind bone
    // direction with the neutral T-pose reference when provided, otherwise with the
    // source bind pose. This is computed independently per bone (no hierarchy compound),
    // and the parent-local direct comparison avoids the world->parent conversion that
    // broke the previous approach.
    //
    // Limbs below the root (LowerArm, Hand, LowerLeg, Foot) are left uncalibrated: their
    // bind orientation RELATIVE TO their parent is essentially the same between rigs
    // (straight elbow / knee), so calibrating the root propagates the correct orientation
    // through the chain at runtime.
    std::vector<XMFLOAT4> bindCalibration(targetNodes.size(), XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f });
    {
        struct LimbRoot { BoneSlot bone; BoneSlot child; };
        const LimbRoot limbRoots[] = {
            { BoneSlot::LeftUpperArm, BoneSlot::LeftLowerArm },
            { BoneSlot::RightUpperArm, BoneSlot::RightLowerArm },
            { BoneSlot::LeftUpperLeg, BoneSlot::LeftLowerLeg },
            { BoneSlot::RightUpperLeg, BoneSlot::RightLowerLeg },
        };

        for (const LimbRoot& lr : limbRoots) {
            // When a separate reference T-pose is supplied, use it only to correct the
            // common A-pose/T-pose arm offset. Leg calibration tends to push the whole
            // body through the hips and is visually much worse than leaving the target
            // lower body in its own bind pose.
            if (referenceTPose &&
                (lr.bone == BoneSlot::LeftUpperLeg || lr.bone == BoneSlot::RightUpperLeg)) {
                continue;
            }

            const int srcThis = calibrationProfile.Get(lr.bone);
            const int srcChild = calibrationProfile.Get(lr.child);
            const int tgtThis = result.targetProfile.Get(lr.bone);
            const int tgtChild = result.targetProfile.Get(lr.child);
            if (srcThis < 0 || srcChild < 0 || tgtThis < 0 || tgtChild < 0) {
                continue;
            }
            if (srcThis >= static_cast<int>(calibrationBindGlobals.size()) ||
                srcChild >= static_cast<int>(calibrationBindGlobals.size()) ||
                tgtThis >= static_cast<int>(targetBindGlobals.size()) ||
                tgtChild >= static_cast<int>(targetBindGlobals.size())) {
                continue;
            }

            // World-space directions from "this" bone to its child at bind.
            const XMVECTOR pSrcThis = LoadTranslation(calibrationBindGlobals[srcThis]);
            const XMVECTOR pSrcChild = LoadTranslation(calibrationBindGlobals[srcChild]);
            const XMVECTOR pTgtThis = LoadTranslation(targetBindGlobals[tgtThis]);
            const XMVECTOR pTgtChild = LoadTranslation(targetBindGlobals[tgtChild]);

            const XMVECTOR dSrcWorld = XMVectorSubtract(pSrcChild, pSrcThis);
            const XMVECTOR dTgtWorld = XMVectorSubtract(pTgtChild, pTgtThis);
            float lsSrc = 0.0f, lsTgt = 0.0f;
            XMStoreFloat(&lsSrc, XMVector3LengthSq(dSrcWorld));
            XMStoreFloat(&lsTgt, XMVector3LengthSq(dTgtWorld));
            if (lsSrc < 0.0001f || lsTgt < 0.0001f) {
                continue;
            }

            // Express directions in the respective parent-local frame. Comparison in
            // parent-local space is robust as long as source and target parents (Chest
            // for arms, Hips for legs) are similarly oriented at bind, which holds for
            // typical T-pose vs A-pose humanoid rigs.
            const auto& targetNodesLocal = targetNodes;
            const int tgtParent = targetNodesLocal[static_cast<size_t>(tgtThis)].parentIndex;
            const int srcParent = calibrationNodes[static_cast<size_t>(srcThis)].parentIndex;

            XMMATRIX rSrcParent = XMMatrixIdentity();
            if (srcParent >= 0 && srcParent < static_cast<int>(calibrationBindGlobals.size())) {
                rSrcParent = ExtractRotationMatrix(calibrationBindGlobals[static_cast<size_t>(srcParent)]);
            }
            XMMATRIX rTgtParent = XMMatrixIdentity();
            if (tgtParent >= 0 && tgtParent < static_cast<int>(targetBindGlobals.size())) {
                rTgtParent = ExtractRotationMatrix(targetBindGlobals[static_cast<size_t>(tgtParent)]);
            }

            const XMVECTOR dSrcParentLocal = XMVector3TransformNormal(dSrcWorld, XMMatrixInverse(nullptr, rSrcParent));
            const XMVECTOR dTgtParentLocal = XMVector3TransformNormal(dTgtWorld, XMMatrixInverse(nullptr, rTgtParent));

            // Rotation in target's parent-local frame that aligns target's bone direction
            // onto source's bone direction (both expressed in their respective parent frames).
            XMVECTOR qCalib = XMQuaternionNormalize(QuaternionFromTo(dTgtParentLocal, dSrcParentLocal));
            if (referenceTPose) {
                float qW = 1.0f;
                XMStoreFloat(&qW, XMVectorSplatW(qCalib));
                const float angle = 2.0f * std::acos(std::clamp(qW, -1.0f, 1.0f));
                if (angle > XMConvertToRadians(60.0f)) {
                    qCalib = XMQuaternionIdentity();
                }
            }

            XMStoreFloat4(&bindCalibration[static_cast<size_t>(tgtThis)], qCalib);
        }
    }

    // For each target node, find the corresponding source node. Direct slot mapping for
    // humanoid bones; spine intermediate bones (Spine -> Chest chain) via proportional
    // chain mapping.
    std::vector<int> sourceNodeForTarget(targetNodes.size(), -1);
    for (size_t slotIndex = 0; slotIndex < kBoneSlotCount; ++slotIndex) {
        const int srcNode = result.sourceProfile.slots[slotIndex].nodeIndex;
        const int dstNode = result.targetProfile.slots[slotIndex].nodeIndex;
        if (srcNode >= 0 && dstNode >= 0 &&
            srcNode < static_cast<int>(sourceNodes.size()) &&
            dstNode < static_cast<int>(targetNodes.size())) {
            sourceNodeForTarget[static_cast<size_t>(dstNode)] = srcNode;
        }
    }
    MapChainNodes(
        source,
        result.sourceProfile.Get(BoneSlot::Spine),
        result.sourceProfile.Get(BoneSlot::Chest),
        target,
        result.targetProfile.Get(BoneSlot::Spine),
        result.targetProfile.Get(BoneSlot::Chest),
        sourceNodeForTarget);

    // Seed each target NodeAnim with bind keyframes so unmapped bones still produce
    // valid keyframe sequences at start/end times.
    const float endTime = result.animation.secondsLength;
    for (size_t targetNodeIndex = 0; targetNodeIndex < targetNodes.size(); ++targetNodeIndex) {
        Model::NodeAnim& na = result.animation.nodeAnims[targetNodeIndex];
        const Model::Node& tgt = targetNodes[targetNodeIndex];
        na.positionKeyframes = {
            Model::VectorKeyframe{ 0.0f, tgt.position },
            Model::VectorKeyframe{ endTime, tgt.position },
        };
        na.rotationKeyframes = {
            Model::QuaternionKeyframe{ 0.0f, tgt.rotation },
            Model::QuaternionKeyframe{ endTime, tgt.rotation },
        };
        na.scaleKeyframes = {
            Model::VectorKeyframe{ 0.0f, tgt.scale },
            Model::VectorKeyframe{ endTime, tgt.scale },
        };
    }

    // Bind-relative LOCAL rotation transfer.
    int matched = 0;
    for (size_t targetNodeIndex = 0; targetNodeIndex < targetNodes.size(); ++targetNodeIndex) {
        const int srcNode = sourceNodeForTarget[targetNodeIndex];
        if (srcNode < 0 || srcNode >= static_cast<int>(srcAnim.nodeAnims.size())) {
            continue;
        }

        const Model::NodeAnim& srcNa = srcAnim.nodeAnims[static_cast<size_t>(srcNode)];
        if (srcNa.rotationKeyframes.empty()) {
            continue;
        }

        const Model::Node& tgtNode_ = targetNodes[targetNodeIndex];

        const XMVECTOR srcBindLocal = LoadQuat(sourceNodes[static_cast<size_t>(srcNode)].rotation);
        const XMVECTOR srcBindInv = XMQuaternionInverse(srcBindLocal);
        const XMVECTOR tgtBindLocal = LoadQuat(tgtNode_.rotation);
        // Apply T-pose bind calibration: rotates target's bind so its direction matches
        // the neutral reference pose (or source bind if no reference was supplied).
        // Source deltas stay relative to source bind to preserve the source local axes.
        const XMVECTOR calibQ = LoadQuat(bindCalibration[targetNodeIndex]);
        const XMVECTOR tgtBindAligned = XMQuaternionNormalize(XMQuaternionMultiply(tgtBindLocal, calibQ));

        Model::NodeAnim& outNa = result.animation.nodeAnims[targetNodeIndex];
        outNa.rotationKeyframes.clear();
        outNa.rotationKeyframes.reserve(srcNa.rotationKeyframes.size());
        for (const auto& kf : srcNa.rotationKeyframes) {
            const XMVECTOR srcAnimLocal = LoadQuat(kf.value);
            const XMVECTOR delta = XMQuaternionMultiply(srcBindInv, srcAnimLocal);
            const XMVECTOR tgtAnimLocal = XMQuaternionNormalize(XMQuaternionMultiply(tgtBindAligned, delta));
            Model::QuaternionKeyframe outKf{};
            outKf.seconds = kf.seconds;
            XMStoreFloat4(&outKf.value, tgtAnimLocal);
            outNa.rotationKeyframes.push_back(outKf);
        }
        if (outNa.rotationKeyframes.size() == 1) {
            outNa.rotationKeyframes.push_back(Model::QuaternionKeyframe{ endTime, outNa.rotationKeyframes.front().value });
        }
        ++matched;
    }

    // Hips position transfer (root motion) scaled by height ratio.
    if (options.transferRootMotion) {
        const int srcHips = result.sourceProfile.Get(BoneSlot::Hips);
        const int tgtHips = result.targetProfile.Get(BoneSlot::Hips);
        if (srcHips >= 0 && tgtHips >= 0 &&
            srcHips < static_cast<int>(srcAnim.nodeAnims.size())) {
            const Model::NodeAnim& srcHipsAnim = srcAnim.nodeAnims[static_cast<size_t>(srcHips)];
            const XMFLOAT3 srcBindPos = sourceNodes[static_cast<size_t>(srcHips)].position;
            const XMFLOAT3 tgtBindPos = targetNodes[static_cast<size_t>(tgtHips)].position;
            const float heightScale = result.targetProfile.height /
                (std::max)(result.sourceProfile.height, 0.001f);

            Model::NodeAnim& outHipsAnim = result.animation.nodeAnims[static_cast<size_t>(tgtHips)];
            if (!srcHipsAnim.positionKeyframes.empty()) {
                outHipsAnim.positionKeyframes.clear();
                outHipsAnim.positionKeyframes.reserve(srcHipsAnim.positionKeyframes.size());
                for (const auto& kf : srcHipsAnim.positionKeyframes) {
                    Model::VectorKeyframe outKf{};
                    outKf.seconds = kf.seconds;
                    outKf.value.x = tgtBindPos.x + (kf.value.x - srcBindPos.x) * heightScale;
                    outKf.value.y = tgtBindPos.y + (kf.value.y - srcBindPos.y) * heightScale;
                    outKf.value.z = tgtBindPos.z + (kf.value.z - srcBindPos.z) * heightScale;
                    outHipsAnim.positionKeyframes.push_back(outKf);
                }
                if (outHipsAnim.positionKeyframes.size() == 1) {
                    outHipsAnim.positionKeyframes.push_back(
                        Model::VectorKeyframe{ endTime, outHipsAnim.positionKeyframes.front().value });
                }
            }
        }
    }

    if (matched <= 0) {
        result.warnings.push_back("No humanoid bones were mapped between source and target.");
        return result;
    }

    result.success = true;
    return result;
}
}

RetargetResult BakeAnimationLocalBindRelative(
    const Model& source,
    int sourceAnimationIndex,
    const Model& target,
    const RetargetOptions& options)
{
    return BakeAnimationLocalBindRelativeCore(source, sourceAnimationIndex, target, nullptr, options);
}

RetargetResult BakeAnimationWithReferenceTPose(
    const Model& source,
    int sourceAnimationIndex,
    const Model& target,
    const Model& referenceTPose,
    const RetargetOptions& options)
{
    return BakeAnimationLocalBindRelativeCore(source, sourceAnimationIndex, target, &referenceTPose, options);
}
}
