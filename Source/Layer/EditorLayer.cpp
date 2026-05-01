#include "EditorLayerInternal.h"
#include "EditorTheme.h"
#include "Graphics.h"

EditorLayer::EditorLayer(GameLayer* gameLayer)
    : m_gameLayer(gameLayer)
    , m_sceneSavePath(kDefaultSceneSavePath)
{
}

void EditorLayer::Initialize()
{
    m_assetBrowser = std::make_unique<AssetBrowser>();
    m_assetBrowser->Initialize();

    if (m_gameLayer) {
        m_inputBridge.Initialize(m_gameLayer->GetRegistry());
    }

    ApplyEditorGrayTheme();
    MarkSceneSaved();
    CheckRecoveryCandidate();
}

void EditorLayer::Finalize()
{
    if (m_gameLayer) {
        m_inputBridge.Finalize(m_gameLayer->GetRegistry());
    }
}

void EditorLayer::Update(const EngineTime& time)
{
    FontManager::Instance().ProcessEditorPreviewFonts();
    ImGuiIO& io = ImGui::GetIO();
    using namespace DirectX;

    HandleEditorShortcuts();
    ProcessDeferredEditorActions();
    UpdateAutosave(time.unscaledDt);

    if (m_gameLayer) {
        m_inputBridge.UpdateHoverState(m_sceneViewHovered);
        m_inputBridge.UpdateTextInputState(io.WantTextInput, m_gameLayer->GetRegistry());
    }

    if (!io.WantTextInput && m_gameLayer) {
        Registry& registry = m_gameLayer->GetRegistry();
        SyncRectTransforms(registry);

        if (m_sceneViewMode == SceneViewMode::Mode2D && m_sceneViewHovered && !m_sceneViewToolbarHovered) {
            if (std::fabs(io.MouseWheel) > 0.0001f) {
                const float zoomFactor = (io.MouseWheel > 0.0f) ? 0.9f : 1.1f;
                m_editor2DZoom = (std::clamp)(m_editor2DZoom * zoomFactor, 1.0f, 2000.0f);
            }

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
                const float aspect = (m_sceneViewRect.w > 0.0f) ? (m_sceneViewRect.z / m_sceneViewRect.w) : (16.0f / 9.0f);
                const float worldPerPixelX = ((m_editor2DZoom * 2.0f) * aspect) / (std::max)(m_sceneViewRect.z, 1.0f);
                const float worldPerPixelY = (m_editor2DZoom * 2.0f) / (std::max)(m_sceneViewRect.w, 1.0f);
                m_editor2DCenter.x -= mouseDelta.x * worldPerPixelX;
                m_editor2DCenter.y += mouseDelta.y * worldPerPixelY;
            }
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (io.KeyShift) {
                ExecuteRedo();
            } else {
                ExecuteUndo();
            }
        } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            ExecuteRedo();
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            ExecuteDuplicateSelection();
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            ExecuteCopySelection();
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
            ExecutePasteSelection();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            ExecuteDeleteSelection();
        }
    }

    XMVECTOR pos = XMLoadFloat3(&m_editorCameraPosition);
    const XMVECTOR rot = XMQuaternionRotationRollPitchYaw(m_editorCameraPitch, m_editorCameraYaw, 0.0f);
    const XMVECTOR forward = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot));
    const XMVECTOR right = XMVector3Normalize(XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot));
    const XMVECTOR up = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot));

    if (m_sceneViewHovered && io.MouseDown[ImGuiMouseButton_Right] && !io.KeyAlt && !m_gizmoWasUsing) {
        m_editorCameraUserOverride = true;
        m_editorCameraYaw += io.MouseDelta.x * 0.005f;
        m_editorCameraPitch += io.MouseDelta.y * 0.005f;
        m_editorCameraPitch = std::clamp(m_editorCameraPitch, -1.55f, 1.55f);

        float speed = m_cameraMoveSpeed * io.DeltaTime;
        if (io.KeyShift) speed *= 3.0f;
        if (io.KeyCtrl) speed *= 0.25f;

        const XMVECTOR moveRot = XMQuaternionRotationRollPitchYaw(m_editorCameraPitch, m_editorCameraYaw, 0.0f);
        const XMVECTOR moveForward = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 0, 1, 0), moveRot));
        const XMVECTOR moveRight = XMVector3Normalize(XMVector3Rotate(XMVectorSet(1, 0, 0, 0), moveRot));
        const XMVECTOR moveUp = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 1, 0, 0), moveRot));

        if (ImGui::IsKeyDown(ImGuiKey_W)) pos += moveForward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) pos -= moveForward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) pos += moveRight * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) pos -= moveRight * speed;
        if (ImGui::IsKeyDown(ImGuiKey_E)) pos += moveUp * speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) pos -= moveUp * speed;
    }

    if (m_sceneViewHovered && io.MouseDown[ImGuiMouseButton_Middle] && !m_gizmoWasUsing) {
        m_editorCameraUserOverride = true;
        const float panSpeed = (m_cameraMoveSpeed * 0.5f) * io.DeltaTime;
        pos -= right * io.MouseDelta.x * panSpeed;
        pos += up * io.MouseDelta.y * panSpeed;
    }

    if (m_sceneViewHovered && io.MouseWheel != 0.0f && !m_gizmoWasUsing) {
        m_editorCameraUserOverride = true;
        pos += forward * (io.MouseWheel * (m_cameraMoveSpeed * 0.5f));
    }

    if (m_gameLayer && m_sceneViewHovered) {
        const auto* edInput = m_inputBridge.GetEditorInput(m_gameLayer->GetRegistry());
        if (edInput) {
            const float orbitSpeed = 2.0f * io.DeltaTime;
            const float flySpeed = m_cameraMoveSpeed * io.DeltaTime;

            if (m_sceneViewMode == SceneViewMode::Mode3D) {
                const float orbitX = edInput->axes[0];
                const float orbitY = edInput->axes[1];
                if (fabsf(orbitX) > 0.01f || fabsf(orbitY) > 0.01f) {
                    m_editorCameraUserOverride = true;
                    m_editorCameraYaw += orbitX * orbitSpeed;
                    m_editorCameraPitch += orbitY * orbitSpeed;
                    m_editorCameraPitch = std::clamp(m_editorCameraPitch, -1.55f, 1.55f);
                }

                const float moveX = edInput->axes[2];
                const float moveZ = edInput->axes[3];
                if (fabsf(moveX) > 0.01f || fabsf(moveZ) > 0.01f) {
                    m_editorCameraUserOverride = true;
                    pos += right * moveX * flySpeed;
                    pos += forward * moveZ * flySpeed;
                }
            } else {
                const float panX = edInput->axes[2];
                const float panY = edInput->axes[3];
                if (fabsf(panX) > 0.01f || fabsf(panY) > 0.01f) {
                    m_editor2DCenter.x += panX * flySpeed;
                    m_editor2DCenter.y += panY * flySpeed;
                }
            }
        }
    }

    XMStoreFloat3(&m_editorCameraPosition, pos);
    SyncMainCameraEntityToEditorCamera();

    if (m_showSequencer && m_gameLayer && m_activeWorkspace == WorkspaceTab::LevelEditor) {
        m_sequencerPanel.Update(time, &m_gameLayer->GetRegistry());
    } else if (m_gameLayer) {
        m_sequencerPanel.Suspend(&m_gameLayer->GetRegistry());
    }
}

