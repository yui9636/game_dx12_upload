#include "AnimatorRuntime.h"
#include <stack>

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

    static void BuildBoneMask(AnimatorRuntimeEntry& entry)
    {
        entry.isUpperBody.clear();
        if (!entry.modelRef) return;

        const auto& nodes = entry.modelRef->GetNodes();
        entry.isUpperBody.assign(nodes.size(), false);
        if (entry.spineNodeIndex < 0 || entry.spineNodeIndex >= static_cast<int>(nodes.size())) {
            return;
        }

        std::stack<int> pending;
        pending.push(entry.spineNodeIndex);
        while (!pending.empty()) {
            const int idx = pending.top();
            pending.pop();
            if (idx < 0 || idx >= static_cast<int>(nodes.size())) {
                continue;
            }
            entry.isUpperBody[idx] = true;
            for (auto* child : nodes[idx].children) {
                const int childIdx = static_cast<int>(child - &nodes[0]);
                if (childIdx >= 0 && childIdx < static_cast<int>(nodes.size())) {
                    pending.push(childIdx);
                }
            }
        }
    }
}

AnimatorRuntimeEntry* AnimatorRuntimeRegistry::Find(EntityID entity)
{
    const auto it = m_entries.find(entity);
    return (it != m_entries.end()) ? &it->second : nullptr;
}

AnimatorRuntimeEntry& AnimatorRuntimeRegistry::Ensure(EntityID entity, Model* model)
{
    AnimatorRuntimeEntry& entry = m_entries[entity];
    if (entry.modelRef != model) {
        Rebind(entry, model);
    }
    return entry;
}

void AnimatorRuntimeRegistry::Remove(EntityID entity)
{
    m_entries.erase(entity);
}

void AnimatorRuntimeRegistry::Clear()
{
    m_entries.clear();
}

void AnimatorRuntimeRegistry::Rebind(AnimatorRuntimeEntry& entry, Model* model)
{
    entry = {};
    entry.modelRef = model;
    if (!model) {
        return;
    }

    entry.rootNodeIndex = 1;
    entry.pelvisNodeIndex = model->GetNodeIndex("pelvis");
    entry.spineNodeIndex = model->GetNodeIndex("spine_01");

    const size_t poseCount = model->GetNodes().size();
    entry.basePoses.resize(poseCount);
    entry.actionPoses.resize(poseCount);
    entry.tempPoses.resize(poseCount);
    entry.finalPoses.resize(poseCount);
    entry.blendOffsets.resize(poseCount);
    FillBindPose(entry.finalPoses, model);
    BuildBoneMask(entry);
}
