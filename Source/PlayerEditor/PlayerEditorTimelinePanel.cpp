// ============================================================================
// PlayerEditor — Timeline panel (track headers, grid, playback toolbar) and
// timeline-related queries (frame limit, animation duration, selector).
// Sibling of PlayerEditorPanel.cpp; split out for readability.
// ============================================================================
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

#include "Icon/IconsFontAwesome7.h"
#include "PlayerEditorPanelInternal.h"
#include "PlayerEditorSession.h"
#include "Animator/AnimatorService.h"
#include "Gameplay/HitStopComponent.h"
#include "Gameplay/PlaybackComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"

using namespace PlayerEditorInternal;

void PlayerEditorPanel::DrawTimelinePanel()
{
    if (!ImGui::Begin(kPETimelineTitle)) { ImGui::End(); return; }

    DrawTimelinePlaybackToolbar();

    float availH = ImGui::GetContentRegionAvail().y;
    float availW = ImGui::GetContentRegionAvail().x;

    // Left: Track Headers
    ImGui::BeginChild("TLHeaders", ImVec2(kTrackHeaderWidth, availH), ImGuiChildFlags_Borders);
    DrawTimelineTrackHeaders(availH);
    ImGui::EndChild();

    ImGui::SameLine();

    // Right: Grid
    ImGui::BeginChild("TLGrid", ImVec2(availW - kTrackHeaderWidth - 8, availH),
        ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    DrawTimelineGrid(availH);
    ImGui::EndChild();

    ImGui::End();
}

void PlayerEditorPanel::DrawTimelinePlaybackToolbar()
{
    const float fps = m_timelineAsset.fps > 0.0f ? m_timelineAsset.fps : 60.0f;
    const int maxFrame = GetTimelineFrameLimit();

    if (m_isPlaying && m_previewState.IsActive()) {
        const float rawPreviewDt = ImGui::GetIO().DeltaTime;
        float previewDt = rawPreviewDt;
        if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
            if (HitStopComponent* hitStop = m_registry->GetComponent<HitStopComponent>(m_previewEntity)) {
                if (hitStop->timer > 0.0f) {
                    previewDt *= hitStop->speedScale;
                    hitStop->timer -= rawPreviewDt;
                    if (hitStop->timer < 0.0f) {
                        hitStop->timer = 0.0f;
                    }
                }
            }
        }
        m_previewState.AdvanceTime(previewDt, m_timelineAsset);
        m_playheadFrame = m_previewState.GetCurrentFrame(fps);
        if (m_playheadFrame < 0) {
            m_playheadFrame = 0;
        }
        if (m_playheadFrame > maxFrame) {
            m_playheadFrame = maxFrame;
        }
        if (!m_previewState.GetDriver()->IsLoop() && m_playheadFrame >= maxFrame) {
            m_isPlaying = false;
        }
        SyncPreviewTimelinePlayback(false);
    }

    if (ImGui::Button(ICON_FA_BACKWARD_STEP)) {
        m_playheadFrame = 0;
        m_isPlaying = false;
        if (m_previewState.IsActive()) {
            m_previewState.SetTime(0.0f);
        }
        if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
            if (HitStopComponent* hitStop = m_registry->GetComponent<HitStopComponent>(m_previewEntity)) {
                hitStop->timer = 0.0f;
                hitStop->speedScale = 0.0f;
            }
        }
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();
    if (ImGui::Button(m_isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY)) {
        if (m_isPlaying) {
            m_isPlaying = false;
            SyncPreviewTimelinePlayback();
        } else if (m_previewState.IsActive()) {
            m_isPlaying = true;
            SyncPreviewTimelinePlayback();
        } else {
            StartSelectedAnimationPreview();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP)) {
        m_isPlaying = false;
        m_playheadFrame = 0;
        if (m_previewState.IsActive()) {
            m_previewState.SetTime(0.0f);
        }
        if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
            if (HitStopComponent* hitStop = m_registry->GetComponent<HitStopComponent>(m_previewEntity)) {
                hitStop->timer = 0.0f;
                hitStop->speedScale = 0.0f;
            }
        }
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FORWARD_STEP)) {
        ++m_playheadFrame;
        if (m_previewState.IsActive()) {
            m_previewState.SetTime(m_playheadFrame / fps);
        }
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(100);
    if (ImGui::DragInt("##Frame", &m_playheadFrame, 1.0f, 0, maxFrame)) {
        if (m_previewState.IsActive()) {
            m_previewState.SetTime(m_playheadFrame / fps);
        }
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();
    ImGui::Text("/ %d", maxFrame);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(50);
    if (ImGui::DragFloat("FPS", &m_timelineAsset.fps, 1.0f, 1.0f, 120.0f, "%.0f")) {
        m_timelineDirty = true;
        RebuildPreviewTimelineRuntimeData();
        SyncPreviewTimelinePlayback();
    }
    ImGui::SameLine();

    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("Zoom##TL", &m_timelineZoom, 0.2f, 5.0f, "%.1f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("Dur(s)", &m_timelineAsset.duration, 0.1f, 0.1f, 300.0f, "%.1f")) {
        m_timelineDirty = true;
        RebuildPreviewTimelineRuntimeData();
        SyncPreviewTimelinePlayback();
    }
}

void PlayerEditorPanel::DrawTimelineTrackHeaders(float height)
{
    bool timelineRuntimeChanged = false;

    if (ImGui::Button(ICON_FA_PLUS " Track")) {
        ImGui::OpenPopup("AddTrackPopup");
    }

    if (ImGui::BeginPopup("AddTrackPopup")) {
        if (ImGui::MenuItem("Hitbox")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::Hitbox, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::Hitbox));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::Hitbox, m_playheadFrame));
            m_timelineDirty = true;
            timelineRuntimeChanged = true;
        }
        if (ImGui::MenuItem("VFX")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::VFX, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::VFX));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::VFX, m_playheadFrame));
            m_timelineDirty = true;
            timelineRuntimeChanged = true;
        }
        if (ImGui::MenuItem("Audio")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::Audio, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::Audio));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::Audio, m_playheadFrame));
            m_timelineDirty = true;
            timelineRuntimeChanged = true;
        }
        if (ImGui::MenuItem("CameraShake")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::CameraShake, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::CameraShake));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::CameraShake, m_playheadFrame));
            m_timelineDirty = true;
            timelineRuntimeChanged = true;
        }
        if (ImGui::MenuItem("Event")) {
            TimelineTrack* track = m_timelineAsset.AddTrack(TimelineTrackType::Event, GenerateDefaultTrackName(m_timelineAsset, TimelineTrackType::Event));
            if (track) track->items.push_back(CreateDefaultTimelineItem(TimelineTrackType::Event, m_playheadFrame));
            m_timelineDirty = true;
            timelineRuntimeChanged = true;
        }
        ImGui::EndPopup();
    }

    if (timelineRuntimeChanged) {
        RebuildPreviewTimelineRuntimeData();
        SyncPreviewTimelinePlayback();
    }

    ImGui::Separator();

    // Track list
    for (int ti = 0; ti < (int)m_timelineAsset.tracks.size(); ++ti) {
        auto& track = m_timelineAsset.tracks[ti];
        ImGui::PushID(track.id);

        bool selected = (m_selectedTrackId == (int)track.id);

        // Mute toggle icon
        if (track.muted)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (ImGui::Selectable(track.name.c_str(), selected, 0, ImVec2(0, kTrackHeight))) {
            m_selectedTrackId = track.id;
            m_selectedItemIdx = -1;
            m_selectionCtx = SelectionContext::TimelineTrack;
        }

        if (track.muted)
            ImGui::PopStyleColor();

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::Checkbox("Muted", &track.muted)) m_timelineDirty = true;
            if (ImGui::Checkbox("Locked", &track.locked)) m_timelineDirty = true;
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_TRASH " Delete Track")) {
                m_timelineAsset.RemoveTrack(track.id);
                if (m_selectedTrackId == static_cast<int>(track.id)) {
                    m_selectedTrackId = -1;
                    m_selectedItemIdx = -1;
                    m_selectionCtx = SelectionContext::None;
                }
                m_timelineDirty = true;
                RebuildPreviewTimelineRuntimeData();
                SyncPreviewTimelinePlayback();
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
}