void EditorLayer::SyncMainCameraEntityToEditorCamera()
{
    if (!m_gameLayer) {
        return;
    }

    const bool useEditorCamera =
        m_activeWorkspace == WorkspaceTab::PlayerEditor ||
        (m_activeWorkspace == WorkspaceTab::LevelEditor &&
            (m_sceneViewHovered || m_lastFocusedWindow == WindowFocusTarget::SceneView));
    if (!useEditorCamera) {
        return;
    }

    using namespace DirectX;

    Registry& registry = m_gameLayer->GetRegistry();
    const Signature targetSig = CreateSignature<TransformComponent, CameraMainTagComponent>();
    const ComponentTypeID transformId = TypeManager::GetComponentTypeID<TransformComponent>();
    const ComponentTypeID cameraCtrlId = TypeManager::GetComponentTypeID<CameraFreeControlComponent>();

    const XMFLOAT3 cameraPosition = GetEditorCameraPosition();
    const XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(m_editorCameraPitch, m_editorCameraYaw, 0.0f);

    for (auto* archetype : registry.GetAllArchetypes()) {
        if (!SignatureMatches(archetype->GetSignature(), targetSig)) {
            continue;
        }

        auto* transformColumn = archetype->GetColumn(transformId);
        auto* cameraCtrlColumn = archetype->GetSignature().test(cameraCtrlId)
            ? archetype->GetColumn(cameraCtrlId)
            : nullptr;
        if (!transformColumn) {
            continue;
        }

        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            auto& transform = *static_cast<TransformComponent*>(transformColumn->Get(i));
            transform.localPosition = cameraPosition;
            XMStoreFloat4(&transform.localRotation, cameraRotation);
            transform.isDirty = true;

            if (cameraCtrlColumn) {
                auto& cameraCtrl = *static_cast<CameraFreeControlComponent*>(cameraCtrlColumn->Get(i));
                cameraCtrl.pitch = m_editorCameraPitch;
                cameraCtrl.yaw = m_editorCameraYaw;
            }
            return;
        }
    }
}

