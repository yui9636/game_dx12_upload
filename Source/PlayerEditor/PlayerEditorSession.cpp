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
#include "Component/ColliderComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NameComponent.h"
#include "Component/NodeSocketComponent.h"
#include "Component/PrefabInstanceComponent.h"
#include "Component/TransformComponent.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/StateMachineAssetComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/StateMachineSystem.h"
#include "Gameplay/TimelineAssetRuntimeBuilder.h"
#include "Gameplay/TimelineLibraryComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Engine/EngineKernel.h"
#include "Input/InputActionMapComponent.h"
#include "Input/InputBindingComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "System/ResourceManager.h"
#include "Undo/EntitySnapshot.h"

namespace
{
    static void StopActiveTimelineAudio(TimelineItemBuffer* buffer)
    {
        if (!buffer) {
            return;
        }

        auto& audioWorld = EngineKernel::Instance().GetAudioWorld();
        for (auto& item : buffer->items) {
            if (item.type != 3) {
                continue;
            }

            if (item.audioHandle != 0) {
                audioWorld.StopVoice(item.audioHandle);
            }

            item.audioActive = false;
            item.audioHandle = 0;
        }
    }

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

    static bool HasTimelineAssetContent(const TimelineAsset& asset)
    {
        return asset.id != 0
            || !asset.name.empty()
            || !asset.tracks.empty();
    }

    static void EnsureTimelineAssetIdentity(TimelineAsset& asset, TimelineLibraryComponent& library)
    {
        if (asset.id != 0) {
            if (asset.id >= library.nextTimelineId) {
                library.nextTimelineId = asset.id + 1;
            }
            return;
        }

        if (library.nextTimelineId == 0) {
            library.nextTimelineId = 1;
        }

        asset.id = library.nextTimelineId++;
    }

    static TimelineAsset* FindTimelineAssetById(TimelineLibraryComponent& library, uint32_t timelineId)
    {
        if (timelineId == 0) {
            return nullptr;
        }

        for (auto& asset : library.assets) {
            if (asset.id == timelineId) {
                return &asset;
            }
        }
        return nullptr;
    }

    static const TimelineAsset* FindTimelineAssetById(const TimelineLibraryComponent& library, uint32_t timelineId)
    {
        if (timelineId == 0) {
            return nullptr;
        }

        for (const auto& asset : library.assets) {
            if (asset.id == timelineId) {
                return &asset;
            }
        }
        return nullptr;
    }

    static TimelineAsset* FindTimelineAssetByAnimationIndex(TimelineLibraryComponent& library, int animationIndex)
    {
        for (auto& asset : library.assets) {
            if (asset.animationIndex == animationIndex) {
                return &asset;
            }
        }
        return nullptr;
    }

    static const TimelineAsset* FindTimelineAssetByAnimationIndex(const TimelineLibraryComponent& library, int animationIndex)
    {
        for (const auto& asset : library.assets) {
            if (asset.animationIndex == animationIndex) {
                return &asset;
            }
        }
        return nullptr;
    }

    static void SyncEditingTimelineIntoLibrary(TimelineLibraryComponent& library, TimelineAsset& editingAsset)
    {
        if (!HasTimelineAssetContent(editingAsset)) {
            return;
        }

        EnsureTimelineAssetIdentity(editingAsset, library);
        TimelineAsset* existing = FindTimelineAssetById(library, editingAsset.id);
        if (existing) {
            *existing = editingAsset;
            return;
        }

        library.assets.push_back(editingAsset);
    }

