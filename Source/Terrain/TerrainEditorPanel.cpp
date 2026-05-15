#include "TerrainEditorPanel.h"
#include "Terrain/TerrainComponent.h"
#include "Terrain/TerrainAsset.h"
#include "Registry/Registry.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Engine/EditorSelection.h"
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
    terrain.asset->GenerateFromNoise();
    terrain.asset->EnsureDefaultLayers();
    terrain.asset->GenerateAutoSplat(terrain.asset->autoSplat);
    terrain.asset->SetupDefaultWater();
    terrain.needsRebuild = true;
    registry.AddComponent(entity, terrain);

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
    asset.GenerateAutoSplat(asset.autoSplat);
    if (asset.water.enabled) {
        asset.water.seaLevel = asset.SuggestVisibleWaterLevel();
    }
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
        tc->asset->GenerateAutoSplat(tc->asset->autoSplat);
        tc->asset->SetupDefaultWater();
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

    char terrainSummary[128]{};
    std::snprintf(
        terrainSummary,
        sizeof(terrainSummary),
        "%.0fx%.0f  H%.0f  R%u  C%zu",
        asset.worldSizeX,
        asset.worldSizeZ,
        asset.heightScale,
        asset.resolution,
        chunkEstimate);
    DrawCompactMetric("Terrain", terrainSummary);
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
        ImGui::TextDisabled("Materials");
        const float layerWidth = (std::max)(
            76.0f,
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f);
        const char* layerLabels[] = { "Grass", "Dirt", "Rock" };
        for (int i = 0; i < (std::min)(layerCount, 3); ++i) {
            if (i > 0) ImGui::SameLine();
            const bool selected = m_brush.mode == TerrainBrush::Mode::Paint && m_brush.layerIndex == i;
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.45f, 0.28f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.54f, 0.34f, 1.0f));
            }
            if (ImGui::Button(layerLabels[i], ImVec2(layerWidth, 28.0f))) {
                m_brush.mode = TerrainBrush::Mode::Paint;
                m_brush.layerIndex = i;
                m_sceneBrushEnabled = true;
            }
            if (selected) {
                ImGui::PopStyleColor(2);
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
        ImGui::TextUnformatted("Terrain");
        ImGui::PushItemWidth(-1);
        bool terrainChanged = false;
        terrainChanged |= ImGui::DragFloat("Width", &asset.worldSizeX, 1.0f, 1.0f, 4096.0f, "%.0f");
        terrainChanged |= ImGui::DragFloat("Depth", &asset.worldSizeZ, 1.0f, 1.0f, 4096.0f, "%.0f");
        terrainChanged |= ImGui::DragFloat("Height", &asset.heightScale, 1.0f, 1.0f, 512.0f, "%.0f");
        terrainChanged |= ImGui::InputInt("Seed", &asset.seed);
        terrainChanged |= ImGui::DragFloat("Noise", &asset.noiseFreq, 0.0001f, 0.0001f, 0.1f, "%.4f");
        ImGui::PopItemWidth();
        if (terrainChanged) {
            tc->needsRebuild = true;
        }

        if (ImGui::Button("Regenerate Terrain", ImVec2(-1, 30.0f))) {
            RegenerateTerrain(asset, *tc);
        }
        if (ImGui::Button("Retexture From Rules", ImVec2(-1, 28.0f))) {
            asset.GenerateAutoSplat(asset.autoSplat);
            tc->needsRebuild = true;
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Rules");
        ImGui::PushItemWidth(-1);
        bool rulesChanged = false;
        rulesChanged |= ImGui::SliderFloat("Rock Height", &asset.autoSplat.rockAltitudeMin, 0.0f, 1.0f, "%.2f");
        rulesChanged |= ImGui::SliderFloat("Rock Slope", &asset.autoSplat.rockSlopeDegrees, 0.0f, 89.0f, "%.0f");
        rulesChanged |= ImGui::SliderFloat("Dirt Mix", &asset.autoSplat.dirtStrength, 0.0f, 1.0f, "%.2f");
        ImGui::PopItemWidth();
        if (rulesChanged) {
            asset.GenerateAutoSplat(asset.autoSplat);
            tc->needsRebuild = true;
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Water");
        bool waterChanged = false;
        waterChanged |= ImGui::Checkbox("Visible", &asset.water.enabled);
        ImGui::PushItemWidth(-1);
        waterChanged |= ImGui::DragFloat("Level", &asset.water.seaLevel, 0.25f, -512.0f, 512.0f, "%.2f");
        ImGui::PopItemWidth();
        if (ImGui::Button("Fit Water", ImVec2(-1, 28.0f))) {
            asset.water.enabled = true;
            asset.water.seaLevel = asset.SuggestVisibleWaterLevel();
            waterChanged = true;
        }
        if (ImGui::Button("Natural Water", ImVec2(-1, 28.0f))) {
            asset.water.enabled = true;
            asset.ApplyNaturalWaterPreset();
            asset.water.seaLevel = asset.SuggestVisibleWaterLevel();
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
            case TerrainBrush::Mode::Raise:   h = std::min(1.0f, h + w * 0.01f); break;
            case TerrainBrush::Mode::Lower:   h = std::max(0.0f, h - w * 0.01f); break;
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
