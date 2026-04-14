#pragma once

#include "CinematicSequenceAsset.h"
#include "EffectRuntime/EffectService.h"
#include "Sequencer/CinematicService.h"

#include <string>
#include <unordered_map>

struct EngineTime;
class Registry;

class SequencerPanel
{
public:
    void Update(const EngineTime& time, Registry* registry);
    void Draw(Registry* registry, bool* p_open = nullptr, bool* outFocused = nullptr);
    void Suspend(Registry* registry);
    const CinematicSequenceAsset& GetSequence() const { return m_sequence; }
    uint64_t GetSelectedSectionId() const { return m_selectedSectionId; }
    bool ShouldDrawCameraPaths() const { return m_sequence.viewSettings.showCameraPaths; }

private:
    struct DisplayRow
    {
        enum class Kind
        {
            FolderHeader,
            MasterTrack,
            BindingHeader,
            BindingTrack
        };

        Kind kind = Kind::BindingTrack;
        int folderIndex = -1;
        int bindingIndex = -1;
        int trackIndex = -1;
        int indent = 0;
    };

    void EnsureDefaults();
    void StepPlayback(const EngineTime& time);
    void SyncPreviewCameraBindings(Registry* registry);
    void SyncPreviewRuntime(Registry* registry);
    void DestroyPreviewCameraBindings(Registry* registry);
    void StopPreviewRuntime();
    void SyncSelectionFromEditor(Registry* registry);
    EntityID ResolveBindingEntity(const CinematicBinding& binding, Registry* registry) const;
    EntityID EnsurePreviewCameraEntity(CinematicBinding& binding, Registry* registry);
    void CreatePreviewCameraBinding(Registry* registry);

    void DrawToolbar(Registry* registry);
    void DrawBody(Registry* registry);
    void DrawTrackTimeline(Registry* registry, float detailsWidth);
    void DrawDetails(Registry* registry);
    bool SaveSequence(bool saveAs = false);
    bool LoadSequence(const std::string& path);
    void NewSequence();
    void RebuildIdCountersFromAsset();

    void BuildDisplayRows();
    void DrawTimelineHeader(float timelineWidth);
    void DrawRowOutliner(Registry* registry, const DisplayRow& row);
    void DrawRowTimeline(Registry* registry, const DisplayRow& row, float width, float rowHeight);

    void AddBindingFromSelectedEntity(Registry* registry);
    void AddTrackToBinding(int bindingIndex, CinematicTrackType type);
    void AddMasterTrack(CinematicTrackType type);
    void AddSectionToTrack(CinematicTrack& track);

    CinematicBinding* FindSelectedBinding();
    CinematicTrack* FindSelectedTrack();
    CinematicSection* FindSelectedSection();

    const CinematicBinding* FindSelectedBinding() const;
    const CinematicTrack* FindSelectedTrack() const;
    const CinematicSection* FindSelectedSection() const;

    uint64_t NextBindingId() { return m_nextBindingId++; }
    uint64_t NextTrackId() { return m_nextTrackId++; }
    uint64_t NextSectionId() { return m_nextSectionId++; }

private:
    CinematicSequenceAsset m_sequence;
    std::vector<DisplayRow> m_displayRows;

    uint64_t m_nextBindingId = 1;
    uint64_t m_nextTrackId = 1;
    uint64_t m_nextSectionId = 1;

    uint64_t m_selectedBindingId = 0;
    uint64_t m_selectedTrackId = 0;
    uint64_t m_selectedSectionId = 0;
    uint64_t m_selectedFolderId = 0;

    bool m_initialized = false;
    bool m_playing = false;
    bool m_loop = true;
    float m_zoom = 1.0f;
    float m_currentFrame = 0.0f;
    float m_lastAppliedFrame = -1.0f;
    int m_playbackSpan = 0;
    bool m_dirty = true;
    bool m_selectionSyncEnabled = true;
    int m_addTrackBindingIndex = -2;
    bool m_showSeconds = false;
    std::string m_documentPath = "Data/Cinematic/Untitled.sequence.json";
    CinematicSequenceHandle m_previewHandle;
    std::unordered_map<uint64_t, EntityID> m_previewBindingEntities;
};