    static EntityID FindFirstMeshEntityRecursive(EntityID entity, Registry& registry)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return Entity::NULL_ID;
        }

        if (registry.GetComponent<MeshComponent>(entity)) {
            return entity;
        }

        const HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        if (!hierarchy) {
            return Entity::NULL_ID;
        }

        EntityID child = hierarchy->firstChild;
        while (!Entity::IsNull(child)) {
            EntityID found = FindFirstMeshEntityRecursive(child, registry);
            if (!Entity::IsNull(found)) {
                return found;
            }

            const HierarchyComponent* childHierarchy = registry.GetComponent<HierarchyComponent>(child);
            child = childHierarchy ? childHierarchy->nextSibling : Entity::NULL_ID;
        }

        return Entity::NULL_ID;
    }

    static float ComputePreviewFitRadius(const Model& model)
    {
        const auto bounds = model.GetWorldBounds();
        const DirectX::XMFLOAT3 ex = bounds.Extents;
        const float radius = std::sqrt(ex.x * ex.x + ex.y * ex.y + ex.z * ex.z);
        return radius > 0.01f ? radius : 1.0f;
    }

    static DirectX::XMFLOAT3 ComputePreviewFitForward(const Model& model)
    {
        const auto bounds = model.GetWorldBounds();
        const DirectX::XMFLOAT3 ex = bounds.Extents;

        float maxTmp = ex.x > ex.y ? ex.x : ex.y;
        float maxDim = maxTmp > ex.z ? maxTmp : ex.z;
        float minTmp = ex.x < ex.y ? ex.x : ex.y;
        float minDim = minTmp < ex.z ? minTmp : ex.z;

        bool isEffect = minDim < maxDim * 0.05f;
        float pitch = DirectX::XMConvertToRadians(25.0f);
        float yaw = DirectX::XMConvertToRadians(45.0f);
        if (isEffect) {
            if (ex.y == minDim) pitch = DirectX::XMConvertToRadians(60.0f);
            else if (ex.z == minDim) yaw = DirectX::XMConvertToRadians(10.0f);
            else if (ex.x == minDim) yaw = DirectX::XMConvertToRadians(80.0f);
        }

        DirectX::XMFLOAT3 forward = {
            -std::cos(pitch) * std::sin(yaw),
            -std::sin(pitch),
            std::cos(pitch) * std::cos(yaw)
        };

        using namespace DirectX;
        XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&forward));
        XMStoreFloat3(&forward, dir);
        return forward;
    }

    static float ComputePreviewFitDistance(const Model& model, float fovY)
    {
        const float radius = ComputePreviewFitRadius(model);
        const float safeFovY = fovY > 0.01f ? fovY : DirectX::XMConvertToRadians(45.0f);
        const float halfFov = safeFovY * 0.5f;
        const float safeSin = std::sin(halfFov);
        if (safeSin <= 0.0001f) {
            return radius * 3.0f;
        }

        const float distance = (radius / safeSin) * 1.3f;
        return distance > 1.0f ? distance : 1.0f;
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
        if (TimelineItemBuffer* existingBuffer = panel.m_registry->GetComponent<TimelineItemBuffer>(panel.m_previewEntity)) {
            StopActiveTimelineAudio(existingBuffer);
        }
        EntitySnapshot::DestroySubtree(panel.m_previewEntity, *panel.m_registry);
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

    DirectX::XMStoreFloat4x4(
        &transform.worldMatrix,
        DirectX::XMMatrixIdentity());

    DirectX::XMStoreFloat4x4(
        &transform.prevWorldMatrix,
        DirectX::XMMatrixIdentity());

    transform.worldPosition = transform.localPosition;
    transform.worldRotation = transform.localRotation;
    transform.worldScale = transform.localScale;



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

    const bool isPrefab = HasExtension(path, { ".prefab" });

    if (isPrefab) {
        if (!panel.m_registry) {
            return false;
        }

        EntitySnapshot::Snapshot snapshot;
        if (!PrefabSystem::LoadPrefabSnapshot(path, snapshot)) {
            return false;
        }

        Suspend(panel);

        EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(snapshot, *panel.m_registry);
        if (Entity::IsNull(restore.root) || !panel.m_registry->IsAlive(restore.root)) {
            return false;
        }

        if (!panel.m_registry->GetComponent<EffectPreviewTagComponent>(restore.root)) {
            panel.m_registry->AddComponent(restore.root, EffectPreviewTagComponent{});
        }

        if (auto* prefabInstance = panel.m_registry->GetComponent<PrefabInstanceComponent>(restore.root)) {
            prefabInstance->prefabAssetPath = path;
            prefabInstance->hasOverrides = false;
        }
        else {
            PrefabInstanceComponent newPrefabInstance{};
            newPrefabInstance.prefabAssetPath = path;
            newPrefabInstance.hasOverrides = false;
            panel.m_registry->AddComponent(restore.root, newPrefabInstance);
        }

        EntityID meshEntity = FindFirstMeshEntityRecursive(restore.root, *panel.m_registry);
        if (Entity::IsNull(meshEntity)) {
            EntitySnapshot::DestroySubtree(restore.root, *panel.m_registry);
            return false;
        }

        MeshComponent* mesh = panel.m_registry->GetComponent<MeshComponent>(meshEntity);
        if (!mesh || mesh->modelFilePath.empty()) {
            EntitySnapshot::DestroySubtree(restore.root, *panel.m_registry);
            return false;
        }

        std::shared_ptr<Model> model = mesh->model;
        if (!model) {
            model = ResourceManager::Instance().CreateModelInstance(mesh->modelFilePath);
            if (!model) {
                EntitySnapshot::DestroySubtree(restore.root, *panel.m_registry);
                return false;
            }
            mesh->model = model;
        }

        panel.m_previewEntity = restore.root;
        panel.m_previewEntityOwned = true;
        panel.m_ownedModel = std::move(model);
        panel.m_model = panel.m_ownedModel.get();
        panel.m_currentModelPath = mesh->modelFilePath;
        panel.ResetSelectionState();
        panel.m_selectedAnimIndex = 0;
        panel.m_previewRenderSize = { 0.0f, 0.0f };
        panel.m_previewModelScale = 1.0f;
        panel.m_sockets.clear();
        panel.m_colliderDirty = false;

        if (const auto* embeddedStateMachine = panel.m_registry->GetComponent<StateMachineAssetComponent>(restore.root)) {
            panel.m_stateMachineAsset = embeddedStateMachine->asset;
        }
        else {
            panel.m_stateMachineAsset = StateMachineAsset{};
        }
        panel.m_stateMachineDirty = false;

        panel.m_timelineAsset = TimelineAsset{};
        SyncTimelineAssetSelection(panel);
        panel.m_timelineDirty = false;

        if (const auto* embeddedInputMap = panel.m_registry->GetComponent<InputActionMapComponent>(restore.root)) {
            panel.m_inputMappingTab.SetEditingMap(embeddedInputMap->asset);
        }
        else {
            panel.m_inputMappingTab.ClearEditingMap();
        }

        PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*panel.m_registry, restore.root);
        PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, restore.root);
        PlayerRuntimeSetup::ResetPlayerRuntimeState(*panel.m_registry, restore.root);

        ImportSocketsFromPreviewEntity(panel);
        RebuildPreviewTimelineRuntimeData(panel);
        const auto bounds = panel.m_model->GetWorldBounds();
        panel.m_pendingCameraFitTarget = bounds.Center;
        panel.m_pendingCameraFitRadius = ComputePreviewFitRadius(*panel.m_model);
        panel.m_pendingCameraFitForward = ComputePreviewFitForward(*panel.m_model);
        panel.m_pendingCameraFitDistance = ComputePreviewFitDistance(*panel.m_model, panel.m_sharedSceneCameraFovY);
        panel.m_hasPendingCameraFit = true;
        return true;
    }

    std::shared_ptr<Model> model = ResourceManager::Instance().CreateModelInstance(path);
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
    panel.m_colliderDirty = false;
    panel.m_timelineDirty = false;
    panel.m_timelineAsset = TimelineAsset{};
    panel.m_stateMachineDirty = false;
    EnsureOwnedPreviewEntity(panel);
    SyncTimelineAssetSelection(panel);
    const auto bounds = panel.m_model->GetWorldBounds();
    panel.m_pendingCameraFitTarget = bounds.Center;
    panel.m_pendingCameraFitRadius = ComputePreviewFitRadius(*panel.m_model);
    panel.m_pendingCameraFitForward = ComputePreviewFitForward(*panel.m_model);
    panel.m_pendingCameraFitDistance = ComputePreviewFitDistance(*panel.m_model, panel.m_sharedSceneCameraFovY);
    panel.m_hasPendingCameraFit = true;
    return true;
}


