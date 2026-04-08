#pragma once

#include "Entity/Entity.h"
#include "Model/Model.h"
#include <DirectXMath.h>
#include <unordered_map>
#include <vector>
#include <string>

struct AnimatorRuntimeEntry
{
    Model* modelRef = nullptr;
    int rootNodeIndex = 1;
    int pelvisNodeIndex = -1;
    int spineNodeIndex = -1;

    std::vector<bool> isUpperBody;
    std::vector<Model::NodePose> basePoses;
    std::vector<Model::NodePose> actionPoses;
    std::vector<Model::NodePose> tempPoses;
    std::vector<Model::NodePose> finalPoses;
    std::vector<Model::NodePose> blendOffsets;

    float offsetBlendDuration = 0.0f;
    float offsetBlendTimer = 0.0f;
    bool useOffsetBlending = false;
    float prevActionTime = 0.0f;

    std::unordered_map<std::string, int> animNameCache;
};

class AnimatorRuntimeRegistry
{
public:
    AnimatorRuntimeEntry* Find(EntityID entity);
    AnimatorRuntimeEntry& Ensure(EntityID entity, Model* model);
    void Remove(EntityID entity);
    void Clear();

private:
    void Rebind(AnimatorRuntimeEntry& entry, Model* model);

private:
    std::unordered_map<EntityID, AnimatorRuntimeEntry> m_entries;
};