void PlayerEditorPanel::DrawTimelineGrid(float height)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    float ppf = kDefaultPPF * m_timelineZoom;
    ppf = (std::max)(kMinPixelsPerFrame, ppf);

    int totalFrames = GetTimelineFrameLimit();

    float totalWidth = totalFrames * ppf;
    float drawWidth = (std::max)(canvasSize.x, totalWidth);

    // Background
    dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + drawWidth, canvasPos.y + canvasSize.y),
        IM_COL32(28, 28, 28, 255));

    // Ruler
    float rulerH = 22.0f;
    int frameStep = (std::max)(1, (int)(40.0f / ppf));
    for (int f = 0; f <= totalFrames; f += frameStep) {
        float x = canvasPos.x + f * ppf;
        bool major = (f % (frameStep * 5) == 0);
        dl->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + rulerH),
            major ? IM_COL32(100, 100, 100, 255) : IM_COL32(60, 60, 60, 255));
        if (major || ppf > 3.0f) {
            char buf[16]; snprintf(buf, sizeof(buf), "%d", f);
            dl->AddText(ImVec2(x + 2, canvasPos.y + 2), IM_COL32(170, 170, 170, 255), buf);
        }
    }

    // Track rows & items
    float yOff = rulerH;
    for (int ti = 0; ti < (int)m_timelineAsset.tracks.size(); ++ti) {
        auto& track = m_timelineAsset.tracks[ti];
        float trackY = canvasPos.y + yOff;

        ImU32 rowCol = (ti % 2 == 0) ? IM_COL32(32, 32, 32, 255) : IM_COL32(38, 38, 38, 255);
        dl->AddRectFilled(ImVec2(canvasPos.x, trackY),
            ImVec2(canvasPos.x + drawWidth, trackY + kTrackHeight), rowCol);

        ImU32 itemCol = track.muted ? IM_COL32(70, 70, 70, 180) : track.color;

        for (int ii = 0; ii < (int)track.items.size(); ++ii) {
            auto& item = track.items[ii];
            float x0 = canvasPos.x + item.startFrame * ppf;
            float x1 = canvasPos.x + item.endFrame * ppf;
            if (track.type == TimelineTrackType::Event) {
                x1 = x0 + (std::max)(12.0f, ppf * 0.75f);
            }
            float y0 = trackY + 2;
            float y1 = trackY + kTrackHeight - 2;

            bool isSelItem = (m_selectedTrackId == (int)track.id && m_selectedItemIdx == ii);
            bool leftResizeHovered = false;
            bool rightResizeHovered = false;
            bool resizingItem = false;

            // Item body with rounded corners
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), itemCol, 4.0f);
            if (isSelItem) {
                dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(255, 255, 255, 255), 4.0f, 0, 2.0f);
            }

            // Frame range label inside item
            if ((x1 - x0) > 40) {
                char lbl[32]; snprintf(lbl, sizeof(lbl), "%d-%d", item.startFrame, item.endFrame);
                dl->AddText(ImVec2(x0 + 3, y0 + 1), IM_COL32(255, 255, 255, 180), lbl);
            }

            if (!track.locked && track.type != TimelineTrackType::Event) {
                const float handleWidth = 10.0f;
                ImGui::SetCursorScreenPos(ImVec2(x0 - handleWidth * 0.5f, y0));
                ImGui::InvisibleButton(("item_l_" + std::to_string(track.id) + "_" + std::to_string(ii)).c_str(), ImVec2(handleWidth, y1 - y0));
                leftResizeHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                    int frameDelta = static_cast<int>(ImGui::GetMouseDragDelta(0).x / ppf);
                    if (frameDelta != 0) {
                        item.startFrame = (std::max)(0, (std::min)(item.endFrame - 1, item.startFrame + frameDelta));
                        m_timelineDirty = true;
                        RebuildPreviewTimelineRuntimeData();
                        SyncPreviewTimelinePlayback();
                        ImGui::ResetMouseDragDelta();
                    }
                    resizingItem = true;
                }

                ImGui::SetCursorScreenPos(ImVec2(x1 - handleWidth * 0.5f, y0));
                ImGui::InvisibleButton(("item_r_" + std::to_string(track.id) + "_" + std::to_string(ii)).c_str(), ImVec2(handleWidth, y1 - y0));
                rightResizeHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                    int frameDelta = static_cast<int>(ImGui::GetMouseDragDelta(0).x / ppf);
                    if (frameDelta != 0) {
                        item.endFrame = (std::max)(item.startFrame + 1, item.endFrame + frameDelta);
                        m_timelineDirty = true;
                        RebuildPreviewTimelineRuntimeData();
                        SyncPreviewTimelinePlayback();
                        ImGui::ResetMouseDragDelta();
                    }
                    resizingItem = true;
                }

                if (leftResizeHovered || rightResizeHovered) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
            }

            // Click / drag body after resize handles so the edges win.
            ImGui::SetCursorScreenPos(ImVec2(x0, y0));
            ImGui::InvisibleButton(("item_" + std::to_string(track.id) + "_" + std::to_string(ii)).c_str(),
                ImVec2((std::max)(x1 - x0, 4.0f), y1 - y0));

            if (ImGui::IsItemClicked(0)) {
                m_selectedTrackId = track.id;
                m_selectedItemIdx = ii;
                m_selectionCtx = SelectionContext::TimelineItem;
            }

            // Drag to move only when not resizing.
            if (!resizingItem && ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !track.locked) {
                float dx = ImGui::GetMouseDragDelta(0).x;
                int frameDelta = (int)(dx / ppf);
                if (frameDelta != 0) {
                    item.startFrame += frameDelta;
                    if (track.type == TimelineTrackType::Event) {
                        item.endFrame = item.startFrame;
                    } else {
                        item.endFrame += frameDelta;
                    }
                    if (item.startFrame < 0) { item.endFrame -= item.startFrame; item.startFrame = 0; }
                    m_timelineDirty = true;
                    RebuildPreviewTimelineRuntimeData();
                    SyncPreviewTimelinePlayback();
                    ImGui::ResetMouseDragDelta();
                }
            }

            // Right-click: delete item
            if (ImGui::IsItemClicked(1)) {
                m_selectedTrackId = track.id;
                m_selectedItemIdx = ii;
                m_selectionCtx = SelectionContext::TimelineItem;
                ImGui::OpenPopup("ItemCtx");
            }
        }

          // Right-click on empty track area is intentionally disabled.
          ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, trackY));
          ImGui::InvisibleButton(("trow_" + std::to_string(track.id)).c_str(),
              ImVec2(drawWidth, kTrackHeight));

        yOff += kTrackHeight;
    }

      // Item context menu
      if (ImGui::BeginPopup("ItemCtx")) {
          if (ImGui::MenuItem(ICON_FA_TRASH " Delete")) {
              for (auto& track : m_timelineAsset.tracks) {
                  if ((int)track.id == m_selectedTrackId && m_selectedItemIdx >= 0 &&
                    m_selectedItemIdx < (int)track.items.size())
                {
                    track.items.erase(track.items.begin() + m_selectedItemIdx);
                    m_selectedItemIdx = -1;
                    m_selectionCtx = SelectionContext::None;
                    m_timelineDirty = true;
                    RebuildPreviewTimelineRuntimeData();
                    SyncPreviewTimelinePlayback();
                    break;
                }
            }
        }
        ImGui::EndPopup();
    }

    // Playhead
    float phX = std::floor(canvasPos.x + m_playheadFrame * ppf) + 0.5f;
    dl->AddLine(ImVec2(phX, canvasPos.y), ImVec2(phX, canvasPos.y + canvasSize.y),
        IM_COL32(255, 70, 70, 255), 2.0f);
    dl->AddTriangleFilled(
        ImVec2(phX - kPlayheadTriSize, canvasPos.y),
        ImVec2(phX + kPlayheadTriSize, canvasPos.y),
        ImVec2(phX, canvasPos.y + kPlayheadTriSize * 1.5f),
        IM_COL32(255, 70, 70, 255));

    // Click ruler to scrub
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("ruler_click", ImVec2(drawWidth, rulerH));
    if (ImGui::IsItemActive()) {
        float mx = ImGui::GetMousePos().x - canvasPos.x;
        m_playheadFrame = (std::max)(0, (std::min)(totalFrames, (int)(mx / ppf)));
        if (m_previewState.IsActive()) {
            const float fps = m_timelineAsset.fps > 0.0f ? m_timelineAsset.fps : 60.0f;
            m_previewState.SetTime(m_playheadFrame / fps);
        }
        SyncPreviewTimelinePlayback();
    }

    // Zoom with scroll wheel
    if (ImGui::IsWindowHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_timelineZoom *= (wheel > 0) ? 1.1f : 0.9f;
            m_timelineZoom = (std::max)(0.1f, (std::min)(8.0f, m_timelineZoom));
        }
    }

    // Dummy for scrollbar
    ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + totalWidth, canvasPos.y + yOff + rulerH));
    ImGui::Dummy(ImVec2(0, 0));
}