void EditorLayer::RenderUI()
{
    if (!m_showPlayerEditor && m_activeWorkspace == WorkspaceTab::PlayerEditor) {
        m_activeWorkspace = WorkspaceTab::LevelEditor;
    }
    if (!m_showEffectEditor && m_activeWorkspace == WorkspaceTab::EffectEditor) {
        m_activeWorkspace = WorkspaceTab::LevelEditor;
    }

    DrawMenuBar();
    DrawWorkspaceTabs();

    const bool showPlayerWorkspace = m_showPlayerEditor && m_activeWorkspace == WorkspaceTab::PlayerEditor;
    const bool showEffectWorkspace = m_showEffectEditor && m_activeWorkspace == WorkspaceTab::EffectEditor;
    const bool showSecondaryWorkspace = showPlayerWorkspace || showEffectWorkspace;
    if (!showPlayerWorkspace) {
        m_playerEditorPanel.Suspend();
    }
    if (showPlayerWorkspace && m_maximizedWindow == WindowFocusTarget::PlayerEditor) {
        m_maximizedWindow = WindowFocusTarget::None;
    }
    if (showEffectWorkspace && m_maximizedWindow == WindowFocusTarget::EffectEditor) {
        m_maximizedWindow = WindowFocusTarget::None;
    }

    if (!showSecondaryWorkspace && m_showMainToolbar) {
        DrawMainToolbar();
    }

    if (showPlayerWorkspace) {
        DrawPlayerEditorWorkspace();
    } else if (showEffectWorkspace) {
        DrawEffectEditorWorkspace();
    } else {
        DrawDockSpace();
    }

    const bool maximizeLeft = (m_maximizedWindow == WindowFocusTarget::Hierarchy);
    const bool maximizeRight = (m_maximizedWindow == WindowFocusTarget::Inspector);
    const bool maximizeBottomAsset = (m_maximizedWindow == WindowFocusTarget::AssetBrowser);
    const bool maximizeBottomSerializer = (m_maximizedWindow == WindowFocusTarget::Serializer);
    const bool maximizeBottomConsole = (m_maximizedWindow == WindowFocusTarget::Console);
    const bool maximizeBottomSequencer = (m_maximizedWindow == WindowFocusTarget::Sequencer);
    const bool maximizeTool = (m_maximizedWindow == WindowFocusTarget::Lighting ||
        m_maximizedWindow == WindowFocusTarget::Audio ||
        m_maximizedWindow == WindowFocusTarget::RenderPasses ||
        m_maximizedWindow == WindowFocusTarget::GridSettings ||
        m_maximizedWindow == WindowFocusTarget::GBufferDebug);

    if (!showSecondaryWorkspace) {
        if (m_showSceneView && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::SceneView)) {
            DrawSceneView();
        }
        if (m_showGameView && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::GameView)) {
            DrawGameView();
        }
        if (m_showHierarchy && (m_maximizedWindow == WindowFocusTarget::None || maximizeLeft)) {
            DrawHierarchy();
        }
        if (m_showInspector && (m_maximizedWindow == WindowFocusTarget::None || maximizeRight)) {
            DrawInspector();
        }
        if (m_showLightingWindow && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::Lighting || maximizeTool)) {
            DrawLightingWindow();
        }
        if (m_showAudioWindow && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::Audio || maximizeTool)) {
            DrawAudioWindow();
        }
        if (m_showRenderPassesWindow && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::RenderPasses || maximizeTool)) {
            DrawRenderPassesWindow();
        }
        if (m_showGridSettingsWindow && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::GridSettings || maximizeTool)) {
            DrawGridSettingsWindow();
        }
        if (m_showGBufferDebug && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::GBufferDebug || maximizeTool)) {
            DrawGBufferDebugWindow();
        }
        if (m_showInputDebug) {
            InputDebugSystem::DrawDebugWindow(m_gameLayer->GetRegistry(), EngineKernel::Instance().GetInputBackend(), EngineKernel::Instance().GetInputEventQueue());
        }
    }

    if (m_showStatusBar) {
        DrawStatusBar();
    }
    DrawUnsavedChangesPopup();
    DrawRecoveryPopup();
    DrawRenamePopup();

    if (!showSecondaryWorkspace && m_showConsole && (m_maximizedWindow == WindowFocusTarget::None || maximizeBottomConsole)) {
        bool consoleFocused = false;
        ApplyPendingWindowFocus(WindowFocusTarget::Console);
        Console::Instance().Draw(kConsoleWindowTitle, &m_showConsole, &consoleFocused);
        SetLastFocusedWindow(WindowFocusTarget::Console, consoleFocused);
    }
    if (!showSecondaryWorkspace && m_showSequencer && (m_maximizedWindow == WindowFocusTarget::None || maximizeBottomSequencer)) {
        DrawSequencer();
    }
    if (!showSecondaryWorkspace && m_showSerializer && (m_maximizedWindow == WindowFocusTarget::None || maximizeBottomSerializer)) {
        DrawModelSerializer();
    }
    if (!showSecondaryWorkspace && m_assetBrowser && m_showAssetBrowser && (m_maximizedWindow == WindowFocusTarget::None || maximizeBottomAsset)) {
        m_assetBrowser->SetRegistry(m_gameLayer ? &m_gameLayer->GetRegistry() : nullptr);
        bool assetFocused = false;
        ApplyPendingWindowFocus(WindowFocusTarget::AssetBrowser);
        m_assetBrowser->RenderUI(&m_showAssetBrowser, &assetFocused);
        SetLastFocusedWindow(WindowFocusTarget::AssetBrowser, assetFocused);
    }
    if (m_showGameLoopEditor) {
        bool focused = false;
        ApplyPendingWindowFocus(WindowFocusTarget::GameLoopEditor);
        m_gameLoopEditorPanel.Draw(&m_showGameLoopEditor, &focused);
        SetLastFocusedWindow(WindowFocusTarget::GameLoopEditor, focused);
    }
}