bool PlayerEditorSession::SavePrefabDocument(PlayerEditorPanel& panel, bool saveAs)
{
    if (!panel.m_registry || Entity::IsNull(panel.m_previewEntity)) {
        return false;
    }

    // 保存前にメモリ上のステートマシンを正規化し、Prefab に
    // 無効な遷移、たとえば古い 0xDDDDDDDD の fromState/toState、
    // が残って実行時評価を隠してしまう状況を防ぐ。
    panel.RemoveBrokenTransitions();

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
        }
        else {
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

    if (!PrefabSystem::SaveEntityToPrefabPath(panel.m_previewEntity, *panel.m_registry, prefabPath)) {
        return false;
    }

    panel.m_timelineDirty = false;
    panel.m_stateMachineDirty = false;
    panel.m_socketDirty = false;
    panel.m_colliderDirty = false;
    panel.m_inputMappingTab.SetEditingMap(panel.m_inputMappingTab.GetEditingMap());
    return true;
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

    PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*panel.m_registry, panel.m_previewEntity);

    if (auto* embeddedStateMachine = panel.m_registry->GetComponent<StateMachineAssetComponent>(panel.m_previewEntity)) {
        embeddedStateMachine->asset = panel.m_stateMachineAsset;
    }

    if (auto* timelineLibrary = panel.m_registry->GetComponent<TimelineLibraryComponent>(panel.m_previewEntity)) {
        SyncEditingTimelineIntoLibrary(*timelineLibrary, panel.m_timelineAsset);
    }

    if (auto* embeddedInputMap = panel.m_registry->GetComponent<InputActionMapComponent>(panel.m_previewEntity)) {
        embeddedInputMap->asset = panel.m_inputMappingTab.GetEditingMap();
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

    const TimelineAsset* sourceTimeline = nullptr;
    if (HasTimelineAssetContent(panel.m_timelineAsset)) {
        sourceTimeline = &panel.m_timelineAsset;
    }

    if (!sourceTimeline) {
        if (auto* existing = panel.m_registry->GetComponent<TimelineComponent>(panel.m_previewEntity)) {
            *existing = TimelineComponent{};
        }
        if (auto* existingBuffer = panel.m_registry->GetComponent<TimelineItemBuffer>(panel.m_previewEntity)) {
            StopActiveTimelineAudio(existingBuffer);
            existingBuffer->items.clear();
        }
        return;
    }

    if (!TimelineAssetRuntimeBuilder::Build(*sourceTimeline, panel.m_selectedAnimIndex, timeline, buffer)) {
        return;
    }

    if (auto* existing = panel.m_registry->GetComponent<TimelineComponent>(panel.m_previewEntity)) {
        *existing = timeline;
    }
    else {
        panel.m_registry->AddComponent(panel.m_previewEntity, timeline);
    }

    if (auto* existingBuffer = panel.m_registry->GetComponent<TimelineItemBuffer>(panel.m_previewEntity)) {
        StopActiveTimelineAudio(existingBuffer);
        *existingBuffer = buffer;
    }
    else {
        panel.m_registry->AddComponent(panel.m_previewEntity, buffer);
    }
}

