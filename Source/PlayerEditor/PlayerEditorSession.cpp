#include "PlayerEditorSession.h"

#include "PlayerEditorPanel.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>

#include "Asset/PrefabSystem.h"
#include "Animator/AnimatorService.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NameComponent.h"
#include "Component/NodeSocketComponent.h"
#include "Component/PrefabInstanceComponent.h"
#include "Component/TransformComponent.h"
#include "StateMachineAssetSerializer.h"
#include "TimelineAssetSerializer.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/StateMachineSystem.h"
#include "Gameplay/TimelineAssetRuntimeBuilder.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Input/InputBindingComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "System/ResourceManager.h"

namespace
{
    static constexpr const char* kTimelineFileFilter =
        "Timeline (*.timeline.json)\0*.timeline.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    static constexpr const char* kStateMachineFileFilter =
        "StateMachine (*.statemachine.json)\0*.statemachine.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    static constexpr const char* kInputMapFileFilter =
        "Input Map (*.inputmap.json)\0*.inputmap.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    static constexpr const char* kPrefabFileFilter =
        "Prefab (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";

    static bool HasExtension(const std::string& path, std::initializer_list<const char*> extensions)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const char* candidate : extensions) {
            if (ext == candidate) {
                return true;
            }
        }
        return false;
    }
}

void PlayerEditorSession::Suspend(PlayerEditorPanel& panel)
{
    panel.m_playheadFrame = 0;
    panel.m_isPlaying = false;
    SyncPreviewTimelinePlayback(panel);
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }
    DestroyOwnedPreviewEntity(panel);
}

void PlayerEditorSession::DestroyOwnedPreviewEntity(PlayerEditorPanel& panel)
{
    if (!panel.m_previewEntityOwned) {
        return;
    }

    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    if (panel.m_registry && !Entity::IsNull(panel.m_previewEntity) && panel.m_registry->IsAlive(panel.m_previewEntity)) {
        panel.m_registry->DestroyEntity(panel.m_previewEntity);
    }

    panel.m_previewEntity = Entity::NULL_ID;
    panel.m_previewEntityOwned = false;
}

void PlayerEditorSession::EnsureOwnedPreviewEntity(PlayerEditorPanel& panel)
{
    if (!panel.m_registry || !panel.HasOpenModel()) {
        return;
    }

    if (panel.CanUsePreviewEntity()) {
        return;
    }

    DestroyOwnedPreviewEntity(panel);

    panel.m_previewEntity = panel.m_registry->CreateEntity();
    panel.m_previewEntityOwned = true;
    panel.m_registry->AddComponent(panel.m_previewEntity, NameComponent{ "Player Preview" });

    TransformComponent transform{};
    transform.localPosition = { 0.0f, 0.0f, 0.0f };
    transform.localScale = { 1.0f, 1.0f, 1.0f };
    transform.isDirty = true;
    panel.m_registry->AddComponent(panel.m_previewEntity, transform);

    MeshComponent mesh{};
    mesh.modelFilePath = panel.m_currentModelPath;
    mesh.model = panel.m_ownedModel;
    mesh.isVisible = true;
    mesh.castShadow = true;
    panel.m_registry->AddComponent(panel.m_previewEntity, mesh);
    panel.m_registry->AddComponent(panel.m_previewEntity, EffectPreviewTagComponent{});

    ApplyEditorBindingsToPreviewEntity(panel);
}

void PlayerEditorSession::SetPreviewEntity(PlayerEditorPanel& panel, EntityID entity)
{
    if (panel.m_previewEntity == entity && !panel.m_previewEntityOwned) {
        return;
    }

    panel.m_isPlaying = false;
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    DestroyOwnedPreviewEntity(panel);
    panel.m_previewEntity = entity;
    panel.m_previewEntityOwned = false;
    ImportSocketsFromPreviewEntity(panel);
}

void PlayerEditorSession::SyncExternalSelection(PlayerEditorPanel& panel, EntityID entity, const std::string& modelPath)
{
    panel.m_selectedEntity = entity;
    panel.m_selectedEntityModelPath = modelPath;

    if (!Entity::IsNull(panel.m_previewEntity) && panel.m_registry && !panel.m_registry->IsAlive(panel.m_previewEntity)) {
        panel.m_previewEntity = Entity::NULL_ID;
        panel.m_previewEntityOwned = false;
    }
}