bool PlayerEditorPanel::TryAssignSelectedBoneToTimelineItem(int boneIndex)
{
    if (boneIndex < 0) {
        return false;
    }

    for (auto& track : m_timelineAsset.tracks) {
        if (static_cast<int>(track.id) != m_selectedTrackId) {
            continue;
        }
        if (m_selectedItemIdx < 0 || m_selectedItemIdx >= static_cast<int>(track.items.size())) {
            return false;
        }

        TimelineItem& item = track.items[m_selectedItemIdx];
        switch (track.type) {
        case TimelineTrackType::Hitbox:
            item.hitbox.nodeIndex = boneIndex;
            item.hitbox.offsetLocal = { 0.0f, 0.0f, 0.0f };
            m_timelineDirty = true;
            RebuildPreviewTimelineRuntimeData();
            if (m_previewState.IsActive()) {
                SyncPreviewTimelinePlayback();
            }
            return true;

        case TimelineTrackType::VFX:
            item.vfx.nodeIndex = boneIndex;
            item.vfx.offsetLocal = { 0.0f, 0.0f, 0.0f };
            m_timelineDirty = true;
            RebuildPreviewTimelineRuntimeData();
            if (m_previewState.IsActive()) {
                SyncPreviewTimelinePlayback();
            }
            return true;

        default:
            return false;
        }
    }

    return false;
}