void EditorLayer::RenderDetachedWindows()
{
    // Player Editor is now hosted inside the main editor as a workspace tab.
}

void EditorLayer::SyncPlayerEditorPanelState()
{
    if (!m_gameLayer) {
        m_playerEditorPanel.SyncExternalSelection(Entity::NULL_ID, "");
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    const EntityID selectedEntity = EditorSelection::Instance().GetPrimaryEntity();

    std::string selectedModelPath;
    if (!Entity::IsNull(selectedEntity) && registry.IsAlive(selectedEntity)) {
        if (auto* mesh = registry.GetComponent<MeshComponent>(selectedEntity)) {
            selectedModelPath = mesh->modelFilePath;
        }
    }

    m_playerEditorPanel.SyncExternalSelection(selectedEntity, selectedModelPath);
}

void EditorLayer::SyncEffectEditorPanelState()
{
    if (!m_gameLayer) {
        m_effectEditorPanel.SetViewportTexture(nullptr);
        m_effectEditorPanel.SetSelectedContext(Entity::NULL_ID, "");
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    const EntityID selectedEntity = EditorSelection::Instance().GetPrimaryEntity();
    std::string meshPath;
    if (!Entity::IsNull(selectedEntity) && registry.IsAlive(selectedEntity)) {
        if (auto* mesh = registry.GetComponent<MeshComponent>(selectedEntity)) {
            meshPath = mesh->modelFilePath;
        }
    }
    m_effectEditorPanel.SetSelectedContext(selectedEntity, meshPath);
}

void EditorLayer::SetPlayerEditorCameraShakeOffset(const DirectX::XMFLOAT3& offset)
{
    m_editorCameraShakeOffset = offset;
}

void EditorLayer::ClearPlayerEditorCameraShakeOffset()
{
    m_editorCameraShakeOffset = { 0.0f, 0.0f, 0.0f };
}