bool PlayerEditorSession::OpenModelFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    std::shared_ptr<Model> model = ResourceManager::Instance().GetModel(path);
    if (!model) {
        return false;
    }

    Suspend(panel);

    panel.m_ownedModel = std::move(model);
    panel.m_model = panel.m_ownedModel.get();
    panel.m_currentModelPath = path;
    panel.ResetSelectionState();
    panel.m_selectedAnimIndex = 0;
    panel.m_previewModelScale = 1.0f;
    panel.m_previewRenderSize = { 0.0f, 0.0f };

    panel.m_sockets.clear();
    panel.m_socketDirty = false;
    panel.m_timelineDirty = false;
    panel.m_stateMachineDirty = false;

    EnsureOwnedPreviewEntity(panel);
    return true;
}

bool PlayerEditorSession::OpenTimelineFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    if (path.empty() || !HasExtension(path, { ".timeline.json", ".json" })) {
        return false;
    }
    if (!TimelineAssetSerializer::Load(path, panel.m_timelineAsset)) {
        return false;
    }
    panel.m_timelineAssetPath = path;
    panel.m_timelineDirty = false;
    RebuildPreviewTimelineRuntimeData(panel);
    return true;
}

bool PlayerEditorSession::OpenStateMachineFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    if (path.empty() || !HasExtension(path, { ".statemachine.json", ".json" })) {
        return false;
    }
    if (!StateMachineAssetSerializer::Load(path, panel.m_stateMachineAsset)) {
        return false;
    }
    panel.m_stateMachineAssetPath = path;
    panel.m_stateMachineDirty = false;
    return true;
}

bool PlayerEditorSession::OpenInputMapFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    return panel.m_inputMappingTab.OpenActionMap(path);
}

bool PlayerEditorSession::SaveTimelineDocument(PlayerEditorPanel& panel, bool saveAs)
{
    std::string path = panel.m_timelineAssetPath;
    if (saveAs || path.empty()) {
        char pathBuffer[MAX_PATH] = {};
        if (!path.empty()) {
            strcpy_s(pathBuffer, path.c_str());
        } else if (!panel.m_timelineAsset.name.empty()) {
            strcpy_s(pathBuffer, ("Assets/Timeline/" + panel.m_timelineAsset.name + ".timeline.json").c_str());
        }
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kTimelineFileFilter, "Save Timeline", "json") != DialogResult::OK) {
            return false;
        }
        path = pathBuffer;
    }

    if (!TimelineAssetSerializer::Save(path, panel.m_timelineAsset)) {
        return false;
    }

    panel.m_timelineAssetPath = path;
    panel.m_timelineDirty = false;
    StateMachineSystem::InvalidateAssetCache(path.c_str());
    return true;
}

bool PlayerEditorSession::SaveStateMachineDocument(PlayerEditorPanel& panel, bool saveAs)
{
    std::string path = panel.m_stateMachineAssetPath;
    if (saveAs || path.empty()) {
        char pathBuffer[MAX_PATH] = {};
        if (!path.empty()) {
            strcpy_s(pathBuffer, path.c_str());
        } else if (!panel.m_stateMachineAsset.name.empty()) {
            strcpy_s(pathBuffer, ("Assets/StateMachine/" + panel.m_stateMachineAsset.name + ".statemachine.json").c_str());
        }
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kStateMachineFileFilter, "Save State Machine", "json") != DialogResult::OK) {
            return false;
        }
        path = pathBuffer;
    }

    if (!StateMachineAssetSerializer::Save(path, panel.m_stateMachineAsset)) {
        return false;
    }

    panel.m_stateMachineAssetPath = path;
    panel.m_stateMachineDirty = false;
    StateMachineSystem::InvalidateAssetCache(path.c_str());
    return true;
}

bool PlayerEditorSession::SaveInputMapDocument(PlayerEditorPanel& panel, bool saveAs)
{
    if (!saveAs) {
        return panel.m_inputMappingTab.SaveActionMap();
    }

    char pathBuffer[MAX_PATH] = {};
    const std::string& currentPath = panel.m_inputMappingTab.GetActionMapPath();
    if (!currentPath.empty()) {
        strcpy_s(pathBuffer, currentPath.c_str());
    }
    if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kInputMapFileFilter, "Save Input Map", "json") != DialogResult::OK) {
        return false;
    }
    return panel.m_inputMappingTab.SaveActionMapAs(pathBuffer);
}

bool PlayerEditorSession::SaveAllDocuments(PlayerEditorPanel& panel, bool saveAs)
{
    bool ok = true;
    if (panel.m_timelineDirty || (saveAs && !panel.m_timelineAsset.tracks.empty())) ok &= SaveTimelineDocument(panel, saveAs);
    if (panel.m_stateMachineDirty || (saveAs && !panel.m_stateMachineAsset.states.empty())) ok &= SaveStateMachineDocument(panel, saveAs);
    if (panel.m_inputMappingTab.IsDirty() || (saveAs && !panel.m_inputMappingTab.GetActionMapPath().empty())) ok &= SaveInputMapDocument(panel, saveAs);
    if (panel.m_socketDirty) {
        ExportSocketsToPreviewEntity(panel);
    }
    return ok;
}

