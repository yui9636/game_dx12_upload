#include "TerrainEditorPanel.h"
#include "Terrain/TerrainComponent.h"
#include "Terrain/TerrainAsset.h"
#include "Registry/Registry.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Engine/EditorSelection.h"
#include "Vegetation/GrassComponent.h"
#include "Terrain/TerrainGpuPipeline.h"
#include "Render/Graphics.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

EntityID CreateDefaultTerrain(Registry& registry)
{
    EntityID entity = registry.CreateEntity();
    registry.AddComponent(entity, NameComponent{ "Terrain" });
    registry.AddComponent(entity, TransformComponent{});
    registry.AddComponent(entity, HierarchyComponent{});

    TerrainComponent terrain;
    terrain.asset = std::make_shared<TerrainAsset>();
    terrain.asset->EnsureDefaultLayers();

    // Default = Flat Plains: gentle grass + dirt with minimal relief so the
    // player has a walkable surface immediately on creation. Other landscapes
    // can be applied via the Quick Presets buttons in the editor.
    auto& a = *terrain.asset;
    a.noiseType = 1;             // Simplex (no Cellular spikes)
    a.noiseFreq = 0.0025f;       // low frequency for broad gentle features
    a.octaves = 3;
    a.lacunarity = 2.0f;
    a.gain = 0.42f;
    a.domainWarpStrength = 25.0f;
    a.terraceSteps = 0.0f;
    a.heightScale = 14.0f;       // shallow vertical range -> walkable
    a.autoSplat.rockAltitudeMin = 0.92f;  // rock only at top fringe
    a.autoSplat.rockSlopeDegrees = 60.0f;
    a.autoSplat.dirtMidAltitude = 0.55f;
    a.autoSplat.dirtStrength = 0.40f;
    a.water.enabled = false;     // no water in the default flat plain

    // Noise + auto-splat only. Skip erosion for the default to keep it tame.
    TerrainGpuPipeline::Instance().Run(
        a,
        TerrainGpuPipeline::StageNoise | TerrainGpuPipeline::StageAutoSplat);
    terrain.needsRebuild = true;
    registry.AddComponent(entity, terrain);

    // Attach a default foliage component (3 layer multi-foliage out of the box).
    GrassComponent grass;
    grass.enabled = true;
    grass.needsRebuild = true;
    grass.EnsureDefaultLayers();
    registry.AddComponent(entity, grass);

    EditorSelection::Instance().SelectEntity(entity);
    return entity;
}

bool BrushModeButton(const char* label, TerrainBrush& brush, TerrainBrush::Mode mode, float width)
{
    const bool selected = brush.mode == mode;
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.42f, 0.78f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.50f, 0.90f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.36f, 0.68f, 1.0f));
    }

    const bool pressed = ImGui::Button(label, ImVec2(width, 30.0f));
    if (pressed) {
        brush.mode = mode;
    }

    if (selected) {
        ImGui::PopStyleColor(3);
    }
    return pressed;
}

void DrawCompactMetric(const char* label, const char* value)
{
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine();
    ImGui::TextUnformatted(value);
}

void RegenerateTerrain(TerrainAsset& asset, TerrainComponent& tc)
{
    asset.GenerateFromNoise();
    if (asset.water.enabled) {
        asset.water.seaLevel = asset.SuggestVisibleWaterLevel();
    }
    asset.GenerateAutoSplat(asset.autoSplat);
    tc.needsRebuild = true;
}

} // 無名名前空間