bool PlayerEditorPanel::DrawAnimationSelector(const char* label, int* animIndex)
{
    if (!animIndex) return false;

    std::string preview = "None";
    const auto* selectedAnimation = m_model &&
        *animIndex >= 0 &&
        *animIndex < static_cast<int>(m_model->GetAnimations().size())
        ? &m_model->GetAnimations()[*animIndex]
        : nullptr;

    if (selectedAnimation) {
        preview = "[" + std::to_string(*animIndex) + "] " + selectedAnimation->name;
    } else if (*animIndex >= 0) {
        preview = "[" + std::to_string(*animIndex) + "] <invalid>";
    }

    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        const bool noneSelected = (*animIndex < 0);
        if (ImGui::Selectable("None", noneSelected)) {
            *animIndex = -1;
            changed = true;
        }
        if (noneSelected) {
            ImGui::SetItemDefaultFocus();
        }

        if (m_model) {
            const auto& animations = m_model->GetAnimations();
            for (int i = 0; i < static_cast<int>(animations.size()); ++i) {
                const bool selected = (*animIndex == i);
                std::string itemLabel = "[" + std::to_string(i) + "] " + animations[i].name;
                if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                    *animIndex = i;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }

        ImGui::EndCombo();
    }

    if (m_model) {
        ImGui::TextDisabled("%d animation(s)", static_cast<int>(m_model->GetAnimations().size()));
    } else {
        ImGui::TextDisabled("No model assigned.");
    }

    return changed;
}