bool PlayerEditorSession::SavePrefabDocument(PlayerEditorPanel& panel, bool saveAs)
{
    if (!panel.m_registry || Entity::IsNull(panel.m_previewEntity)) {
        return false;
    }
    if (panel.HasAnyDirtyDocument()) {
        if (!SaveAllDocuments(panel, false) && panel.HasAnyDirtyDocument()) {
            if (!SaveAllDocuments(panel, true)) {
                return false;
            }
        }
    }

    ApplyEditorBindingsToPreviewEntity(panel);
    ExportSocketsToPreviewEntity(panel);
    PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*panel.m_registry, panel.m_previewEntity);
    PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, panel.m_previewEntity);
    PlayerRuntimeSetup::ResetPlayerRuntimeState(*panel.m_registry, panel.m_previewEntity);

    std::string prefabPath;
    if (!saveAs) {
        if (const PrefabInstanceComponent* prefab = panel.m_registry->GetComponent<PrefabInstanceComponent>(panel.m_previewEntity)) {
            prefabPath = prefab->prefabAssetPath;
        }
    }

    if (prefabPath.empty()) {
        char pathBuffer[MAX_PATH] = {};
        if (const PrefabInstanceComponent* prefab = panel.m_registry->GetComponent<PrefabInstanceComponent>(panel.m_previewEntity);
            prefab && !prefab->prefabAssetPath.empty()) {
            strcpy_s(pathBuffer, prefab->prefabAssetPath.c_str());
        } else {
            const std::string defaultName = panel.m_currentModelPath.empty()
                ? "Assets/Prefab/Player.prefab"
                : ("Assets/Prefab/" + std::filesystem::path(panel.m_currentModelPath).stem().string() + ".prefab");
            strcpy_s(pathBuffer, defaultName.c_str());
        }
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kPrefabFileFilter, "Save Player Prefab", "prefab") != DialogResult::OK) {
            return false;
        }
        prefabPath = pathBuffer;
    }

    return PrefabSystem::SaveEntityToPrefabPath(panel.m_previewEntity, *panel.m_registry, prefabPath);
}

void PlayerEditorSession::RevertAllDocuments(PlayerEditorPanel& panel)
{
    if (!panel.m_timelineAssetPath.empty()) {
        TimelineAssetSerializer::Load(panel.m_timelineAssetPath, panel.m_timelineAsset);
        panel.m_timelineDirty = false;
    }
    if (!panel.m_stateMachineAssetPath.empty()) {
        StateMachineAssetSerializer::Load(panel.m_stateMachineAssetPath, panel.m_stateMachineAsset);
        panel.m_stateMachineDirty = false;
    }
    panel.m_inputMappingTab.ReloadActionMap();
    ImportSocketsFromPreviewEntity(panel);
    panel.m_socketDirty = false;
}

void PlayerEditorSession::ApplyEditorBindingsToPreviewEntity(PlayerEditorPanel& panel)
{
    if (!panel.CanUsePreviewEntity()) {
        return;
    }

    if (auto* mesh = panel.m_registry->GetComponent<MeshComponent>(panel.m_previewEntity)) {
        mesh->model = panel.m_ownedModel;
        mesh->modelFilePath = panel.m_currentModelPath;
        mesh->isVisible = true;
    }

    {
        StateMachineParamsComponent* stateMachine = panel.m_registry->GetComponent<StateMachineParamsComponent>(panel.m_previewEntity);
        if (!stateMachine && !panel.m_stateMachineAssetPath.empty()) {
            panel.m_registry->AddComponent<StateMachineParamsComponent>(panel.m_previewEntity, StateMachineParamsComponent{});
            stateMachine = panel.m_registry->GetComponent<StateMachineParamsComponent>(panel.m_previewEntity);
        }
        if (stateMachine && !panel.m_stateMachineAssetPath.empty()) {
            strcpy_s(stateMachine->assetPath, panel.m_stateMachineAssetPath.c_str());
        } else if (stateMachine) {
            stateMachine->assetPath[0] = '\0';
        }
    }

    {
        InputBindingComponent* inputBinding = panel.m_registry->GetComponent<InputBindingComponent>(panel.m_previewEntity);
        if (!inputBinding && !panel.m_inputMappingTab.GetActionMapPath().empty()) {
            panel.m_registry->AddComponent<InputBindingComponent>(panel.m_previewEntity, InputBindingComponent{});
            inputBinding = panel.m_registry->GetComponent<InputBindingComponent>(panel.m_previewEntity);
        }
        if (inputBinding && !panel.m_inputMappingTab.GetActionMapPath().empty()) {
            strcpy_s(inputBinding->actionMapAssetPath, panel.m_inputMappingTab.GetActionMapPath().c_str());
        } else if (inputBinding) {
            inputBinding->actionMapAssetPath[0] = '\0';
        }
    }

    if (auto* transform = panel.m_registry->GetComponent<TransformComponent>(panel.m_previewEntity)) {
        transform->localScale = { panel.m_previewModelScale, panel.m_previewModelScale, panel.m_previewModelScale };
        transform->isDirty = true;
    }

    ExportSocketsToPreviewEntity(panel);
    RebuildPreviewTimelineRuntimeData(panel);
}

