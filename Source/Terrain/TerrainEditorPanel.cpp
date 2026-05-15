#include "TerrainEditorPanel.h"
#include "Terrain/TerrainComponent.h"
#include "Terrain/TerrainAsset.h"
#include "Registry/Registry.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace {

const char* kTabNames[] = { "Generate", "Sculpt", "Paint", "Layers", "Settings" };

} // namespace

void TerrainEditorPanel::Draw(Registry& registry, EntityID selectedEntity)
{
    ImGui::Begin("Terrain Editor");

    if (Entity::IsNull(selectedEntity)) {
        ImGui::TextDisabled("Select a Terrain entity in the Hierarchy.");
        ImGui::End();
        return;
    }

    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(selectedEntity);
    if (!tc) {
        ImGui::TextDisabled("Selected entity has no TerrainComponent.");
        ImGui::End();
        return;
    }

    // タブバー
    if (ImGui::BeginTabBar("TerrainTabs")) {
        if (ImGui::BeginTabItem("Generate"))  { DrawGenerateTab(registry, selectedEntity); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Sculpt"))    { DrawSculptTab(registry, selectedEntity);   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Paint"))     { DrawPaintTab(registry, selectedEntity);    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Layers"))    { DrawLayersTab(registry, selectedEntity);   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Settings"))  { DrawSettingsTab(registry, selectedEntity); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void TerrainEditorPanel::DrawGenerateTab(Registry& registry, EntityID entity)
{
    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(entity);
    if (!tc || !tc->asset) return;
    auto& asset = *tc->asset;

    ImGui::SeparatorText("Noise Parameters");
    int noiseType = asset.noiseType;
    if (ImGui::Combo("Noise Type", &noiseType, "OpenSimplex2\0OpenSimplex2S\0Cellular\0Perlin\0ValueCubic\0Value\0"))
        asset.noiseType = noiseType;
    ImGui::DragFloat("Frequency",  &asset.noiseFreq,  0.0001f, 0.0001f, 0.1f, "%.4f");
    ImGui::DragInt("Octaves",      &asset.octaves,    1, 1, 8);
    ImGui::DragFloat("Lacunarity", &asset.lacunarity, 0.01f, 1.0f, 4.0f);
    ImGui::DragFloat("Gain",       &asset.gain,       0.01f, 0.1f, 1.0f);
    ImGui::InputInt("Seed",        &asset.seed);

    ImGui::Spacing();
    if (ImGui::Button("Generate", ImVec2(-1, 0))) {
        asset.GenerateFromNoise();
        tc->needsRebuild = true;
    }
}

void TerrainEditorPanel::DrawSculptTab(Registry& registry, EntityID entity)
{
    ImGui::SeparatorText("Brush Settings");

    const char* modeNames[] = { "Raise", "Lower", "Smooth", "Flatten" };
    int modeIdx = static_cast<int>(m_brush.mode);
    if (modeIdx >= 4) modeIdx = 0;
    if (ImGui::Combo("Mode##sculpt", &modeIdx, modeNames, 4))
        m_brush.mode = static_cast<TerrainBrush::Mode>(modeIdx);

    ImGui::DragFloat("Radius##s",   &m_brush.radius,   0.5f, 0.5f, 100.0f);
    ImGui::DragFloat("Strength##s", &m_brush.strength, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Falloff##s",  &m_brush.falloff,  0.01f, 0.0f, 1.0f);

    if (m_brush.mode == TerrainBrush::Mode::Flatten)
        ImGui::DragFloat("Target Height", &m_brush.targetHeight, 0.01f, 0.0f, 1.0f);

    ImGui::TextDisabled("Click in Scene View to sculpt (not yet wired).");
}

void TerrainEditorPanel::DrawPaintTab(Registry& registry, EntityID entity)
{
    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(entity);
    if (!tc || !tc->asset) return;
    auto& asset = *tc->asset;

    ImGui::SeparatorText("Paint Layer");
    int layerCount = static_cast<int>(asset.layers.size());
    if (layerCount == 0) {
        ImGui::TextDisabled("No layers defined. Add layers in the Layers tab first.");
        return;
    }
    ImGui::SliderInt("Layer##paint", &m_brush.layerIndex, 0, layerCount - 1);
    ImGui::DragFloat("Radius##p",    &m_brush.radius,   0.5f, 0.5f, 100.0f);
    ImGui::DragFloat("Strength##p",  &m_brush.strength, 0.01f, 0.0f, 1.0f);
    ImGui::TextDisabled("Click in Scene View to paint (not yet wired).");
}

void TerrainEditorPanel::DrawLayersTab(Registry& registry, EntityID entity)
{
    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(entity);
    if (!tc || !tc->asset) return;
    auto& asset = *tc->asset;

    ImGui::SeparatorText("Texture Layers (max 4)");
    for (int i = 0; i < static_cast<int>(asset.layers.size()); ++i) {
        ImGui::PushID(i);
        auto& layer = asset.layers[i];
        ImGui::Text("Layer %d", i);
        char albedoBuf[256]; strncpy_s(albedoBuf, layer.albedoPath.c_str(), sizeof(albedoBuf) - 1);
        if (ImGui::InputText("Albedo",    albedoBuf, sizeof(albedoBuf))) layer.albedoPath = albedoBuf;
        char normalBuf[256]; strncpy_s(normalBuf, layer.normalPath.c_str(), sizeof(normalBuf) - 1);
        if (ImGui::InputText("Normal",    normalBuf, sizeof(normalBuf))) layer.normalPath = normalBuf;
        ImGui::DragFloat("Tile Scale", &layer.tileScale, 0.1f, 0.1f, 32.0f);
        if (ImGui::Button("Remove")) { asset.layers.erase(asset.layers.begin() + i); ImGui::PopID(); break; }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (asset.layers.size() < 4) {
        if (ImGui::Button("Add Layer")) {
            asset.layers.push_back(TerrainLayer{});
        }
    }
}

void TerrainEditorPanel::DrawSettingsTab(Registry& registry, EntityID entity)
{
    TerrainComponent* tc = registry.GetComponent<TerrainComponent>(entity);
    if (!tc || !tc->asset) return;
    auto& asset = *tc->asset;

    ImGui::SeparatorText("Terrain Settings");
    float wsx = asset.worldSizeX, wsz = asset.worldSizeZ;
    float hs   = asset.heightScale;
    if (ImGui::DragFloat("World Size X", &wsx, 1.0f, 1.0f, 4096.0f)) asset.worldSizeX = wsx;
    if (ImGui::DragFloat("World Size Z", &wsz, 1.0f, 1.0f, 4096.0f)) asset.worldSizeZ = wsz;
    if (ImGui::DragFloat("Height Scale", &hs,  1.0f, 1.0f, 512.0f))  asset.heightScale = hs;

    ImGui::Spacing();
    if (ImGui::Button("Rebuild Mesh")) tc->needsRebuild = true;
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
    const float radiusNorm = m_brush.radius / asset.worldSizeX;

    const int px = static_cast<int>(normX * (asset.resolution - 1));
    const int pz = static_cast<int>(normZ * (asset.resolution - 1));
    const int radiusPx = static_cast<int>(radiusNorm * (asset.resolution - 1)) + 1;

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
            default: break;
            }
        }
    }
    tc->needsRebuild = true;
}
