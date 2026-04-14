#include "EditorLayerInternal.h"

void EditorLayer::DrawDockSpace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const bool hasWorkspaceTabs = m_showPlayerEditor || m_showEffectEditor;
    const float workspaceTabHeight = hasWorkspaceTabs ? 34.0f : 0.0f;
    const float toolbarHeight = (m_showMainToolbar && m_activeWorkspace == WorkspaceTab::LevelEditor) ? 32.0f : 0.0f;
    const float statusBarHeight = m_showStatusBar ? 26.0f : 0.0f;
    const ImVec2 dockspacePos(viewport->WorkPos.x, viewport->WorkPos.y + workspaceTabHeight + toolbarHeight);
    const ImVec2 dockspaceSize(
        viewport->WorkSize.x,
        (std::max)(1.0f, viewport->WorkSize.y - workspaceTabHeight - toolbarHeight - statusBarHeight));

    ImGui::SetNextWindowPos(dockspacePos);
    ImGui::SetNextWindowSize(dockspaceSize);

    // 背景を描画しないフラグ（残像防止のため背景を塗る設定にします）
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    // ★ここでDockSpaceの土台ウィンドウを描画（これが残像を消すキャンバスになります）
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");

    // 中央ノードのパススルー設定
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    static int s_layoutVersionApplied = 0;
    const int kDockLayoutVersion = 3 + static_cast<int>(m_maximizedWindow) * 100;
    if (m_forceDockLayoutReset) {
        s_layoutVersionApplied = 0;
        m_forceDockLayoutReset = false;
    }
    if (s_layoutVersionApplied != kDockLayoutVersion)
    {
        s_layoutVersionApplied = kDockLayoutVersion;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, dockspaceSize);

        if (m_maximizedWindow != WindowFocusTarget::None) {
            ImGui::DockBuilderDockWindow(GetWindowTitleForFocus(m_maximizedWindow), dockspace_id);
            ImGui::DockBuilderFinish(dockspace_id);
            ImGui::End();
            return;
        }

        // 画面を分割していく
        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
        ImGuiID dock_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

        // 各ウィンドウを分割したエリアにはめ込む
        ImGui::DockBuilderDockWindow(kHierarchyWindowTitle, dock_left);
        ImGui::DockBuilderDockWindow(kInspectorWindowTitle, dock_right);

        ImGui::DockBuilderDockWindow(kConsoleWindowTitle, dock_down);
        ImGui::DockBuilderDockWindow(kAssetBrowserWindowTitle, dock_down);
        ImGui::DockBuilderDockWindow(kSerializerWindowTitle, dock_down);
        ImGui::DockBuilderDockWindow(kSequencerWindowTitle, dock_down);

        ImGui::DockBuilderDockWindow(kSceneViewWindowTitle, dock_main_id);
        ImGui::DockBuilderDockWindow(kGameViewWindowTitle, dock_main_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }



    ImGui::End();
}

