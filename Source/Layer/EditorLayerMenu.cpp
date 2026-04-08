#include "EditorLayerInternal.h"

namespace {
    struct ClipboardEntry
    {
        EntitySnapshot::Snapshot snapshot;
        EntityID parentEntity = Entity::NULL_ID;
    };

    std::vector<ClipboardEntry> s_entityClipboard;

    std::vector<ClipboardEntry> CopySelectionRoots(Registry& registry, const EditorSelection& selection)
    {
        std::vector<ClipboardEntry> clipboard;
        for (EntityID selectedRoot : GetSelectedRootEntities(registry, selection)) {
            EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedRoot, registry);
            if (snapshot.nodes.empty()) {
                continue;
            }
            EntityID parentEntity = Entity::NULL_ID;
            if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(selectedRoot)) {
                parentEntity = hierarchy->parent;
            }
            clipboard.push_back(ClipboardEntry{ std::move(snapshot), parentEntity });
        }
        return clipboard;
    }
}

void EditorLayer::ExecuteUndo()
{
    if (m_gameLayer) {
        UndoSystem::Instance().Undo(m_gameLayer->GetRegistry());
    }
}

void EditorLayer::ExecuteRedo()
{
    if (m_gameLayer) {
        UndoSystem::Instance().Redo(m_gameLayer->GetRegistry());
    }
}

void EditorLayer::ExecuteDuplicateSelection()
{
    if (!m_gameLayer) {
        return;
    }
    auto& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity || selection.GetSelectedEntityCount() == 0) {
        return;
    }
    const std::vector<EntityID> liveRoots = DuplicateSelectionRoots(registry, selection);
    if (!liveRoots.empty()) {
        selection.SetEntitySelection(liveRoots, liveRoots.back());
    }
}

void EditorLayer::ExecuteDeleteSelection()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity || selection.GetSelectedEntityCount() == 0) {
        return;
    }

    const std::vector<EntityID> selectedRoots = GetSelectedRootEntities(registry, selection);
    auto composite = std::make_unique<CompositeUndoAction>("Delete Entities");
    bool canDeleteAny = false;
    for (EntityID selectedRoot : selectedRoots) {
        if (!PrefabSystem::CanDelete(selectedRoot, registry)) {
            LOG_WARN("[Prefab] Prefab instance children cannot be deleted directly. Use Unpack first.");
            continue;
        }
        EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedRoot, registry);
        if (!snapshot.nodes.empty()) {
            composite->Add(std::make_unique<DeleteEntityAction>(std::move(snapshot), selectedRoot));
            canDeleteAny = true;
        }
    }
    if (canDeleteAny) {
        UndoSystem::Instance().ExecuteAction(std::move(composite), registry);
        selection.Clear();
    }
}

void EditorLayer::ExecuteFrameSelected()
{
    FocusSelectedEntity();
}

void EditorLayer::ExecuteSelectAll()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    std::vector<EntityID> entities;
    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<HierarchyComponent>())) {
            continue;
        }
        const auto& archetypeEntities = archetype->GetEntities();
        for (EntityID entity : archetypeEntities) {
            if (!registry.IsAlive(entity) || IsUtilityEntity(registry, entity)) {
                continue;
            }
            entities.push_back(entity);
        }
    }
    if (!entities.empty()) {
        EditorSelection::Instance().SetEntitySelection(entities, entities.front());
    }
}

void EditorLayer::ExecuteDeselect()
{
    EditorSelection::Instance().Clear();
}

void EditorLayer::ExecuteRenameSelection()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    auto* name = registry.GetComponent<NameComponent>(entity);
    if (!name) {
        return;
    }
    strcpy_s(m_renameBuffer, name->name.c_str());
    m_requestRenamePopup = true;
}

void EditorLayer::ExecuteCopySelection()
{
    if (!m_gameLayer) {
        return;
    }
    auto& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity || selection.GetSelectedEntityCount() == 0) {
        return;
    }
    s_entityClipboard = CopySelectionRoots(registry, selection);
}

