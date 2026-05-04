#include "GameLoopEditorPanelInternal.h"

GameLoopEditorPanelInternal::GameLoopEditorPanelInternal()
{
    std::strncpy(m_loadPath, m_currentPath.string().c_str(), sizeof(m_loadPath) - 1);
    std::strncpy(m_saveAsPath, m_currentPath.string().c_str(), sizeof(m_saveAsPath) - 1);
    m_asset.version = kGameFlowAssetVersion;
}

void GameLoopEditorPanelInternal::Draw(bool* p_open, bool* outFocused)
{
    ImGui::SetNextWindowSize(ImVec2(1180, 720), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("GameFlow Editor", p_open, ImGuiWindowFlags_MenuBar)) {
        if (outFocused) *outFocused = false;
        ImGui::End();
        return;
    }

    if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    DrawToolbar();
    DrawFileDialogBar();
    DrawMainLayout();
    DrawScenePickerPopup();

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            m_connecting = false;
            m_connectionDragged = false;
            m_connectFromNodeId = 0;
            ClearSelection();
        }
    }

    ImGui::End();
}

void GameLoopEditorPanelInternal::DrawToolbar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Empty")) {
            m_asset = GameLoopAsset{};
            m_asset.version = kGameFlowAssetVersion;
            m_nodeViews.clear();
            ClearSelection();
            m_fitRequested = true;
            m_dirty = true;
        }
        if (ImGui::MenuItem("Load")) {
            std::strncpy(m_loadPath, m_currentPath.string().c_str(), sizeof(m_loadPath) - 1);
            m_fileDialogMode = FileDialogMode::Load;
            m_statusMessage.clear();
        }
        if (ImGui::MenuItem("Save")) Save();
        if (ImGui::MenuItem("Save As")) {
            std::strncpy(m_saveAsPath, m_currentPath.string().c_str(), sizeof(m_saveAsPath) - 1);
            m_fileDialogMode = FileDialogMode::SaveAs;
            m_statusMessage.clear();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Fit Graph")) m_fitRequested = true;
        if (ImGui::MenuItem("Zoom 100%")) m_graphZoom = 1.0f;
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Validate")) Validate();

    ImGui::SameLine();
    ImGui::TextDisabled("%s%s", m_dirty ? "*" : "", m_currentPath.filename().string().c_str());

    ImGui::EndMenuBar();
}

void GameLoopEditorPanelInternal::DrawFileDialogBar()
{
    if (!m_statusMessage.empty()) {
        ImGui::TextDisabled("%s", m_statusMessage.c_str());
    }

    if (m_fileDialogMode == FileDialogMode::Load) {
        ImGui::Separator();
        ImGui::TextUnformatted("Load GameFlow");
        ImGui::SameLine();
        ImGui::SetNextItemWidth((std::max)(260.0f, ImGui::GetContentRegionAvail().x - 150.0f));
        ImGui::InputText("##LoadGameFlowPath", m_loadPath, sizeof(m_loadPath));
        ImGui::SameLine();
        if (ImGui::Button("Load##GameFlowFile")) {
            Load(m_loadPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##LoadGameFlowFile")) {
            m_fileDialogMode = FileDialogMode::None;
        }
        ImGui::Separator();
        return;
    }

    if (m_fileDialogMode == FileDialogMode::SaveAs) {
        ImGui::Separator();
        ImGui::TextUnformatted("Save GameFlow As");
        ImGui::SameLine();
        ImGui::SetNextItemWidth((std::max)(260.0f, ImGui::GetContentRegionAvail().x - 150.0f));
        ImGui::InputText("##SaveGameFlowPath", m_saveAsPath, sizeof(m_saveAsPath));
        ImGui::SameLine();
        if (ImGui::Button("Save##GameFlowFile")) {
            m_currentPath = m_saveAsPath;
            Save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##SaveGameFlowFile")) {
            m_fileDialogMode = FileDialogMode::None;
        }
        ImGui::Separator();
    }
}

void GameLoopEditorPanelInternal::DrawMainLayout()
{
    ImVec2 avail = ImGui::GetContentRegionAvail();

    float inspectW = 390.0f;
    float validateH = 110.0f;
    float topH = MaxF(260.0f, avail.y - validateH - 8.0f);
    float graphW = MaxF(260.0f, avail.x - inspectW - 8.0f);

    ImGui::BeginChild("Graph", ImVec2(graphW, topH), true, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    DrawGraph(ImGui::GetContentRegionAvail());
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Inspector", ImVec2(0, topH), true);
    DrawInspector();
    ImGui::EndChild();

    ImGui::BeginChild("Validate", ImVec2(0, 0), true);
    DrawValidateSummary();
    ImGui::EndChild();
}