void TerrainEditorPanel::Draw(Registry& registry, EntityID selectedEntity)
{
    ImGui::Begin("Terrain Editor");

    if (Entity::IsNull(selectedEntity)) {
        ImGui::TextDisabled("Select a Terrain entity in the Hierarchy.");
        if (ImGui::Button("Create Terrain", ImVec2(-1, 0))) {
            selectedEntity = CreateDefaultTerrain(registry);
        }
        if (Entity::IsNull(selectedEntity)) {
            ImGui::End();
            return;
        }
    }

    if (!registry.IsAlive(selectedEntity)) {
        ImGui::End();
        return;
    }

    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(selectedEntity);
    if (!tc) {
        ImGui::TextDisabled("Selected entity has no TerrainComponent.");
        if (ImGui::Button("Create Terrain", ImVec2(-1, 0))) {
            selectedEntity = CreateDefaultTerrain(registry);
            tc = registry.GetComponent<TerrainComponent>(selectedEntity);
        }
        if (!tc) {
            ImGui::End();
            return;
        }
    }

    if (!tc->asset) {
        tc->asset = std::make_shared<TerrainAsset>();
        tc->asset->GenerateFromNoise();
        tc->asset->EnsureDefaultLayers();
        tc->asset->SetupDefaultWater();
        tc->asset->GenerateAutoSplat(tc->asset->autoSplat);
        tc->needsRebuild = true;
    }

    if (tc->asset->heightData.empty()) {
        tc->asset->GenerateFromNoise();
        tc->needsRebuild = true;
    }

    if (tc->asset->UpgradeLegacyDefaultWorldSize()) {
        tc->needsRebuild = true;
    }

    auto& asset = *tc->asset;
    if (asset.layers.empty()) {
        asset.EnsureDefaultLayers();
        tc->needsRebuild = true;
    }

    const int layerCount = (std::max)(static_cast<int>(asset.layers.size()), 1);
    m_brush.layerIndex = std::clamp(m_brush.layerIndex, 0, layerCount - 1);

    const size_t chunkEstimate =
        static_cast<size_t>(asset.chunkCountX) * static_cast<size_t>(asset.chunkCountZ);

    const char* kNoiseTypeNames[] = { "Hybrid", "Simplex", "Cellular" };
    const int   noiseTypeNameIdx = std::clamp(asset.noiseType, 0, 2);
    char terrainSummary[160]{};
    std::snprintf(
        terrainSummary,
        sizeof(terrainSummary),
        "%.0fx%.0fm  H%.0f  R%u  Chunks %zu  Noise:%s  Seed:%d  Water:%s",
        asset.worldSizeX,
        asset.worldSizeZ,
        asset.heightScale,
        asset.resolution,
        chunkEstimate,
        kNoiseTypeNames[noiseTypeNameIdx],
        asset.seed,
        asset.water.enabled ? "on" : "off");
    DrawCompactMetric("Terrain", terrainSummary);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "World size in meters / Heightscale / Resolution / Chunk count / Noise type / Seed / Water state");
    }
    if (tc->needsRebuild) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.76f, 0.28f, 1.0f), "Rebuild pending");
    }

    ImGui::Spacing();
    if (ImGui::BeginTable(
        "TerrainEditorOneScreen",
        2,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings,
        ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn("BrushColumn", ImGuiTableColumnFlags_WidthStretch, 0.62f);
        ImGui::TableSetupColumn("SupportColumn", ImGuiTableColumnFlags_WidthStretch, 0.38f);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::BeginChild("BrushPanel", ImVec2(0.0f, 0.0f), true);
        ImGui::TextUnformatted("Brush");
        ImGui::SameLine();
        ImGui::Checkbox("Active", &m_sceneBrushEnabled);

        const float modeSpacing = ImGui::GetStyle().ItemSpacing.x;
        const float modeWidth = (std::max)(
            56.0f,
            (ImGui::GetContentRegionAvail().x - modeSpacing * 4.0f) / 5.0f);
        if (BrushModeButton("Raise", m_brush, TerrainBrush::Mode::Raise, modeWidth)) m_sceneBrushEnabled = true;
        ImGui::SameLine();
        if (BrushModeButton("Lower", m_brush, TerrainBrush::Mode::Lower, modeWidth)) m_sceneBrushEnabled = true;
        ImGui::SameLine();
        if (BrushModeButton("Smooth", m_brush, TerrainBrush::Mode::Smooth, modeWidth)) m_sceneBrushEnabled = true;
        ImGui::SameLine();
        if (BrushModeButton("Flat", m_brush, TerrainBrush::Mode::Flatten, modeWidth)) m_sceneBrushEnabled = true;
        ImGui::SameLine();
        if (BrushModeButton("Paint", m_brush, TerrainBrush::Mode::Paint, modeWidth)) m_sceneBrushEnabled = true;

        ImGui::Spacing();
        if (ImGui::BeginTable("BrushSliders", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
            ImGui::TableSetupColumn("Main", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("Sub", ImGuiTableColumnFlags_WidthStretch, 0.5f);

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::SliderFloat("Radius", &m_brush.radius, 0.5f, 200.0f, "%.1f");
            ImGui::SliderFloat("Strength", &m_brush.strength, 0.0f, 1.0f, "%.2f");
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            ImGui::SliderFloat("Falloff", &m_brush.falloff, 0.0f, 4.0f, "%.2f");
            if (m_brush.mode == TerrainBrush::Mode::Flatten) {
                ImGui::SliderFloat("Flat Height", &m_brush.targetHeight, 0.0f, 1.0f, "%.2f");
            } else {
                ImGui::BeginDisabled(m_brush.mode != TerrainBrush::Mode::Paint);
                ImGui::SliderInt("Paint Layer", &m_brush.layerIndex, 0, layerCount - 1);
                ImGui::EndDisabled();
            }
            ImGui::PopItemWidth();
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Splat Paint (click to select a layer, then paint in scene view)");
        const float layerWidth = (std::max)(
            76.0f,
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f);
        // Suffix "##splat" disambiguates from any future button using the same words.
        const char* layerLabels[] = { "Grass##splat", "Dirt##splat", "Rock##splat" };
        const ImVec4 layerColors[] = {
            ImVec4(0.26f, 0.55f, 0.30f, 1.0f),   // grass = green
            ImVec4(0.55f, 0.39f, 0.22f, 1.0f),   // dirt = brown
            ImVec4(0.45f, 0.45f, 0.50f, 1.0f),   // rock = grey
        };
        for (int i = 0; i < (std::min)(layerCount, 3); ++i) {
            if (i > 0) ImGui::SameLine();
            const bool selected = m_brush.mode == TerrainBrush::Mode::Paint && m_brush.layerIndex == i;
            const ImVec4 baseCol = layerColors[i];
            const ImVec4 dim = ImVec4(baseCol.x * 0.55f, baseCol.y * 0.55f, baseCol.z * 0.55f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        selected ? baseCol : dim);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, baseCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  baseCol);
            if (ImGui::Button(layerLabels[i], ImVec2(layerWidth, 28.0f))) {
                m_brush.mode = TerrainBrush::Mode::Paint;
                m_brush.layerIndex = i;
                m_sceneBrushEnabled = true;
            }
            ImGui::PopStyleColor(3);
        }

        // Foliage / multi-layer vegetation scattering.
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Foliage (Multi-Layer)", ImGuiTreeNodeFlags_DefaultOpen)) {
            GrassComponent* gc = registry.GetComponent<GrassComponent>(selectedEntity);
            if (!gc) {
                ImGui::TextDisabled("No foliage component on this terrain.");
                if (ImGui::Button("Enable Foliage", ImVec2(-1, 28.0f))) {
                    GrassComponent newGrass;
                    newGrass.enabled = true;
                    newGrass.needsRebuild = true;
                    newGrass.EnsureDefaultLayers();
                    registry.AddComponent(selectedEntity, newGrass);
                }
            } else {
                bool foliageChanged = false;
                foliageChanged |= ImGui::Checkbox("Enabled##foliage", &gc->enabled);
                ImGui::PushItemWidth(-100.0f);
                foliageChanged |= ImGui::SliderFloat("Draw Dist", &gc->drawDistance, 10.0f, 500.0f, "%.0f");
                foliageChanged |= ImGui::DragFloat3("Wind Dir",   &gc->windDirection.x, 0.01f, -1.0f, 1.0f);
                ImGui::PopItemWidth();

                // Totals across layers
                uint32_t total = 0;
                for (const auto& l : gc->layers) total += l.lastInstanceCount;
                ImGui::Text("Total instances: %u (%d layers)", total, (int)gc->layers.size());
                if (gc->needsRebuild) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.95f, 0.76f, 0.28f, 1.0f), " (rebuild pending)");
                }

                // Layer list
                ImGui::Separator();
                if (ImGui::Button("+ Add Layer", ImVec2(120, 22))) {
                    gc->layers.emplace_back();
                    foliageChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset Defaults", ImVec2(140, 22))) {
                    gc->layers.clear();
                    gc->EnsureDefaultLayers();
                    foliageChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Force Rebuild", ImVec2(140, 22))) {
                    foliageChanged = true;
                }

                for (int li = 0; li < (int)gc->layers.size(); ++li) {
                    FoliageLayer& layer = gc->layers[li];
                    ImGui::PushID(li);
                    char header[128];
                    std::snprintf(header, sizeof(header), "[%d] %s  (instances=%u)",
                                  li, layer.name.c_str(), layer.lastInstanceCount);
                    if (ImGui::CollapsingHeader(header)) {
                        foliageChanged |= ImGui::Checkbox("Enabled", &layer.enabled);
                        ImGui::SameLine();
                        if (ImGui::Button("Remove", ImVec2(80, 20))) {
                            gc->layers.erase(gc->layers.begin() + li);
                            foliageChanged = true;
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PushItemWidth(-110.0f);
                        char nameBuf[64];
                        strncpy_s(nameBuf, layer.name.c_str(), sizeof(nameBuf) - 1);
                        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                            layer.name = nameBuf;
                            foliageChanged = true;
                        }
                        char meshBuf[256];
                        strncpy_s(meshBuf, layer.meshPath.c_str(), sizeof(meshBuf) - 1);
                        if (ImGui::InputText("Model", meshBuf, sizeof(meshBuf))) {
                            layer.meshPath = meshBuf;
                            foliageChanged = true;
                        }
                        const char* channelNames[] = { "R (Grass)", "G (Dirt)", "B (Rock)", "A", "Any" };
                        int channelIdx = (layer.splatChannel < 0 || layer.splatChannel > 3) ? 4 : layer.splatChannel;
                        if (ImGui::Combo("Splat Ch", &channelIdx, channelNames, 5)) {
                            layer.splatChannel = (channelIdx == 4) ? -1 : channelIdx;
                            foliageChanged = true;
                        }
                        foliageChanged |= ImGui::SliderFloat("Density",      &layer.densityMultiplier, 0.0f, 4.0f, "%.2f");
                        int maxPer = (int)layer.maxPerCell;
                        if (ImGui::SliderInt("Max/Cell", &maxPer, 1, 16)) {
                            layer.maxPerCell = (uint32_t)maxPer;
                            foliageChanged = true;
                        }
                        foliageChanged |= ImGui::SliderFloat("Threshold",    &layer.densityThreshold, 0.0f, 1.0f, "%.2f");
                        foliageChanged |= ImGui::SliderFloat("Size",         &layer.sizeScale,        0.05f, 6.0f, "%.2f");
                        foliageChanged |= ImGui::SliderFloat("Size Var",     &layer.sizeVariance,     0.0f, 1.0f, "%.2f");
                        foliageChanged |= ImGui::SliderFloat("Min Alt",      &layer.minAltitudeNorm,  0.0f, 1.0f, "%.2f");
                        foliageChanged |= ImGui::SliderFloat("Max Alt",      &layer.maxAltitudeNorm,  0.0f, 1.0f, "%.2f");
                        foliageChanged |= ImGui::SliderFloat("Max Slope deg",&layer.maxSlopeDegrees,  0.0f, 90.0f, "%.0f");
                        foliageChanged |= ImGui::Checkbox("Use Wind",        &layer.useWind);
                        if (layer.useWind) {
                            foliageChanged |= ImGui::SliderFloat("Wind Str", &layer.windStrength, 0.0f, 1.0f, "%.2f");
                            foliageChanged |= ImGui::SliderFloat("Wind Spd", &layer.windSpeed,    0.0f, 4.0f, "%.2f");
                        }
                        foliageChanged |= ImGui::ColorEdit3("Color Top",     &layer.colorTop.x);
                        foliageChanged |= ImGui::ColorEdit3("Color Bottom",  &layer.colorBottom.x);
                        ImGui::PopItemWidth();
                    }
                    ImGui::PopID();
                }

                if (foliageChanged) gc->needsRebuild = true;
            }
        }

        // PBR layer texture paths (collapsing for compactness)
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("PBR Layer Textures", ImGuiTreeNodeFlags_None)) {
            for (int i = 0; i < (std::min)(layerCount, 3); ++i) {
                ImGui::PushID(i);
                if (i < static_cast<int>(asset.layers.size())) {
                    auto& layer = asset.layers[i];
                    ImGui::Separator();
                    ImGui::Text("%s (Layer %d)", layerLabels[i], i);
                    char albedoBuf[256];
                    char normalBuf[256];
                    char mraBuf[256];
                    strncpy_s(albedoBuf, layer.albedoPath.c_str(),    sizeof(albedoBuf) - 1);
                    strncpy_s(normalBuf, layer.normalPath.c_str(),    sizeof(normalBuf) - 1);
                    strncpy_s(mraBuf,    layer.roughnessPath.c_str(), sizeof(mraBuf)    - 1);
                    ImGui::PushItemWidth(-80.0f);
                    if (ImGui::InputText("Albedo", albedoBuf, sizeof(albedoBuf))) {
                        layer.albedoPath = albedoBuf;
                        tc->needsRebuild = true;
                    }
                    if (ImGui::InputText("Normal", normalBuf, sizeof(normalBuf))) {
                        layer.normalPath = normalBuf;
                        tc->needsRebuild = true;
                    }
                    if (ImGui::InputText("MRA (M/R/AO)", mraBuf, sizeof(mraBuf))) {
                        layer.roughnessPath = mraBuf;
                        tc->needsRebuild = true;
                    }
                    if (ImGui::DragFloat("Tile", &layer.tileScale, 0.1f, 0.1f, 64.0f, "%.1f")) {
                        tc->needsRebuild = true;
                    }
                    ImGui::PopItemWidth();
                }
                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("TerrainSetupPanel", ImVec2(0.0f, 0.0f), true);

        // -- Quick Presets --------------------------------------------------
        ImGui::TextUnformatted("Quick Presets");
        auto runPipeline = [&](uint32_t stages) {
            TerrainGpuPipeline::Instance().Run(asset, stages);
            tc->needsRebuild = true;
        };
        auto applyPreset = [&](auto&& setup, uint32_t stages) {
            setup(asset);
            runPipeline(stages);
        };
        const ImVec2 presetBtn(-1.0f, 24.0f);
        const ImVec2 halfBtn((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f, 24.0f);

        if (ImGui::Button("Flat Plains", halfBtn)) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 1;            // Simplex
                a.noiseFreq = 0.0025f;
                a.octaves = 3;
                a.lacunarity = 2.0f;
                a.gain = 0.42f;
                a.domainWarpStrength = 25.0f;
                a.terraceSteps = 0.0f;
                a.heightScale = 14.0f;
                a.autoSplat = { 0.92f, 60.0f, 0.55f, 0.40f };
                a.water.enabled = false;
            }, TerrainGpuPipeline::StageNoise | TerrainGpuPipeline::StageAutoSplat);
        }
        ImGui::SameLine();
        if (ImGui::Button("Rolling Hills", halfBtn)) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 1;
                a.noiseFreq = 0.0035f;
                a.octaves = 4;
                a.lacunarity = 2.0f;
                a.gain = 0.5f;
                a.domainWarpStrength = 60.0f;
                a.terraceSteps = 0.0f;
                a.heightScale = 32.0f;
                a.autoSplat = { 0.78f, 38.0f, 0.5f, 0.45f };
            }, TerrainGpuPipeline::StageAll);
        }
        if (ImGui::Button("Mountain Range", halfBtn)) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 0;            // Hybrid
                a.noiseFreq = 0.0045f;
                a.octaves = 5;
                a.lacunarity = 2.2f;
                a.gain = 0.5f;
                a.domainWarpStrength = 100.0f;
                a.terraceSteps = 0.0f;
                a.heightScale = 80.0f;
                a.autoSplat = { 0.55f, 28.0f, 0.42f, 0.40f };
                a.erosion.iterations = 80000;
            }, TerrainGpuPipeline::StageAll);
        }
        ImGui::SameLine();
        if (ImGui::Button("Plateau / Mesa", halfBtn)) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 1;
                a.noiseFreq = 0.003f;
                a.octaves = 4;
                a.lacunarity = 2.0f;
                a.gain = 0.5f;
                a.domainWarpStrength = 50.0f;
                a.terraceSteps = 8.0f;
                a.heightScale = 55.0f;
                a.autoSplat = { 0.7f, 45.0f, 0.45f, 0.4f };
            }, TerrainGpuPipeline::StageNoise | TerrainGpuPipeline::StageAutoSplat);
        }
        if (ImGui::Button("Lake Basin", halfBtn)) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 1;
                a.noiseFreq = 0.0028f;
                a.octaves = 4;
                a.lacunarity = 2.0f;
                a.gain = 0.5f;
                a.domainWarpStrength = 70.0f;
                a.terraceSteps = 0.0f;
                a.heightScale = 30.0f;
                a.water.enabled = true;
                a.autoSplat = { 0.85f, 50.0f, 0.4f, 0.5f };
            }, TerrainGpuPipeline::StageAll);
        }
        ImGui::SameLine();
        if (ImGui::Button("Rocky Wasteland", halfBtn)) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 2;            // Cellular
                a.noiseFreq = 0.006f;
                a.octaves = 4;
                a.lacunarity = 2.0f;
                a.gain = 0.5f;
                a.domainWarpStrength = 50.0f;
                a.terraceSteps = 0.0f;
                a.heightScale = 50.0f;
                a.autoSplat = { 0.35f, 22.0f, 0.35f, 0.35f };
                a.water.enabled = false;
            }, TerrainGpuPipeline::StageAll);
        }
        if (ImGui::Button("Archipelago", halfBtn)) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 1;
                a.noiseFreq = 0.0045f;
                a.octaves = 4;
                a.lacunarity = 2.0f;
                a.gain = 0.55f;
                a.domainWarpStrength = 90.0f;
                a.terraceSteps = 0.0f;
                a.heightScale = 35.0f;
                a.water.enabled = true;
                a.water.seaLevel = 2.0f;   // high sea -> islands
                a.autoSplat = { 0.8f, 45.0f, 0.4f, 0.5f };
            }, TerrainGpuPipeline::StageAll);
        }
        ImGui::SameLine();
        if (ImGui::Button("Volcanic Crater", halfBtn)) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 0;
                a.noiseFreq = 0.005f;
                a.octaves = 5;
                a.lacunarity = 2.3f;
                a.gain = 0.55f;
                a.domainWarpStrength = 60.0f;
                a.terraceSteps = 0.0f;
                a.heightScale = 90.0f;
                a.autoSplat = { 0.45f, 25.0f, 0.4f, 0.5f };
                a.erosion.iterations = 100000;
            }, TerrainGpuPipeline::StageAll);
        }

        ImGui::Spacing();
        ImGui::Separator();

        // -- Dimensions / Mesh ----------------------------------------------
        ImGui::TextUnformatted("Dimensions");
        ImGui::PushItemWidth(-110.0f);
        bool dimsChanged = false;
        dimsChanged |= ImGui::DragFloat("Width##dim",   &asset.worldSizeX, 1.0f, 1.0f, 8192.0f, "%.0f m");
        dimsChanged |= ImGui::DragFloat("Depth##dim",   &asset.worldSizeZ, 1.0f, 1.0f, 8192.0f, "%.0f m");
        dimsChanged |= ImGui::DragFloat("Height##dim",  &asset.heightScale, 1.0f, 1.0f, 512.0f, "%.0f m");
        int resInt = (int)asset.resolution;
        if (ImGui::SliderInt("Resolution##dim", &resInt, 64, 1024)) {
            asset.resolution = (uint32_t)std::clamp(resInt, 64, 1024);
            dimsChanged = true;
        }
        int cx = (int)asset.chunkCountX, cz = (int)asset.chunkCountZ;
        if (ImGui::SliderInt("Chunks X##dim", &cx, 1, 16)) { asset.chunkCountX = (uint32_t)cx; dimsChanged = true; }
        if (ImGui::SliderInt("Chunks Z##dim", &cz, 1, 16)) { asset.chunkCountZ = (uint32_t)cz; dimsChanged = true; }
        ImGui::PopItemWidth();
        if (dimsChanged) {
            tc->needsRebuild = true;
        }

        // -- Action buttons -------------------------------------------------
        const float halfActW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Regenerate (All)", ImVec2(halfActW, 28.0f))) {
            runPipeline(TerrainGpuPipeline::StageAll);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("GPU: Noise + Erosion + AutoSplat");
        ImGui::SameLine();
        if (ImGui::Button("Retexture##act", ImVec2(halfActW, 28.0f))) {
            runPipeline(TerrainGpuPipeline::StageAutoSplat);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Re-runs AutoSplat only (uses existing heightmap)");

        if (ImGui::Button("Randomize Seed", ImVec2(halfActW, 24.0f))) {
            asset.seed = static_cast<int>((uint32_t)rand() ^ ((uint32_t)rand() << 15));
            asset.erosion.seed = asset.seed + 7919;
            runPipeline(TerrainGpuPipeline::StageAll);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Flat##act", ImVec2(halfActW, 24.0f))) {
            applyPreset([](TerrainAsset& a) {
                a.noiseType = 1;
                a.noiseFreq = 0.0025f;
                a.octaves = 3;
                a.lacunarity = 2.0f;
                a.gain = 0.42f;
                a.domainWarpStrength = 25.0f;
                a.terraceSteps = 0.0f;
                a.heightScale = 14.0f;
                a.autoSplat = { 0.92f, 60.0f, 0.55f, 0.40f };
                a.water.enabled = false;
            }, TerrainGpuPipeline::StageNoise | TerrainGpuPipeline::StageAutoSplat);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Resets the terrain to the default Flat Plains preset");

        // -- Noise / Shape (GPU compute) ------------------------------------
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Terrain Noise", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushItemWidth(-100.0f);
            const char* kNoiseTypes[] = { "Hybrid (Simplex + Cellular)", "Simplex", "Cellular" };
            int noiseTypeIdx = std::clamp(asset.noiseType, 0, 2);
            if (ImGui::Combo("Noise Type##n", &noiseTypeIdx, kNoiseTypes, 3)) {
                asset.noiseType = noiseTypeIdx;
            }
            ImGui::SliderFloat("Frequency##n",  &asset.noiseFreq,  0.0001f, 0.05f, "%.4f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cycles per meter. Low = big features, High = small features.");
            ImGui::SliderInt("Octaves##n",      &asset.octaves,    1, 8);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("More octaves = more detail but slower. 3-5 is typical.");
            ImGui::SliderFloat("Lacunarity##n", &asset.lacunarity, 1.5f, 4.0f, "%.2f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Frequency multiplier between octaves. 2.0 standard.");
            ImGui::SliderFloat("Gain##n",       &asset.gain,       0.1f, 0.9f, "%.2f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Amplitude multiplier per octave. <0.5 smoother, >0.5 rougher.");
            ImGui::SliderFloat("Warp Strength (m)##n", &asset.domainWarpStrength, 0.0f, 250.0f, "%.0f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Domain warp magnitude in meters. Hides the underlying noise grid pattern.");
            ImGui::SliderFloat("Terrace Steps (0=off)##n", &asset.terraceSteps, 0.0f, 32.0f, "%.0f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Quantize height into N flat plateaus. 0 = smooth, 8-16 = mesa landscape.");
            ImGui::InputInt("Seed##n", &asset.seed);
            ImGui::PopItemWidth();
        }

        // -- Phase 1.5: Hydraulic Erosion -----------------------------------
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Erosion (Hydraulic)", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& ep = asset.erosion;
            ImGui::PushItemWidth(-100.0f);
            ImGui::SliderInt("Iterations",   &ep.iterations,        1000, 200000);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of water droplets simulated. More = stronger erosion.");
            ImGui::SliderInt("Radius",       &ep.erosionRadius,     1, 8);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Erosion brush radius in cells. Wider = smoother valleys.");
            ImGui::SliderInt("Lifetime",     &ep.maxDropletLifetime,5, 80);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Max steps a droplet can travel. Longer = deeper rivers.");
            ImGui::SliderFloat("Inertia",    &ep.inertia,           0.0f, 1.0f, "%.2f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much droplets keep their old direction. 0 = pure gravity, 1 = straight lines.");
            ImGui::SliderFloat("Capacity",   &ep.sedimentCapacityFactor, 0.5f, 16.0f, "%.2f");
            ImGui::SliderFloat("Erode Spd",  &ep.erodeSpeed,        0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Deposit Spd",&ep.depositSpeed,      0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Evaporate",  &ep.evaporateSpeed,    0.0f, 0.1f, "%.3f");
            ImGui::SliderFloat("Gravity",    &ep.gravity,           0.5f, 20.0f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::Button("Run Erosion (keeps current shape)", ImVec2(-1, 28.0f))) {
                asset.RunHydraulicErosion(ep);
                tc->needsRebuild = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Uploads existing heightmap, applies erosion, refreshes splat");
            ImGui::TextDisabled("All terrain ops run on GPU compute (~30-100ms).");
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Rules");
        ImGui::PushItemWidth(-1);
        bool rulesChanged = false;
        rulesChanged |= ImGui::SliderFloat("Rock Height", &asset.autoSplat.rockAltitudeMin, 0.0f, 1.0f, "%.2f");
        rulesChanged |= ImGui::SliderFloat("Rock Slope", &asset.autoSplat.rockSlopeDegrees, 0.0f, 89.0f, "%.0f");
        rulesChanged |= ImGui::SliderFloat("Dirt Height", &asset.autoSplat.dirtMidAltitude, 0.0f, 1.0f, "%.2f");
        rulesChanged |= ImGui::SliderFloat("Dirt Mix", &asset.autoSplat.dirtStrength, 0.0f, 1.0f, "%.2f");
        ImGui::PopItemWidth();
        if (rulesChanged) {
            asset.GenerateAutoSplat(asset.autoSplat);
            tc->needsRebuild = true;
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Water");
        bool waterChanged = false;
        bool waterShapeChanged = false;
        waterShapeChanged |= ImGui::Checkbox("Visible", &asset.water.enabled);
        ImGui::PushItemWidth(-1);
        waterShapeChanged |= ImGui::DragFloat("Level", &asset.water.seaLevel, 0.25f, -512.0f, 512.0f, "%.2f");
        waterChanged |= ImGui::DragFloat("Depth Fade", &asset.water.depthFade, 0.1f, 0.5f, 64.0f, "%.1f");
        waterChanged |= ImGui::DragFloat("Wave Size", &asset.water.waveScale, 0.001f, 0.0f, 0.10f, "%.3f");
        waterChanged |= ImGui::DragFloat("Wave Speed", &asset.water.waveSpeed, 0.01f, 0.0f, 3.0f, "%.2f");
        waterChanged |= ImGui::ColorEdit4("Shallow", &asset.water.shallowColor.x);
        waterChanged |= ImGui::ColorEdit4("Deep", &asset.water.deepColor.x);
        ImGui::PopItemWidth();
        if (ImGui::Button("Fit Water", ImVec2(-1, 28.0f))) {
            asset.water.enabled = true;
            asset.water.seaLevel = asset.SuggestVisibleWaterLevel();
            waterShapeChanged = true;
        }
        if (ImGui::Button("Natural Water", ImVec2(-1, 28.0f))) {
            asset.water.enabled = true;
            asset.ApplyNaturalWaterPreset();
            asset.water.seaLevel = asset.SuggestVisibleWaterLevel();
            waterShapeChanged = true;
        }
        if (waterShapeChanged) {
            asset.GenerateAutoSplat(asset.autoSplat);
            waterChanged = true;
        }
        if (waterChanged) {
            tc->needsRebuild = true;
        }

        ImGui::EndChild();
        ImGui::EndTable();
    }

    ImGui::End();
}

void TerrainEditorPanel::ApplyBrush(Registry& registry, EntityID entity,
                                     float worldHitX, float worldHitZ)
{
    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(entity);
    if (!tc || !tc->asset) return;
    auto& asset = *tc->asset;
    if (asset.heightData.empty()) return;

    const float halfW = asset.worldSizeX * 0.5f;
    const float halfD = asset.worldSizeZ * 0.5f;
    const float normX = (worldHitX + halfW) / asset.worldSizeX;
    const float normZ = (worldHitZ + halfD) / asset.worldSizeZ;

    const int px = static_cast<int>(normX * (asset.resolution - 1));
    const int pz = static_cast<int>(normZ * (asset.resolution - 1));
    const float cellSize = (asset.worldSizeX + asset.worldSizeZ) * 0.5f /
        static_cast<float>((std::max)(asset.resolution - 1u, 1u));
    const int radiusPx = static_cast<int>(m_brush.radius / (std::max)(cellSize, 0.001f)) + 1;

    const size_t expectedSplatSize =
        static_cast<size_t>(asset.resolution) * static_cast<size_t>(asset.resolution) * 4u;
    if (m_brush.mode == TerrainBrush::Mode::Paint && asset.splatData.size() != expectedSplatSize) {
        asset.splatData.assign(expectedSplatSize, 0);
        for (uint32_t i = 0; i < asset.resolution * asset.resolution; ++i) {
            asset.splatData[static_cast<size_t>(i) * 4u] = 255;
        }
    }

    for (int dz = -radiusPx; dz <= radiusPx; ++dz) {
        for (int dx = -radiusPx; dx <= radiusPx; ++dx) {
            int ix = px + dx, iz = pz + dz;
            if (ix < 0 || iz < 0 || ix >= static_cast<int>(asset.resolution) ||
                iz >= static_cast<int>(asset.resolution)) continue;

            float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz)) /
                         static_cast<float>(radiusPx);
            if (dist > 1.0f) continue;
            float w = (1.0f - dist) * m_brush.strength;
            if (m_brush.falloff > 0.0f) w *= std::pow(1.0f - dist, m_brush.falloff);

            float& h = asset.heightData[static_cast<size_t>(iz) * asset.resolution + ix];
            switch (m_brush.mode) {
            case TerrainBrush::Mode::Raise:   h = (std::min)(1.0f, h + w * 0.01f); break;
            case TerrainBrush::Mode::Lower:   h = (std::max)(0.0f, h - w * 0.01f); break;
            case TerrainBrush::Mode::Flatten: h += (m_brush.targetHeight - h) * w; break;
            case TerrainBrush::Mode::Smooth:
            {
                float sum = 0.0f;
                int count = 0;
                for (int sz = -1; sz <= 1; ++sz) {
                    for (int sx = -1; sx <= 1; ++sx) {
                        const int nx = std::clamp(ix + sx, 0, static_cast<int>(asset.resolution) - 1);
                        const int nz = std::clamp(iz + sz, 0, static_cast<int>(asset.resolution) - 1);
                        sum += asset.heightData[static_cast<size_t>(nz) * asset.resolution + nx];
                        ++count;
                    }
                }
                const float avg = count > 0 ? sum / static_cast<float>(count) : h;
                h += (avg - h) * w;
                break;
            }
            case TerrainBrush::Mode::Paint:
            {
                const int layer = std::clamp(m_brush.layerIndex, 0, 2);
                const size_t p = (static_cast<size_t>(iz) * asset.resolution + ix) * 4u;
                float weights[3] = {
                    asset.splatData[p + 0] / 255.0f,
                    asset.splatData[p + 1] / 255.0f,
                    asset.splatData[p + 2] / 255.0f
                };
                weights[layer] = std::clamp(weights[layer] + w * 0.08f, 0.0f, 1.0f);
                const float fade = 1.0f - w * 0.08f;
                for (int i = 0; i < 3; ++i) {
                    if (i != layer) {
                        weights[i] *= fade;
                    }
                }
                const float sum = (std::max)(weights[0] + weights[1] + weights[2], 0.0001f);
                asset.splatData[p + 0] = static_cast<uint8_t>(std::clamp(weights[0] / sum * 255.0f, 0.0f, 255.0f));
                asset.splatData[p + 1] = static_cast<uint8_t>(std::clamp(weights[1] / sum * 255.0f, 0.0f, 255.0f));
                asset.splatData[p + 2] = static_cast<uint8_t>(std::clamp(weights[2] / sum * 255.0f, 0.0f, 255.0f));
                asset.splatData[p + 3] = 0;
                break;
            }
            default: break;
            }
        }
    }
    // ペイントはスプラットテクスチャだけアップロードすれば描画に反映できる。
    // メッシュには触らないので毎フレームの重い RebuildEntity を回避する。
    if (m_brush.mode == TerrainBrush::Mode::Paint) {
        tc->needsSplatUpload = true;
    } else {
        tc->needsRebuild = true;
    }
}
