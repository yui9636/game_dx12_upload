#pragma once

#include "Model/Model.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace HumanoidRetarget
{
    enum class BoneSlot : uint8_t
    {
        Root,
        Hips,
        Spine,
        Chest,
        Neck,
        Head,
        LeftShoulder,
        LeftUpperArm,
        LeftLowerArm,
        LeftHand,
        RightShoulder,
        RightUpperArm,
        RightLowerArm,
        RightHand,
        LeftUpperLeg,
        LeftLowerLeg,
        LeftFoot,
        LeftToes,
        RightUpperLeg,
        RightLowerLeg,
        RightFoot,
        RightToes,
        Count
    };

    constexpr size_t kBoneSlotCount = static_cast<size_t>(BoneSlot::Count);

    const char* ToString(BoneSlot slot);

    struct SlotMatch
    {
        int nodeIndex = -1;
        float confidence = 0.0f;
    };

    struct HumanoidProfile
    {
        std::array<SlotMatch, kBoneSlotCount> slots{};
        float height = 1.0f;
        bool valid = false;
        std::vector<std::string> warnings;

        int Get(BoneSlot slot) const
        {
            return slots[static_cast<size_t>(slot)].nodeIndex;
        }
    };

    struct RetargetOptions
    {
        float sampleRate = 30.0f;
        bool transferRootMotion = true;
        bool keepTargetScale = true;
        std::string outputNameSuffix = "_Retargeted";
    };

    struct RetargetResult
    {
        bool success = false;
        Model::Animation animation;
        HumanoidProfile sourceProfile;
        HumanoidProfile targetProfile;
        std::vector<std::string> warnings;
    };

    HumanoidProfile AnalyzeSkeleton(const Model& model);

    RetargetResult BakeAnimation(
        const Model& source,
        int sourceAnimationIndex,
        const Model& target,
        const RetargetOptions& options = {});
}
