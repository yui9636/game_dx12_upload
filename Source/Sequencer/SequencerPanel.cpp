#include "SequencerPanel.h"

#include "CinematicSequenceSerializer.h"
#include "CinematicService.h"
#include "Animator/AnimatorService.h"
#include "Archetype/Archetype.h"
#include "Component/CameraComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NameComponent.h"
#include "Component/SequencerPreviewCameraComponent.h"
#include "Component/TransformComponent.h"
#include "Console/Logger.h"
#include "Engine/EditorSelection.h"
#include "Engine/EngineKernel.h"
#include "Engine/EngineTime.h"
#include "Icon/IconFontManager.h"
#include "Message/MessageData.h"
#include "Message/Messenger.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "System/ResourceManager.h"
#include "Type/TypeInfo.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <unordered_set>
#include <utility>

using namespace DirectX;

namespace
{
    constexpr const char* kSequencerTitle = ICON_FA_FILM " Sequencer";
    constexpr float kOutlinerWidth = 280.0f;
    constexpr float kRowHeight = 26.0f;
    constexpr float kHeaderHeight = 28.0f;
    constexpr float kTimelineTopPadding = 4.0f;
    constexpr const char* kSequencerCameraModelPath = "Data/Model/Camera/Camera.cereal";

    static ImU32 ABGRToImU32(uint32_t rgba)
    {
        const uint8_t a = static_cast<uint8_t>((rgba >> 24) & 0xFF);
        const uint8_t r = static_cast<uint8_t>((rgba >> 16) & 0xFF);
        const uint8_t g = static_cast<uint8_t>((rgba >> 8) & 0xFF);
        const uint8_t b = static_cast<uint8_t>(rgba & 0xFF);
        return IM_COL32(r, g, b, a);
    }

    static XMFLOAT3 Lerp(const XMFLOAT3& a, const XMFLOAT3& b, float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    static float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    static XMFLOAT4 QuaternionFromEulerDegrees(const XMFLOAT3& eulerDeg)
    {
        XMVECTOR q = XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(eulerDeg.x),
            XMConvertToRadians(eulerDeg.y),
            XMConvertToRadians(eulerDeg.z));
        XMFLOAT4 out;
        XMStoreFloat4(&out, XMQuaternionNormalize(q));
        return out;
    }

    static XMFLOAT4 LookRotation(const XMFLOAT3& eye, const XMFLOAT3& target)
    {
        XMVECTOR eyeV = XMLoadFloat3(&eye);
        XMVECTOR targetV = XMLoadFloat3(&target);
        XMVECTOR forward = XMVector3Normalize(targetV - eyeV);
        const XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        XMMATRIX view = XMMatrixLookToLH(eyeV, forward, up);
        XMMATRIX invView = XMMatrixInverse(nullptr, view);
        XMVECTOR rot = XMQuaternionRotationMatrix(invView);
        XMFLOAT4 out;
        XMStoreFloat4(&out, XMQuaternionNormalize(rot));
        return out;
    }

    static float FrameToNormalized(int frame, int start, int end)
    {
        if (end <= start) {
            return 0.0f;
        }
        return (std::clamp)(static_cast<float>(frame - start) / static_cast<float>(end - start), 0.0f, 1.0f);
    }

    static bool IsInsideFrame(const CinematicSection& section, int frame)
    {
        return frame >= section.startFrame && frame <= section.endFrame;
    }

    static bool RangeTouchesFrame(int a, int b, int frame)
    {
        const int minFrame = (std::min)(a, b);
        const int maxFrame = (std::max)(a, b);
        return frame >= minFrame && frame <= maxFrame;
    }

    static bool RangeIntersectsSection(int a, int b, const CinematicSection& section)
    {
        const int minFrame = (std::min)(a, b);
        const int maxFrame = (std::max)(a, b);
        return maxFrame >= section.startFrame && minFrame <= section.endFrame;
    }

    static const char* GetBindingKindLabel(CinematicBindingKind kind)
    {
        switch (kind) {
        case CinematicBindingKind::Entity: return "Entity";
        case CinematicBindingKind::Spawnable: return "Spawnable";
        case CinematicBindingKind::PreviewOnly: return "PreviewOnly";
        default: return "Unknown";
        }
    }
}

void SequencerPanel::EnsureDefaults()
{
    if (m_initialized) {
        return;
    }

    m_sequence.name = "New Sequence";
    m_sequence.frameRate = 60.0f;
    m_sequence.durationFrames = 600;
    m_sequence.playRangeStart = 0;
    m_sequence.playRangeEnd = 600;
    m_sequence.workRangeStart = 0;
    m_sequence.workRangeEnd = 600;
    m_sequence.viewSettings.timelineZoom = m_zoom;
    m_sequence.viewSettings.showSeconds = m_showSeconds;
    m_sequence.viewSettings.showCameraPaths = true;

    AddMasterTrack(CinematicTrackType::Camera);
    m_initialized = true;
}

void SequencerPanel::NewSequence()
{
    StopPreviewRuntime();
    m_sequence = {};
    m_displayRows.clear();
    m_sequence.name = "New Sequence";
    m_sequence.frameRate = 60.0f;
    m_sequence.durationFrames = 600;
    m_sequence.playRangeStart = 0;
    m_sequence.playRangeEnd = 600;
    m_sequence.workRangeStart = 0;
    m_sequence.workRangeEnd = 600;
    m_sequence.viewSettings.timelineZoom = 1.0f;
    m_sequence.viewSettings.showSeconds = false;
    m_sequence.viewSettings.showCameraPaths = true;
    m_documentPath = "Data/Cinematic/Untitled.sequence.json";
    m_currentFrame = 0.0f;
    m_lastAppliedFrame = -1.0f;
    m_playbackSpan = 0;
    m_playing = false;
    m_selectedFolderId = 0;
    m_selectedBindingId = 0;
    m_selectedTrackId = 0;
    m_selectedSectionId = 0;
    RebuildIdCountersFromAsset();
    AddMasterTrack(CinematicTrackType::Camera);
    m_dirty = true;
    m_initialized = true;
}

void SequencerPanel::RebuildIdCountersFromAsset()
{
    uint64_t maxBindingId = 0;
    uint64_t maxTrackId = 0;
    uint64_t maxSectionId = 0;

    for (const auto& track : m_sequence.masterTracks) {
        maxTrackId = (std::max)(maxTrackId, track.trackId);
        for (const auto& section : track.sections) {
            maxSectionId = (std::max)(maxSectionId, section.sectionId);
        }
    }

    for (const auto& binding : m_sequence.bindings) {
        maxBindingId = (std::max)(maxBindingId, binding.bindingId);
        for (const auto& track : binding.tracks) {
            maxTrackId = (std::max)(maxTrackId, track.trackId);
            for (const auto& section : track.sections) {
                maxSectionId = (std::max)(maxSectionId, section.sectionId);
            }
        }
    }

    m_nextBindingId = maxBindingId + 1;
    m_nextTrackId = maxTrackId + 1;
    m_nextSectionId = maxSectionId + 1;
}

bool SequencerPanel::SaveSequence(bool saveAs)
{
    char path[MAX_PATH] = {};
    strncpy_s(path, m_documentPath.c_str(), _TRUNCATE);

    if (saveAs || m_documentPath.empty()) {
        if (Dialog::SaveFileName(path, MAX_PATH, "Sequencer JSON\0*.sequence.json\0JSON\0*.json\0", "Save Sequence", "json") != DialogResult::OK) {
            return false;
        }
        m_documentPath = path;
    }

    if (m_documentPath.find("Data/") != 0 && m_documentPath.find("Data\\") != 0) {
        m_documentPath = "Data/Cinematic/" + std::filesystem::path(m_documentPath).filename().string();
    }

    m_sequence.viewSettings.timelineZoom = m_zoom;
    m_sequence.viewSettings.showSeconds = m_showSeconds;

    return CinematicSequenceSerializer::Save(m_documentPath, m_sequence);
}

bool SequencerPanel::LoadSequence(const std::string& path)
{
    StopPreviewRuntime();
    CinematicSequenceAsset loaded;
    if (!CinematicSequenceSerializer::Load(path, loaded)) {
        return false;
    }

    m_sequence = std::move(loaded);
    m_documentPath = path;
    m_currentFrame = static_cast<float>(m_sequence.playRangeStart);
    m_lastAppliedFrame = -1.0f;
    m_playbackSpan = 0;
    m_playing = false;
    m_selectedBindingId = 0;
    m_selectedTrackId = 0;
    m_selectedSectionId = 0;
    m_selectedFolderId = 0;
    RebuildIdCountersFromAsset();
    m_zoom = (std::max)(0.25f, m_sequence.viewSettings.timelineZoom);
    m_showSeconds = m_sequence.viewSettings.showSeconds;
    m_initialized = true;
    m_dirty = true;
    return true;
}