void EditorLayer::ExecutePasteSelection()
{
    if (!m_gameLayer || s_entityClipboard.empty()) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    EntityID explicitParent = Entity::NULL_ID;
    if (selection.GetType() == SelectionType::Entity) {
        const EntityID selectedEntity = selection.GetPrimaryEntity();
        if (!Entity::IsNull(selectedEntity) && registry.IsAlive(selectedEntity) &&
            PrefabSystem::CanCreateChild(selectedEntity, registry)) {
            explicitParent = selectedEntity;
        }
    }

    auto composite = std::make_unique<CompositeUndoAction>("Paste Entities");
    std::vector<CreateEntityAction*> actions;
    for (const ClipboardEntry& entry : s_entityClipboard) {
        EntitySnapshot::Snapshot snapshot = entry.snapshot;
        if (snapshot.nodes.empty()) {
            continue;
        }
        EntitySnapshot::AppendRootNameSuffix(snapshot, " (Copy)");
        EntityID parentEntity = explicitParent;
        if (Entity::IsNull(parentEntity) && !Entity::IsNull(entry.parentEntity) && registry.IsAlive(entry.parentEntity)) {
            parentEntity = entry.parentEntity;
        }
        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parentEntity, "Paste Entity");
        actions.push_back(action.get());
        composite->Add(std::move(action));
    }

    if (composite->Empty()) {
        return;
    }

    UndoSystem::Instance().ExecuteAction(std::move(composite), registry);
    std::vector<EntityID> pastedEntities;
    pastedEntities.reserve(actions.size());
    for (CreateEntityAction* action : actions) {
        if (!Entity::IsNull(action->GetLiveRoot())) {
            pastedEntities.push_back(action->GetLiveRoot());
        }
    }
    if (!pastedEntities.empty()) {
        selection.SetEntitySelection(pastedEntities, pastedEntities.back());
    }
}

void EditorLayer::ExecuteResetTransform()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
        return;
    }

    auto composite = std::make_unique<CompositeUndoAction>("Reset Transform");
    bool changed = false;
    if (auto* rect = registry.GetComponent<RectTransformComponent>(entity)) {
        RectTransformComponent before = *rect;
        rect->anchoredPosition = { 0.0f, 0.0f };
        rect->sizeDelta = { 100.0f, 100.0f };
        rect->anchorMin = { 0.5f, 0.5f };
        rect->anchorMax = { 0.5f, 0.5f };
        rect->pivot = { 0.5f, 0.5f };
        rect->rotationZ = 0.0f;
        rect->scale2D = { 1.0f, 1.0f };
        composite->Add(std::make_unique<ComponentUndoAction<RectTransformComponent>>(entity, before, *rect));
        changed = true;
    }

    if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
        TransformComponent before = *transform;
        transform->localPosition = { 0.0f, 0.0f, 0.0f };
        transform->localRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        transform->localScale = { 1.0f, 1.0f, 1.0f };
        transform->isDirty = true;
        if (auto* rect = registry.GetComponent<RectTransformComponent>(entity)) {
            SyncRectTransformToTransform(*rect, *transform);
        }
        composite->Add(std::make_unique<ComponentUndoAction<TransformComponent>>(entity, before, *transform));
        changed = true;
    }

    if (changed && !composite->Empty()) {
        UndoSystem::Instance().RecordAction(std::move(composite));
        HierarchySystem::MarkDirtyRecursive(entity, registry);
        PrefabSystem::MarkPrefabOverride(entity, registry);
    }
}

void EditorLayer::ExecuteCreateEmptyParent()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity || selection.GetSelectedEntityCount() == 0) {
        return;
    }

    const std::vector<EntityID> selectedRoots = GetSelectedRootEntities(registry, selection);
    if (selectedRoots.empty()) {
        return;
    }

    std::vector<EntityID> oldParents;
    oldParents.reserve(selectedRoots.size());
    EntityID commonParent = Entity::NULL_ID;
    bool commonParentInitialized = false;
    for (EntityID root : selectedRoots) {
        EntityID parentEntity = Entity::NULL_ID;
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(root)) {
            parentEntity = hierarchy->parent;
        }
        oldParents.push_back(parentEntity);
        if (!commonParentInitialized) {
            commonParent = parentEntity;
            commonParentInitialized = true;
        } else if (commonParent != parentEntity) {
            commonParent = Entity::NULL_ID;
        }
    }

    auto action = std::make_unique<CreateEmptyParentAction>(selectedRoots, oldParents, commonParent);
    auto* actionPtr = action.get();
    UndoSystem::Instance().ExecuteAction(std::move(action), registry);
    if (!Entity::IsNull(actionPtr->GetLiveRoot())) {
        EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
    }
}

void EditorLayer::ExecuteUnpackPrefab()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    EntityID root = PrefabSystem::FindPrefabRoot(entity, registry);
    if (Entity::IsNull(root)) {
        return;
    }
    auto* prefabInstance = registry.GetComponent<PrefabInstanceComponent>(root);
    if (!prefabInstance) {
        return;
    }

    UndoSystem::Instance().ExecuteAction(
        std::make_unique<OptionalComponentUndoAction<PrefabInstanceComponent>>(
            root,
            std::optional<PrefabInstanceComponent>(*prefabInstance),
            std::nullopt,
            "Unpack Prefab"),
        registry);
    EditorSelection::Instance().SelectEntity(root);
}

void EditorLayer::ExecuteFocusSearch()
{
    m_showHierarchy = true;
    RequestWindowFocus(WindowFocusTarget::Hierarchy);
    HierarchyECSUI::RequestSearchFocus();
}

