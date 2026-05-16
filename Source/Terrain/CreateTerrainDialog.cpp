#include "CreateTerrainDialog.h"
#include <imgui.h>
#include <algorithm>

void CreateTerrainDialog::Open(CreateCallback cb)
{
    m_callback = std::move(cb);
    m_open     = true;
    ImGui::OpenPopup("Create Terrain##dialog");
}

void CreateTerrainDialog::Draw()
{
    if (!m_open) return;

    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Create Terrain##dialog", &m_open,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::InputText("Name",        m_name, sizeof(m_name));
        ImGui::InputInt("Resolution",   &m_resolution);
        ImGui::InputFloat("World Size X", &m_worldSizeX, 1.0f, 10.0f);
        ImGui::InputFloat("World Size Z", &m_worldSizeZ, 1.0f, 10.0f);
        ImGui::InputFloat("Height Scale", &m_heightScale, 1.0f, 10.0f);
        ImGui::InputInt("Chunks X",    &m_chunkCountX);
        ImGui::InputInt("Chunks Z",    &m_chunkCountZ);

        m_resolution  = std::max(16,  m_resolution);
        m_chunkCountX = std::max(1,   m_chunkCountX);
        m_chunkCountZ = std::max(1,   m_chunkCountZ);
        m_worldSizeX  = std::max(1.0f, m_worldSizeX);
        m_worldSizeZ  = std::max(1.0f, m_worldSizeZ);
        m_heightScale = std::max(0.1f, m_heightScale);

        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            auto asset = std::make_shared<TerrainAsset>();
            asset->resolution  = static_cast<uint32_t>(m_resolution);
            asset->worldSizeX  = m_worldSizeX;
            asset->worldSizeZ  = m_worldSizeZ;
            asset->heightScale = m_heightScale;
            asset->chunkCountX = static_cast<uint32_t>(m_chunkCountX);
            asset->chunkCountZ = static_cast<uint32_t>(m_chunkCountZ);
            asset->GenerateFromNoise();
            asset->EnsureDefaultLayers();
            asset->SetupDefaultWater();
            asset->GenerateAutoSplat(asset->autoSplat);
            if (m_callback) m_callback(std::move(asset));
            m_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