void SequencerPanel::Update(const EngineTime& time, Registry* registry)
{
    EnsureDefaults();
    CinematicService::Instance().SetRegistry(registry);
    SyncPreviewCameraBindings(registry);
    SyncSelectionFromEditor(registry);
    StepPlayback(time);
    if (m_dirty || std::fabs(m_lastAppliedFrame - m_currentFrame) > 0.001f) {
        SyncPreviewRuntime(registry);
        m_lastAppliedFrame = m_currentFrame;
        m_dirty = false;
    }
}

void SequencerPanel::Suspend(Registry* registry)
{
    StopPreviewRuntime();
    DestroyPreviewCameraBindings(registry);
}

void SequencerPanel::StepPlayback(const EngineTime& time)
{
    if (!m_playing) {
        return;
    }

    const float previousFrame = m_currentFrame;
    m_currentFrame += time.unscaledDt * m_sequence.frameRate;

    if (m_loop) {
        const float rangeStart = static_cast<float>(m_sequence.playRangeStart);
        const float rangeEnd = static_cast<float>(m_sequence.playRangeEnd);
        const float rangeLength = (std::max)(1.0f, rangeEnd - rangeStart);
        while (m_currentFrame > rangeEnd) {
            m_currentFrame = rangeStart + std::fmod(m_currentFrame - rangeStart, rangeLength);
            ++m_playbackSpan;
        }
    } else if (m_currentFrame > static_cast<float>(m_sequence.playRangeEnd)) {
        m_currentFrame = static_cast<float>(m_sequence.playRangeEnd);
        m_playing = false;
    }

    if (std::fabs(previousFrame - m_currentFrame) > 0.001f) {
        m_dirty = true;
    }
}

void SequencerPanel::Draw(Registry* registry, bool* p_open, bool* outFocused)
{
    EnsureDefaults();

    if (!ImGui::Begin(kSequencerTitle, p_open)) {
        if (outFocused) {
            *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        }
        ImGui::End();
        return;
    }

    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    DrawToolbar(registry);
    ImGui::Separator();
    DrawBody(registry);

    ImGui::End();
}

void SequencerPanel::DrawToolbar(Registry* registry)
{
    if (ImGui::Button("New")) {
        NewSequence();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        char path[MAX_PATH] = {};
        strncpy_s(path, m_documentPath.c_str(), _TRUNCATE);
        if (Dialog::OpenFileName(path, MAX_PATH, "Sequencer JSON\0*.sequence.json;*.json\0JSON\0*.json\0", "Open Sequence") == DialogResult::OK) {
            LoadSequence(path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SaveSequence(false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        SaveSequence(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Selected")) {
        AddBindingFromSelectedEntity(registry);
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Camera")) {
        CreatePreviewCameraBinding(registry);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Folder")) {
        CinematicFolder folder;
        folder.folderId = NextBindingId();
        folder.displayName = "New Folder";
        m_sequence.folders.push_back(folder);
        m_selectedFolderId = folder.folderId;
        m_selectedBindingId = 0;
        m_selectedTrackId = 0;
        m_selectedSectionId = 0;
        m_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(m_playing ? "Pause" : "Play")) {
        m_playing = !m_playing;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        m_playing = false;
        m_currentFrame = static_cast<float>(m_sequence.playRangeStart);
        ++m_playbackSpan;
        StopPreviewRuntime();
        m_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Loop", &m_loop)) {
        m_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Seconds", &m_showSeconds)) {
        m_sequence.viewSettings.showSeconds = m_showSeconds;
        m_dirty = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("##Zoom", &m_zoom, 0.25f, 4.0f, "Zoom %.2fx")) {
        m_sequence.viewSettings.timelineZoom = m_zoom;
        m_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Camera Paths", &m_sequence.viewSettings.showCameraPaths)) {
        m_dirty = true;
    }
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::SliderFloat("##Playhead", &m_currentFrame,
        static_cast<float>(m_sequence.playRangeStart),
        static_cast<float>(m_sequence.playRangeEnd), "Frame %.0f")) {
        m_playing = false;
        m_dirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s | %d frames | %.1f fps",
        m_sequence.name.c_str(),
        m_sequence.durationFrames,
        m_sequence.frameRate);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::DragInt("Play Start", &m_sequence.playRangeStart, 1.0f, 0, m_sequence.durationFrames)) {
        m_sequence.playRangeStart = (std::clamp)(m_sequence.playRangeStart, 0, m_sequence.durationFrames);
        m_sequence.playRangeEnd = (std::max)(m_sequence.playRangeEnd, m_sequence.playRangeStart);
        m_currentFrame = (std::clamp)(m_currentFrame, static_cast<float>(m_sequence.playRangeStart), static_cast<float>(m_sequence.playRangeEnd));
        m_dirty = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::DragInt("Play End", &m_sequence.playRangeEnd, 1.0f, 0, m_sequence.durationFrames)) {
        m_sequence.playRangeEnd = (std::clamp)(m_sequence.playRangeEnd, m_sequence.playRangeStart, m_sequence.durationFrames);
        m_currentFrame = (std::clamp)(m_currentFrame, static_cast<float>(m_sequence.playRangeStart), static_cast<float>(m_sequence.playRangeEnd));
        m_dirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_documentPath.c_str());
}

void SequencerPanel::DrawBody(Registry* registry)
{
    constexpr float kDetailsWidth = 340.0f;
    DrawTrackTimeline(registry, kDetailsWidth);
}

void SequencerPanel::DrawTrackTimeline(Registry* registry, float detailsWidth)
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float leftWidth = (std::max)(200.0f, avail.x - detailsWidth - 8.0f);

    ImGui::BeginChild("##SequencerLeft", ImVec2(leftWidth, 0.0f), false);
    BuildDisplayRows();

    ImGui::Columns(2, "##SequencerColumns", false);
    ImGui::SetColumnWidth(0, kOutlinerWidth);

    ImGui::BeginChild("##OutlinerHeader", ImVec2(0.0f, kHeaderHeight), true);
    ImGui::TextUnformatted("Track Outliner");
    ImGui::EndChild();

    ImGui::NextColumn();
    DrawTimelineHeader(ImGui::GetContentRegionAvail().x);
    ImGui::Columns(1);

    ImGui::BeginChild("##SequencerRows", ImVec2(0.0f, 0.0f), true);
    if (ImGui::BeginTable("##SequencerRowsTable", 2,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Outliner", ImGuiTableColumnFlags_WidthFixed, kOutlinerWidth);
        ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_displayRows.size()), kRowHeight);
        while (clipper.Step()) {
            for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                const DisplayRow& row = m_displayRows[rowIndex];
                ImGui::TableNextRow(ImGuiTableRowFlags_None, kRowHeight);
                ImGui::TableSetColumnIndex(0);
                DrawRowOutliner(registry, row);
                ImGui::TableSetColumnIndex(1);
                DrawRowTimeline(registry, row, ImGui::GetContentRegionAvail().x, kRowHeight);
            }
        }

        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##SequencerDetails", ImVec2(0.0f, 0.0f), true);
    DrawDetails(registry);
    ImGui::EndChild();
}