void EditorLayer::ExecuteResetView()
{
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        m_editor2DCenter = { 0.0f, 0.0f };
        m_editor2DZoom = 10.0f;
    } else {
        m_editorCameraPosition = { 0.0f, 12.0f, -80.0f };
        m_editorCameraYaw = 0.0f;
        m_editorCameraPitch = 0.0f;
        m_editorCameraUserOverride = false;
        m_editorCameraAutoFramed = false;
    }
}

void EditorLayer::ExecuteGamePlay()
{
    EngineKernel& kernel = EngineKernel::Instance();
    if (kernel.GetMode() == EngineMode::Editor || kernel.GetMode() == EngineMode::Pause) {
        kernel.Play();
    }
}

void EditorLayer::ExecuteGamePauseToggle()
{
    EngineKernel& kernel = EngineKernel::Instance();
    if (kernel.GetMode() == EngineMode::Play || kernel.GetMode() == EngineMode::Pause) {
        kernel.Pause();
    }
}

void EditorLayer::ExecuteGameStop()
{
    EngineKernel& kernel = EngineKernel::Instance();
    if (kernel.GetMode() != EngineMode::Editor) {
        kernel.Stop();
    }
}

void EditorLayer::ExecuteGameStep()
{
    EngineKernel& kernel = EngineKernel::Instance();
    if (kernel.GetMode() == EngineMode::Pause) {
        kernel.Step();
    }
}

void EditorLayer::ExecuteGameResetPreview()
{
    m_gameViewResolutionPreset = GameViewResolutionPreset::Free;
    m_gameViewAspectPolicy = GameViewAspectPolicy::Fit;
    m_gameViewScalePolicy = GameViewScalePolicy::AutoFit;
    m_gameViewShowSafeArea = false;
    m_gameViewShowPixelPreview = false;
    m_gameViewShowStatsOverlay = false;
    m_gameViewShowUIOverlay = true;
    m_gameViewShow2DOverlay = true;
}

void EditorLayer::ExecuteCloseSecondaryWindows()
{
    m_showLightingWindow = false;
    m_showAudioWindow = false;
    m_showRenderPassesWindow = false;
    m_showGridSettingsWindow = false;
    m_showGBufferDebug = false;
    m_showPlayerEditor = false;
    m_showEffectEditor = false;
    m_activeWorkspace = WorkspaceTab::LevelEditor;
    if (m_maximizedWindow == WindowFocusTarget::Lighting ||
        m_maximizedWindow == WindowFocusTarget::Audio ||
        m_maximizedWindow == WindowFocusTarget::RenderPasses ||
        m_maximizedWindow == WindowFocusTarget::GridSettings ||
        m_maximizedWindow == WindowFocusTarget::GBufferDebug ||
        m_maximizedWindow == WindowFocusTarget::PlayerEditor ||
        m_maximizedWindow == WindowFocusTarget::EffectEditor) {
        m_maximizedWindow = WindowFocusTarget::None;
    }
}

void EditorLayer::ExecuteResetLayout()
{
    m_showSceneView = true;
    m_showGameView = true;
    m_showHierarchy = true;
    m_showInspector = true;
    m_showAssetBrowser = true;
    m_showConsole = true;
    m_showLightingWindow = false;
    m_showAudioWindow = false;
    m_showRenderPassesWindow = false;
    m_showGridSettingsWindow = false;
    m_showGBufferDebug = false;
    m_showStatusBar = true;
    m_showMainToolbar = true;
    m_showSceneGrid = true;
    m_showSceneGizmo = true;
    m_showSceneStatsOverlay = false;
    m_showSceneSelectionOutline = false;
    m_showSceneLightIcons = true;
    m_showSceneCameraIcons = true;
    m_showSceneBounds = false;
    m_showSceneCollision = false;
    m_showPlayerEditor = false;
    m_showEffectEditor = false;
    m_sceneShadingMode = SceneShadingMode::Lit;
    m_activeWorkspace = WorkspaceTab::LevelEditor;
    m_maximizedWindow = WindowFocusTarget::None;
    m_forceDockLayoutReset = true;
}

void EditorLayer::ExecuteMaximizeActivePanel()
{
    if (m_lastFocusedWindow == WindowFocusTarget::None) {
        return;
    }
    if (m_lastFocusedWindow == WindowFocusTarget::PlayerEditor) {
        m_showPlayerEditor = true;
        m_activeWorkspace = WorkspaceTab::PlayerEditor;
        m_pendingWindowFocus = WindowFocusTarget::PlayerEditor;
        return;
    }
    if (m_lastFocusedWindow == WindowFocusTarget::EffectEditor) {
        m_showEffectEditor = true;
        m_activeWorkspace = WorkspaceTab::EffectEditor;
        m_pendingWindowFocus = WindowFocusTarget::EffectEditor;
        return;
    }
    m_maximizedWindow = (m_maximizedWindow == m_lastFocusedWindow) ? WindowFocusTarget::None : m_lastFocusedWindow;
    m_forceDockLayoutReset = true;
}

