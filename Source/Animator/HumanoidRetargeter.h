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
        // Optional CCD-style hand IK pass. Default off because the no-elbow-constraint
        // shortest-rotation step often introduces bone-axis twist when the source and
        // target are already close in proportion (typical same-rig retarget case).
        bool enableArmIk = false;
        // Mixamo and similar hubs often bake hips rotation into the root animation. The
        // earlier implementation hard-reset hips rotation to bind before computing source
        // global poses, which silently dropped that motion for every descendant bone.
        // Keep that off-by-default behavior.
        bool zeroHipsRotation = false;
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

    // Slot-based bind-relative LOCAL rotation transfer. Pairs source/target bones via
    // humanoid slots (not by name) and copies source's parent-frame rotation delta from
    // bind, applied to target's bind. This handles two important real-world cases the
    // world-delta variant doesn't:
    //   - Different naming conventions (e.g. Mixamo vs UE5 Mannequin)
    //   - Different rest poses (T-pose vs A-pose): target keeps its own rest while
    //     receiving the source's bind-relative motion
    RetargetResult BakeAnimationLocalBindRelative(
        const Model& source,
        int sourceAnimationIndex,
        const Model& target,
        const RetargetOptions& options = {});

    // Reference T-pose retargeting. Uses a neutral humanoid model only to calibrate
    // arm rest-pose direction, while source animation deltas stay in the source FBX's
    // own local axes. Body, hips, and legs keep the target bind posture to avoid
    // parent-chain rotation compounding.
    RetargetResult BakeAnimationWithReferenceTPose(
        const Model& source,
        int sourceAnimationIndex,
        const Model& target,
        const Model& referenceTPose,
        const RetargetOptions& options = {});
}