void SequencerPanel::BuildDisplayRows()
{
    m_displayRows.clear();
    std::unordered_set<uint64_t> childFolderIds;
    std::unordered_set<uint64_t> assignedBindingIds;
    std::unordered_set<uint64_t> assignedMasterTrackIds;
    for (const auto& folder : m_sequence.folders) {
        for (uint64_t childId : folder.childFolderIds) {
            childFolderIds.insert(childId);
        }
        for (uint64_t bindingId : folder.bindingIds) {
            assignedBindingIds.insert(bindingId);
        }
        for (uint64_t trackId : folder.masterTrackIds) {
            assignedMasterTrackIds.insert(trackId);
        }
    }

    auto findFolderIndex = [&](uint64_t folderId) -> int {
        for (size_t i = 0; i < m_sequence.folders.size(); ++i) {
            if (m_sequence.folders[i].folderId == folderId) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };
    auto findMasterTrackIndex = [&](uint64_t trackId) -> int {
        for (size_t i = 0; i < m_sequence.masterTracks.size(); ++i) {
            if (m_sequence.masterTracks[i].trackId == trackId) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };
    auto findBindingIndex = [&](uint64_t bindingId) -> int {
        for (size_t i = 0; i < m_sequence.bindings.size(); ++i) {
            if (m_sequence.bindings[i].bindingId == bindingId) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    std::function<void(uint64_t, int)> appendFolder = [&](uint64_t folderId, int indent) {
        const int folderIndex = findFolderIndex(folderId);
        if (folderIndex < 0) {
            return;
        }
        const auto& folder = m_sequence.folders[folderIndex];
        DisplayRow folderRow;
        folderRow.kind = DisplayRow::Kind::FolderHeader;
        folderRow.folderIndex = folderIndex;
        folderRow.indent = indent;
        m_displayRows.push_back(folderRow);
        if (!folder.expanded) {
            return;
        }
        for (uint64_t childId : folder.childFolderIds) {
            appendFolder(childId, indent + 1);
        }
        for (uint64_t trackId : folder.masterTrackIds) {
            const int trackIndex = findMasterTrackIndex(trackId);
            if (trackIndex >= 0) {
                DisplayRow row;
                row.kind = DisplayRow::Kind::MasterTrack;
                row.trackIndex = trackIndex;
                row.indent = indent + 1;
                m_displayRows.push_back(row);
            }
        }
        for (uint64_t bindingId : folder.bindingIds) {
            const int bindingIndex = findBindingIndex(bindingId);
            if (bindingIndex < 0) {
                continue;
            }
            DisplayRow header;
            header.kind = DisplayRow::Kind::BindingHeader;
            header.bindingIndex = bindingIndex;
            header.indent = indent + 1;
            m_displayRows.push_back(header);

            const auto& binding = m_sequence.bindings[bindingIndex];
            for (size_t trackIndex = 0; trackIndex < binding.tracks.size(); ++trackIndex) {
                DisplayRow row;
                row.kind = DisplayRow::Kind::BindingTrack;
                row.bindingIndex = bindingIndex;
                row.trackIndex = static_cast<int>(trackIndex);
                row.indent = indent + 2;
                m_displayRows.push_back(row);
            }
        }
    };

    for (const auto& folder : m_sequence.folders) {
        if (!childFolderIds.count(folder.folderId)) {
            appendFolder(folder.folderId, 0);
        }
    }

    for (size_t i = 0; i < m_sequence.masterTracks.size(); ++i) {
        if (assignedMasterTrackIds.count(m_sequence.masterTracks[i].trackId)) {
            continue;
        }
        DisplayRow row;
        row.kind = DisplayRow::Kind::MasterTrack;
        row.trackIndex = static_cast<int>(i);
        row.indent = 0;
        m_displayRows.push_back(row);
    }

    for (size_t bindingIndex = 0; bindingIndex < m_sequence.bindings.size(); ++bindingIndex) {
        if (assignedBindingIds.count(m_sequence.bindings[bindingIndex].bindingId)) {
            continue;
        }
        DisplayRow header;
        header.kind = DisplayRow::Kind::BindingHeader;
        header.bindingIndex = static_cast<int>(bindingIndex);
        header.indent = 0;
        m_displayRows.push_back(header);

        const auto& binding = m_sequence.bindings[bindingIndex];
        for (size_t trackIndex = 0; trackIndex < binding.tracks.size(); ++trackIndex) {
            DisplayRow row;
            row.kind = DisplayRow::Kind::BindingTrack;
            row.bindingIndex = static_cast<int>(bindingIndex);
            row.trackIndex = static_cast<int>(trackIndex);
            row.indent = 1;
            m_displayRows.push_back(row);
        }
    }
}

void SequencerPanel::DrawTimelineHeader(float timelineWidth)
{
    ImGui::BeginChild("##TimelineHeader", ImVec2(0.0f, kHeaderHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const float pixelsPerFrame = (timelineWidth / (std::max)(1, m_sequence.durationFrames)) * m_zoom;
    const int frameStep = (m_zoom < 0.6f) ? 60 : ((m_zoom < 1.5f) ? 30 : 10);

    const float workStartX = origin.x + (m_sequence.workRangeStart - m_sequence.playRangeStart) * pixelsPerFrame;
    const float workEndX = origin.x + (m_sequence.workRangeEnd - m_sequence.playRangeStart) * pixelsPerFrame;
    drawList->AddRectFilled(ImVec2(workStartX, origin.y), ImVec2(workEndX, origin.y + size.y), IM_COL32(48, 58, 72, 110));

    for (int frame = m_sequence.playRangeStart; frame <= m_sequence.playRangeEnd; frame += frameStep) {
        const float x = origin.x + (frame - m_sequence.playRangeStart) * pixelsPerFrame;
        drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + size.y), IM_COL32(70, 70, 76, 255));
        std::string frameText;
        if (m_showSeconds) {
            const float seconds = static_cast<float>(frame) / (std::max)(1.0f, m_sequence.frameRate);
            char secondsText[32] = {};
            sprintf_s(secondsText, "%.2fs", seconds);
            frameText = secondsText;
        } else {
            frameText = std::to_string(frame);
        }
        drawList->AddText(ImVec2(x + 4.0f, origin.y + 6.0f), IM_COL32(190, 190, 190, 255), frameText.c_str());
    }

    const float playheadX = origin.x + (m_currentFrame - static_cast<float>(m_sequence.playRangeStart)) * pixelsPerFrame;
    drawList->AddLine(ImVec2(playheadX, origin.y), ImVec2(playheadX, origin.y + size.y), IM_COL32(255, 120, 80, 255), 2.0f);
    ImGui::EndChild();
}

void SequencerPanel::DrawRowOutliner(Registry* registry, const DisplayRow& row)
{
    switch (row.kind) {
    case DisplayRow::Kind::FolderHeader:
    {
        CinematicFolder& folder = m_sequence.folders[row.folderIndex];
        ImGui::Indent(14.0f * static_cast<float>(row.indent));
        const char* arrow = folder.expanded ? ICON_FA_CARET_DOWN : ICON_FA_CARET_RIGHT;
        if (ImGui::SmallButton((std::string(arrow) + "##foldertoggle" + std::to_string(folder.folderId)).c_str())) {
            folder.expanded = !folder.expanded;
        }
        ImGui::SameLine();
        const bool selected = (m_selectedFolderId == folder.folderId);
        if (ImGui::Selectable((std::string(ICON_FA_FOLDER " ") + folder.displayName).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            m_selectedFolderId = folder.folderId;
            m_selectedBindingId = 0;
            m_selectedTrackId = 0;
            m_selectedSectionId = 0;
        }
        ImGui::Unindent(14.0f * static_cast<float>(row.indent));
        break;
    }
    case DisplayRow::Kind::MasterTrack:
    {
        CinematicTrack& track = m_sequence.masterTracks[row.trackIndex];
        const bool selected = (m_selectedTrackId == track.trackId);
        ImGui::Indent(14.0f * static_cast<float>(row.indent));
        if (ImGui::Selectable((std::string("[Master] ") + track.displayName).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            m_selectedFolderId = 0;
            m_selectedBindingId = 0;
            m_selectedTrackId = track.trackId;
            m_selectedSectionId = 0;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox((std::string("M##mastermute") + std::to_string(track.trackId)).c_str(), &track.muted)) {
            m_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox((std::string("L##masterlock") + std::to_string(track.trackId)).c_str(), &track.locked)) {
            m_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string("+##mastersec") + std::to_string(track.trackId)).c_str())) {
            AddSectionToTrack(track);
        }
        ImGui::Unindent(14.0f * static_cast<float>(row.indent));
        break;
    }
    case DisplayRow::Kind::BindingHeader:
    {
        CinematicBinding& binding = m_sequence.bindings[row.bindingIndex];
        const bool selected = (m_selectedBindingId == binding.bindingId && m_selectedTrackId == 0 && m_selectedSectionId == 0);
        ImGui::Indent(14.0f * static_cast<float>(row.indent));
        if (ImGui::Selectable(binding.displayName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            m_selectedFolderId = 0;
            m_selectedBindingId = binding.bindingId;
            m_selectedTrackId = 0;
            m_selectedSectionId = 0;
            const EntityID entity = ResolveBindingEntity(binding, registry);
            if (!Entity::IsNull(entity)) {
                EditorSelection::Instance().SelectEntity(entity);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", GetBindingKindLabel(binding.bindingKind));
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string("+ Track##") + std::to_string(binding.bindingId)).c_str())) {
            m_addTrackBindingIndex = row.bindingIndex;
            ImGui::OpenPopup("Add Track Popup");
        }
        ImGui::Unindent(14.0f * static_cast<float>(row.indent));
        break;
    }
    case DisplayRow::Kind::BindingTrack:
    {
        CinematicTrack& track = m_sequence.bindings[row.bindingIndex].tracks[row.trackIndex];
        const bool selected = (m_selectedTrackId == track.trackId && m_selectedSectionId == 0);
        ImGui::Indent(14.0f * static_cast<float>(row.indent));
        if (ImGui::Selectable(track.displayName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
            m_selectedFolderId = 0;
            m_selectedBindingId = m_sequence.bindings[row.bindingIndex].bindingId;
            m_selectedTrackId = track.trackId;
            m_selectedSectionId = 0;
            const EntityID entity = ResolveBindingEntity(m_sequence.bindings[row.bindingIndex], registry);
            if (!Entity::IsNull(entity)) {
                EditorSelection::Instance().SelectEntity(entity);
            }
        }
        ImGui::SameLine();
        if (ImGui::Checkbox((std::string("M##trackmute") + std::to_string(track.trackId)).c_str(), &track.muted)) {
            m_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Checkbox((std::string("L##tracklock") + std::to_string(track.trackId)).c_str(), &track.locked)) {
            m_dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton((std::string("+##sec") + std::to_string(track.trackId)).c_str())) {
            AddSectionToTrack(track);
        }
        ImGui::Unindent(14.0f * static_cast<float>(row.indent));
        break;
    }
    }

    if (ImGui::BeginPopup("Add Track Popup")) {
        const CinematicTrackType trackTypes[] = {
            CinematicTrackType::Transform,
            CinematicTrackType::Camera,
            CinematicTrackType::Animation,
            CinematicTrackType::Effect,
            CinematicTrackType::Audio,
            CinematicTrackType::Event,
            CinematicTrackType::CameraShake
        };
        for (CinematicTrackType type : trackTypes) {
            if (ImGui::Selectable(GetCinematicTrackTypeLabel(type))) {
                if (m_addTrackBindingIndex >= 0 && m_addTrackBindingIndex < static_cast<int>(m_sequence.bindings.size())) {
                    AddTrackToBinding(m_addTrackBindingIndex, type);
                } else {
                    AddMasterTrack(type);
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

void SequencerPanel::DrawRowTimeline(Registry* registry, const DisplayRow& row, float width, float rowHeight)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 rowSize(width, rowHeight);
    const float pixelsPerFrame = (width / (std::max)(1, m_sequence.durationFrames)) * m_zoom;

    drawList->AddRectFilled(origin, ImVec2(origin.x + rowSize.x, origin.y + rowSize.y), IM_COL32(33, 35, 41, 255));

    for (int frame = m_sequence.playRangeStart; frame <= m_sequence.playRangeEnd; frame += 30) {
        const float x = origin.x + (frame - m_sequence.playRangeStart) * pixelsPerFrame;
        drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + rowSize.y), IM_COL32(58, 60, 66, 255));
    }

    auto drawSections = [&](CinematicTrack& track, int bindingIndex) {
        for (CinematicSection& section : track.sections) {
            const float x0 = origin.x + (section.startFrame - m_sequence.playRangeStart) * pixelsPerFrame;
            const float x1 = origin.x + (section.endFrame - m_sequence.playRangeStart) * pixelsPerFrame;
            if (x1 < origin.x - 32.0f || x0 > origin.x + width + 32.0f) {
                continue;
            }
            const ImVec2 minPos(x0, origin.y + kTimelineTopPadding);
            const ImVec2 maxPos((std::max)(x0 + 24.0f, x1), origin.y + rowHeight - kTimelineTopPadding);
            const bool selected = (m_selectedSectionId == section.sectionId);
            const ImU32 fill = selected ? IM_COL32(255, 255, 255, 80) : ABGRToImU32(section.color);
            const float sectionWidth = maxPos.x - minPos.x;

            drawList->AddRectFilled(minPos, maxPos, fill, 4.0f);
            drawList->AddRect(minPos, maxPos, selected ? IM_COL32(255, 255, 255, 220) : IM_COL32(25, 25, 25, 180), 4.0f, 0, selected ? 2.0f : 1.0f);
            if (track.type == CinematicTrackType::Event || track.type == CinematicTrackType::CameraShake) {
                const ImVec2 center((minPos.x + maxPos.x) * 0.5f, (minPos.y + maxPos.y) * 0.5f);
                const float markerRadius = selected ? 8.0f : 6.0f;
                drawList->AddTriangleFilled(
                    ImVec2(center.x, center.y - markerRadius),
                    ImVec2(center.x + markerRadius, center.y),
                    ImVec2(center.x, center.y + markerRadius),
                    selected ? IM_COL32(255, 255, 255, 220) : IM_COL32(250, 240, 120, 230));
                drawList->AddTriangle(
                    ImVec2(center.x, center.y - markerRadius),
                    ImVec2(center.x + markerRadius, center.y),
                    ImVec2(center.x, center.y + markerRadius),
                    IM_COL32(30, 30, 32, 230),
                    1.0f);
            } else if (sectionWidth > 48.0f) {
                drawList->AddText(ImVec2(minPos.x + 6.0f, minPos.y + 4.0f), IM_COL32(240, 240, 240, 255), section.label.c_str());
            }

            ImGui::SetCursorScreenPos(minPos);
            if (ImGui::InvisibleButton((std::string("##Section") + std::to_string(section.sectionId)).c_str(), ImVec2(maxPos.x - minPos.x, maxPos.y - minPos.y))) {
                if (bindingIndex >= 0) {
                    m_selectedBindingId = m_sequence.bindings[bindingIndex].bindingId;
                    const EntityID entity = ResolveBindingEntity(m_sequence.bindings[bindingIndex], registry);
                    if (!Entity::IsNull(entity)) {
                        EditorSelection::Instance().SelectEntity(entity);
                    }
                }
                m_selectedTrackId = track.trackId;
                m_selectedSectionId = section.sectionId;
            }
        }
    };

    if (row.kind == DisplayRow::Kind::MasterTrack) {
        drawSections(m_sequence.masterTracks[row.trackIndex], -1);
    } else if (row.kind == DisplayRow::Kind::BindingTrack) {
        drawSections(m_sequence.bindings[row.bindingIndex].tracks[row.trackIndex], row.bindingIndex);
    }

    const float playheadX = origin.x + (m_currentFrame - static_cast<float>(m_sequence.playRangeStart)) * pixelsPerFrame;
    drawList->AddLine(ImVec2(playheadX, origin.y), ImVec2(playheadX, origin.y + rowSize.y), IM_COL32(255, 120, 80, 255), 2.0f);
    ImGui::Dummy(rowSize);
}

EntityID SequencerPanel::ResolveBindingEntity(const CinematicBinding& binding, Registry* registry) const
{
    if (!Entity::IsNull(binding.targetEntity)) {
        return binding.targetEntity;
    }

    const auto it = m_previewBindingEntities.find(binding.bindingId);
    if (it == m_previewBindingEntities.end()) {
        return Entity::NULL_ID;
    }
    if (!registry || registry->IsAlive(it->second)) {
        return it->second;
    }
    return Entity::NULL_ID;
}

EntityID SequencerPanel::EnsurePreviewCameraEntity(CinematicBinding& binding, Registry* registry)
{
    if (!registry || binding.bindingKind != CinematicBindingKind::PreviewOnly) {
        return Entity::NULL_ID;
    }

    auto isValidPreview = [&](EntityID entity) {
        if (Entity::IsNull(entity) || !registry->IsAlive(entity)) {
            return false;
        }
        const auto* preview = registry->GetComponent<SequencerPreviewCameraComponent>(entity);
        return preview && preview->bindingId == binding.bindingId;
    };

    auto existing = m_previewBindingEntities.find(binding.bindingId);
    if (existing != m_previewBindingEntities.end() && isValidPreview(existing->second)) {
        if (auto* name = registry->GetComponent<NameComponent>(existing->second)) {
            name->name = binding.displayName;
        }
        return existing->second;
    }

    for (Archetype* archetype : registry->GetAllArchetypes()) {
        if (!archetype->GetSignature().test(TypeManager::GetComponentTypeID<SequencerPreviewCameraComponent>())) {
            continue;
        }
        const auto& entities = archetype->GetEntities();
        for (EntityID entity : entities) {
            if (!isValidPreview(entity)) {
                continue;
            }
            m_previewBindingEntities[binding.bindingId] = entity;
            if (auto* name = registry->GetComponent<NameComponent>(entity)) {
                name->name = binding.displayName;
            }
            return entity;
        }
    }

    const EntityID entity = registry->CreateEntity();
    registry->AddComponent(entity, NameComponent{ binding.displayName });
    TransformComponent transform{};
    transform.localPosition = { 0.0f, 2.0f, -10.0f };
    transform.isDirty = true;
    registry->AddComponent(entity, transform);
    registry->AddComponent(entity, HierarchyComponent{});

    MeshComponent mesh{};
    mesh.modelFilePath = kSequencerCameraModelPath;
    mesh.model = ResourceManager::Instance().CreateModelInstance(mesh.modelFilePath);
    mesh.castShadow = false;
    registry->AddComponent(entity, mesh);

    registry->AddComponent(entity, CameraLensComponent{});
    registry->AddComponent(entity, CameraMatricesComponent{});
    registry->AddComponent(entity, SequencerPreviewCameraComponent{ binding.bindingId, true, true });

    m_previewBindingEntities[binding.bindingId] = entity;
    return entity;
}

void SequencerPanel::CreatePreviewCameraBinding(Registry* registry)
{
    if (!registry) {
        return;
    }

    int nextIndex = 1;
    std::string displayName;
    for (;;) {
        displayName = "Shot Camera " + std::to_string(nextIndex);
        const bool exists = std::any_of(m_sequence.bindings.begin(), m_sequence.bindings.end(),
            [&](const CinematicBinding& binding) {
                return binding.displayName == displayName;
            });
        if (!exists) {
            break;
        }
        ++nextIndex;
    }

    CinematicBinding binding;
    binding.bindingId = NextBindingId();
    binding.displayName = displayName;
    binding.bindingKind = CinematicBindingKind::PreviewOnly;
    binding.spawnPrefabPath = kSequencerCameraModelPath;
    binding.tracks.push_back(MakeDefaultCinematicTrack(CinematicTrackType::Camera, NextTrackId()));

    m_sequence.bindings.push_back(binding);
    CinematicBinding& newBinding = m_sequence.bindings.back();
    const EntityID entity = EnsurePreviewCameraEntity(newBinding, registry);

    if (m_selectedFolderId != 0) {
        for (auto& folder : m_sequence.folders) {
            if (folder.folderId == m_selectedFolderId) {
                folder.bindingIds.push_back(newBinding.bindingId);
                break;
            }
        }
    }

    m_selectedBindingId = newBinding.bindingId;
    m_selectedFolderId = 0;
    m_selectedTrackId = 0;
    m_selectedSectionId = 0;
    m_dirty = true;

    if (!Entity::IsNull(entity)) {
        EditorSelection::Instance().SelectEntity(entity);
    }
}

void SequencerPanel::SyncPreviewCameraBindings(Registry* registry)
{
    if (!registry) {
        return;
    }

    std::unordered_set<uint64_t> livePreviewBindings;
    for (auto& binding : m_sequence.bindings) {
        if (binding.bindingKind != CinematicBindingKind::PreviewOnly) {
            continue;
        }
        livePreviewBindings.insert(binding.bindingId);
        EnsurePreviewCameraEntity(binding, registry);
    }

    std::vector<uint64_t> staleBindingIds;
    for (const auto& [bindingId, entity] : m_previewBindingEntities) {
        if (livePreviewBindings.find(bindingId) == livePreviewBindings.end() ||
            Entity::IsNull(entity) ||
            !registry->IsAlive(entity)) {
            staleBindingIds.push_back(bindingId);
        }
    }

    std::vector<EntityID> strayEntities;
    for (Archetype* archetype : registry->GetAllArchetypes()) {
        if (!archetype->GetSignature().test(TypeManager::GetComponentTypeID<SequencerPreviewCameraComponent>())) {
            continue;
        }
        auto* previewColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<SequencerPreviewCameraComponent>());
        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            const EntityID entity = archetype->GetEntities()[i];
            const auto& preview = *static_cast<SequencerPreviewCameraComponent*>(previewColumn->Get(i));
            if (livePreviewBindings.find(preview.bindingId) == livePreviewBindings.end()) {
                strayEntities.push_back(entity);
            } else {
                m_previewBindingEntities[preview.bindingId] = entity;
            }
        }
    }

    auto& selection = EditorSelection::Instance();
    for (uint64_t bindingId : staleBindingIds) {
        const auto it = m_previewBindingEntities.find(bindingId);
        if (it == m_previewBindingEntities.end()) {
            continue;
        }
        if (selection.GetType() == SelectionType::Entity && selection.GetPrimaryEntity() == it->second) {
            selection.Clear();
        }
        if (!Entity::IsNull(it->second) && registry->IsAlive(it->second)) {
            registry->DestroyEntity(it->second);
        }
        m_previewBindingEntities.erase(it);
    }

    for (EntityID entity : strayEntities) {
        if (selection.GetType() == SelectionType::Entity && selection.GetPrimaryEntity() == entity) {
            selection.Clear();
        }
        if (registry->IsAlive(entity)) {
            registry->DestroyEntity(entity);
        }
    }
}

void SequencerPanel::DestroyPreviewCameraBindings(Registry* registry)
{
    if (!registry) {
        m_previewBindingEntities.clear();
        return;
    }

    auto& selection = EditorSelection::Instance();
    for (auto& [_, entity] : m_previewBindingEntities) {
        if (selection.GetType() == SelectionType::Entity && selection.GetPrimaryEntity() == entity) {
            selection.Clear();
        }
        if (!Entity::IsNull(entity) && registry->IsAlive(entity)) {
            registry->DestroyEntity(entity);
        }
    }
    m_previewBindingEntities.clear();
}

void SequencerPanel::AddBindingFromSelectedEntity(Registry* registry)
{
    if (!registry) {
        return;
    }

    const EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    if (Entity::IsNull(entity) || !registry->IsAlive(entity)) {
        return;
    }

    if (const auto* preview = registry->GetComponent<SequencerPreviewCameraComponent>(entity)) {
        for (const CinematicBinding& binding : m_sequence.bindings) {
            if (binding.bindingId == preview->bindingId) {
                m_selectedBindingId = binding.bindingId;
                m_selectedTrackId = 0;
                m_selectedSectionId = 0;
                return;
            }
        }
    }

    for (const CinematicBinding& binding : m_sequence.bindings) {
        if (ResolveBindingEntity(binding, registry) == entity) {
            m_selectedBindingId = binding.bindingId;
            m_selectedTrackId = 0;
            m_selectedSectionId = 0;
            return;
        }
    }

    CinematicBinding binding;
    binding.bindingId = NextBindingId();
    binding.targetEntity = entity;
    binding.bindingKind = CinematicBindingKind::Entity;
    if (const auto* name = registry->GetComponent<NameComponent>(entity)) {
        binding.displayName = name->name;
    } else {
        binding.displayName = "Entity " + std::to_string(static_cast<uint64_t>(entity));
    }
    const CinematicTrackType defaultTrackType = registry->GetComponent<CameraLensComponent>(entity)
        ? CinematicTrackType::Camera
        : CinematicTrackType::Transform;
    binding.tracks.push_back(MakeDefaultCinematicTrack(defaultTrackType, NextTrackId()));
    m_sequence.bindings.push_back(binding);
    if (m_selectedFolderId != 0) {
        for (auto& folder : m_sequence.folders) {
            if (folder.folderId == m_selectedFolderId) {
                folder.bindingIds.push_back(binding.bindingId);
                break;
            }
        }
    }
    m_selectedBindingId = binding.bindingId;
    m_selectedFolderId = 0;
    m_selectedTrackId = 0;
    m_selectedSectionId = 0;
    m_dirty = true;
}

void SequencerPanel::AddTrackToBinding(int bindingIndex, CinematicTrackType type)
{
    if (bindingIndex < 0 || bindingIndex >= static_cast<int>(m_sequence.bindings.size())) {
        return;
    }

    CinematicTrack track = MakeDefaultCinematicTrack(type, NextTrackId());
    m_selectedBindingId = m_sequence.bindings[bindingIndex].bindingId;
    m_selectedTrackId = track.trackId;
    m_selectedSectionId = 0;
    m_sequence.bindings[bindingIndex].tracks.push_back(track);
    m_dirty = true;
}

void SequencerPanel::AddMasterTrack(CinematicTrackType type)
{
    CinematicTrack track = MakeDefaultCinematicTrack(type, NextTrackId());
    if (type == CinematicTrackType::Camera) {
        track.displayName = "Camera Cuts";
    }
    if (m_selectedFolderId != 0) {
        for (auto& folder : m_sequence.folders) {
            if (folder.folderId == m_selectedFolderId) {
                folder.masterTrackIds.push_back(track.trackId);
                break;
            }
        }
    }
    m_selectedFolderId = 0;
    m_selectedBindingId = 0;
    m_selectedTrackId = track.trackId;
    m_selectedSectionId = 0;
    m_sequence.masterTracks.push_back(track);
    m_dirty = true;
}

void SequencerPanel::AddSectionToTrack(CinematicTrack& track)
{
    const int start = static_cast<int>(m_currentFrame);
    const int end = (std::min)(m_sequence.playRangeEnd, start + 120);
    CinematicSection section = MakeDefaultCinematicSection(track.type, NextSectionId(), start, end);
    const bool isMasterTrack = std::any_of(m_sequence.masterTracks.begin(), m_sequence.masterTracks.end(),
        [&](const CinematicTrack& masterTrack) {
            return masterTrack.trackId == track.trackId;
        });
    if (isMasterTrack && track.type == CinematicTrackType::Camera) {
        if (!m_sequence.bindings.empty()) {
            section.camera.cameraBindingId = m_sequence.bindings.front().bindingId;
            section.label = m_sequence.bindings.front().displayName;
        } else {
            section.label = "Camera Cut";
        }
    }
    track.sections.push_back(section);
    m_selectedTrackId = track.trackId;
    m_selectedSectionId = section.sectionId;
    m_dirty = true;
}

CinematicBinding* SequencerPanel::FindSelectedBinding()
{
    if (m_selectedBindingId == 0) {
        return nullptr;
    }
    for (auto& binding : m_sequence.bindings) {
        if (binding.bindingId == m_selectedBindingId) {
            return &binding;
        }
    }
    return nullptr;
}

const CinematicBinding* SequencerPanel::FindSelectedBinding() const
{
    if (m_selectedBindingId == 0) {
        return nullptr;
    }
    for (const auto& binding : m_sequence.bindings) {
        if (binding.bindingId == m_selectedBindingId) {
            return &binding;
        }
    }
    return nullptr;
}

CinematicTrack* SequencerPanel::FindSelectedTrack()
{
    if (m_selectedTrackId == 0) {
        return nullptr;
    }
    for (auto& track : m_sequence.masterTracks) {
        if (track.trackId == m_selectedTrackId) {
            return &track;
        }
    }
    for (auto& binding : m_sequence.bindings) {
        for (auto& track : binding.tracks) {
            if (track.trackId == m_selectedTrackId) {
                return &track;
            }
        }
    }
    return nullptr;
}

const CinematicTrack* SequencerPanel::FindSelectedTrack() const
{
    if (m_selectedTrackId == 0) {
        return nullptr;
    }
    for (const auto& track : m_sequence.masterTracks) {
        if (track.trackId == m_selectedTrackId) {
            return &track;
        }
    }
    for (const auto& binding : m_sequence.bindings) {
        for (const auto& track : binding.tracks) {
            if (track.trackId == m_selectedTrackId) {
                return &track;
            }
        }
    }
    return nullptr;
}

CinematicSection* SequencerPanel::FindSelectedSection()
{
    if (m_selectedSectionId == 0) {
        return nullptr;
    }
    for (auto& track : m_sequence.masterTracks) {
        for (auto& section : track.sections) {
            if (section.sectionId == m_selectedSectionId) {
                return &section;
            }
        }
    }
    for (auto& binding : m_sequence.bindings) {
        for (auto& track : binding.tracks) {
            for (auto& section : track.sections) {
                if (section.sectionId == m_selectedSectionId) {
                    return &section;
                }
            }
        }
    }
    return nullptr;
}

const CinematicSection* SequencerPanel::FindSelectedSection() const
{
    if (m_selectedSectionId == 0) {
        return nullptr;
    }
    for (const auto& track : m_sequence.masterTracks) {
        for (const auto& section : track.sections) {
            if (section.sectionId == m_selectedSectionId) {
                return &section;
            }
        }
    }
    for (const auto& binding : m_sequence.bindings) {
        for (const auto& track : binding.tracks) {
            for (const auto& section : track.sections) {
                if (section.sectionId == m_selectedSectionId) {
                    return &section;
                }
            }
        }
    }
    return nullptr;
}

void SequencerPanel::DrawDetails(Registry* registry)
{
    auto findFolderById = [&](uint64_t folderId) -> CinematicFolder* {
        for (auto& folder : m_sequence.folders) {
            if (folder.folderId == folderId) {
                return &folder;
            }
        }
        return nullptr;
    };
    auto findBindingFolderId = [&](uint64_t bindingId) -> uint64_t {
        for (const auto& folder : m_sequence.folders) {
            if (std::find(folder.bindingIds.begin(), folder.bindingIds.end(), bindingId) != folder.bindingIds.end()) {
                return folder.folderId;
            }
        }
        return 0;
    };
    auto findMasterTrackFolderId = [&](uint64_t trackId) -> uint64_t {
        for (const auto& folder : m_sequence.folders) {
            if (std::find(folder.masterTrackIds.begin(), folder.masterTrackIds.end(), trackId) != folder.masterTrackIds.end()) {
                return folder.folderId;
            }
        }
        return 0;
    };
    auto removeBindingFromFolders = [&](uint64_t bindingId) {
        for (auto& folder : m_sequence.folders) {
            folder.bindingIds.erase(std::remove(folder.bindingIds.begin(), folder.bindingIds.end(), bindingId), folder.bindingIds.end());
        }
    };
    auto removeMasterTrackFromFolders = [&](uint64_t trackId) {
        for (auto& folder : m_sequence.folders) {
            folder.masterTrackIds.erase(std::remove(folder.masterTrackIds.begin(), folder.masterTrackIds.end(), trackId), folder.masterTrackIds.end());
        }
    };

    ImGui::TextUnformatted("Details");
    ImGui::Separator();

    if (CinematicSection* section = FindSelectedSection()) {
        const bool isMasterTrack = std::any_of(m_sequence.masterTracks.begin(), m_sequence.masterTracks.end(),
            [&](const CinematicTrack& track) {
                return track.trackId == m_selectedTrackId;
            });
        ImGui::TextDisabled("Section");
        char textBuffer[256] = {};
        strncpy_s(textBuffer, section->label.c_str(), _TRUNCATE);
        if (ImGui::InputText("Label", textBuffer, sizeof(textBuffer))) {
            section->label = textBuffer;
            m_dirty = true;
        }
        if (ImGui::DragInt("Start", &section->startFrame, 1.0f, m_sequence.playRangeStart, m_sequence.playRangeEnd)) m_dirty = true;
        if (ImGui::DragInt("End", &section->endFrame, 1.0f, m_sequence.playRangeStart, m_sequence.playRangeEnd)) m_dirty = true;
        if (section->endFrame < section->startFrame) section->endFrame = section->startFrame;
        if (ImGui::DragFloat("Ease In", &section->easeInFrames, 0.25f, 0.0f, static_cast<float>(section->endFrame - section->startFrame))) m_dirty = true;
        if (ImGui::DragFloat("Ease Out", &section->easeOutFrames, 0.25f, 0.0f, static_cast<float>(section->endFrame - section->startFrame))) m_dirty = true;
        if (ImGui::Checkbox("Muted", &section->muted)) m_dirty = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Locked", &section->locked)) m_dirty = true;

        if (ImGui::BeginCombo("Eval Policy", GetCinematicEvalPolicyLabel(section->evalPolicy))) {
            const CinematicEvalPolicy policies[] = {
                CinematicEvalPolicy::Static,
                CinematicEvalPolicy::Animated,
                CinematicEvalPolicy::TriggerOnly
            };
            for (CinematicEvalPolicy policy : policies) {
                const bool selected = policy == section->evalPolicy;
                if (ImGui::Selectable(GetCinematicEvalPolicyLabel(policy), selected)) {
                    section->evalPolicy = policy;
                    m_dirty = true;
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("Seek Policy", GetCinematicSeekPolicyLabel(section->seekPolicy))) {
            const CinematicSeekPolicy policies[] = {
                CinematicSeekPolicy::TriggerOnSeek,
                CinematicSeekPolicy::SkipOnSeek,
                CinematicSeekPolicy::EvaluateRangeOnSeek
            };
            for (CinematicSeekPolicy policy : policies) {
                const bool selected = policy == section->seekPolicy;
                if (ImGui::Selectable(GetCinematicSeekPolicyLabel(policy), selected)) {
                    section->seekPolicy = policy;
                    m_dirty = true;
                }
            }
            ImGui::EndCombo();
        }

        switch (section->trackType) {
        case CinematicTrackType::Transform:
            if (ImGui::DragFloat3("Start Pos", &section->transform.startPosition.x, 0.1f)) m_dirty = true;
            if (ImGui::DragFloat3("End Pos", &section->transform.endPosition.x, 0.1f)) m_dirty = true;
            if (ImGui::DragFloat3("Start Rot", &section->transform.startRotationEuler.x, 0.5f)) m_dirty = true;
            if (ImGui::DragFloat3("End Rot", &section->transform.endRotationEuler.x, 0.5f)) m_dirty = true;
            if (ImGui::DragFloat3("Start Scale", &section->transform.startScale.x, 0.01f, 0.001f, 100.0f)) m_dirty = true;
            if (ImGui::DragFloat3("End Scale", &section->transform.endScale.x, 0.01f, 0.001f, 100.0f)) m_dirty = true;
            break;
        case CinematicTrackType::Camera:
        {
            if (isMasterTrack) {
                const char* currentCameraLabel = "None";
                for (const auto& binding : m_sequence.bindings) {
                    if (binding.bindingId == section->camera.cameraBindingId) {
                        currentCameraLabel = binding.displayName.c_str();
                        break;
                    }
                }
                if (ImGui::BeginCombo("Camera Binding", currentCameraLabel)) {
                    for (const auto& binding : m_sequence.bindings) {
                        const EntityID entity = ResolveBindingEntity(binding, registry);
                        if (!Entity::IsNull(entity) && registry && !registry->GetComponent<CameraLensComponent>(entity)) {
                            continue;
                        }
                        const bool selected = binding.bindingId == section->camera.cameraBindingId;
                        if (ImGui::Selectable(binding.displayName.c_str(), selected)) {
                            section->camera.cameraBindingId = binding.bindingId;
                            section->label = binding.displayName;
                            m_dirty = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::DragFloat("Blend Ease In", &section->camera.blendEaseIn, 0.25f, 0.0f, static_cast<float>(section->endFrame - section->startFrame))) m_dirty = true;
                if (ImGui::DragFloat("Blend Ease Out", &section->camera.blendEaseOut, 0.25f, 0.0f, static_cast<float>(section->endFrame - section->startFrame))) m_dirty = true;
                break;
            }

            int mode = static_cast<int>(section->camera.cameraMode);
            if (ImGui::BeginCombo("Camera Mode", GetCinematicCameraModeLabel(section->camera.cameraMode))) {
                for (int i = 0; i < 2; ++i) {
                    if (ImGui::Selectable(GetCinematicCameraModeLabel(static_cast<CinematicCameraMode>(i)), mode == i)) {
                        section->camera.cameraMode = static_cast<CinematicCameraMode>(i);
                        m_dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (section->camera.cameraMode == CinematicCameraMode::FreeCamera) {
                if (ImGui::DragFloat3("Start Pos", &section->camera.startPosition.x, 0.1f)) m_dirty = true;
                if (ImGui::DragFloat3("End Pos", &section->camera.endPosition.x, 0.1f)) m_dirty = true;
                if (ImGui::DragFloat3("Start Rot", &section->camera.startRotationEuler.x, 0.5f)) m_dirty = true;
                if (ImGui::DragFloat3("End Rot", &section->camera.endRotationEuler.x, 0.5f)) m_dirty = true;
            } else {
                if (ImGui::DragFloat3("Start Eye", &section->camera.startEye.x, 0.1f)) m_dirty = true;
                if (ImGui::DragFloat3("End Eye", &section->camera.endEye.x, 0.1f)) m_dirty = true;
                if (ImGui::DragFloat3("Start Target", &section->camera.startTarget.x, 0.1f)) m_dirty = true;
                if (ImGui::DragFloat3("End Target", &section->camera.endTarget.x, 0.1f)) m_dirty = true;
            }
            if (ImGui::DragFloat("Start FOV", &section->camera.startFovDeg, 0.1f, 1.0f, 170.0f)) m_dirty = true;
            if (ImGui::DragFloat("End FOV", &section->camera.endFovDeg, 0.1f, 1.0f, 170.0f)) m_dirty = true;
            if (ImGui::DragFloat("Blend Ease In", &section->camera.blendEaseIn, 0.25f, 0.0f, static_cast<float>(section->endFrame - section->startFrame))) m_dirty = true;
            if (ImGui::DragFloat("Blend Ease Out", &section->camera.blendEaseOut, 0.25f, 0.0f, static_cast<float>(section->endFrame - section->startFrame))) m_dirty = true;
            break;
        }
        case CinematicTrackType::Animation:
            if (ImGui::InputInt("Anim Index", &section->animation.animationIndex)) m_dirty = true;
            if (ImGui::DragFloat("Play Rate", &section->animation.playRate, 0.01f, 0.0f, 4.0f)) m_dirty = true;
            if (ImGui::Checkbox("Loop", &section->animation.loop)) m_dirty = true;
            break;
        case CinematicTrackType::Effect:
            strncpy_s(textBuffer, section->effect.effectAssetPath.c_str(), _TRUNCATE);
            if (ImGui::InputText("Effect", textBuffer, sizeof(textBuffer))) { section->effect.effectAssetPath = textBuffer; m_dirty = true; }
            if (ImGui::Checkbox("Loop", &section->effect.loop)) m_dirty = true;
            if (ImGui::Checkbox("Fire On Enter", &section->effect.fireOnEnterOnly)) m_dirty = true;
            if (ImGui::Checkbox("Stop On Exit", &section->effect.stopOnExit)) m_dirty = true;
            if (ImGui::BeginCombo("Retrigger", GetCinematicRetriggerPolicyLabel(section->effect.retriggerPolicy))) {
                const CinematicRetriggerPolicy policies[] = {
                    CinematicRetriggerPolicy::RestartIfActive,
                    CinematicRetriggerPolicy::IgnoreIfActive,
                    CinematicRetriggerPolicy::LayerIfAllowed
                };
                for (CinematicRetriggerPolicy policy : policies) {
                    const bool selected = policy == section->effect.retriggerPolicy;
                    if (ImGui::Selectable(GetCinematicRetriggerPolicyLabel(policy), selected)) {
                        section->effect.retriggerPolicy = policy;
                        m_dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::DragFloat3("Offset Pos", &section->effect.offsetPosition.x, 0.1f)) m_dirty = true;
            if (ImGui::DragFloat3("Offset Rot", &section->effect.offsetRotation.x, 0.5f)) m_dirty = true;
            if (ImGui::DragFloat3("Offset Scale", &section->effect.offsetScale.x, 0.01f, 0.001f, 100.0f)) m_dirty = true;
            break;
        case CinematicTrackType::Audio:
            strncpy_s(textBuffer, section->audio.audioAssetPath.c_str(), _TRUNCATE);
            if (ImGui::InputText("Audio", textBuffer, sizeof(textBuffer))) { section->audio.audioAssetPath = textBuffer; m_dirty = true; }
            if (ImGui::Checkbox("3D", &section->audio.is3D)) m_dirty = true;
            if (ImGui::Checkbox("Loop", &section->audio.loop)) m_dirty = true;
            if (ImGui::Checkbox("Stop On Exit", &section->audio.stopOnExit)) m_dirty = true;
            if (ImGui::DragFloat("Volume", &section->audio.volume, 0.01f, 0.0f, 4.0f)) m_dirty = true;
            if (ImGui::DragFloat("Pitch", &section->audio.pitch, 0.01f, 0.1f, 4.0f)) m_dirty = true;
            if (ImGui::DragFloat("Start Offset", &section->audio.startOffsetSec, 0.01f, 0.0f, 60.0f)) m_dirty = true;
            if (ImGui::BeginCombo("Retrigger", GetCinematicRetriggerPolicyLabel(section->audio.retriggerPolicy))) {
                const CinematicRetriggerPolicy policies[] = {
                    CinematicRetriggerPolicy::RestartIfActive,
                    CinematicRetriggerPolicy::IgnoreIfActive,
                    CinematicRetriggerPolicy::LayerIfAllowed
                };
                for (CinematicRetriggerPolicy policy : policies) {
                    const bool selected = policy == section->audio.retriggerPolicy;
                    if (ImGui::Selectable(GetCinematicRetriggerPolicyLabel(policy), selected)) {
                        section->audio.retriggerPolicy = policy;
                        m_dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
            break;
        case CinematicTrackType::Event:
            strncpy_s(textBuffer, section->eventData.eventName.c_str(), _TRUNCATE);
            if (ImGui::InputText("Event Name", textBuffer, sizeof(textBuffer))) { section->eventData.eventName = textBuffer; m_dirty = true; }
            strncpy_s(textBuffer, section->eventData.eventCategory.c_str(), _TRUNCATE);
            if (ImGui::InputText("Category", textBuffer, sizeof(textBuffer))) { section->eventData.eventCategory = textBuffer; m_dirty = true; }
            strncpy_s(textBuffer, section->eventData.payloadJson.c_str(), _TRUNCATE);
            if (ImGui::InputTextMultiline("Payload", textBuffer, sizeof(textBuffer), ImVec2(-1.0f, 80.0f))) { section->eventData.payloadJson = textBuffer; m_dirty = true; }
            if (ImGui::Checkbox("Fire Once", &section->eventData.fireOnce)) m_dirty = true;
            break;
        case CinematicTrackType::CameraShake:
            if (ImGui::DragFloat("Duration", &section->shake.duration, 0.01f, 0.0f, 10.0f)) m_dirty = true;
            if (ImGui::DragFloat("Amplitude", &section->shake.amplitude, 0.01f, 0.0f, 10.0f)) m_dirty = true;
            if (ImGui::DragFloat("Frequency", &section->shake.frequency, 0.1f, 0.0f, 120.0f)) m_dirty = true;
            if (ImGui::DragFloat("Decay", &section->shake.decay, 0.01f, 0.0f, 1.0f)) m_dirty = true;
            if (ImGui::DragFloat("Hit Stop", &section->shake.hitStopDuration, 0.001f, 0.0f, 1.0f)) m_dirty = true;
            if (ImGui::DragFloat("Time Scale", &section->shake.timeScale, 0.01f, 0.0f, 1.0f)) m_dirty = true;
            break;
        default:
            ImGui::TextDisabled("No detail editor for this track.");
            break;
        }
        return;
    }

    if (CinematicFolder* folder = findFolderById(m_selectedFolderId)) {
        ImGui::TextDisabled("Folder");
        char folderName[256] = {};
        strncpy_s(folderName, folder->displayName.c_str(), _TRUNCATE);
        if (ImGui::InputText("Folder Name", folderName, sizeof(folderName))) {
            folder->displayName = folderName;
            m_dirty = true;
        }
        if (ImGui::Checkbox("Expanded", &folder->expanded)) {
            m_dirty = true;
        }
        ImGui::Text("Bindings: %d", static_cast<int>(folder->bindingIds.size()));
        ImGui::Text("Master Tracks: %d", static_cast<int>(folder->masterTrackIds.size()));
        return;
    }

    if (const CinematicTrack* track = FindSelectedTrack()) {
        ImGui::TextDisabled("Track");
        ImGui::Text("Type: %s", GetCinematicTrackTypeLabel(track->type));
        ImGui::Text("Sections: %d", static_cast<int>(track->sections.size()));
        ImGui::Text("Muted: %s", track->muted ? "Yes" : "No");
        ImGui::Text("Locked: %s", track->locked ? "Yes" : "No");
        const uint64_t currentFolderId = findMasterTrackFolderId(track->trackId);
        const char* previewLabel = "None";
        if (const CinematicFolder* folder = currentFolderId != 0 ? findFolderById(currentFolderId) : nullptr) {
            previewLabel = folder->displayName.c_str();
        }
        if (ImGui::BeginCombo("Folder", previewLabel)) {
            const bool noneSelected = currentFolderId == 0;
            if (ImGui::Selectable("None", noneSelected)) {
                removeMasterTrackFromFolders(track->trackId);
                m_dirty = true;
            }
            for (auto& folder : m_sequence.folders) {
                const bool selected = currentFolderId == folder.folderId;
                if (ImGui::Selectable(folder.displayName.c_str(), selected)) {
                    removeMasterTrackFromFolders(track->trackId);
                    folder.masterTrackIds.push_back(track->trackId);
                    m_dirty = true;
                }
            }
            ImGui::EndCombo();
        }
        return;
    }

    if (const CinematicBinding* binding = FindSelectedBinding()) {
        const EntityID runtimeEntity = ResolveBindingEntity(*binding, registry);
        ImGui::TextDisabled("Binding");
        ImGui::Text("Name: %s", binding->displayName.c_str());
        ImGui::Text("Kind: %s", GetBindingKindLabel(binding->bindingKind));
        ImGui::Text("Entity: %llu", static_cast<unsigned long long>(runtimeEntity));
        ImGui::Text("Tracks: %d", static_cast<int>(binding->tracks.size()));
        const uint64_t currentFolderId = findBindingFolderId(binding->bindingId);
        const char* previewLabel = "None";
        if (const CinematicFolder* folder = currentFolderId != 0 ? findFolderById(currentFolderId) : nullptr) {
            previewLabel = folder->displayName.c_str();
        }
        if (ImGui::BeginCombo("Folder", previewLabel)) {
            const bool noneSelected = currentFolderId == 0;
            if (ImGui::Selectable("None", noneSelected)) {
                removeBindingFromFolders(binding->bindingId);
                m_dirty = true;
            }
            for (auto& folder : m_sequence.folders) {
                const bool selected = currentFolderId == folder.folderId;
                if (ImGui::Selectable(folder.displayName.c_str(), selected)) {
                    removeBindingFromFolders(binding->bindingId);
                    folder.bindingIds.push_back(binding->bindingId);
                    m_dirty = true;
                }
            }
            ImGui::EndCombo();
        }
        return;
    }

    ImGui::TextDisabled("Sequence");
    char sequenceName[256] = {};
    strncpy_s(sequenceName, m_sequence.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("Name", sequenceName, sizeof(sequenceName))) {
        m_sequence.name = sequenceName;
        m_dirty = true;
    }
    if (ImGui::DragFloat("Frame Rate", &m_sequence.frameRate, 0.1f, 1.0f, 240.0f)) m_dirty = true;
    if (ImGui::DragInt("Duration", &m_sequence.durationFrames, 1.0f, 1, 100000)) {
        m_sequence.durationFrames = (std::max)(1, m_sequence.durationFrames);
        m_sequence.playRangeEnd = (std::min)(m_sequence.playRangeEnd, m_sequence.durationFrames);
        m_sequence.workRangeEnd = (std::min)(m_sequence.workRangeEnd, m_sequence.durationFrames);
        m_dirty = true;
    }
    if (ImGui::DragInt("Work Start", &m_sequence.workRangeStart, 1.0f, 0, m_sequence.durationFrames)) m_dirty = true;
    if (ImGui::DragInt("Work End", &m_sequence.workRangeEnd, 1.0f, 0, m_sequence.durationFrames)) m_dirty = true;
    ImGui::Separator();
    ImGui::TextWrapped("Add a binding from the selected entity, or create a Sequencer camera to author shots directly in the scene.");
}

void SequencerPanel::SyncPreviewRuntime(Registry* registry)
{
    if (!registry) {
        return;
    }

    auto& service = CinematicService::Instance();
    service.SetRegistry(registry);

    if (!m_previewHandle.IsValid() || !service.IsAlive(m_previewHandle)) {
        m_previewHandle = service.PlaySequenceAsset(m_sequence);
        service.PauseSequence(m_previewHandle, true);
    } else if (m_dirty) {
        service.UpdateSequenceAsset(m_previewHandle, m_sequence);
    }

    for (const CinematicBinding& binding : m_sequence.bindings) {
        const EntityID entity = ResolveBindingEntity(binding, registry);
        if (!Entity::IsNull(entity)) {
            service.BindEntity(m_previewHandle, binding.bindingId, entity);
        }
    }
    service.SeekSequence(m_previewHandle, m_currentFrame);
}

void SequencerPanel::StopPreviewRuntime()
{
    if (!m_previewHandle.IsValid()) {
        return;
    }
    auto& service = CinematicService::Instance();
    if (service.IsAlive(m_previewHandle)) {
        service.StopSequence(m_previewHandle);
    }
    m_previewHandle.Reset();
}

void SequencerPanel::SyncSelectionFromEditor(Registry* registry)
{
    if (!m_selectionSyncEnabled || !registry) {
        return;
    }
    if (m_selectedTrackId != 0 || m_selectedSectionId != 0) {
        return;
    }

    const EntityID selectedEntity = EditorSelection::Instance().GetPrimaryEntity();
    if (Entity::IsNull(selectedEntity) || !registry->IsAlive(selectedEntity)) {
        return;
    }

    for (const auto& binding : m_sequence.bindings) {
        if (ResolveBindingEntity(binding, registry) == selectedEntity) {
            m_selectedBindingId = binding.bindingId;
            return;
        }
    }
}
