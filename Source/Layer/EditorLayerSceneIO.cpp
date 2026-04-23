#include "EditorLayerInternal.h"
#include "Gameplay/PlayerRuntimeSetup.h"

void EditorLayer::DrawUnsavedChangesPopup()
{
    if (m_openUnsavedChangesPopup) {
        ImGui::OpenPopup("Unsaved Scene Changes");
        m_openUnsavedChangesPopup = false;
    }

    if (ImGui::BeginPopupModal("Unsaved Scene Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s Current scene has unsaved changes.", ICON_FA_TRIANGLE_EXCLAMATION);
        ImGui::Text("Save before continuing?");
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            bool saved = SaveCurrentScene();
            if (!saved) {
                saved = SaveCurrentSceneAs();
            }
            if (saved) {
                ExecutePendingSceneAction();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
            ExecutePendingSceneAction();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_pendingSceneAction = PendingSceneAction::None;
            m_pendingSceneLoadPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::DrawRecoveryPopup()
{
    if (m_openRecoveryPopup) {
        ImGui::OpenPopup("Autosave Recovery");
        m_openRecoveryPopup = false;
    }

    if (ImGui::BeginPopupModal("Autosave Recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s A newer autosave was found for this scene.", ICON_FA_TRIANGLE_EXCLAMATION);
        if (!m_pendingRecoveryScenePath.empty()) {
            ImGui::TextWrapped("Scene: %s", m_pendingRecoveryScenePath.string().c_str());
        }
        if (!m_pendingRecoveryAutosavePath.empty()) {
            ImGui::TextWrapped("Autosave: %s", m_pendingRecoveryAutosavePath.string().c_str());
        }
        ImGui::Separator();

        if (ImGui::Button("Recover", ImVec2(140, 0))) {
            const std::filesystem::path autosavePath = m_pendingRecoveryAutosavePath;
            const std::filesystem::path scenePath = m_pendingRecoveryScenePath;
            const bool recovered = !autosavePath.empty() && LoadSceneFromPath(autosavePath);
            if (recovered) {
                m_sceneSavePath = scenePath.empty() ? autosavePath.string() : scenePath.string();
                const uint64_t revision = UndoSystem::Instance().GetECSRevision();
                m_savedSceneRevision = (revision == (std::numeric_limits<uint64_t>::max)()) ? revision : revision + 1;
                LOG_INFO("[Editor] Recovered autosave: %s", autosavePath.string().c_str());
            }
            m_pendingRecoveryAutosavePath.clear();
            m_pendingRecoveryScenePath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(140, 0))) {
            m_pendingRecoveryAutosavePath.clear();
            m_pendingRecoveryScenePath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


void EditorLayer::ProcessDeferredEditorActions()
{
    const bool requestNewScene = m_requestNewScene;
    const bool requestOpenScene = m_requestOpenScene;
    const bool requestSaveSceneAs = m_requestSaveSceneAs;

    m_requestNewScene = false;
    m_requestOpenScene = false;
    m_requestSaveSceneAs = false;

    std::filesystem::path pendingScenePath;
    const bool hasPendingSceneFromAssetBrowser = m_assetBrowser && m_assetBrowser->ConsumePendingSceneLoad(pendingScenePath);

    if (requestNewScene) {
        RequestSceneAction(PendingSceneAction::NewScene);
    }
    if (requestOpenScene) {
        RequestSceneAction(PendingSceneAction::OpenSceneDialog);
    }
    if (requestSaveSceneAs) {
        SaveCurrentSceneAs();
    }
    if (hasPendingSceneFromAssetBrowser) {
        RequestSceneAction(PendingSceneAction::LoadScenePath, pendingScenePath);
    }
}

void EditorLayer::UpdateAutosave(float deltaSeconds)
{
    if (!m_gameLayer || deltaSeconds <= 0.0f) {
        return;
    }

    if (!IsSceneDirty()) {
        m_autosaveAccumulator = 0.0;
        return;
    }

    m_autosaveAccumulator += static_cast<double>(deltaSeconds);
    if (m_autosaveAccumulator < kAutosaveIntervalSeconds) {
        return;
    }

    const std::filesystem::path scenePath = SanitizeSceneDefaultPath(m_sceneSavePath);
    const std::filesystem::path autosavePath = BuildAutosaveScenePath(scenePath);
    std::error_code ec;
    std::filesystem::create_directories(autosavePath.parent_path(), ec);
    if (ec) {
        LOG_WARN("[Editor] Failed to create autosave directory: %s", autosavePath.parent_path().string().c_str());
        m_autosaveAccumulator = 0.0;
        return;
    }

    const SceneFileMetadata metadata = BuildSceneMetadata(m_sceneViewMode);
    if (PrefabSystem::SaveRegistryAsScene(m_gameLayer->GetRegistry(), autosavePath, &metadata)) {
        TrimAutosaveGenerations(autosavePath.parent_path());
        LOG_INFO("[Editor] Autosaved scene: %s", autosavePath.string().c_str());
    } else {
        LOG_WARN("[Editor] Failed to autosave scene: %s", autosavePath.string().c_str());
    }

    m_autosaveAccumulator = 0.0;
}

void EditorLayer::CheckRecoveryCandidate()
{
    if (m_hasCheckedRecovery) {
        return;
    }
    m_hasCheckedRecovery = true;

    const std::filesystem::path scenePath = SanitizeSceneDefaultPath(m_sceneSavePath);
    const std::filesystem::path autosavePath = FindLatestAutosaveForScene(scenePath);
    if (autosavePath.empty() || !IsAutosaveNewerThanScene(autosavePath, scenePath)) {
        return;
    }

    m_pendingRecoveryAutosavePath = autosavePath;
    m_pendingRecoveryScenePath = scenePath;
    m_openRecoveryPopup = true;
}

bool EditorLayer::IsSceneDirty() const
{
    return UndoSystem::Instance().GetECSRevision() != m_savedSceneRevision;
}

void EditorLayer::MarkSceneSaved()
{
    m_savedSceneRevision = UndoSystem::Instance().GetECSRevision();
}

void EditorLayer::RequestSceneAction(PendingSceneAction action, std::filesystem::path scenePath)
{
    if (action == PendingSceneAction::None) {
        return;
    }

    m_pendingSceneAction = action;
    m_pendingSceneLoadPath = std::move(scenePath);

    if (IsSceneDirty()) {
        m_openUnsavedChangesPopup = true;
        return;
    }

    ExecutePendingSceneAction();
}

bool EditorLayer::ExecutePendingSceneAction()
{
    const PendingSceneAction action = m_pendingSceneAction;
    const std::filesystem::path scenePath = m_pendingSceneLoadPath;
    m_pendingSceneAction = PendingSceneAction::None;
    m_pendingSceneLoadPath.clear();

    switch (action) {
    case PendingSceneAction::NewScene:
        NewScene();
        return true;
    case PendingSceneAction::OpenSceneDialog:
        return OpenScene();
    case PendingSceneAction::LoadScenePath:
        return !scenePath.empty() && LoadSceneFromPath(scenePath);
    default:
        return false;
    }
}


void EditorLayer::NewScene()
{
    if (!m_gameLayer) {
        return;
    }

    EngineKernel::Instance().ResetRenderStateForSceneChange();

    Registry& registry = m_gameLayer->GetRegistry();
    EditorSelection::Instance().Clear();
    UndoSystem::Instance().ClearECSHistory();
    m_scenePickPending = false;
    m_scenePickBlockedByGizmo = false;
    m_gizmoWasUsing = false;
    m_gizmoIsOver = false;
    m_gizmoUndoEntity = Entity::NULL_ID;
    m_hasGizmoBeforeTransform = false;
    m_pendingRecoveryAutosavePath.clear();
    m_pendingRecoveryScenePath.clear();
    m_openRecoveryPopup = false;
    m_autosaveAccumulator = 0.0;
    m_hasCheckedRecovery = true;

    ClearRegistryEntities(registry);
    CreateDefaultSceneEntities(registry);

    m_sceneViewMode = SceneViewMode::Mode3D;
    m_sceneSavePath = kDefaultSceneSavePath;
    MarkSceneSaved();
    LOG_INFO("[Editor] New scene created.");
}

bool EditorLayer::LoadSceneFromPath(const std::filesystem::path& scenePath)
{
    if (!m_gameLayer) {
        return false;
    }

    EngineKernel::Instance().ResetRenderStateForSceneChange();
    SceneFileMetadata metadata;
    if (!PrefabSystem::LoadSceneIntoRegistry(scenePath, m_gameLayer->GetRegistry(), &metadata)) {
        LOG_WARN("[Editor] Failed to load scene: %s", scenePath.string().c_str());
        return false;
    }
    PlayerRuntimeSetup::EnsureAllPlayerRuntimeComponents(m_gameLayer->GetRegistry(), true);

    UndoSystem::Instance().ClearECSHistory();
    EditorSelection::Instance().Clear();
    m_sceneSavePath = scenePath.string();
    m_pendingRecoveryAutosavePath.clear();
    m_pendingRecoveryScenePath.clear();
    m_openRecoveryPopup = false;
    m_autosaveAccumulator = 0.0;
    m_hasCheckedRecovery = true;
    m_sceneViewMode = SceneViewModeFromString(metadata.sceneViewMode);
    MarkSceneSaved();
    LOG_INFO("[Editor] Scene loaded: %s", scenePath.string().c_str());
    return true;
}

bool EditorLayer::OpenScene()
{
    if (!m_gameLayer) {
        return false;
    }

    char filePath[MAX_PATH] = {};
    const std::filesystem::path initialPath = SanitizeSceneDefaultPath(m_sceneSavePath);
    strcpy_s(filePath, initialPath.string().c_str());
    if (Dialog::OpenFileName(filePath, MAX_PATH, kSceneDialogFilter, "Open Scene", nullptr) != DialogResult::OK) {
        return false;
    }

    return LoadSceneFromPath(std::filesystem::path(filePath));
}

bool EditorLayer::SaveCurrentScene()
{
    if (!m_gameLayer) {
        return false;
    }

    const std::filesystem::path scenePath = SanitizeSceneDefaultPath(m_sceneSavePath);
    const SceneFileMetadata metadata = BuildSceneMetadata(m_sceneViewMode);
    const bool success = PrefabSystem::SaveRegistryAsScene(m_gameLayer->GetRegistry(), scenePath, &metadata);
    if (success) {
        LOG_INFO("[Editor] Scene saved: %s", scenePath.string().c_str());
        m_sceneSavePath = scenePath.string();
        m_pendingRecoveryAutosavePath.clear();
        m_pendingRecoveryScenePath.clear();
        m_autosaveAccumulator = 0.0;
        MarkSceneSaved();
    } else {
        LOG_WARN("[Editor] Failed to save scene: %s", scenePath.string().c_str());
    }
    return success;
}

bool EditorLayer::SaveCurrentSceneAs()
{
    if (!m_gameLayer) {
        return false;
    }

    char filePath[MAX_PATH] = {};
    strcpy_s(filePath, SanitizeSceneDefaultPath(m_sceneSavePath).c_str());
    if (Dialog::SaveFileName(filePath, MAX_PATH, kSceneDialogFilter, "Save Scene As", "scene", nullptr) != DialogResult::OK) {
        return false;
    }

    m_sceneSavePath = filePath;
    return SaveCurrentScene();
}