void PlayerEditorSession::RebuildPreviewTimelineRuntimeData(PlayerEditorPanel& panel)
{
    if (Entity::IsNull(panel.m_previewEntity)) {
        return;
    }

    PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, panel.m_previewEntity);

    TimelineComponent timeline{};
    TimelineItemBuffer buffer{};
    if (!TimelineAssetRuntimeBuilder::Build(panel.m_timelineAsset, panel.m_selectedAnimIndex, timeline, buffer)) {
        return;
    }
    if (auto* existing = panel.m_registry->GetComponent<TimelineComponent>(panel.m_previewEntity)) {
        *existing = timeline;
    } else {
        panel.m_registry->AddComponent(panel.m_previewEntity, timeline);
    }
    if (auto* existingBuffer = panel.m_registry->GetComponent<TimelineItemBuffer>(panel.m_previewEntity)) {
        *existingBuffer = buffer;
    } else {
        panel.m_registry->AddComponent(panel.m_previewEntity, buffer);
    }
}

void PlayerEditorSession::SyncPreviewTimelinePlayback(PlayerEditorPanel& panel)
{
    if (!panel.m_previewState.IsActive()) {
        return;
    }
    panel.m_previewState.SetTime(panel.m_playheadFrame / (panel.m_timelineAsset.fps > 0.0f ? panel.m_timelineAsset.fps : 60.0f));
}

void PlayerEditorSession::ImportFromSelectedEntity(PlayerEditorPanel& panel)
{
    if (!panel.m_registry || Entity::IsNull(panel.m_selectedEntity)) {
        return;
    }

    if (!panel.m_selectedEntityModelPath.empty()) {
        OpenModelFromPath(panel, panel.m_selectedEntityModelPath);
    }

    SetPreviewEntity(panel, panel.m_selectedEntity);

    if (StateMachineParamsComponent* stateMachine = panel.m_registry->GetComponent<StateMachineParamsComponent>(panel.m_selectedEntity);
        stateMachine && stateMachine->assetPath[0] != '\0') {
        OpenStateMachineFromPath(panel, stateMachine->assetPath);
    }

    if (InputBindingComponent* inputBinding = panel.m_registry->GetComponent<InputBindingComponent>(panel.m_selectedEntity);
        inputBinding && inputBinding->actionMapAssetPath[0] != '\0') {
        OpenInputMapFromPath(panel, inputBinding->actionMapAssetPath);
    }

    ImportSocketsFromPreviewEntity(panel);
}

void PlayerEditorSession::ImportSocketsFromPreviewEntity(PlayerEditorPanel& panel)
{
    if (!panel.m_registry || !panel.CanUsePreviewEntity()) {
        panel.m_sockets.clear();
        panel.m_socketDirty = false;
        return;
    }
    if (const auto* sockets = panel.m_registry->GetComponent<NodeSocketComponent>(panel.m_previewEntity)) {
        panel.m_sockets = sockets->sockets;
    } else {
        panel.m_sockets.clear();
    }
    panel.m_socketDirty = false;
}

void PlayerEditorSession::ExportSocketsToPreviewEntity(PlayerEditorPanel& panel)
{
    if (!panel.m_registry || !panel.CanUsePreviewEntity()) {
        return;
    }
    auto* sockets = panel.m_registry->GetComponent<NodeSocketComponent>(panel.m_previewEntity);
    if (!sockets) {
        panel.m_registry->AddComponent(panel.m_previewEntity, NodeSocketComponent{ panel.m_sockets });
        return;
    }
    sockets->sockets = panel.m_sockets;
    panel.m_socketDirty = false;
}