void EditorLayer::DrawWorkspaceTabs()
{
    if (!m_showPlayerEditor && !m_showEffectEditor) {
        m_activeWorkspace = WorkspaceTab::LevelEditor;
        return;
    }

    constexpr float kWorkspaceTabHeight = 34.0f;
    constexpr float kTabHeight = 24.0f;
    constexpr float kTabSpacing = 4.0f;
    constexpr float kTabPaddingX = 14.0f;
    constexpr float kCloseButtonWidth = 22.0f;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + ImGui::GetFrameHeight()));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, kWorkspaceTabHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
    bool playerTabOpen = m_showPlayerEditor;
    bool effectTabOpen = m_showEffectEditor;

    if (ImGui::Begin("##WorkspaceTabs", nullptr, flags))
    {
        const ImVec4 activeTabColor = ImGui::GetStyleColorVec4(ImGuiCol_TabActive);
        const ImVec4 inactiveTabColor = ImGui::GetStyleColorVec4(ImGuiCol_Tab);
        const ImVec4 hoveredTabColor = ImGui::GetStyleColorVec4(ImGuiCol_TabHovered);

        auto drawWorkspaceTabButton = [&](const char* id, const char* label, bool active) -> bool
        {
            const float width = ImGui::CalcTextSize(label).x + kTabPaddingX * 2.0f;
            ImGui::PushID(id);
            ImGui::PushStyleColor(ImGuiCol_Button, active ? activeTabColor : inactiveTabColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredTabColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeTabColor);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 1.0f : 0.0f);

            const bool clicked = ImGui::Button(label, ImVec2(width, kTabHeight));

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            ImGui::PopID();
            return clicked;
        };

        auto drawWorkspaceTabClose = [&](const char* id, bool active) -> bool
        {
            ImGui::PushID(id);
            ImGui::PushStyleColor(ImGuiCol_Button, active ? activeTabColor : inactiveTabColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredTabColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeTabColor);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 1.0f : 0.0f);

            const bool clicked = ImGui::Button("x", ImVec2(kCloseButtonWidth, kTabHeight));

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            ImGui::PopID();
            return clicked;
        };

        if (drawWorkspaceTabButton("LevelWorkspaceTab", ICON_FA_CUBE " Level Editor",
            m_activeWorkspace == WorkspaceTab::LevelEditor)) {
            m_activeWorkspace = WorkspaceTab::LevelEditor;
        }

        if (playerTabOpen) {
            ImGui::SameLine(0.0f, kTabSpacing);

            const bool playerActive = (m_activeWorkspace == WorkspaceTab::PlayerEditor);
            if (drawWorkspaceTabButton("PlayerWorkspaceTab", ICON_FA_USER " Player Editor", playerActive)) {
                m_activeWorkspace = WorkspaceTab::PlayerEditor;
                m_pendingWindowFocus = WindowFocusTarget::PlayerEditor;
            }

            ImGui::SameLine(0.0f, 1.0f);
            if (drawWorkspaceTabClose("PlayerWorkspaceTabClose", playerActive)) {
                playerTabOpen = false;
            }
        }

        if (effectTabOpen) {
            ImGui::SameLine(0.0f, kTabSpacing);

            const bool effectActive = (m_activeWorkspace == WorkspaceTab::EffectEditor);
            if (drawWorkspaceTabButton("EffectWorkspaceTab", ICON_FA_WAND_MAGIC_SPARKLES " Effect Editor", effectActive)) {
                m_activeWorkspace = WorkspaceTab::EffectEditor;
                m_pendingWindowFocus = WindowFocusTarget::EffectEditor;
            }

            ImGui::SameLine(0.0f, 1.0f);
            if (drawWorkspaceTabClose("EffectWorkspaceTabClose", effectActive)) {
                effectTabOpen = false;
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    m_showPlayerEditor = playerTabOpen;
    m_showEffectEditor = effectTabOpen;
    if (!m_showPlayerEditor && m_activeWorkspace == WorkspaceTab::PlayerEditor) {
        m_activeWorkspace = WorkspaceTab::LevelEditor;
        RequestWindowFocus(WindowFocusTarget::SceneView);
    }
    if (!m_showEffectEditor && m_activeWorkspace == WorkspaceTab::EffectEditor) {
        m_activeWorkspace = WorkspaceTab::LevelEditor;
        RequestWindowFocus(WindowFocusTarget::SceneView);
    }
}

void EditorLayer::DrawPlayerEditorWorkspace()
{
    constexpr float kWorkspaceTabHeight = 34.0f;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float workspaceTabHeight = m_showPlayerEditor ? kWorkspaceTabHeight : 0.0f;
    const float statusBarHeight = m_showStatusBar ? 26.0f : 0.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + workspaceTabHeight));
    ImGui::SetNextWindowSize(ImVec2(
        viewport->WorkSize.x,
        (std::max)(1.0f, viewport->WorkSize.y - workspaceTabHeight - statusBarHeight)));

    SyncPlayerEditorPanelState();

    bool playerEditorFocused = false;
    ApplyPendingWindowFocus(WindowFocusTarget::PlayerEditor);
    m_playerEditorPanel.DrawWorkspace(
        m_gameLayer ? &m_gameLayer->GetRegistry() : nullptr,
        &playerEditorFocused);
    SetLastFocusedWindow(WindowFocusTarget::PlayerEditor, playerEditorFocused);
}

void EditorLayer::DrawEffectEditorWorkspace()
{
    constexpr float kWorkspaceTabHeight = 34.0f;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const bool hasWorkspaceTabs = m_showPlayerEditor || m_showEffectEditor;
    const float workspaceTabHeight = hasWorkspaceTabs ? kWorkspaceTabHeight : 0.0f;
    const float statusBarHeight = m_showStatusBar ? 26.0f : 0.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + workspaceTabHeight));
    ImGui::SetNextWindowSize(ImVec2(
        viewport->WorkSize.x,
        (std::max)(1.0f, viewport->WorkSize.y - workspaceTabHeight - statusBarHeight)));

    SyncEffectEditorPanelState();

    bool effectEditorFocused = false;
    ApplyPendingWindowFocus(WindowFocusTarget::EffectEditor);
    m_effectEditorPanel.DrawWorkspace(
        m_gameLayer ? &m_gameLayer->GetRegistry() : nullptr,
        &effectEditorFocused);
    SetLastFocusedWindow(WindowFocusTarget::EffectEditor, effectEditorFocused);
}

void EditorLayer::DrawModelSerializer()
{
    bool serializerFocused = false;
    ApplyPendingWindowFocus(WindowFocusTarget::Serializer);
    m_modelSerializerPanel.Draw(&m_showSerializer, &serializerFocused);
    SetLastFocusedWindow(WindowFocusTarget::Serializer, serializerFocused);
}


