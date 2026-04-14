#pragma once

#include "CinematicSequenceAsset.h"
#include "EffectRuntime/EffectService.h"

#include "Entity/Entity.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Registry;
struct EngineTime;

struct CinematicSequenceHandle
{
    uint64_t id = 0;

    bool IsValid() const { return id != 0; }
    void Reset() { id = 0; }
};

struct CinematicBindingContext
{
    std::unordered_map<uint64_t, EntityID> bindingOverrides;
};

class CinematicService
{
public:
    static CinematicService& Instance();

    void SetRegistry(Registry* registry) { m_registry = registry; }
    Registry* GetRegistry() const { return m_registry; }

    void Update(const EngineTime& time);

    CinematicSequenceHandle PlaySequence(const std::string& assetPath, const CinematicBindingContext& bindingContext = {});
    CinematicSequenceHandle PlaySequenceAsset(const CinematicSequenceAsset& asset, const CinematicBindingContext& bindingContext = {});
    void StopSequence(const CinematicSequenceHandle& handle);
    void PauseSequence(const CinematicSequenceHandle& handle, bool paused);
    void SeekSequence(const CinematicSequenceHandle& handle, float frame);
    void SetPlaybackRate(const CinematicSequenceHandle& handle, float rate);
    void BindEntity(const CinematicSequenceHandle& handle, uint64_t bindingId, EntityID entity);
    void UpdateSequenceAsset(const CinematicSequenceHandle& handle, const CinematicSequenceAsset& asset);

    bool IsAlive(const CinematicSequenceHandle& handle) const;
    const CinematicSequenceAsset* GetAsset(const CinematicSequenceHandle& handle) const;

private:
    struct RuntimeEntry
    {
        CinematicSequenceHandle handle;
        std::string assetPath;
        CinematicSequenceAsset asset;
        CinematicBindingContext bindingContext;
        float currentFrame = 0.0f;
        float playbackRate = 1.0f;
        bool playing = true;
        bool paused = false;
        float lastAppliedFrame = -1.0f;
        int playbackSpan = 0;
        bool dirty = true;

        std::unordered_set<EntityID> animationDrivenEntities;
        std::unordered_set<uint64_t> activeSections;
        std::unordered_set<uint64_t> enteredSectionsThisFrame;
        std::unordered_set<uint64_t> exitedSectionsThisFrame;
        std::unordered_map<uint64_t, int> triggeredSectionSpan;
        std::unordered_map<uint64_t, std::vector<EffectHandle>> effectSectionHandles;
        std::unordered_map<uint64_t, std::vector<uint64_t>> audioSectionHandles;
        std::unordered_set<EntityID> originalMainTaggedEntities;
        bool capturedOriginalMainTaggedEntities = false;
    };

    void ApplyEntry(RuntimeEntry& entry, float previousFrame, int previousSpan);
    void ClearEntry(RuntimeEntry& entry);
    EntityID ResolveBindingEntity(const RuntimeEntry& entry, const CinematicBinding& binding) const;
    void RestoreMainCameraTags(RuntimeEntry& entry);
    void DispatchEvent(const CinematicSection& section) const;

    RuntimeEntry* Find(const CinematicSequenceHandle& handle);
    const RuntimeEntry* Find(const CinematicSequenceHandle& handle) const;

private:
    Registry* m_registry = nullptr;
    uint64_t m_nextHandleId = 1;
    std::unordered_map<uint64_t, RuntimeEntry> m_entries;
};