float PlayerEditorPanel::GetSelectedAnimationDurationSeconds() const
{
    if (m_model &&
        m_selectedAnimIndex >= 0 &&
        m_selectedAnimIndex < static_cast<int>(m_model->GetAnimations().size())) {
        return (std::max)(0.0f, m_model->GetAnimations()[m_selectedAnimIndex].secondsLength);
    }

    return 0.0f;
}

int PlayerEditorPanel::GetTimelineFrameLimit() const
{
    const float fps = m_timelineAsset.fps > 0.0f ? m_timelineAsset.fps : 60.0f;
    int maxFrame = m_timelineAsset.GetFrameCount();

    for (const auto& track : m_timelineAsset.tracks) {
        for (const auto& item : track.items) {
            if (item.endFrame > maxFrame) {
                maxFrame = item.endFrame;
            }
        }
        for (const auto& keyframe : track.keyframes) {
            if (keyframe.frame > maxFrame) {
                maxFrame = keyframe.frame;
            }
        }
    }

    const float animationDurationSeconds = GetSelectedAnimationDurationSeconds();
    if (animationDurationSeconds > 0.0f && fps > 0.0f) {
        const int animationFrameCount = static_cast<int>(animationDurationSeconds * fps);
        if (animationFrameCount > maxFrame) {
            maxFrame = animationFrameCount;
        }
    }

    if (maxFrame <= 0) {
        maxFrame = 600;
    }
    return maxFrame;
}