void EditorLayer::RequestWindowFocus(WindowFocusTarget target)
{
    if (m_maximizedWindow != WindowFocusTarget::None && m_maximizedWindow != target) {
        m_maximizedWindow = WindowFocusTarget::None;
    }
    switch (target) {
    case WindowFocusTarget::SceneView: m_showSceneView = true; break;
    case WindowFocusTarget::GameView: m_showGameView = true; break;
    case WindowFocusTarget::Hierarchy: m_showHierarchy = true; break;
    case WindowFocusTarget::Inspector: m_showInspector = true; break;
    case WindowFocusTarget::AssetBrowser: m_showAssetBrowser = true; break;
    case WindowFocusTarget::Console: m_showConsole = true; break;
    case WindowFocusTarget::Lighting: m_showLightingWindow = true; break;
    case WindowFocusTarget::Audio: m_showAudioWindow = true; break;
    case WindowFocusTarget::RenderPasses: m_showRenderPassesWindow = true; break;
    case WindowFocusTarget::GridSettings: m_showGridSettingsWindow = true; break;
    case WindowFocusTarget::GBufferDebug: m_showGBufferDebug = true; break;
    case WindowFocusTarget::PlayerEditor:
        m_showPlayerEditor = true;
        m_activeWorkspace = WorkspaceTab::PlayerEditor;
        break;
    case WindowFocusTarget::EffectEditor:
        m_showEffectEditor = true;
        m_activeWorkspace = WorkspaceTab::EffectEditor;
        break;
    default: break;
    }
    if (target != WindowFocusTarget::PlayerEditor && target != WindowFocusTarget::EffectEditor) {
        m_activeWorkspace = WorkspaceTab::LevelEditor;
    }
    m_pendingWindowFocus = target;
}

void EditorLayer::ApplyPendingWindowFocus(WindowFocusTarget target)
{
    if (m_pendingWindowFocus == target) {
        ImGui::SetNextWindowFocus();
        m_pendingWindowFocus = WindowFocusTarget::None;
    }
}

void EditorLayer::SetLastFocusedWindow(WindowFocusTarget target, bool focused)
{
    if (focused) {
        m_lastFocusedWindow = target;
    }
}

void EditorLayer::SaveCameraBookmark(size_t slot)
{
    if (slot >= m_cameraBookmarks.size()) {
        return;
    }
    auto& bookmark = m_cameraBookmarks[slot];
    bookmark.valid = true;
    bookmark.mode = m_sceneViewMode;
    bookmark.cameraPosition = m_editorCameraPosition;
    bookmark.cameraYaw = m_editorCameraYaw;
    bookmark.cameraPitch = m_editorCameraPitch;
    bookmark.center2D = m_editor2DCenter;
    bookmark.zoom2D = m_editor2DZoom;
}

void EditorLayer::LoadCameraBookmark(size_t slot)
{
    if (slot >= m_cameraBookmarks.size()) {
        return;
    }
    const auto& bookmark = m_cameraBookmarks[slot];
    if (!bookmark.valid) {
        return;
    }
    m_sceneViewMode = bookmark.mode;
    m_editorCameraPosition = bookmark.cameraPosition;
    m_editorCameraYaw = bookmark.cameraYaw;
    m_editorCameraPitch = bookmark.cameraPitch;
    m_editor2DCenter = bookmark.center2D;
    m_editor2DZoom = bookmark.zoom2D;
    RequestWindowFocus(WindowFocusTarget::SceneView);
}


