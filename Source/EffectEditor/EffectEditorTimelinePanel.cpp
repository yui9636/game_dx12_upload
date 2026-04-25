// Timeline panel for EffectEditorPanel — extracted from EffectEditorPanel.cpp.
// Method body remains a member function of EffectEditorPanel.

#include "EffectEditorPanel.h"
#include "EffectEditorPanelInternal.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>

#include "Component/EffectParameterOverrideComponent.h"
#include "Component/EffectPlaybackComponent.h"
#include "EffectRuntime/EffectGraphAsset.h"
#include "EffectRuntime/EffectGraphSerializer.h"
#include "EffectRuntime/EffectParameterBindings.h"
#include "EffectRuntime/EffectRuntimeRegistry.h"
#include "Entity/Entity.h"
#include "Icon/IconsFontAwesome7.h"
#include "Registry/Registry.h"

using namespace EffectEditorInternal;

namespace
{
    inline float SafeDuration(float duration) { return (duration > 0.01f) ? duration : 0.01f; }
}

void EffectEditorPanel::DrawTimelinePanel()
{
    if (!ImGui::Begin(kTimelineWindowTitle)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##EffectBottomTabs")) {
        if (ImGui::BeginTabItem("Curves")) {
            EffectPlaybackComponent* playback = nullptr;
            if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
                playback = m_registry->GetComponent<EffectPlaybackComponent>(m_previewEntity);
            }

            const float duration = SafeDuration(m_asset.previewDefaults.duration);
            const float currentTime = playback ? std::clamp(playback->currentTime, 0.0f, duration) : 0.0f;
            const float sampleT = std::clamp(currentTime / duration, 0.0f, 1.0f);

            EffectGraphNode* emitterNode = FindNodeByType(EffectGraphNodeType::ParticleEmitter);
            EffectGraphNode* colorNode = FindNodeByType(EffectGraphNodeType::Color);
            EffectGraphNode* spriteNode = FindNodeByType(EffectGraphNodeType::SpriteRenderer);

            const auto easeSample = [](float t, float bias) {
                return std::pow((std::max)(0.0f, (std::min)(1.0f, t)), (std::max)(0.05f, bias));
            };
            const auto lerpSample = [](float a, float b, float t) {
                return a + (b - a) * t;
            };

            if (!emitterNode || !colorNode || !spriteNode) {
                ImGui::TextDisabled("Curves require Particle Emitter, Color, and Sprite Renderer modules.");
                if (ImGui::Button("Build Missing Modules")) {
                    EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                    EnsureNodeByType(EffectGraphNodeType::Color);
                    EnsureNodeByType(EffectGraphNodeType::SpriteRenderer);
                    EnsureGuiAuthoringLinks(m_asset);
                    m_compileDirty = true;
                }
                ImGui::EndTabItem();
                ImGui::EndTabBar();
                ImGui::End();
                return;
            }

            ImGui::TextDisabled("Life curves preview and edit");
            ImGui::SameLine();
            ImGui::Text("Time %.2f / %.2f", currentTime, duration);
            ImGui::Separator();

            if (ImGui::BeginChild("##EffectCurveControls", ImVec2(330.0f, 0.0f), true)) {
                ImGui::TextUnformatted("Size Over Life");
                m_compileDirty |= ImGui::DragFloat("Start Size##Curve", &emitterNode->vectorValue.y, 0.01f, 0.01f, 25.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("End Size##Curve", &emitterNode->vectorValue.z, 0.01f, 0.00f, 25.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Size Bias##Curve", &spriteNode->vectorValue3.x, 0.01f, 0.05f, 8.0f, "%.2f");
                ImGui::Spacing();
                ImGui::TextUnformatted("Color / Alpha Over Life");
                m_compileDirty |= ImGui::ColorEdit4("Start Color##Curve", &colorNode->vectorValue.x);
                m_compileDirty |= ImGui::ColorEdit4("End Color##Curve", &colorNode->vectorValue2.x);
                m_compileDirty |= ImGui::DragFloat("Alpha Scale##Curve", &spriteNode->vectorValue2.z, 0.01f, 0.01f, 4.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Alpha Bias##Curve", &spriteNode->vectorValue3.y, 0.01f, 0.05f, 8.0f, "%.2f");
                ImGui::Spacing();
                const float sizeValue = lerpSample(emitterNode->vectorValue.y, emitterNode->vectorValue.z, easeSample(sampleT, spriteNode->vectorValue3.x));
                const float alphaScale = (spriteNode->vectorValue2.z > 0.0f) ? spriteNode->vectorValue2.z : 1.0f;
                const float alphaT = easeSample(sampleT, spriteNode->vectorValue3.y);
                const float alphaValue = lerpSample(colorNode->vectorValue.w, colorNode->vectorValue2.w, alphaT) * alphaScale;
                ImGui::Text("Sample Size: %.3f", sizeValue);
                ImGui::Text("Sample Alpha: %.3f", alphaValue);

                // Phase 1C: Size Curve Multi-Key
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextUnformatted("Size Curve (Multi-Key)");
                int sizeKeyCount = static_cast<int>(spriteNode->vectorValue6.w);
                if (sizeKeyCount < 2) sizeKeyCount = 2;
                if (ImGui::SliderInt("Key Count##SizeCurve", &sizeKeyCount, 2, 4)) {
                    spriteNode->vectorValue6.w = static_cast<float>(sizeKeyCount);
                    if (sizeKeyCount >= 3) {
                        if (spriteNode->vectorValue5.x <= 0.0f) spriteNode->vectorValue5.x = emitterNode->vectorValue.y;
                        if (spriteNode->vectorValue5.y <= 0.0f) spriteNode->vectorValue5.y = (emitterNode->vectorValue.y + emitterNode->vectorValue.z) * 0.5f;
                        if (spriteNode->vectorValue5.z <= 0.0f) spriteNode->vectorValue5.z = emitterNode->vectorValue.z;
                        if (spriteNode->vectorValue5.w <= 0.0f) spriteNode->vectorValue5.w = emitterNode->vectorValue.z;
                        if (spriteNode->vectorValue6.x <= 0.0f) spriteNode->vectorValue6.x = 0.33f;
                        if (spriteNode->vectorValue6.y <= 0.0f) spriteNode->vectorValue6.y = 0.66f;
                    }
                    m_compileDirty = true;
                }
                if (sizeKeyCount >= 3) {
                    m_compileDirty |= ImGui::DragFloat("S0 (t=0)##SC", &spriteNode->vectorValue5.x, 0.01f, 0.0f, 25.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat("S1##SC", &spriteNode->vectorValue5.y, 0.01f, 0.0f, 25.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat("T1##SC", &spriteNode->vectorValue6.x, 0.005f, 0.01f, 0.99f, "%.2f");
                    if (sizeKeyCount >= 4) {
                        m_compileDirty |= ImGui::DragFloat("S2##SC", &spriteNode->vectorValue5.z, 0.01f, 0.0f, 25.0f, "%.2f");
                        m_compileDirty |= ImGui::DragFloat("T2##SC", &spriteNode->vectorValue6.y, 0.005f, 0.01f, 0.99f, "%.2f");
                    }
                    m_compileDirty |= ImGui::DragFloat("S3 (t=1)##SC", &spriteNode->vectorValue5.w, 0.01f, 0.0f, 25.0f, "%.2f");
                }

                // Phase 1C: Color Gradient Multi-Key
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextUnformatted("Color Gradient (Multi-Key)");
                int gradKeyCount = colorNode->intValue;
                if (gradKeyCount < 2) gradKeyCount = 2;
                if (ImGui::SliderInt("Key Count##Gradient", &gradKeyCount, 2, 4)) {
                    colorNode->intValue = gradKeyCount;
                    if (gradKeyCount >= 3) {
                        if (colorNode->scalar <= 0.0f) colorNode->scalar = 0.33f;
                        if (colorNode->scalar2 <= 0.0f) colorNode->scalar2 = 0.66f;
                    }
                    m_compileDirty = true;
                }
                if (gradKeyCount >= 3) {
                    m_compileDirty |= ImGui::ColorEdit4("Mid Color 1##GC", &colorNode->vectorValue3.x);
                    m_compileDirty |= ImGui::DragFloat("Time 1##GC", &colorNode->scalar, 0.005f, 0.01f, 0.99f, "%.2f");
                    if (gradKeyCount >= 4) {
                        m_compileDirty |= ImGui::ColorEdit4("Mid Color 2##GC", &colorNode->vectorValue4.x);
                        m_compileDirty |= ImGui::DragFloat("Time 2##GC", &colorNode->scalar2, 0.005f, 0.01f, 0.99f, "%.2f");
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("##EffectCurvePreview", ImVec2(0.0f, 0.0f), true)) {
                const auto drawCurvePanel = [&](const char* label,
                                                float startValue,
                                                float endValue,
                                                float bias,
                                                ImU32 curveColor,
                                                float minValue,
                                                float maxValue,
                                                float indicatorValue) {
                    ImGui::TextUnformatted(label);
                    const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 148.0f);
                    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    ImGui::InvisibleButton(label, canvasSize);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const ImVec2 canvasMax(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
                    drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(24, 26, 31, 255), 4.0f);
                    drawList->AddRect(canvasPos, canvasMax, IM_COL32(58, 64, 76, 255), 4.0f);

                    for (int grid = 1; grid < 4; ++grid) {
                        const float gx = canvasPos.x + (canvasSize.x * grid / 4.0f);
                        const float gy = canvasPos.y + (canvasSize.y * grid / 4.0f);
                        drawList->AddLine(ImVec2(gx, canvasPos.y), ImVec2(gx, canvasMax.y), IM_COL32(40, 44, 52, 255));
                        drawList->AddLine(ImVec2(canvasPos.x, gy), ImVec2(canvasMax.x, gy), IM_COL32(40, 44, 52, 255));
                    }

                    ImVec2 prev = canvasPos;
                    for (int i = 0; i <= 64; ++i) {
                        const float t = static_cast<float>(i) / 64.0f;
                        const float eased = easeSample(t, bias);
                        const float value = lerpSample(startValue, endValue, eased);
                        const float normalized = (maxValue - minValue) > 0.0001f
                            ? std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f)
                            : 0.0f;
                        const ImVec2 point(
                            canvasPos.x + t * canvasSize.x,
                            canvasMax.y - normalized * canvasSize.y);
                        if (i > 0) {
                            drawList->AddLine(prev, point, curveColor, 2.0f);
                        }
                        prev = point;
                    }

                    const float indicatorX = canvasPos.x + sampleT * canvasSize.x;
                    drawList->AddLine(ImVec2(indicatorX, canvasPos.y), ImVec2(indicatorX, canvasMax.y), IM_COL32(255, 120, 70, 255), 2.0f);
                    const float indicatorNorm = (maxValue - minValue) > 0.0001f
                        ? std::clamp((indicatorValue - minValue) / (maxValue - minValue), 0.0f, 1.0f)
                        : 0.0f;
                    const ImVec2 indicatorPos(indicatorX, canvasMax.y - indicatorNorm * canvasSize.y);
                    drawList->AddCircleFilled(indicatorPos, 4.0f, IM_COL32(255, 220, 120, 255));
                };

                const float sizeSample = lerpSample(emitterNode->vectorValue.y, emitterNode->vectorValue.z, easeSample(sampleT, spriteNode->vectorValue3.x));
                drawCurvePanel(
                    "Size Curve",
                    emitterNode->vectorValue.y,
                    emitterNode->vectorValue.z,
                    spriteNode->vectorValue3.x,
                    IM_COL32(98, 194, 255, 255),
                    0.0f,
                    (std::max)(emitterNode->vectorValue.y, emitterNode->vectorValue.z) * 1.15f + 0.05f,
                    sizeSample);

                ImGui::Spacing();
                ImGui::TextUnformatted("Color Gradient");
                const ImVec2 gradientSize(ImGui::GetContentRegionAvail().x, 40.0f);
                const ImVec2 gradientPos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##EffectColorGradient", gradientSize);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 gradientMax(gradientPos.x + gradientSize.x, gradientPos.y + gradientSize.y);
                drawList->AddRectFilledMultiColor(
                    gradientPos,
                    gradientMax,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(colorNode->vectorValue.x, colorNode->vectorValue.y, colorNode->vectorValue.z, 1.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(colorNode->vectorValue2.x, colorNode->vectorValue2.y, colorNode->vectorValue2.z, 1.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(colorNode->vectorValue2.x, colorNode->vectorValue2.y, colorNode->vectorValue2.z, 1.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(colorNode->vectorValue.x, colorNode->vectorValue.y, colorNode->vectorValue.z, 1.0f)));
                drawList->AddRect(gradientPos, gradientMax, IM_COL32(64, 68, 78, 255), 4.0f);
                const float gradientX = gradientPos.x + sampleT * gradientSize.x;
                drawList->AddLine(ImVec2(gradientX, gradientPos.y), ImVec2(gradientX, gradientMax.y), IM_COL32(255, 120, 70, 255), 2.0f);

                ImGui::Spacing();
                const float alphaScale = (spriteNode->vectorValue2.z > 0.0f) ? spriteNode->vectorValue2.z : 1.0f;
                const float alphaSample = lerpSample(colorNode->vectorValue.w, colorNode->vectorValue2.w, easeSample(sampleT, spriteNode->vectorValue3.y)) * alphaScale;
                drawCurvePanel(
                    "Alpha Curve",
                    colorNode->vectorValue.w * alphaScale,
                    colorNode->vectorValue2.w * alphaScale,
                    spriteNode->vectorValue3.y,
                    IM_COL32(255, 196, 96, 255),
                    0.0f,
                    1.1f,
                    alphaSample);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_SLIDERS " Parameters")) {
            if (m_asset.exposedParameters.empty()) {
                ImGui::TextDisabled("No exposed parameters. Add parameters in the Blackboard.");
            } else {
                auto* overrideComponent = (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity))
                    ? m_registry->GetComponent<EffectParameterOverrideComponent>(m_previewEntity)
                    : nullptr;

                if (overrideComponent) {
                    if (overrideComponent->scalarNames.size() != m_asset.exposedParameters.size()) {
                        overrideComponent->scalarNames.clear();
                        overrideComponent->scalarValues.clear();
                        overrideComponent->colorNames.clear();
                        overrideComponent->colorValues.clear();
                        for (const auto& param : m_asset.exposedParameters) {
                            if (param.valueType == EffectValueType::Float) {
                                overrideComponent->scalarNames.push_back(param.name);
                                overrideComponent->scalarValues.push_back(param.defaultValue.x);
                            } else if (param.valueType == EffectValueType::Color) {
                                overrideComponent->colorNames.push_back(param.name);
                                overrideComponent->colorValues.push_back(param.defaultValue);
                            }
                        }
                        overrideComponent->enabled = true;
                    }

                    ImGui::TextDisabled("Drag sliders to adjust parameters in real-time");
                    ImGui::Separator();

                    for (size_t i = 0; i < overrideComponent->scalarNames.size() && i < overrideComponent->scalarValues.size(); ++i) {
                        const std::string& name = overrideComponent->scalarNames[i];
                        ImGui::DragFloat(name.c_str(), &overrideComponent->scalarValues[i], 0.01f, 0.0f, 100.0f, "%.3f");
                        if (const char* desc = EffectParameterBindings::DescribeBinding(EffectValueType::Float, name)) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%s)", desc);
                        }
                    }
                    if (!overrideComponent->scalarNames.empty() && !overrideComponent->colorNames.empty()) {
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                    }
                    for (size_t i = 0; i < overrideComponent->colorNames.size() && i < overrideComponent->colorValues.size(); ++i) {
                        const std::string& name = overrideComponent->colorNames[i];
                        ImGui::ColorEdit4(name.c_str(), &overrideComponent->colorValues[i].x);
                        if (const char* desc = EffectParameterBindings::DescribeBinding(EffectValueType::Color, name)) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%s)", desc);
                        }
                    }
                } else {
                    ImGui::TextDisabled("Start preview to enable live parameter editing.");
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_FOLDER_OPEN " Presets")) {
            ImGui::TextDisabled("Save / Load effect presets");
            ImGui::Separator();

            if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save as Preset")) {
                std::filesystem::path presetDir = "Data/Effect/Presets";
                std::error_code ec;
                std::filesystem::create_directories(presetDir, ec);
                std::string presetName = m_asset.name.empty() ? "Untitled" : m_asset.name;
                std::string presetPath = (presetDir / (presetName + ".effectgraph.json")).string();
                EffectGraphSerializer::Save(presetPath, m_asset);
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_ARROWS_ROTATE " Refresh")) {
                // Trigger rescan on next frame
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Available presets:");

            std::error_code ec;
            const std::filesystem::path presetDir = "Data/Effect/Presets";
            if (std::filesystem::exists(presetDir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(presetDir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    const auto& path = entry.path();
                    if (path.extension() != ".json") continue;
                    std::string filename = path.stem().string();
                    // Remove .effectgraph suffix if present
                    if (filename.size() > 12 && filename.substr(filename.size() - 12) == ".effectgraph") {
                        filename = filename.substr(0, filename.size() - 12);
                    }
                    if (ImGui::Selectable(filename.c_str())) {
                        EffectGraphAsset loaded;
                        if (EffectGraphSerializer::Load(path.string(), loaded)) {
                            m_asset = std::move(loaded);
                            m_compileDirty = true;
                            m_syncNodePositions = true;
                            CompileDocument();
                            QueuePreviewSpawn();
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("No presets directory found. Save a preset to create it.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Niagara Log")) {
            DrawCompiledPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_CLOCK " Timeline")) {
            EffectPlaybackComponent* playback = nullptr;
            if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
                playback = m_registry->GetComponent<EffectPlaybackComponent>(m_previewEntity);
            }

            const bool hasPreviewPlayback = playback != nullptr;
            const float duration = SafeDuration(m_asset.previewDefaults.duration);
            float currentTime = hasPreviewPlayback ? playback->currentTime : 0.0f;
            currentTime = std::clamp(currentTime, 0.0f, duration);

            if (ImGui::Button(ICON_FA_BACKWARD_STEP " Restart")) {
                if (m_compileDirty || !m_compiled || !m_compiled->valid) {
                    CompileDocument();
                }
                QueuePreviewSpawn();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_PLAY " Play")) {
                if (!hasPreviewPlayback || m_compileDirty || playback->runtimeInstanceId == 0) {
                    CompileDocument();
                    QueuePreviewSpawn();
                } else {
                    playback->isPaused = false;
                    playback->isPlaying = true;
                    playback->stopRequested = false;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_PAUSE " Pause")) {
                if (hasPreviewPlayback) {
                    playback->isPaused = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_STOP " Stop")) {
                StopPreview();
                playback = nullptr;
                currentTime = 0.0f;
            }
            ImGui::SameLine();
            const char* stateLabel = "[Stopped]";
            ImU32 stateColor = IM_COL32(160, 160, 160, 255);
            if (playback) {
                if (playback->isPaused) {
                    stateLabel = "[Paused]";
                    stateColor = IM_COL32(255, 196, 60, 255);
                } else if (playback->isPlaying) {
                    stateLabel = "[Playing]";
                    stateColor = IM_COL32(110, 220, 140, 255);
                } else {
                    stateLabel = "[Idle]";
                    stateColor = IM_COL32(160, 160, 160, 255);
                }
            }
            ImGui::TextColored(ImColor(stateColor).Value, "%s", stateLabel);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::DragFloat("Duration", &m_asset.previewDefaults.duration, 0.01f, 0.10f, 120.0f, "%.2f s")) {
                m_compileDirty = true;
                if (playback) {
                    playback->duration = m_asset.previewDefaults.duration;
                }
            }
            ImGui::SameLine();
            ImGui::Checkbox("Scrub resumes play", &m_scrubResumesPlay);

            float scrubTime = currentTime;
            ImGui::SetNextItemWidth(-1.0f);
            const bool scrubChanged = ImGui::SliderFloat("##EffectTimelineScrub", &scrubTime, 0.0f, duration, "%.2f s");
            if (ImGui::IsItemActivated()) {
                m_scrubWasPlaying = (playback != nullptr) && playback->isPlaying && !playback->isPaused;
            }
            if (scrubChanged) {
                if (!playback) {
                    if (m_compileDirty || !m_compiled || !m_compiled->valid) {
                        CompileDocument();
                    }
                    QueuePreviewSpawnAt(scrubTime, true);
                    m_scrubWasPlaying = false;
                } else {
                    playback->currentTime = scrubTime;
                    playback->isPaused = true;
                    playback->stopRequested = false;
                    if (playback->runtimeInstanceId != 0) {
                        if (auto* runtime = EffectRuntimeRegistry::Instance().GetRuntimeInstance(playback->runtimeInstanceId)) {
                            runtime->time = scrubTime;
                        }
                    }
                }
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (m_scrubResumesPlay && m_scrubWasPlaying && playback) {
                    playback->isPaused = false;
                }
                m_scrubWasPlaying = false;
            }

            const float leftPaneWidth = 220.0f;
            const float rowHeight = 28.0f;
            const char* tracks[] = {
                "System Spawn",
                "Emitter Update",
                "Render Output"
            };

            ImGui::BeginChild("##EffectTimelineTrackLabels", ImVec2(leftPaneWidth, 0.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextDisabled("Tracks");
            ImGui::Separator();
            ImDrawList* trackDrawList = ImGui::GetWindowDrawList();
            for (int trackIndex = 0; trackIndex < static_cast<int>(IM_ARRAYSIZE(tracks)); ++trackIndex) {
                const ImVec2 rowPos = ImGui::GetCursorScreenPos();
                const ImVec2 rowSize(ImGui::GetContentRegionAvail().x, rowHeight);
                const ImVec2 rowMax(rowPos.x + rowSize.x, rowPos.y + rowSize.y);
                const ImU32 fill = (trackIndex % 2 == 0) ? IM_COL32(48, 50, 56, 255) : IM_COL32(42, 44, 49, 255);
                trackDrawList->AddRectFilled(rowPos, rowMax, fill);
                trackDrawList->AddText(ImVec2(rowPos.x + 10.0f, rowPos.y + 6.0f), IM_COL32(208, 212, 220, 255), tracks[trackIndex]);
                ImGui::Dummy(rowSize);
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("##EffectTimelineCanvas", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
            const ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
            const ImVec2 canvasMax(canvasMin.x + canvasAvail.x, canvasMin.y + canvasAvail.y);
            const float rulerHeight = 28.0f;
            const float usableHeight = (canvasAvail.y > rulerHeight) ? (canvasAvail.y - rulerHeight) : 0.0f;
            const float tickCount = 12.0f;

            drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(23, 25, 30, 255), 4.0f);
            drawList->AddRect(canvasMin, canvasMax, IM_COL32(62, 68, 80, 255), 4.0f);

            for (int tick = 0; tick <= static_cast<int>(tickCount); ++tick) {
                const float t = static_cast<float>(tick) / tickCount;
                const float x = canvasMin.x + t * canvasAvail.x;
                drawList->AddLine(ImVec2(x, canvasMin.y), ImVec2(x, canvasMax.y), IM_COL32(42, 46, 56, 255), 1.0f);

                char label[32];
                sprintf_s(label, "%.2f", duration * t);
                drawList->AddText(ImVec2(x + 4.0f, canvasMin.y + 4.0f), IM_COL32(180, 186, 198, 255), label);
            }

            for (int row = 0; row < static_cast<int>(IM_ARRAYSIZE(tracks)); ++row) {
                const float y0 = canvasMin.y + rulerHeight + row * rowHeight;
                const float y1 = y0 + rowHeight;
                const ImU32 fill = (row % 2 == 0) ? IM_COL32(24, 27, 33, 255) : IM_COL32(28, 31, 38, 255);
                drawList->AddRectFilled(ImVec2(canvasMin.x, y0), ImVec2(canvasMax.x, y1), fill);
                drawList->AddLine(ImVec2(canvasMin.x, y1), ImVec2(canvasMax.x, y1), IM_COL32(45, 50, 60, 255), 1.0f);
            }

            const float playheadRatio = scrubTime / duration;
            const float playheadX = canvasMin.x + playheadRatio * canvasAvail.x;
            drawList->AddLine(ImVec2(playheadX, canvasMin.y), ImVec2(playheadX, canvasMax.y), IM_COL32(255, 110, 60, 255), 2.0f);

            const float rangeStartX = canvasMin.x;
            const float rangeEndX = canvasMax.x;
            drawList->AddRectFilled(
                ImVec2(rangeStartX, canvasMin.y + rulerHeight + 1.0f),
                ImVec2(rangeEndX, canvasMin.y + rulerHeight + rowHeight - 2.0f),
                IM_COL32(60, 130, 220, 80));
            drawList->AddRectFilled(
                ImVec2(rangeStartX, canvasMin.y + rulerHeight + rowHeight + 1.0f),
                ImVec2(rangeEndX, canvasMin.y + rulerHeight + rowHeight * 2.0f - 2.0f),
                IM_COL32(60, 200, 120, 72));

            if (m_compiled && (m_compiled->meshRenderer.enabled || m_compiled->particleRenderer.enabled)) {
                const ImU32 renderColor = m_compiled->particleRenderer.enabled ? IM_COL32(214, 126, 255, 80) : IM_COL32(255, 196, 60, 80);
                drawList->AddRectFilled(
                    ImVec2(rangeStartX, canvasMin.y + rulerHeight + rowHeight * 2.0f + 1.0f),
                    ImVec2(rangeEndX, canvasMin.y + rulerHeight + rowHeight * 3.0f - 2.0f),
                    renderColor);
            }

            const float canvasHeight = (usableHeight > rowHeight * 3.0f) ? usableHeight : (rowHeight * 3.0f);
            ImGui::InvisibleButton("##EffectTimelineCanvasArea", ImVec2(canvasAvail.x, canvasHeight));
            if (ImGui::IsItemActivated()) {
                m_scrubWasPlaying = (playback != nullptr) && playback->isPlaying && !playback->isPaused;
            }
            if (ImGui::IsItemActive() && canvasAvail.x > 0.0f) {
                const ImVec2 mousePos = ImGui::GetIO().MousePos;
                const float mouseRatio = std::clamp((mousePos.x - canvasMin.x) / canvasAvail.x, 0.0f, 1.0f);
                const float newTime = mouseRatio * duration;
                if (!playback) {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        if (m_compileDirty || !m_compiled || !m_compiled->valid) {
                            CompileDocument();
                        }
                        QueuePreviewSpawnAt(newTime, true);
                        m_scrubWasPlaying = false;
                    }
                } else {
                    playback->currentTime = newTime;
                    playback->isPaused = true;
                    playback->stopRequested = false;
                    if (playback->runtimeInstanceId != 0) {
                        if (auto* runtime = EffectRuntimeRegistry::Instance().GetRuntimeInstance(playback->runtimeInstanceId)) {
                            runtime->time = newTime;
                        }
                    }
                }
            }
            if (ImGui::IsItemDeactivated()) {
                if (m_scrubResumesPlay && m_scrubWasPlaying && playback) {
                    playback->isPaused = false;
                }
                m_scrubWasPlaying = false;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