void EditorLayer::DrawGameView()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ApplyPendingWindowFocus(WindowFocusTarget::GameView);
    if (ImGui::Begin(kGameViewWindowTitle, &m_showGameView, window_flags))
    {
        SetLastFocusedWindow(WindowFocusTarget::GameView, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
        if (ImGui::BeginChild("##GameViewToolbar", ImVec2(0.0f, 34.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            auto tooltip = [](const char* text) {
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    ImGui::SetTooltip("%s", text);
                }
            };

            auto drawMenuLikeToggle = [&](const char* label, bool* value, const char* help) {
                if (*value) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.21f, 0.47f, 0.82f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.53f, 0.90f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
                }
                if (ImGui::Button(label)) {
                    *value = !*value;
                }
                tooltip(help);
                if (*value) {
                    ImGui::PopStyleColor(3);
                }
            };

            if (ImGui::BeginCombo("##GameViewResolutionPreset", GetGameViewPresetLabel(m_gameViewResolutionPreset)))
            {
                const GameViewResolutionPreset presets[] = {
                    GameViewResolutionPreset::Free,
                    GameViewResolutionPreset::HD1080,
                    GameViewResolutionPreset::HD720,
                    GameViewResolutionPreset::Portrait1080x1920,
                    GameViewResolutionPreset::Portrait750x1334
                };
                for (GameViewResolutionPreset preset : presets) {
                    const bool selected = (m_gameViewResolutionPreset == preset);
                    if (ImGui::Selectable(GetGameViewPresetLabel(preset), selected)) {
                        m_gameViewResolutionPreset = preset;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            tooltip("Resolution preset");
            ImGui::SameLine();

            if (ImGui::BeginCombo("##GameViewAspectPolicy", GetGameViewAspectPolicyLabel(m_gameViewAspectPolicy))) {
                if (ImGui::Selectable("Fit", m_gameViewAspectPolicy == GameViewAspectPolicy::Fit)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::Fit;
                }
                if (ImGui::Selectable("Fill", m_gameViewAspectPolicy == GameViewAspectPolicy::Fill)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::Fill;
                }
                if (ImGui::Selectable("1:1 Pixel", m_gameViewAspectPolicy == GameViewAspectPolicy::PixelPerfect)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::PixelPerfect;
                }
                ImGui::EndCombo();
            }
            tooltip("Aspect policy");
            ImGui::SameLine();

            if (ImGui::BeginCombo("##GameViewScalePolicy", GetGameViewScalePolicyLabel(m_gameViewScalePolicy))) {
                if (ImGui::Selectable("Auto", m_gameViewScalePolicy == GameViewScalePolicy::AutoFit)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::AutoFit;
                }
                if (ImGui::Selectable("1x", m_gameViewScalePolicy == GameViewScalePolicy::Scale1x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale1x;
                }
                if (ImGui::Selectable("2x", m_gameViewScalePolicy == GameViewScalePolicy::Scale2x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale2x;
                }
                if (ImGui::Selectable("3x", m_gameViewScalePolicy == GameViewScalePolicy::Scale3x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale3x;
                }
                ImGui::EndCombo();
            }
            tooltip("Preview scale");
            ImGui::SameLine(0.0f, 8.0f);

            drawMenuLikeToggle("Safe", &m_gameViewShowSafeArea, "Show safe area");
            ImGui::SameLine();
            drawMenuLikeToggle("Pixel", &m_gameViewShowPixelPreview, "Show pixel preview");
            ImGui::SameLine();
            drawMenuLikeToggle("Stats", &m_gameViewShowStatsOverlay, "Show stats overlay");
            ImGui::SameLine();
            drawMenuLikeToggle("UI", &m_gameViewShowUIOverlay, "Show UI overlay");
            ImGui::SameLine();
            drawMenuLikeToggle("2D", &m_gameViewShow2DOverlay, "Show 2D overlay");
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                ExecuteGameResetPreview();
            }
            tooltip("Reset preview");

            if (m_gameLayer) {
                Registry& registry = m_gameLayer->GetRegistry();
                std::string cameraLabel = "No Camera";
                if (m_sceneViewMode == SceneViewMode::Mode2D) {
                    for (Archetype* archetype : registry.GetAllArchetypes()) {
                        if (!archetype->GetSignature().test(TypeManager::GetComponentTypeID<Camera2DComponent>())) {
                            continue;
                        }
                        const auto& entities = archetype->GetEntities();
                        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                            cameraLabel = "2D";
                            if (auto* name = registry.GetComponent<NameComponent>(entities[i])) {
                                cameraLabel = name->name;
                            }
                            break;
                        }
                        break;
                    }
                } else {
                    for (Archetype* archetype : registry.GetAllArchetypes()) {
                        const Signature signature = archetype->GetSignature();
                        if (!signature.test(TypeManager::GetComponentTypeID<CameraMainTagComponent>()) &&
                            !signature.test(TypeManager::GetComponentTypeID<CameraLensComponent>())) {
                            continue;
                        }
                        const auto& entities = archetype->GetEntities();
                        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                            cameraLabel = "Camera";
                            if (auto* name = registry.GetComponent<NameComponent>(entities[i])) {
                                cameraLabel = name->name;
                            }
                            break;
                        }
                        break;
                    }
                }
                ImGui::SameLine(0.0f, 8.0f);
                ImGui::TextDisabled("%s", cameraLabel.c_str());
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (m_gameLayer) {
            auto& registry = m_gameLayer->GetRegistry();
            auto archetypes = registry.GetAllArchetypes();
            for (auto* arch : archetypes) {
                if (arch->GetSignature().test(TypeManager::GetComponentTypeID<CameraFreeControlComponent>())) {
                    auto* ctrlCol = arch->GetColumn(TypeManager::GetComponentTypeID<CameraFreeControlComponent>());
                    for (size_t i = 0; i < arch->GetEntityCount(); ++i) {
                        static_cast<CameraFreeControlComponent*>(ctrlCol->Get(i))->isHovered = hovered;
                    }
                }
            }
        }

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        m_gameViewSize = { viewportSize.x, viewportSize.y };

        ITexture* gameTexture = m_gameViewTexture ? m_gameViewTexture : m_sceneViewTexture;
        void* gameTextureId = nullptr;
        if (gameTexture) {
            gameTextureId = ImGuiRenderer::GetTextureID(gameTexture);
        } else if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
            Graphics& graphics = Graphics::Instance();
            FrameBuffer* gameViewBuffer = graphics.GetFrameBuffer(FrameBufferId::Scene);
            if ((!gameViewBuffer || !gameViewBuffer->GetColorTexture(0))) {
                gameViewBuffer = graphics.GetFrameBuffer(FrameBufferId::Display);
            }
            if (gameViewBuffer && gameViewBuffer->GetColorTexture(0)) {
                gameTextureId = gameViewBuffer->GetImGuiTextureID();
                gameTexture = gameViewBuffer->GetColorTexture(0);
            }
        }

        const DirectX::XMUINT2 presetResolution = GetGameViewPresetResolution(m_gameViewResolutionPreset);
        ImVec2 imageSize = viewportSize;
        if (presetResolution.x > 0 && presetResolution.y > 0) {
            const float presetAspect = static_cast<float>(presetResolution.x) / static_cast<float>(presetResolution.y);
            if (m_gameViewScalePolicy == GameViewScalePolicy::AutoFit) {
                switch (m_gameViewAspectPolicy) {
                case GameViewAspectPolicy::Fill:
                    imageSize = FillSizeKeepingAspect(viewportSize, presetAspect);
                    break;
                case GameViewAspectPolicy::PixelPerfect:
                    imageSize = ImVec2(static_cast<float>(presetResolution.x), static_cast<float>(presetResolution.y));
                    if (imageSize.x > viewportSize.x || imageSize.y > viewportSize.y) {
                        imageSize = FitSizeKeepingAspect(viewportSize, presetAspect);
                    }
                    break;
                case GameViewAspectPolicy::Fit:
                default:
                    imageSize = FitSizeKeepingAspect(viewportSize, presetAspect);
                    break;
                }
            } else {
                float scale = 1.0f;
                if (m_gameViewScalePolicy == GameViewScalePolicy::Scale2x) scale = 2.0f;
                else if (m_gameViewScalePolicy == GameViewScalePolicy::Scale3x) scale = 3.0f;
                imageSize = ImVec2(static_cast<float>(presetResolution.x) * scale, static_cast<float>(presetResolution.y) * scale);
                if (imageSize.x > viewportSize.x || imageSize.y > viewportSize.y) {
                    imageSize = FitSizeKeepingAspect(viewportSize, presetAspect);
                }
            }
        }
        const ImVec2 imageCursor(
            ImGui::GetCursorPosX() + (viewportSize.x - imageSize.x) * 0.5f,
            ImGui::GetCursorPosY() + (viewportSize.y - imageSize.y) * 0.5f);
        if (imageSize.x > 0.0f && imageSize.y > 0.0f) {
            ImGui::SetCursorPos(imageCursor);
        }

        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
        m_gameViewRect = { imageMin.x, imageMin.y, imageSize.x, imageSize.y };

        if (gameTextureId) {
            ImGui::Image((ImTextureID)gameTextureId, imageSize);
        } else {
            ImGui::Dummy(imageSize);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(ImVec2(imageMin.x, imageMin.y), ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), IM_COL32(80, 80, 80, 255));
            drawList->AddText(ImVec2(imageMin.x + 16.0f, imageMin.y + 16.0f), IM_COL32(220, 220, 220, 255), "No preview texture");
        }

        if (m_sceneViewMode == SceneViewMode::Mode2D) {
            DirectX::XMFLOAT4X4 gameView{};
            DirectX::XMFLOAT4X4 gameProjection{};
            if (TryBuildGameView2DViewProjection(gameView, gameProjection)) {
                if (m_gameViewShowUIOverlay) {
                    Draw2DOverlayForRect(m_gameViewRect, gameView, gameProjection, false);
                }
                if (m_gameViewShow2DOverlay) {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddText(ImVec2(imageMin.x + 12.0f, imageMin.y + 12.0f), IM_COL32(190, 220, 255, 220), "2D Preview");
                }
            } else if (m_gameViewShow2DOverlay) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddText(ImVec2(imageMin.x + 16.0f, imageMin.y + 16.0f), IM_COL32(255, 220, 160, 255), "No active 2D camera");
            }
        }

        if (m_gameViewShowSafeArea && imageSize.x > 8.0f && imageSize.y > 8.0f) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float insetX = imageSize.x * 0.05f;
            const float insetY = imageSize.y * 0.05f;
            const ImVec2 safeMin(imageMin.x + insetX, imageMin.y + insetY);
            const ImVec2 safeMax(imageMin.x + imageSize.x - insetX, imageMin.y + imageSize.y - insetY);
            drawList->AddRect(safeMin, safeMax, IM_COL32(255, 214, 102, 220), 0.0f, 0, 2.0f);
            drawList->AddText(ImVec2(safeMin.x + 6.0f, safeMin.y + 6.0f), IM_COL32(255, 214, 102, 230), "Safe Area");
        }

        if (m_gameViewShowPixelPreview && presetResolution.x > 0 && presetResolution.y > 0) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            std::string label = std::string("Preview ") + GetGameViewPresetLabel(m_gameViewResolutionPreset);
            drawList->AddText(ImVec2(imageMin.x + 8.0f, imageMin.y + imageSize.y - 22.0f), IM_COL32(200, 220, 255, 220), label.c_str());
        }

        if (m_gameViewShowStatsOverlay) {
            DrawStatsOverlay(m_gameViewRect, "Game");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::DrawSequencer()
{
    bool focused = false;
    ApplyPendingWindowFocus(WindowFocusTarget::Sequencer);
    m_sequencerPanel.Draw(m_gameLayer ? &m_gameLayer->GetRegistry() : nullptr, &m_showSequencer, &focused);
    SetLastFocusedWindow(WindowFocusTarget::Sequencer, focused);
}

void EditorLayer::DrawHierarchy()
{
    if (!m_gameLayer) return;
    bool focused = false;
    ApplyPendingWindowFocus(WindowFocusTarget::Hierarchy);
    HierarchyECSUI::Render(&m_gameLayer->GetRegistry(), &m_showHierarchy, &focused);
    SetLastFocusedWindow(WindowFocusTarget::Hierarchy, focused);
}

void EditorLayer::DrawInspector()
{
    if (!m_gameLayer) return;  // InspectorECSUI::Render が Begin/End を管理
    bool focused = false;
    ApplyPendingWindowFocus(WindowFocusTarget::Inspector);
    InspectorECSUI::Render(&m_gameLayer->GetRegistry(), &m_showInspector, &focused);
    SetLastFocusedWindow(WindowFocusTarget::Inspector, focused);
}

void EditorLayer::DrawLightingWindow()
{
    if (!m_showLightingWindow) return;

    // ウィンドウを描画
    ApplyPendingWindowFocus(WindowFocusTarget::Lighting);
    if (ImGui::Begin(kLightingWindowTitle, &m_showLightingWindow))
    {
        SetLastFocusedWindow(WindowFocusTarget::Lighting, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
        // 1. Environment の自動生成UI
        auto& env = GetOrCreateEnvironmentComponent(m_gameLayer->GetRegistry());
        DrawGlobalSettingsUI(env);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader(ICON_FA_CUBE " Reflection Probe", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& probe = GetOrCreateReflectionProbeComponent(m_gameLayer->GetRegistry());
            bool changed = false;

            if (ImGui::BeginTable("ReflectionProbeTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("position");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.0f);
                changed |= ImGui::DragFloat3("##ReflectionProbePosition", &probe.position.x, 0.05f);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("radius");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.0f);
                changed |= ImGui::DragFloat("##ReflectionProbeRadius", &probe.radius, 0.1f, 0.1f, 10000.0f, "%.2f");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("status");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(probe.cubemapTexture ? "Cubemap Ready" : "No Cubemap");

                ImGui::EndTable();
            }

            if (changed) {
                probe.needsBake = true;
            }

            if (ImGui::Button(ICON_FA_ROTATE " Rebake Probe")) {
                probe.needsBake = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Dirty: %s", probe.needsBake ? "Yes" : "No");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // 2. PostEffect の自動生成UI
        auto& post = m_gameLayer->GetPostEffect();
        DrawGlobalSettingsUI(post);
    }
    ImGui::End();
}

void EditorLayer::DrawAudioWindow()
{
    if (!m_showAudioWindow || !m_gameLayer) {
        return;
    }

    ApplyPendingWindowFocus(WindowFocusTarget::Audio);
    if (!ImGui::Begin(kAudioWindowTitle, &m_showAudioWindow)) {
        ImGui::End();
        return;
    }

    SetLastFocusedWindow(WindowFocusTarget::Audio, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

    Registry& registry = m_gameLayer->GetRegistry();
    AudioSettingsComponent& settings = GetOrCreateAudioSettingsComponent(registry);
    auto& audio = EngineKernel::Instance().GetAudioWorld();
    const EntityID listenerEntity = audio.GetActiveListenerEntity();
    const std::string previewPath = audio.GetPreviewClipPath();

    auto voices = audio.GetDebugVoices();
    auto buses = audio.GetDebugBuses();
    auto cachedClips = audio.GetCachedClips();

    if (ImGui::BeginTabBar("AudioDebugTabs")) {
        if (ImGui::BeginTabItem("Mixer")) {
            DrawGlobalSettingsUI(settings);

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_STOP " Stop All Voices")) {
                audio.StopAllVoices();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_TRASH " Clear Clip Cache")) {
                audio.ClearClipCache();
                cachedClips.clear();
            }
            if (!previewPath.empty()) {
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_VOLUME_XMARK " Stop Preview")) {
                    audio.StopPreview();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::TextDisabled("Active Listener:");
            ImGui::SameLine();
            if (Entity::IsNull(listenerEntity)) {
                ImGui::TextUnformatted("None");
            } else {
                const std::string listenerLabel = GetEntityLabel(registry, listenerEntity);
                ImGui::TextWrapped("%s", listenerLabel.c_str());
            }

            ImGui::TextDisabled("Preview:");
            ImGui::SameLine();
            if (previewPath.empty()) {
                ImGui::TextUnformatted("None");
            } else {
                const std::string previewName = std::filesystem::path(previewPath).filename().string();
                ImGui::TextWrapped("%s", previewName.c_str());
            }

            int streamingVoiceCount = 0;
            for (const auto& bus : buses) {
                streamingVoiceCount += bus.streamingVoiceCount;
            }
            ImGui::TextDisabled("Voices: %d  |  Streams: %d  |  Cache: %zu", static_cast<int>(voices.size()), streamingVoiceCount, cachedClips.size());

            if (ImGui::BeginTable("AudioBusTable", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Bus", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Effective", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Voices", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Streams", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Mute", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupColumn("Solo", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableHeadersRow();

                const auto activeSoloBus = audio.GetSoloBus();
                for (const auto& bus : buses) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(GetAudioBusTypeLabel(bus.bus));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f", bus.baseVolume);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.2f", bus.effectiveVolume);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%d", bus.activeVoiceCount);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", bus.streamingVoiceCount);
                    ImGui::TableSetColumnIndex(5);
                    bool muted = bus.muted;
                    std::string muteId = std::string("##MuteBus") + GetAudioBusTypeLabel(bus.bus);
                    if (ImGui::Checkbox(muteId.c_str(), &muted)) {
                        audio.SetBusMuted(bus.bus, muted);
                    }
                    ImGui::TableSetColumnIndex(6);
                    bool solo = activeSoloBus.has_value() && *activeSoloBus == bus.bus;
                    std::string soloId = std::string("##SoloBus") + GetAudioBusTypeLabel(bus.bus);
                    if (ImGui::Checkbox(soloId.c_str(), &solo)) {
                        audio.SetSoloBus(solo ? std::optional<AudioBusType>(bus.bus) : std::nullopt);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Voices")) {
            ImGui::TextDisabled("Active Voices");
            if (voices.empty()) {
                ImGui::TextDisabled("No active voices.");
            } else if (ImGui::BeginTable("AudioVoices", 10, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 300.0f))) {
                ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Clip");
                ImGui::TableSetupColumn("Bus", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("3D", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed, 45.0f);
                ImGui::TableSetupColumn("Vol", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupColumn("Pitch", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();

                for (const auto& voice : voices) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%llu", static_cast<unsigned long long>(voice.handle));
                    ImGui::TableSetColumnIndex(1);
                    const std::string clipName = std::filesystem::path(voice.clipPath).filename().string();
                    ImGui::TextWrapped("%s", clipName.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(GetAudioBusTypeLabel(voice.bus));
                    ImGui::TableSetColumnIndex(3);
                    if (Entity::IsNull(voice.entity)) {
                        ImGui::TextDisabled("Transient");
                    } else {
                        const std::string entityLabel = GetEntityLabel(registry, voice.entity);
                        ImGui::TextWrapped("%s", entityLabel.c_str());
                    }
                    ImGui::TableSetColumnIndex(4);
                    if (voice.preview) {
                        ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "Preview");
                    } else if (voice.playing) {
                        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Playing");
                    } else {
                        ImGui::TextDisabled("Stopped");
                    }
                    ImGui::TableSetColumnIndex(5);
                    ImGui::TextUnformatted(voice.is3D ? "Yes" : "No");
                    ImGui::TableSetColumnIndex(6);
                    ImGui::TextUnformatted(voice.loop ? "Yes" : "No");
                    ImGui::TableSetColumnIndex(7);
                    ImGui::Text("%.2f", voice.volume);
                    ImGui::TableSetColumnIndex(8);
                    ImGui::Text("%.2f", voice.pitch);
                    ImGui::TableSetColumnIndex(9);
                    ImGui::Text("%.2f / %.2f", voice.cursorSeconds, voice.lengthSeconds);
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Cache")) {
            ImGui::TextDisabled("Cached Audio Clips: %zu", cachedClips.size());
            if (cachedClips.empty()) {
                ImGui::TextDisabled("No cached clip metadata.");
            } else if (ImGui::BeginTable("AudioClipCache", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 260.0f))) {
                ImGui::TableSetupColumn("Clip");
                ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Sample Rate", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Streaming", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Size KB", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                for (const auto& clip : cachedClips) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextWrapped("%s", std::filesystem::path(clip.sourcePath).filename().string().c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f", clip.lengthSec);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u", clip.channelCount);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%u", clip.sampleRate);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(clip.streaming ? "Yes" : "No");
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%.1f", static_cast<double>(clip.fileSizeBytes) / 1024.0);
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void EditorLayer::DrawRenderPassesWindow()
{
    if (!m_showRenderPassesWindow || !m_gameLayer) {
        return;
    }

    auto resetPassDefaults = [](PostEffectComponent& post) {
        post.enableComputeCulling = true;
        post.enableAsyncCompute = true;
        post.enableGTAO = true;
        post.enableSSGI = false;
        post.enableVolumetricFog = false;
        post.enableSSR = false;
        post.enableBloom = true;
        post.enableColorFilter = true;
        post.enableDoF = false;
        post.enableMotionBlur = true;
    };

    auto disableOptionalPasses = [](PostEffectComponent& post) {
        post.enableComputeCulling = false;
        post.enableAsyncCompute = false;
        post.enableGTAO = false;
        post.enableSSGI = false;
        post.enableVolumetricFog = false;
        post.enableSSR = false;
        post.enableBloom = false;
        post.enableColorFilter = false;
        post.enableDoF = false;
        post.enableMotionBlur = false;
    };

    PostEffectComponent& post = m_gameLayer->GetPostEffect();

    ApplyPendingWindowFocus(WindowFocusTarget::RenderPasses);
    if (ImGui::Begin(kRenderPassesWindowTitle, &m_showRenderPassesWindow)) {
        SetLastFocusedWindow(WindowFocusTarget::RenderPasses, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

        ImGui::TextDisabled("Heavy render features");
        ImGui::Separator();

        if (ImGui::BeginTable("RenderPassToggleTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 170.0f);
            ImGui::TableSetupColumn("Toggle", ImGuiTableColumnFlags_WidthStretch);

            auto drawBoolRow = [](const char* label, bool* value, const char* help) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", label);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    ImGui::SetTooltip("%s", help);
                }
                ImGui::TableSetColumnIndex(1);
                std::string id = std::string("##") + label;
                ImGui::Checkbox(id.c_str(), value);
            };

            drawBoolRow("Compute Culling", &post.enableComputeCulling, "GPU driven visible instance culling");
            drawBoolRow("Async Compute", &post.enableAsyncCompute, "Use async compute queue when compute culling is enabled");
            drawBoolRow("GTAO", &post.enableGTAO, "Ground-truth ambient occlusion pass");
            drawBoolRow("SSGI", &post.enableSSGI, "Screen space global illumination");
            drawBoolRow("SSR", &post.enableSSR, "Screen space reflections");
            drawBoolRow("Volumetric Fog", &post.enableVolumetricFog, "Half resolution volumetric fog");
            drawBoolRow("Bloom", &post.enableBloom, "Bloom extraction and bloom blend");
            drawBoolRow("Color Filter", &post.enableColorFilter, "Exposure / hue / vignette / flash adjustments");
            drawBoolRow("Depth of Field", &post.enableDoF, "Depth of field blur");
            drawBoolRow("Motion Blur", &post.enableMotionBlur, "Velocity-based motion blur");

            ImGui::EndTable();
        }

        if (!post.enableComputeCulling) {
            post.enableAsyncCompute = false;
        }

        ImGui::Spacing();
        if (ImGui::Button("Disable All Optional Passes")) {
            disableOptionalPasses(post);
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore Defaults")) {
            resetPassDefaults(post);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Main GBuffer / Lighting / Shadow / PostProcess output are always required and are not disabled here.");
    }
    ImGui::End();
}

void EditorLayer::DrawGridSettingsWindow()
{
    if (!m_showGridSettingsWindow) {
        return;
    }

    ApplyPendingWindowFocus(WindowFocusTarget::GridSettings);
    if (ImGui::Begin(kGridSettingsWindowTitle, &m_showGridSettingsWindow)) {
        SetLastFocusedWindow(WindowFocusTarget::GridSettings, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

        ImGui::TextDisabled("Scene View 3D grid");
        ImGui::Spacing();

        if (ImGui::BeginTable("GridSettingsTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("showGrid");
            ImGui::TableSetColumnIndex(1);
            ImGui::Checkbox("##ShowSceneGrid", &m_showSceneGrid);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("cellSize");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::DragFloat("##SceneGridCellSize", &m_sceneGridCellSize, 0.5f, 1.0f, 500.0f, "%.1f")) {
                m_sceneGridCellSize = (std::clamp)(m_sceneGridCellSize, 1.0f, 500.0f);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("halfLineCount");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##SceneGridHalfLineCount", &m_sceneGridHalfLineCount, 4, 128)) {
                m_sceneGridHalfLineCount = (std::clamp)(m_sceneGridHalfLineCount, 4, 128);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("totalSpan");
            ImGui::TableSetColumnIndex(1);
            const float totalSpan = static_cast<float>(m_sceneGridHalfLineCount * 2) * m_sceneGridCellSize;
            ImGui::Text("%.1f units", totalSpan);

            ImGui::EndTable();
        }

        if (ImGui::Button("Compact Preset")) {
            m_sceneGridCellSize = 20.0f;
            m_sceneGridHalfLineCount = 24;
        }
        ImGui::SameLine();
        if (ImGui::Button("Default Preset")) {
            m_sceneGridCellSize = 20.0f;
            m_sceneGridHalfLineCount = 32;
        }
    }
    ImGui::End();
}

void EditorLayer::DrawGBufferDebugWindow()
{
    if (!m_showGBufferDebug) return;

    ApplyPendingWindowFocus(WindowFocusTarget::GBufferDebug);
    if (ImGui::Begin(kGBufferWindowTitle, &m_showGBufferDebug))
    {
        SetLastFocusedWindow(WindowFocusTarget::GBufferDebug, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
        ITexture* textures[4] = { m_gbufferTexture0, m_gbufferTexture1, m_gbufferTexture2, m_gbufferTexture3 };
        bool hasViewTextures = textures[0] || textures[1] || textures[2] || textures[3];
        FrameBuffer* gbuffer =
            (hasViewTextures || Graphics::Instance().GetAPI() == GraphicsAPI::DX12)
            ? nullptr
            : Graphics::Instance().GetFrameBuffer(FrameBufferId::GBuffer);
        if (hasViewTextures || gbuffer)
        {
            // ウィンドウ幅に合わせてアスペクト比を計算
            float w = ImGui::GetContentRegionAvail().x;
            float h = w * (Graphics::Instance().GetScreenHeight() / Graphics::Instance().GetScreenWidth());
            ImVec2 size(w, h);

            ImGui::TextDisabled("Target 0: Albedo (RGB) + Metallic (A)");
            void* tex0 = hasViewTextures ? (textures[0] ? ImGuiRenderer::GetTextureID(textures[0]) : nullptr) : gbuffer->GetImGuiTextureID(0);
            if (tex0) {
                ImGui::Image((ImTextureID)tex0, size);
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Target 1: Normal (RGB) + Roughness (A)");
            void* tex1 = hasViewTextures ? (textures[1] ? ImGuiRenderer::GetTextureID(textures[1]) : nullptr) : gbuffer->GetImGuiTextureID(1);
            if (tex1) {
                ImGui::Image((ImTextureID)tex1, size);
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Target 2: World Position (RGB) + Depth (A)");
            void* tex2 = hasViewTextures ? (textures[2] ? ImGuiRenderer::GetTextureID(textures[2]) : nullptr) : gbuffer->GetImGuiTextureID(2);
            if (tex2) {
                ImGui::Image((ImTextureID)tex2, size);
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Target 3: Velocity (RG)");
            void* tex3 = hasViewTextures ? (textures[3] ? ImGuiRenderer::GetTextureID(textures[3]) : nullptr) : gbuffer->GetImGuiTextureID(3);
            if (tex3) {
                ImGui::Image((ImTextureID)tex3, size);
            }

        }
    }
    ImGui::End();
}