void EditorLayer::DrawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        Registry* registry = m_gameLayer ? &m_gameLayer->GetRegistry() : nullptr;
        auto& selection = EditorSelection::Instance();
        const bool hasEntitySelection = selection.GetType() == SelectionType::Entity && selection.GetSelectedEntityCount() > 0;
        const bool hasPrimarySelection = hasEntitySelection && !Entity::IsNull(selection.GetPrimaryEntity());
        const bool canPaste = !s_entityClipboard.empty();
        bool canUnpackPrefab = false;
        if (registry && hasPrimarySelection) {
            canUnpackPrefab = !Entity::IsNull(PrefabSystem::FindPrefabRoot(selection.GetPrimaryEntity(), *registry));
        }

        if (ImGui::BeginMenu("File(F)")) {
            if (ImGui::MenuItem(ICON_FA_FILE " New Scene", "Ctrl+N")) {
                m_requestNewScene = true;
            }
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Scene...", "Ctrl+O")) {
                m_requestOpenScene = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save", "Ctrl+S")) {
                SaveCurrentScene();
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save As...")) {
                m_requestSaveSceneAs = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit(E)")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, UndoSystem::Instance().CanUndoECS())) {
                ExecuteUndo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y / Ctrl+Shift+Z", false, UndoSystem::Instance().CanRedoECS())) {
                ExecuteRedo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasEntitySelection)) {
                ExecuteCopySelection();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPaste)) {
                ExecutePasteSelection();
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasEntitySelection)) {
                ExecuteDuplicateSelection();
            }
            if (ImGui::MenuItem("Delete", "Delete", false, hasEntitySelection)) {
                ExecuteDeleteSelection();
            }
            if (ImGui::MenuItem("Rename", "F2", false, hasPrimarySelection)) {
                ExecuteRenameSelection();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Frame Selected", "F", false, hasPrimarySelection)) {
                ExecuteFrameSelected();
            }
            if (ImGui::MenuItem("Select All", "Ctrl+A", false, registry != nullptr)) {
                ExecuteSelectAll();
            }
            if (ImGui::MenuItem("Deselect", "Esc", false, selection.GetType() != SelectionType::None)) {
                ExecuteDeselect();
            }
            if (ImGui::MenuItem("Focus Search", "Ctrl+K")) {
                ExecuteFocusSearch();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Transform", nullptr, false, hasPrimarySelection)) {
                ExecuteResetTransform();
            }
            if (ImGui::MenuItem("Create Empty Parent", nullptr, false, hasEntitySelection)) {
                ExecuteCreateEmptyParent();
            }
            if (ImGui::MenuItem("Unpack Prefab", nullptr, false, canUnpackPrefab)) {
                ExecuteUnpackPrefab();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View(V)")) {
            if (ImGui::MenuItem("3D Mode", nullptr, m_sceneViewMode == SceneViewMode::Mode3D)) {
                m_sceneViewMode = SceneViewMode::Mode3D;
            }
            if (ImGui::MenuItem("2D Mode", nullptr, m_sceneViewMode == SceneViewMode::Mode2D)) {
                m_sceneViewMode = SceneViewMode::Mode2D;
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Overlays")) {
                ImGui::MenuItem("Show Grid", nullptr, &m_showSceneGrid);
                ImGui::MenuItem("Show Gizmo", nullptr, &m_showSceneGizmo);
                ImGui::MenuItem("Show Stats Overlay", nullptr, &m_showSceneStatsOverlay);
                ImGui::MenuItem("Show Selection Outline", nullptr, &m_showSceneSelectionOutline);
                ImGui::MenuItem("Show Light Icons", nullptr, &m_showSceneLightIcons);
                ImGui::MenuItem("Show Camera Icons", nullptr, &m_showSceneCameraIcons);
                ImGui::MenuItem("Show Bounds", nullptr, &m_showSceneBounds);
                ImGui::MenuItem("Show Collision", nullptr, &m_showSceneCollision);
                ImGui::MenuItem("Show Safe Area", nullptr, &m_gameViewShowSafeArea);
                ImGui::MenuItem("Show Pixel Preview", nullptr, &m_gameViewShowPixelPreview);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Grid")) {
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::DragFloat("Cell Size", &m_sceneGridCellSize, 0.5f, 1.0f, 500.0f, "%.1f")) {
                    m_sceneGridCellSize = (std::clamp)(m_sceneGridCellSize, 1.0f, 500.0f);
                }
                if (ImGui::SliderInt("Half Line Count", &m_sceneGridHalfLineCount, 4, 128)) {
                    m_sceneGridHalfLineCount = (std::clamp)(m_sceneGridHalfLineCount, 4, 128);
                }
                if (ImGui::MenuItem("Open Grid Settings...")) {
                    m_showGridSettingsWindow = true;
                    RequestWindowFocus(WindowFocusTarget::GridSettings);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Shading")) {
                if (ImGui::MenuItem("Lit", nullptr, m_sceneShadingMode == SceneShadingMode::Lit)) {
                    m_sceneShadingMode = SceneShadingMode::Lit;
                }
                if (ImGui::MenuItem("Unlit", nullptr, m_sceneShadingMode == SceneShadingMode::Unlit)) {
                    m_sceneShadingMode = SceneShadingMode::Unlit;
                }
                if (ImGui::MenuItem("Wireframe", nullptr, m_sceneShadingMode == SceneShadingMode::Wireframe)) {
                    m_sceneShadingMode = SceneShadingMode::Wireframe;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Snap")) {
                ImGui::MenuItem("Translate", nullptr, &m_translateSnapEnabled);
                ImGui::MenuItem("Rotate", nullptr, &m_rotateSnapEnabled);
                ImGui::MenuItem("Scale", nullptr, &m_scaleSnapEnabled);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Camera Speed")) {
                if (ImGui::MenuItem("Slow", nullptr, m_cameraMoveSpeed == 5.0f)) m_cameraMoveSpeed = 5.0f;
                if (ImGui::MenuItem("Normal", nullptr, m_cameraMoveSpeed == 20.0f)) m_cameraMoveSpeed = 20.0f;
                if (ImGui::MenuItem("Fast", nullptr, m_cameraMoveSpeed == 50.0f)) m_cameraMoveSpeed = 50.0f;
                if (ImGui::MenuItem("Very Fast", nullptr, m_cameraMoveSpeed == 100.0f)) m_cameraMoveSpeed = 100.0f;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Bookmarks")) {
                for (size_t i = 0; i < m_cameraBookmarks.size(); ++i) {
                    std::string saveLabel = "Save Slot " + std::to_string(i + 1);
                    std::string recallLabel = "Recall Slot " + std::to_string(i + 1);
                    if (ImGui::MenuItem(saveLabel.c_str())) {
                        SaveCameraBookmark(i);
                    }
                    if (ImGui::MenuItem(recallLabel.c_str(), nullptr, false, m_cameraBookmarks[i].valid)) {
                        LoadCameraBookmark(i);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset View")) {
                ExecuteResetView();
            }
            if (ImGui::MenuItem("Focus Scene View")) {
                RequestWindowFocus(WindowFocusTarget::SceneView);
            }
            if (ImGui::MenuItem("Focus Game View")) {
                RequestWindowFocus(WindowFocusTarget::GameView);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Game(G)")) {
            const EngineMode mode = EngineKernel::Instance().GetMode();
            if (ImGui::MenuItem("Play", nullptr, mode == EngineMode::Play, mode == EngineMode::Editor || mode == EngineMode::Pause)) {
                ExecuteGamePlay();
            }
            if (ImGui::MenuItem("Pause", nullptr, mode == EngineMode::Pause, mode == EngineMode::Play || mode == EngineMode::Pause)) {
                ExecuteGamePauseToggle();
            }
            if (ImGui::MenuItem("Step", nullptr, false, mode == EngineMode::Pause)) {
                ExecuteGameStep();
            }
            if (ImGui::MenuItem("Stop", nullptr, false, mode != EngineMode::Editor)) {
                ExecuteGameStop();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Focus Game View")) {
                RequestWindowFocus(WindowFocusTarget::GameView);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Resolution Preset")) {
                const GameViewResolutionPreset presets[] = {
                    GameViewResolutionPreset::Free,
                    GameViewResolutionPreset::HD1080,
                    GameViewResolutionPreset::HD720,
                    GameViewResolutionPreset::Portrait1080x1920,
                    GameViewResolutionPreset::Portrait750x1334
                };
                for (GameViewResolutionPreset preset : presets) {
                    if (ImGui::MenuItem(GetGameViewPresetLabel(preset), nullptr, m_gameViewResolutionPreset == preset)) {
                        m_gameViewResolutionPreset = preset;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Aspect Policy")) {
                if (ImGui::MenuItem("Fit", nullptr, m_gameViewAspectPolicy == GameViewAspectPolicy::Fit)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::Fit;
                }
                if (ImGui::MenuItem("Fill", nullptr, m_gameViewAspectPolicy == GameViewAspectPolicy::Fill)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::Fill;
                }
                if (ImGui::MenuItem("1:1 Pixel", nullptr, m_gameViewAspectPolicy == GameViewAspectPolicy::PixelPerfect)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::PixelPerfect;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Scale")) {
                if (ImGui::MenuItem("Auto", nullptr, m_gameViewScalePolicy == GameViewScalePolicy::AutoFit)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::AutoFit;
                }
                if (ImGui::MenuItem("1x", nullptr, m_gameViewScalePolicy == GameViewScalePolicy::Scale1x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale1x;
                }
                if (ImGui::MenuItem("2x", nullptr, m_gameViewScalePolicy == GameViewScalePolicy::Scale2x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale2x;
                }
                if (ImGui::MenuItem("3x", nullptr, m_gameViewScalePolicy == GameViewScalePolicy::Scale3x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale3x;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::MenuItem("Safe Area", nullptr, &m_gameViewShowSafeArea);
            ImGui::MenuItem("Pixel Preview", nullptr, &m_gameViewShowPixelPreview);
            ImGui::MenuItem("Stats Overlay", nullptr, &m_gameViewShowStatsOverlay);
            ImGui::MenuItem("UI Overlay", nullptr, &m_gameViewShowUIOverlay);
            ImGui::MenuItem("2D Overlay", nullptr, &m_gameViewShow2DOverlay);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Preview")) {
                ExecuteGameResetPreview();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window(W)")) {
            ImGui::MenuItem(kSceneViewWindowTitle, nullptr, &m_showSceneView);
            ImGui::MenuItem(kGameViewWindowTitle, nullptr, &m_showGameView);
            ImGui::MenuItem(kHierarchyWindowTitle, nullptr, &m_showHierarchy);
            ImGui::MenuItem(kInspectorWindowTitle, nullptr, &m_showInspector);
            ImGui::MenuItem(kAssetBrowserWindowTitle, nullptr, &m_showAssetBrowser);
            ImGui::MenuItem(kConsoleWindowTitle, nullptr, &m_showConsole);
            ImGui::Separator();
            ImGui::MenuItem(ICON_FA_SUN " Lighting Settings", nullptr, &m_showLightingWindow);
            ImGui::MenuItem(kAudioWindowTitle, nullptr, &m_showAudioWindow);
            ImGui::MenuItem("Render Passes", nullptr, &m_showRenderPassesWindow);
            ImGui::MenuItem("Grid Settings", nullptr, &m_showGridSettingsWindow);
            ImGui::MenuItem(ICON_FA_IMAGES " G-Buffer Debug", nullptr, &m_showGBufferDebug);
            ImGui::MenuItem(ICON_FA_GAMEPAD " Input Debug", nullptr, &m_showInputDebug);
            bool showPlayerEditor = m_showPlayerEditor;
            if (ImGui::MenuItem(ICON_FA_USER " Player Editor", nullptr, &showPlayerEditor)) {
                m_showPlayerEditor = showPlayerEditor;
                if (m_showPlayerEditor) {
                    m_activeWorkspace = WorkspaceTab::PlayerEditor;
                    m_pendingWindowFocus = WindowFocusTarget::PlayerEditor;
                } else if (m_activeWorkspace == WorkspaceTab::PlayerEditor) {
                    m_activeWorkspace = WorkspaceTab::LevelEditor;
                    m_pendingWindowFocus = WindowFocusTarget::SceneView;
                }
            }
            bool showEffectEditor = m_showEffectEditor;
            if (ImGui::MenuItem(ICON_FA_WAND_MAGIC_SPARKLES " Effect Editor", nullptr, &showEffectEditor)) {
                m_showEffectEditor = showEffectEditor;
                if (m_showEffectEditor) {
                    m_activeWorkspace = WorkspaceTab::EffectEditor;
                    m_pendingWindowFocus = WindowFocusTarget::EffectEditor;
                } else if (m_activeWorkspace == WorkspaceTab::EffectEditor) {
                    m_activeWorkspace = WorkspaceTab::LevelEditor;
                    m_pendingWindowFocus = WindowFocusTarget::SceneView;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Focus Hierarchy")) RequestWindowFocus(WindowFocusTarget::Hierarchy);
            if (ImGui::MenuItem("Focus Inspector")) RequestWindowFocus(WindowFocusTarget::Inspector);
            if (ImGui::MenuItem("Focus Asset Browser")) RequestWindowFocus(WindowFocusTarget::AssetBrowser);
            if (ImGui::MenuItem("Focus Console")) RequestWindowFocus(WindowFocusTarget::Console);
            if (ImGui::MenuItem("Focus Audio", nullptr, false, m_showAudioWindow)) RequestWindowFocus(WindowFocusTarget::Audio);
            if (ImGui::MenuItem("Focus Render Passes", nullptr, false, m_showRenderPassesWindow)) RequestWindowFocus(WindowFocusTarget::RenderPasses);
            if (ImGui::MenuItem("Focus Grid Settings", nullptr, false, m_showGridSettingsWindow)) RequestWindowFocus(WindowFocusTarget::GridSettings);
            ImGui::Separator();
            if (ImGui::MenuItem("Maximize Active Panel", nullptr, false, m_lastFocusedWindow != WindowFocusTarget::None)) {
                ExecuteMaximizeActivePanel();
            }
            if (ImGui::MenuItem("Restore Panel Layout")) {
                ExecuteResetLayout();
            }
            if (ImGui::MenuItem("Close All Secondary Windows")) {
                ExecuteCloseSecondaryWindows();
            }
            ImGui::Separator();
            ImGui::MenuItem("Show Status Bar", nullptr, &m_showStatusBar);
            ImGui::MenuItem("Show Main Toolbar", nullptr, &m_showMainToolbar);
            if (ImGui::MenuItem("Reset Layout")) {
                ExecuteResetLayout();
            }
            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 300.0f);
        if (IsSceneDirty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.30f, 1.0f), ICON_FA_CIRCLE " Unsaved");
            ImGui::SameLine();
        }
        ImGui::TextDisabled(ICON_FA_GEAR " %.1f FPS", ImGui::GetIO().Framerate);
        ImGui::SameLine();

        float totalTime = EngineKernel::Instance().GetTime().totalTime;
        ImGui::TextDisabled(ICON_FA_CLOCK " %.2f", totalTime);

        ImGui::EndMainMenuBar();
    }
}

void EditorLayer::DrawMainToolbar()
{
    auto& ifm = IconFontManager::Instance();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    const float workspaceTabHeight = (m_showPlayerEditor || m_showEffectEditor) ? 34.0f : 0.0f;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + ImGui::GetFrameHeight() + workspaceTabHeight));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, 32));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    if (ImGui::Begin("##MainToolbar", nullptr, flags))
    {
        // 左側: ファイル操作
        if (ifm.IconButton(ICON_FA_FLOPPY_DISK, IconSemantic::Default, IconFontSize::Medium, nullptr)) { SaveCurrentScene(); }
        ImGui::SameLine();
        if (ifm.IconButton(ICON_FA_ARROW_ROTATE_LEFT, IconSemantic::Default, IconFontSize::Medium, nullptr)) {
            ExecuteUndo();
        }
        ImGui::SameLine();
        if (ifm.IconButton(ICON_FA_ARROW_ROTATE_RIGHT, IconSemantic::Default, IconFontSize::Medium, nullptr)) {
            ExecuteRedo();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // 中央: 再生コントロール
        float centerX = ImGui::GetWindowWidth() * 0.5f;
        ImGui::SetCursorPosX(centerX - 58.0f);

        // ★現在のモードをカーネルから取得
        EngineMode mode = EngineKernel::Instance().GetMode();

        // [Play]
        bool isPlaying = (mode == EngineMode::Play);
        ImVec4 playColor = isPlaying ? ImVec4(0.26f, 0.90f, 0.26f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, playColor);
        if (ifm.IconButton(ICON_FA_PLAY, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            ExecuteGamePlay();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // [Pause]
        bool isPaused = (mode == EngineMode::Pause);
        ImVec4 pauseColor = isPaused ? ImVec4(1.00f, 0.60f, 0.00f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, pauseColor);
        if (ifm.IconButton(ICON_FA_PAUSE, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            ExecuteGamePauseToggle();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        const bool canStep = (mode == EngineMode::Pause);
        ImVec4 stepColor = canStep ? ImVec4(0.70f, 0.85f, 1.00f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, stepColor);
        if (!canStep) {
            ImGui::BeginDisabled();
        }
        if (ifm.IconButton(ICON_FA_FORWARD_STEP, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            ExecuteGameStep();
        }
        if (!canStep) {
            ImGui::EndDisabled();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // [Stop]
        bool canStop = (mode != EngineMode::Editor);
        ImVec4 stopColor = canStop ? ImVec4(1.00f, 0.25f, 0.25f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, stopColor);
        if (ifm.IconButton(ICON_FA_SQUARE, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            if (canStop) ExecuteGameStop();
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);
}

void EditorLayer::DrawStatusBar()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float statusBarHeight = 26.0f;

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - statusBarHeight));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, statusBarHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    if (ImGui::Begin("##EditorStatusBar", nullptr, flags)) {
        const std::string sceneName = std::filesystem::path(m_sceneSavePath).filename().string();
        ImGui::TextDisabled("%s %s", ICON_FA_FILE, sceneName.empty() ? "Untitled.scene" : sceneName.c_str());
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextDisabled("%s %s", ICON_FA_FLOPPY_DISK, IsSceneDirty() ? "Unsaved" : "Saved");
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextDisabled("Speed %.1f", m_cameraMoveSpeed);
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextDisabled("Snap T:%s R:%s S:%s",
            m_translateSnapEnabled ? "On" : "Off",
            m_rotateSnapEnabled ? "On" : "Off",
            m_scaleSnapEnabled ? "On" : "Off");
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        auto& selection = EditorSelection::Instance();
        if (selection.GetType() == SelectionType::Entity && selection.GetSelectedEntityCount() > 0) {
            if (selection.GetSelectedEntityCount() == 1) {
                ImGui::TextDisabled("Entity %llu", static_cast<unsigned long long>(selection.GetPrimaryEntity()));
            } else {
                ImGui::TextDisabled("%zu Selected (Primary %llu)",
                    selection.GetSelectedEntityCount(),
                    static_cast<unsigned long long>(selection.GetPrimaryEntity()));
            }
        } else if (selection.GetType() == SelectionType::Asset) {
            ImGui::TextDisabled("%s Asset", ICON_FA_FOLDER_OPEN);
        } else {
            ImGui::TextDisabled("No Selection");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}


void EditorLayer::DrawRenamePopup()
{
    if (m_requestRenamePopup) {
        ImGui::OpenPopup("Rename Entity");
        m_requestRenamePopup = false;
    }

    if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Enter a new name for the selected entity.");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(320.0f);
        ImGui::SetKeyboardFocusHere();
        const bool submit = ImGui::InputText("##RenameEntity", m_renameBuffer, sizeof(m_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        if (submit || ImGui::Button("OK", ImVec2(120, 0))) {
            if (m_gameLayer) {
                Registry& registry = m_gameLayer->GetRegistry();
                const EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
                if (auto* name = registry.GetComponent<NameComponent>(entity)) {
                    NameComponent before = *name;
                    NameComponent after = before;
                    after.name = m_renameBuffer;
                    if (after.name != before.name && !after.name.empty()) {
                        UndoSystem::Instance().ExecuteAction(
                            std::make_unique<ComponentUndoAction<NameComponent>>(entity, before, after),
                            registry);
                        PrefabSystem::MarkPrefabOverride(entity, registry);
                    }
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