float PlayerEditorPanel::GetTimelinePlaybackDurationSeconds() const
{
    const float fps = m_timelineAsset.fps > 0.0f ? m_timelineAsset.fps : 60.0f;
    const int frameLimit = GetTimelineFrameLimit();
    if (fps <= 0.0f || frameLimit <= 0) {
        return 0.0f;
    }
    return static_cast<float>(frameLimit) / fps;
}

void PlayerEditorPanel::StartSelectedAnimationPreview()
{
    const bool hasPreviewTarget = m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity);
    if (!hasPreviewTarget) {
        return;
    }

    if (!m_model || m_model->GetAnimations().empty()) {
        return;
    }

    if (m_selectedAnimIndex < 0 || m_selectedAnimIndex >= static_cast<int>(m_model->GetAnimations().size())) {
        m_selectedAnimIndex = 0;
    }

    PlayerEditorSession::SyncTimelineAssetSelection(*this);
    AnimatorService::Instance().EnsureAnimator(m_previewEntity);
    if (!m_previewState.IsActive()) {
        m_previewState.EnterPreview(m_previewEntity);
    }

    RebuildPreviewTimelineRuntimeData();
    if (HitStopComponent* hitStop = m_registry->GetComponent<HitStopComponent>(m_previewEntity)) {
        hitStop->timer = 0.0f;
        hitStop->speedScale = 0.0f;
    }
    m_playheadFrame = 0;
    m_previewState.SetAnimationIndex(m_selectedAnimIndex);
    m_previewState.SetTime(0.0f);
    m_isPlaying = true;
    SyncPreviewTimelinePlayback();
}