void PlayerEditorSession::SyncPreviewTimelinePlayback(PlayerEditorPanel& panel)
{
    if (!panel.m_previewState.IsActive()) {
        return;
    }

    const float fps = panel.m_timelineAsset.fps > 0.0f ? panel.m_timelineAsset.fps : 60.0f;
    float previewTime = panel.m_playheadFrame / fps;
    const float animationDuration = panel.GetSelectedAnimationDurationSeconds();
    if (animationDuration > 0.0f && previewTime > animationDuration) {
        if (panel.m_previewState.GetDriver()->IsLoop()) {
            previewTime = std::fmod(previewTime, animationDuration);
        } else {
            previewTime = animationDuration;
        }
    }
    panel.m_previewState.SetTime(previewTime);
}

void PlayerEditorSession::SyncTimelineAssetSelection(PlayerEditorPanel& panel)
{
    TimelineLibraryComponent* timelineLibrary = nullptr;
    if (panel.m_registry) {
        if (!Entity::IsNull(panel.m_previewEntity) && panel.m_registry->IsAlive(panel.m_previewEntity)) {
            timelineLibrary = panel.m_registry->GetComponent<TimelineLibraryComponent>(panel.m_previewEntity);
        }

        if (!timelineLibrary &&
            !Entity::IsNull(panel.m_selectedEntity) &&
            panel.m_registry->IsAlive(panel.m_selectedEntity)) {
            timelineLibrary = panel.m_registry->GetComponent<TimelineLibraryComponent>(panel.m_selectedEntity);
        }
    }

    if (timelineLibrary && HasTimelineAssetContent(panel.m_timelineAsset)) {
        SyncEditingTimelineIntoLibrary(*timelineLibrary, panel.m_timelineAsset);
    }

    const TimelineAsset* matchedAsset = nullptr;
    if (timelineLibrary) {
        if (panel.m_selectedAnimIndex >= 0) {
            matchedAsset = FindTimelineAssetByAnimationIndex(*timelineLibrary, panel.m_selectedAnimIndex);
            if (!matchedAsset) {
                matchedAsset = FindTimelineAssetByAnimationIndex(*timelineLibrary, -1);
            }
        }
        else if (!timelineLibrary->assets.empty()) {
            matchedAsset = &timelineLibrary->assets.front();
        }
    }

    if (matchedAsset) {
        panel.m_timelineAsset = *matchedAsset;
    }
    else {
        panel.m_timelineAsset = TimelineAsset{};
        panel.m_timelineAsset.animationIndex = panel.m_selectedAnimIndex;
        panel.m_timelineAsset.ownerModelPath = panel.m_currentModelPath;
        const float selectedAnimationDuration = panel.GetSelectedAnimationDurationSeconds();
        if (selectedAnimationDuration > 0.0f) {
            panel.m_timelineAsset.duration = selectedAnimationDuration;
        }
    }

    if (panel.m_timelineAsset.animationIndex < 0) {
        panel.m_timelineAsset.animationIndex = panel.m_selectedAnimIndex;
    }
    if (panel.m_timelineAsset.ownerModelPath.empty()) {
        panel.m_timelineAsset.ownerModelPath = panel.m_currentModelPath;
    }
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

    if (const auto* embeddedStateMachine = panel.m_registry->GetComponent<StateMachineAssetComponent>(panel.m_selectedEntity)) {
        panel.m_stateMachineAsset = embeddedStateMachine->asset;
        panel.m_stateMachineDirty = false;
    }

    panel.m_timelineAsset = TimelineAsset{};
    SyncTimelineAssetSelection(panel);
    panel.m_timelineDirty = false;
    panel.m_colliderDirty = false;

    if (const auto* embeddedInputMap = panel.m_registry->GetComponent<InputActionMapComponent>(panel.m_selectedEntity)) {
        panel.m_inputMappingTab.SetEditingMap(embeddedInputMap->asset);
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
    }
    else {
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
