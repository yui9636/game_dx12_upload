#include "EditorLayerInternal.h"

void EditorLayer::DrawSceneView()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ApplyPendingWindowFocus(WindowFocusTarget::SceneView);
    if (ImGui::Begin(kSceneViewWindowTitle, &m_showSceneView, window_flags))
    {
        SetLastFocusedWindow(WindowFocusTarget::SceneView, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
        m_sceneViewToolbarHovered = false;
        // =========================================================
        // ★ 判定の強化：子ウィンドウやアイテム（画像）の上でもホバーとみなす
        // =========================================================
        bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        m_sceneViewHovered = hovered;
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

        // ホバー中に右クリックされたらフォーカスを奪う（WASD操作のため）
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::SetWindowFocus();
        }

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        m_sceneViewSize = { viewportSize.x, viewportSize.y };
        ImVec2 imageMin = ImGui::GetCursorScreenPos();
        m_sceneViewRect = { imageMin.x, imageMin.y, viewportSize.x, viewportSize.y };
        void* sceneViewTexture = nullptr;
        if (m_sceneShadingMode == SceneShadingMode::Unlit) {
            if (m_gbufferTexture0) {
                sceneViewTexture = ImGuiRenderer::GetTextureID(m_gbufferTexture0);
            } else if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
                if (FrameBuffer* gbuffer = Graphics::Instance().GetFrameBuffer(FrameBufferId::GBuffer)) {
                    sceneViewTexture = gbuffer->GetImGuiTextureID(0);
                }
            }
        }
        if (!sceneViewTexture && m_sceneViewTexture) {
            sceneViewTexture = ImGuiRenderer::GetTextureID(m_sceneViewTexture);
        }
        if (sceneViewTexture) {
            ImGui::Image((ImTextureID)sceneViewTexture, viewportSize);
        } else if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
            Graphics& graphics = Graphics::Instance();
            FrameBuffer* sceneViewBuffer = graphics.GetFrameBuffer(FrameBufferId::Display);
            if (sceneViewBuffer) {
                if (ITexture* color = sceneViewBuffer->GetColorTexture(0)) {
                    if (void* fallbackTexture = sceneViewBuffer->GetImGuiTextureID()) {
                        ImGui::Image((ImTextureID)fallbackTexture, viewportSize);
                    }
                }
            }
        }

        const DirectX::XMFLOAT4X4 view = GetEditorViewMatrix();
        const float aspect = (m_sceneViewRect.w > 0.0f) ? (m_sceneViewRect.z / m_sceneViewRect.w) : (16.0f / 9.0f);
        const DirectX::XMFLOAT4X4 projection = BuildEditorProjectionMatrix(aspect);
        if (m_showSceneGrid && m_sceneViewMode == SceneViewMode::Mode2D) {
            DrawSceneGridOverlay(m_sceneViewRect, view, projection);
        }
        ImGuizmo::BeginFrame();
        HandleSceneAssetDrop();
        Draw2DOverlay();
        if (m_showSceneSelectionOutline) {
            DrawSelectionOutlineOverlay(m_sceneViewRect, view, projection);
        }
        if (m_showSceneLightIcons || m_showSceneCameraIcons) {
            DrawSceneIconOverlay(m_sceneViewRect, view, projection);
        }
        if (m_showSequencer && m_activeWorkspace == WorkspaceTab::LevelEditor) {
            DrawSequencerCameraOverlay(m_sceneViewRect, view, projection);
        }
        if (m_showSceneBounds || m_sceneShadingMode == SceneShadingMode::Wireframe) {
            DrawSceneBoundsOverlay(m_sceneViewRect, view, projection);
        }
        if (m_showSceneCollision) {
            DrawSceneCollisionOverlay(m_sceneViewRect, view, projection);
        }
        if (m_showSceneStatsOverlay) {
            DrawStatsOverlay(m_sceneViewRect, "Scene");
        }
        DrawSceneViewToolbar();
        DrawTransformGizmo();
        HandleScenePicking();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

DirectX::XMFLOAT3 EditorLayer::GetEditorCameraDirection() const
{
    using namespace DirectX;
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        return { 0.0f, 0.0f, 1.0f };
    }
    const XMVECTOR rot = XMQuaternionRotationRollPitchYaw(m_editorCameraPitch, m_editorCameraYaw, 0.0f);
    const XMVECTOR forward = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot));
    DirectX::XMFLOAT3 out{};
    XMStoreFloat3(&out, forward);
    return out;
}

DirectX::XMFLOAT4X4 EditorLayer::GetEditorViewMatrix() const
{
    using namespace DirectX;
    const DirectX::XMFLOAT3 eyePosition = GetEditorCameraPosition();
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        const XMVECTOR eye = XMLoadFloat3(&eyePosition);
        const XMMATRIX view = XMMatrixLookToLH(eye, XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0));
        DirectX::XMFLOAT4X4 out{};
        XMStoreFloat4x4(&out, view);
        return out;
    }
    const XMVECTOR eye = XMLoadFloat3(&eyePosition);
    const XMFLOAT3 dirFloat = GetEditorCameraDirection();
    const XMVECTOR dir = XMLoadFloat3(&dirFloat);
    const XMMATRIX view = XMMatrixLookToLH(eye, dir, XMVectorSet(0, 1, 0, 0));
    DirectX::XMFLOAT4X4 out{};
    XMStoreFloat4x4(&out, view);
    return out;
}

DirectX::XMFLOAT4X4 EditorLayer::BuildEditorProjectionMatrix(float aspect) const
{
    using namespace DirectX;
    const float safeAspect = aspect > 0.0f ? aspect : (16.0f / 9.0f);
    const XMMATRIX proj = (m_sceneViewMode == SceneViewMode::Mode2D)
        ? XMMatrixOrthographicLH(safeAspect * m_editor2DZoom * 2.0f, m_editor2DZoom * 2.0f, 0.1f, 1000.0f)
        : XMMatrixPerspectiveFovLH(m_editorCameraFovY, safeAspect, 0.1f, 100000.0f);
    DirectX::XMFLOAT4X4 out{};
    XMStoreFloat4x4(&out, proj);
    return out;
}

void EditorLayer::SetEditorCameraLookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target)
{
    using namespace DirectX;
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        m_editor2DCenter = { target.x, target.y };
        m_editorCameraAutoFramed = true;
        return;
    }
    m_editorCameraPosition = position;
    m_editorCameraAutoFramed = true;

    const XMVECTOR pos = XMLoadFloat3(&position);
    const XMVECTOR tgt = XMLoadFloat3(&target);
    XMVECTOR dir = XMVector3Normalize(tgt - pos);

    DirectX::XMFLOAT3 dir3{};
    XMStoreFloat3(&dir3, dir);
    m_editorCameraYaw = std::atan2(dir3.x, dir3.z);
    const float xzLen = std::sqrt(dir3.x * dir3.x + dir3.z * dir3.z);
    m_editorCameraPitch = std::atan2(dir3.y, xzLen);
}


void EditorLayer::HandleEditorShortcuts()
{
    if (!m_gameLayer) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (io.KeyShift) {
            m_requestSaveSceneAs = true;
        } else {
            SaveCurrentScene();
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        m_requestNewScene = true;
        return;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        m_requestOpenScene = true;
        return;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        ExecuteSelectAll();
        return;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        ExecuteFocusSearch();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
        ExecuteRenameSelection();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        ExecuteDeselect();
        return;
    }
    if (!io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        ExecuteFrameSelected();
        return;
    }

    if (!m_sceneViewHovered || io.KeyCtrl || io.KeyAlt || io.MouseDown[ImGuiMouseButton_Right]) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        m_gizmoOperation = GizmoOperation::Translate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        m_gizmoOperation = GizmoOperation::Rotate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        m_gizmoOperation = GizmoOperation::Scale;
    } else if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
        m_translateSnapEnabled = !m_translateSnapEnabled;
    } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
        m_rotateSnapEnabled = !m_rotateSnapEnabled;
    } else if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
        m_scaleSnapEnabled = !m_scaleSnapEnabled;
    }
}

void EditorLayer::DrawSceneViewToolbar()
{
    if (m_sceneViewRect.z <= 1.0f || m_sceneViewRect.w <= 1.0f) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.11f, 0.68f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.22f, 0.26f, 0.90f));

    ImGui::SetNextWindowPos(ImVec2(m_sceneViewRect.x + m_sceneViewRect.z - 12.0f, m_sceneViewRect.y + 12.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##SceneViewToolbarOverlay", nullptr, flags)) {
        m_sceneViewToolbarHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        auto showCompactTooltip = [](const char* text) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", text);
            }
        };

        auto drawModeButton = [&](const char* label, SceneViewMode mode, const char* tooltip) {
            const bool selected = (m_sceneViewMode == mode);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.21f, 0.47f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.53f, 0.90f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
            }
            if (ImGui::Button(label)) {
                m_sceneViewMode = mode;
            }
            showCompactTooltip(tooltip);
            if (selected) {
                ImGui::PopStyleColor(3);
            }
        };

        auto drawOpButton = [&](const char* label, GizmoOperation op, const char* tooltip) {
            const bool selected = (m_gizmoOperation == op);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.21f, 0.47f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.53f, 0.90f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
            }
            if (ImGui::Button(label)) {
                m_gizmoOperation = op;
            }
            showCompactTooltip(tooltip);
            if (selected) {
                ImGui::PopStyleColor(3);
            }
        };

        auto drawToggleButton = [&](const char* label, bool active, const char* tooltip) {
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.21f, 0.47f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.53f, 0.90f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
            }
            const bool pressed = ImGui::Button(label);
            showCompactTooltip(tooltip);
            if (active) {
                ImGui::PopStyleColor(3);
            }
            return pressed;
        };

        auto drawViewPresetButton = [&](const char* label, const DirectX::XMFLOAT3& forward) {
            if (ImGui::Selectable(label, false, 0, ImVec2(70.0f, 0.0f))) {
                DirectX::XMFLOAT3 target = m_editorCameraPosition;
                float distance = 10.0f;
                auto& selection = EditorSelection::Instance();
                if (selection.GetType() == SelectionType::Entity && m_gameLayer) {
                    Registry& registry = m_gameLayer->GetRegistry();
                    const EntityID entity = selection.GetEntity();
                    if (!Entity::IsNull(entity) && registry.IsAlive(entity)) {
                        if (auto* mesh = registry.GetComponent<MeshComponent>(entity); mesh && mesh->model) {
                            const auto& bounds = mesh->model->GetWorldBounds();
                            target = bounds.Center;
                            distance = (std::max)(ComputeFocusDistance(Max3(bounds.Extents.x, bounds.Extents.y, bounds.Extents.z), m_editorCameraFovY), 5.0f);
                        } else if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                            target = transform->worldPosition;
                        }
                    }
                } else {
                    const DirectX::XMFLOAT3 currentForward = GetEditorCameraDirection();
                    target.x = m_editorCameraPosition.x + currentForward.x * distance;
                    target.y = m_editorCameraPosition.y + currentForward.y * distance;
                    target.z = m_editorCameraPosition.z + currentForward.z * distance;
                }
                SetEditorCameraDirection(forward, target, distance);
                ImGui::CloseCurrentPopup();
            }
        };

        drawModeButton("3D", SceneViewMode::Mode3D, "3D mode");
        ImGui::SameLine();
        drawModeButton("2D", SceneViewMode::Mode2D, "2D mode");
        ImGui::SameLine(0.0f, 8.0f);

        drawOpButton("W", GizmoOperation::Translate, "Move");
        ImGui::SameLine();
        drawOpButton("E", GizmoOperation::Rotate, "Rotate");
        ImGui::SameLine();
        drawOpButton("R", GizmoOperation::Scale, "Scale");
        ImGui::SameLine();

        if (ImGui::Button(m_gizmoSpace == GizmoSpace::Local ? "Lcl" : "Wld")) {
            m_gizmoSpace = (m_gizmoSpace == GizmoSpace::Local) ? GizmoSpace::World : GizmoSpace::Local;
        }
        showCompactTooltip(m_gizmoSpace == GizmoSpace::Local ? "Local gizmo space" : "World gizmo space");
        ImGui::SameLine();

        if (ImGui::Button("F")) {
            FocusSelectedEntity();
        }
        showCompactTooltip("Frame selected");
        ImGui::SameLine(0.0f, 8.0f);

        if (drawToggleButton("T", m_translateSnapEnabled, "Move snap")) {
            m_translateSnapEnabled = !m_translateSnapEnabled;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(48.0f);
        ImGui::DragFloat("##MoveSnapStep", &m_translateSnapStep, 0.05f, 0.05f, 1000.0f, "%.2f");
        showCompactTooltip("Move snap step");
        ImGui::SameLine();

        if (drawToggleButton("R", m_rotateSnapEnabled, "Rotate snap")) {
            m_rotateSnapEnabled = !m_rotateSnapEnabled;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(42.0f);
        ImGui::DragFloat("##RotateSnapStep", &m_rotateSnapStep, 1.0f, 1.0f, 180.0f, "%.0f");
        showCompactTooltip("Rotate snap step");
        ImGui::SameLine();

        if (drawToggleButton("S", m_scaleSnapEnabled, "Scale snap")) {
            m_scaleSnapEnabled = !m_scaleSnapEnabled;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(48.0f);
        ImGui::DragFloat("##ScaleSnapStep", &m_scaleSnapStep, 0.05f, 0.01f, 100.0f, "%.2f");
        showCompactTooltip("Scale snap step");

        if (m_sceneViewMode == SceneViewMode::Mode2D) {
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextDisabled("Zoom");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::SliderFloat("##Scene2DZoom", &m_editor2DZoom, 1.0f, 200.0f, "%.1f");
        } else {
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button("View")) {
                ImGui::OpenPopup("##SceneViewPresetPopup");
            }
            showCompactTooltip("Camera view presets");
            if (ImGui::BeginPopup("##SceneViewPresetPopup")) {
                drawViewPresetButton("Front", { 0.0f, 0.0f, 1.0f });
                drawViewPresetButton("Back", { 0.0f, 0.0f, -1.0f });
                drawViewPresetButton("Left", { -1.0f, 0.0f, 0.0f });
                drawViewPresetButton("Right", { 1.0f, 0.0f, 0.0f });
                drawViewPresetButton("Top", { 0.0f, -1.0f, 0.0f });
                drawViewPresetButton("Bottom", { 0.0f, 1.0f, 0.0f });
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Cam");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(84.0f);
            ImGui::SliderFloat("##SceneCameraSpeed", &m_cameraMoveSpeed, 1.0f, 100.0f, "%.1f");
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

void EditorLayer::Draw2DOverlay()
{
    if (!m_gameLayer || m_sceneViewMode != SceneViewMode::Mode2D || m_sceneViewRect.z <= 1.0f || m_sceneViewRect.w <= 1.0f) {
        return;
    }

    using namespace DirectX;
    const XMFLOAT4X4 view = GetEditorViewMatrix();
    const float aspect = (m_sceneViewRect.w > 0.0f) ? (m_sceneViewRect.z / m_sceneViewRect.w) : (16.0f / 9.0f);
    const XMFLOAT4X4 projection = BuildEditorProjectionMatrix(aspect);
    Draw2DOverlayForRect(m_sceneViewRect, view, projection, true);
}

void EditorLayer::Draw2DOverlayForRect(const DirectX::XMFLOAT4& viewRect,
                                       const DirectX::XMFLOAT4X4& view,
                                       const DirectX::XMFLOAT4X4& projection,
                                       bool drawSelection)
{
    if (!m_gameLayer || viewRect.z <= 1.0f || viewRect.w <= 1.0f) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const EntityID primary = EditorSelection::Instance().GetPrimaryEntity();

    const std::vector<UI2DDrawEntry> entries = UI2DDrawSystem::CollectDrawEntries(registry);

    for (const UI2DDrawEntry& entry : entries) {
        auto* rect = entry.rect;
        auto* canvas = entry.canvas;
        auto* transform = entry.transform;
        auto* hierarchy = entry.hierarchy;
        if (!rect || !canvas || !transform || (hierarchy && !hierarchy->isActive) || !canvas->visible) {
            continue;
        }

        std::array<DirectX::XMFLOAT2, 4> corners{};
        UIHitTestSystem::ComputeScreenCorners(*transform, *rect, viewRect, view, projection, corners);
        if (canvas->pixelSnap) {
            for (auto& corner : corners) {
                corner.x = std::round(corner.x);
                corner.y = std::round(corner.y);
            }
        }
        const ImVec2 p0(corners[0].x, corners[0].y);
        const ImVec2 p1(corners[1].x, corners[1].y);
        const ImVec2 p2(corners[2].x, corners[2].y);
        const ImVec2 p3(corners[3].x, corners[3].y);

        const bool isSelected = drawSelection && EditorSelection::Instance().IsEntitySelected(entry.entity);
        const ImU32 outlineColor = isSelected
            ? IM_COL32(80, 180, 255, 255)
            : IM_COL32(190, 190, 190, 180);
        const ImU32 fillColor = (drawSelection && entry.entity == primary)
            ? IM_COL32(80, 180, 255, 32)
            : IM_COL32(255, 255, 255, 16);

        if (auto* sprite = entry.sprite; sprite && !sprite->textureAssetPath.empty()) {
            if (auto texture = ResourceManager::Instance().GetTexture(sprite->textureAssetPath)) {
                if (void* textureId = ImGuiRenderer::GetTextureID(texture.get())) {
                    const ImU32 tintColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        sprite->tint.x,
                        sprite->tint.y,
                        sprite->tint.z,
                        sprite->tint.w));
                    drawList->AddImageQuad((ImTextureID)textureId, p0, p1, p2, p3, ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1), tintColor);
                }
            }
        } else {
            drawList->AddQuadFilled(p0, p1, p2, p3, fillColor);
        }

        if (auto* text = entry.text; text && !text->text.empty()) {
            ImFont* font = ImGui::GetFont();
            if (!text->fontAssetPath.empty()) {
                if (ImFont* previewFont = FontManager::Instance().GetEditorPreviewFont(text->fontAssetPath)) {
                    font = previewFont;
                } else {
                    FontManager::Instance().QueueEditorPreviewFont(text->fontAssetPath);
                }
            }
            const float fontSize = (std::max)(8.0f, text->fontSize);
            const float wrapWidth = text->wrapping ? (std::max)(0.0f, rect->sizeDelta.x) : 0.0f;
            const ImVec2 textSize = font->CalcTextSizeA(
                fontSize,
                (std::numeric_limits<float>::max)(),
                wrapWidth,
                text->text.c_str());

            ImVec2 textPos = p0;
            if (text->alignment == TextAlignment::Center) {
                textPos.x += (rect->sizeDelta.x - textSize.x) * 0.5f;
            } else if (text->alignment == TextAlignment::Right) {
                textPos.x += rect->sizeDelta.x - textSize.x;
            }
            textPos.y += (std::max)(0.0f, (rect->sizeDelta.y - textSize.y) * 0.5f);
            if (canvas->pixelSnap) {
                textPos.x = std::round(textPos.x);
                textPos.y = std::round(textPos.y);
            }

            const ImU32 textColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
                text->color.x,
                text->color.y,
                text->color.z,
                text->color.w));

            drawList->AddText(font, fontSize, textPos, textColor, text->text.c_str(), nullptr, wrapWidth);
        }

          if (drawSelection && m_showSceneSelectionOutline) {
              drawList->AddQuad(p0, p1, p2, p3, outlineColor, 1.5f);
          }
    }
}

bool EditorLayer::TryBuildGameView2DViewProjection(DirectX::XMFLOAT4X4& outView,
                                                   DirectX::XMFLOAT4X4& outProjection) const
{
    if (!m_gameLayer || m_gameViewRect.z <= 1.0f || m_gameViewRect.w <= 1.0f) {
        return false;
    }

    using namespace DirectX;
    Registry& registry = m_gameLayer->GetRegistry();
    EntityID cameraEntity = Entity::NULL_ID;
    TransformComponent* cameraTransform = nullptr;
    Camera2DComponent* camera2D = nullptr;

    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<Camera2DComponent>()) ||
            !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
            continue;
        }

        auto* cameraColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<Camera2DComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        auto* hierarchyColumn = signature.test(TypeManager::GetComponentTypeID<HierarchyComponent>())
            ? archetype->GetColumn(TypeManager::GetComponentTypeID<HierarchyComponent>())
            : nullptr;
        const auto& entities = archetype->GetEntities();
        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            auto* currentCamera = static_cast<Camera2DComponent*>(cameraColumn->Get(i));
            auto* currentTransform = static_cast<TransformComponent*>(transformColumn->Get(i));
            auto* hierarchy = hierarchyColumn ? static_cast<HierarchyComponent*>(hierarchyColumn->Get(i)) : nullptr;
            if (!currentCamera || !currentTransform) {
                continue;
            }
            if (hierarchy && !hierarchy->isActive) {
                continue;
            }
            cameraEntity = entities[i];
            cameraTransform = currentTransform;
            camera2D = currentCamera;
            break;
        }
        if (!Entity::IsNull(cameraEntity)) {
            break;
        }
    }

    if (!cameraTransform || !camera2D) {
        return false;
    }

    const XMVECTOR eye = XMVectorSet(cameraTransform->worldPosition.x,
                                     cameraTransform->worldPosition.y,
                                     cameraTransform->worldPosition.z,
                                     1.0f);
    const XMMATRIX view = XMMatrixLookToLH(eye, XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0));
    const float aspect = m_gameViewRect.w > 0.0f ? (m_gameViewRect.z / m_gameViewRect.w) : (16.0f / 9.0f);
    const float zoom = (std::max)(camera2D->zoom, 0.01f);
    const float orthoSize = (std::max)(camera2D->orthographicSize / zoom, 0.01f);
    const XMMATRIX proj = XMMatrixOrthographicLH(aspect * orthoSize * 2.0f,
                                                 orthoSize * 2.0f,
                                                 camera2D->nearZ,
                                                 camera2D->farZ);
    XMStoreFloat4x4(&outView, view);
    XMStoreFloat4x4(&outProjection, proj);
    return true;
}

void EditorLayer::DrawTransformGizmo()
{
    static std::optional<TransformComponent> s_gizmoBeforeState;
    static std::optional<RectTransformComponent> s_rectGizmoBeforeState;
    const bool wasOverLastFrame = m_gizmoIsOver;

    m_gizmoIsOver = false;

    if (!m_showSceneGizmo || !m_gameLayer || m_sceneViewRect.z <= 1.0f || m_sceneViewRect.w <= 1.0f) {
        m_gizmoWasUsing = false;
        m_hasGizmoBeforeTransform = false;
        m_gizmoUndoEntity = Entity::NULL_ID;
        s_gizmoBeforeState.reset();
        s_rectGizmoBeforeState.reset();
        return;
    }

    if (EngineKernel::Instance().GetMode() != EngineMode::Editor) {
        m_gizmoWasUsing = false;
        m_hasGizmoBeforeTransform = false;
        m_gizmoUndoEntity = Entity::NULL_ID;
        s_gizmoBeforeState.reset();
        return;
    }

    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity) {
        m_gizmoWasUsing = false;
        m_hasGizmoBeforeTransform = false;
        m_gizmoUndoEntity = Entity::NULL_ID;
        s_gizmoBeforeState.reset();
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    EntityID entity = selection.GetEntity();
    auto* transform = registry.GetComponent<TransformComponent>(entity);
    auto* rectTransform = registry.GetComponent<RectTransformComponent>(entity);
    if (!transform) {
        m_gizmoWasUsing = false;
        m_hasGizmoBeforeTransform = false;
        m_gizmoUndoEntity = Entity::NULL_ID;
        s_gizmoBeforeState.reset();
        s_rectGizmoBeforeState.reset();
        return;
    }

    if (m_sceneViewMode == SceneViewMode::Mode2D && rectTransform) {
        Editor2D::SyncRectTransformToTransform(*rectTransform, *transform);
        HierarchySystem::MarkDirtyRecursive(entity, registry);
        HierarchySystem hierarchySystem;
        hierarchySystem.Update(registry);
    }

    using namespace DirectX;
    XMFLOAT4X4 view = GetEditorViewMatrix();
    const float aspect = (m_sceneViewRect.w > 0.0f) ? (m_sceneViewRect.z / m_sceneViewRect.w) : (16.0f / 9.0f);
    XMFLOAT4X4 projection = BuildEditorProjectionMatrix(aspect);
    XMFLOAT4X4 world = transform->worldMatrix;

    ImGuizmo::SetOrthographic(m_sceneViewMode == SceneViewMode::Mode2D);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_sceneViewRect.x, m_sceneViewRect.y, m_sceneViewRect.z, m_sceneViewRect.w);

    if (!m_gizmoWasUsing && wasOverLastFrame && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt) {
        const std::vector<EntityID> duplicatedRoots = DuplicateSelectionRoots(registry, selection);
        if (!duplicatedRoots.empty()) {
            selection.SetEntitySelection(duplicatedRoots, duplicatedRoots.back());
            entity = selection.GetEntity();
            transform = registry.GetComponent<TransformComponent>(entity);
            rectTransform = registry.GetComponent<RectTransformComponent>(entity);
            if (!transform) {
                m_gizmoWasUsing = false;
                m_hasGizmoBeforeTransform = false;
                m_gizmoUndoEntity = Entity::NULL_ID;
                s_gizmoBeforeState.reset();
                return;
            }
        }
    }

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    switch (m_gizmoOperation) {
    case GizmoOperation::Rotate: operation = ImGuizmo::ROTATE; break;
    case GizmoOperation::Scale: operation = ImGuizmo::SCALE; break;
    default: break;
    }

    ImGuizmo::MODE mode = (m_gizmoOperation == GizmoOperation::Scale)
        ? ImGuizmo::LOCAL
        : (m_gizmoSpace == GizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD);

    float snapValues[3] = { 0.0f, 0.0f, 0.0f };
    const float* snap = nullptr;
    if (m_gizmoOperation == GizmoOperation::Translate && m_translateSnapEnabled) {
        snapValues[0] = m_translateSnapStep;
        snapValues[1] = m_translateSnapStep;
        snapValues[2] = m_translateSnapStep;
        snap = snapValues;
    } else if (m_gizmoOperation == GizmoOperation::Rotate && m_rotateSnapEnabled) {
        snapValues[0] = m_rotateSnapStep;
        snap = snapValues;
    } else if (m_gizmoOperation == GizmoOperation::Scale && m_scaleSnapEnabled) {
        snapValues[0] = m_scaleSnapStep;
        snapValues[1] = m_scaleSnapStep;
        snapValues[2] = m_scaleSnapStep;
        snap = snapValues;
    }

    ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], operation, mode, &world.m[0][0], nullptr, snap);
    const bool isUsing = ImGuizmo::IsUsing();
    m_gizmoIsOver = ImGuizmo::IsOver();

    if (isUsing && !m_gizmoWasUsing) {
        m_gizmoUndoEntity = entity;
        if (m_sceneViewMode == SceneViewMode::Mode2D && rectTransform) {
            s_rectGizmoBeforeState = *rectTransform;
        } else {
            s_gizmoBeforeState = *transform;
        }
    }

    if (isUsing) {
        const XMMATRIX worldMatrix = XMLoadFloat4x4(&world);
        XMMATRIX localMatrix = worldMatrix;

        EntityID parentEntity = Entity::NULL_ID;
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
            parentEntity = hierarchy->parent;
        } else if (transform->parent != 0) {
            parentEntity = transform->parent;
        }

        if (!Entity::IsNull(parentEntity)) {
            if (auto* parentTransform = registry.GetComponent<TransformComponent>(parentEntity)) {
                const XMMATRIX parentWorld = XMLoadFloat4x4(&parentTransform->worldMatrix);
                localMatrix = worldMatrix * XMMatrixInverse(nullptr, parentWorld);
            }
        }

        XMVECTOR scale;
        XMVECTOR rotation;
        XMVECTOR translation;
        if (XMMatrixDecompose(&scale, &rotation, &translation, localMatrix)) {
            DirectX::XMFLOAT3 localPosition{};
            DirectX::XMFLOAT4 localRotation{};
            DirectX::XMFLOAT3 localScale{};

            XMStoreFloat3(&localPosition, translation);
            XMStoreFloat4(&localRotation, rotation);
            XMStoreFloat3(&localScale, scale);

            if (m_sceneViewMode == SceneViewMode::Mode2D && rectTransform) {
                rectTransform->anchoredPosition = { localPosition.x, localPosition.y };
                const DirectX::XMVECTOR q = XMLoadFloat4(&localRotation);
                const DirectX::XMFLOAT4 qf = localRotation;
                const float sinyCosp = 2.0f * (qf.w * qf.z + qf.x * qf.y);
                const float cosyCosp = 1.0f - 2.0f * (qf.y * qf.y + qf.z * qf.z);
                rectTransform->rotationZ = DirectX::XMConvertToDegrees(std::atan2(sinyCosp, cosyCosp));
                rectTransform->scale2D = {
                    (std::max)(std::fabs(localScale.x), kMinScaleValue),
                    (std::max)(std::fabs(localScale.y), kMinScaleValue)
                };
                SyncRectTransformToTransform(*rectTransform, *transform);
            } else {
                transform->localPosition = localPosition;
                transform->localRotation = localRotation;
                NormalizeQuaternion(transform->localRotation);
                transform->localScale = ClampScale(localScale);
            }
            DirectX::XMStoreFloat4x4(&transform->localMatrix, localMatrix);
            DirectX::XMStoreFloat4x4(&transform->worldMatrix, worldMatrix);
            XMVECTOR worldScale;
            XMVECTOR worldRotation;
            XMVECTOR worldTranslation;
            if (XMMatrixDecompose(&worldScale, &worldRotation, &worldTranslation, worldMatrix)) {
                XMStoreFloat3(&transform->worldPosition, worldTranslation);
                XMStoreFloat4(&transform->worldRotation, worldRotation);
                NormalizeQuaternion(transform->worldRotation);
                XMStoreFloat3(&transform->worldScale, worldScale);
            }
            transform->isDirty = true;
            HierarchySystem::MarkDirtyRecursive(entity, registry);
            if (auto* meshComponent = registry.GetComponent<MeshComponent>(entity)) {
                if (meshComponent->model) {
                    meshComponent->model->UpdateTransform(transform->worldMatrix);
                }
            }
            PrefabSystem::MarkPrefabOverride(entity, registry);
        }
    }

    if (isUsing && !m_gizmoWasUsing) {
        m_hasGizmoBeforeTransform = true;
    }
    if (!isUsing && m_gizmoWasUsing) {
        if (m_sceneViewMode == SceneViewMode::Mode2D && rectTransform) {
            if (m_hasGizmoBeforeTransform && s_rectGizmoBeforeState.has_value() && m_gizmoUndoEntity == entity) {
                auto* currentRect = registry.GetComponent<RectTransformComponent>(entity);
                if (currentRect && std::memcmp(&*s_rectGizmoBeforeState, currentRect, sizeof(RectTransformComponent)) != 0) {
                    UndoSystem::Instance().RecordAction(
                        std::make_unique<ComponentUndoAction<RectTransformComponent>>(entity,
                                                                                      *s_rectGizmoBeforeState,
                                                                                      *currentRect));
                }
            }
        } else {
            if (m_hasGizmoBeforeTransform && s_gizmoBeforeState.has_value() && m_gizmoUndoEntity == entity) {
                if (auto* currentTransform = registry.GetComponent<TransformComponent>(entity)) {
                    if (HasMeaningfulTransformChange(*s_gizmoBeforeState, *currentTransform)) {
                        UndoSystem::Instance().RecordAction(
                            std::make_unique<ComponentUndoAction<TransformComponent>>(entity,
                                                                                      *s_gizmoBeforeState,
                                                                                      *currentTransform));
                    }
                }
            }
        }
        s_gizmoBeforeState.reset();
        s_rectGizmoBeforeState.reset();
        m_hasGizmoBeforeTransform = false;
        m_gizmoUndoEntity = Entity::NULL_ID;
    }

    m_gizmoWasUsing = isUsing;
}

void EditorLayer::HandleScenePicking()
{
    if (!m_gameLayer) {
        m_scenePickPending = false;
        return;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_sceneViewHovered && !m_sceneViewToolbarHovered) {
        const ImVec2 mousePos = ImGui::GetMousePos();
        m_scenePickPending = true;
        m_scenePickBlockedByGizmo = m_gizmoIsOver || m_gizmoWasUsing;
        m_scenePickStart = { mousePos.x, mousePos.y };
    }

    if (!m_scenePickPending) {
        return;
    }

    if (m_gizmoIsOver || m_gizmoWasUsing) {
        m_scenePickBlockedByGizmo = true;
    }

    if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        return;
    }

    const ImVec2 mousePos = ImGui::GetMousePos();
    const float dx = mousePos.x - m_scenePickStart.x;
    const float dy = mousePos.y - m_scenePickStart.y;
    const bool treatAsClick = (dx * dx + dy * dy) <= (kSceneViewPickDragThreshold * kSceneViewPickDragThreshold);
    const bool blockedByGizmo = m_scenePickBlockedByGizmo;
    m_scenePickPending = false;
    m_scenePickBlockedByGizmo = false;

    if (!m_sceneViewHovered || m_sceneViewToolbarHovered || blockedByGizmo || !treatAsClick) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    using namespace DirectX;
    const XMFLOAT4X4 view = GetEditorViewMatrix();
    const float aspect = (m_sceneViewRect.w > 0.0f) ? (m_sceneViewRect.z / m_sceneViewRect.w) : (16.0f / 9.0f);
    const XMFLOAT4X4 projection = BuildEditorProjectionMatrix(aspect);

    auto& selection = EditorSelection::Instance();
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        UIHitTestResult hit = UIHitTestSystem::PickTopmost(
            registry,
            m_sceneViewRect,
            view,
            projection,
            { ImGui::GetMousePos().x, ImGui::GetMousePos().y });

        if (!Entity::IsNull(hit.entity)) {
            if (ImGui::GetIO().KeyCtrl) {
                selection.ToggleEntity(hit.entity, true);
            } else {
                selection.SelectEntity(hit.entity);
            }
        } else if (!ImGui::GetIO().KeyCtrl) {
            selection.Clear();
        }
        return;
    }

    XMFLOAT3 rayOrigin{};
    XMFLOAT3 rayDirection{};
    if (!BuildWorldRay(m_sceneViewRect, view, projection, ImGui::GetMousePos(), rayOrigin, rayDirection)) {
        return;
    }

    EntityID selectedEntity = Entity::NULL_ID;
    float maxDistance = 100000.0f;

    PhysicsRaycastResult hit = PhysicsManager::Instance().CastRay(rayOrigin, rayDirection, maxDistance);
    if (hit.hasHit && registry.IsAlive(hit.entityID)) {
        bool pickable = true;
        if (auto* mesh = registry.GetComponent<MeshComponent>(hit.entityID)) {
            pickable = mesh->isVisible;
        }
        if (pickable) {
            selectedEntity = hit.entityID;
            maxDistance = hit.distance;
        }
    }

    const EntityID fallbackEntity = PickRenderableFallback(registry, rayOrigin, rayDirection, maxDistance);
    if (!Entity::IsNull(fallbackEntity)) {
        selectedEntity = fallbackEntity;
    }

    if (!Entity::IsNull(selectedEntity)) {
        if (ImGui::GetIO().KeyCtrl) {
            selection.ToggleEntity(selectedEntity, true);
        } else {
            selection.SelectEntity(selectedEntity);
        }
    } else if (!ImGui::GetIO().KeyCtrl) {
        selection.Clear();
    }
}

void EditorLayer::HandleSceneAssetDrop()
{
    if (!m_gameLayer || m_sceneViewRect.z <= 1.0f || m_sceneViewRect.w <= 1.0f) {
        return;
    }

    if (!ImGui::BeginDragDropTarget()) {
        return;
    }

    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET", ImGuiDragDropFlags_AcceptBeforeDelivery);
    if (!payload) {
        ImGui::EndDragDropTarget();
        return;
    }

    std::filesystem::path assetPath(static_cast<const char*>(payload->Data));
    std::string ext = assetPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    using namespace DirectX;
    const XMFLOAT4X4 view = GetEditorViewMatrix();
    const float aspect = (m_sceneViewRect.w > 0.0f) ? (m_sceneViewRect.z / m_sceneViewRect.w) : (16.0f / 9.0f);
    const XMFLOAT4X4 projection = BuildEditorProjectionMatrix(aspect);

    XMFLOAT3 placementPosition{};
    XMFLOAT3 rayOrigin{};
    XMFLOAT3 rayDirection{};
    if (BuildWorldRay(m_sceneViewRect, view, projection, ImGui::GetMousePos(), rayOrigin, rayDirection)) {
        PhysicsRaycastResult hit = PhysicsManager::Instance().CastRay(rayOrigin, rayDirection, 100000.0f);
        if (hit.hasHit) {
            placementPosition = hit.position;
        } else {
            placementPosition = { 0.0f, 0.0f, 0.0f };
        }
    } else {
        placementPosition = { 0.0f, 0.0f, 0.0f };
    }

    if (m_sceneViewMode == SceneViewMode::Mode2D && (IsSupportedSpriteAsset(assetPath) || IsSupportedFontAsset(assetPath))) {
        DirectX::XMFLOAT3 canvasPoint{};
        if (UIHitTestSystem::ScreenToCanvasPoint(
                m_sceneViewRect,
                view,
                projection,
                { ImGui::GetMousePos().x, ImGui::GetMousePos().y },
                canvasPoint)) {
            placementPosition = canvasPoint;
        }
    }

    if (ext == ".prefab" || IsSupportedModelAsset(assetPath) || (m_sceneViewMode == SceneViewMode::Mode2D && (IsSupportedSpriteAsset(assetPath) || IsSupportedFontAsset(assetPath)))) {
        ImVec2 screenPos{};
        if (ProjectWorldToSceneScreen(m_sceneViewRect, view, projection, placementPosition, screenPos)) {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            const ImU32 ringColor = IM_COL32(96, 196, 255, 220);
            const ImU32 fillColor = IM_COL32(96, 196, 255, 48);
            drawList->AddCircleFilled(screenPos, 10.0f, fillColor, 24);
            drawList->AddCircle(screenPos, 10.0f, ringColor, 24, 2.0f);
            drawList->AddLine(ImVec2(screenPos.x - 14.0f, screenPos.y), ImVec2(screenPos.x + 14.0f, screenPos.y), ringColor, 2.0f);
            drawList->AddLine(ImVec2(screenPos.x, screenPos.y - 14.0f), ImVec2(screenPos.x, screenPos.y + 14.0f), ringColor, 2.0f);

            const bool isOriginPlacement = std::fabs(placementPosition.x) < 0.0001f &&
                                           std::fabs(placementPosition.y) < 0.0001f &&
                                           std::fabs(placementPosition.z) < 0.0001f;
            const bool isTextPlacement = (m_sceneViewMode == SceneViewMode::Mode2D && IsSupportedFontAsset(assetPath));
            const std::string previewText = isTextPlacement
                ? (isOriginPlacement ? "Place Text at Origin" : "Place Text")
                : (isOriginPlacement ? "Place at Origin" : "Place on Surface");
            drawList->AddText(ImVec2(screenPos.x + 16.0f, screenPos.y - 8.0f), IM_COL32(255, 255, 255, 230), previewText.c_str());
        }
    }

    if (!payload->IsDelivery()) {
        ImGui::EndDragDropTarget();
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    auto selectCreated2DEntity = [&](EntityID liveRoot) {
        if (Entity::IsNull(liveRoot)) {
            return;
        }
        Editor2D::FinalizeCreatedEntity(registry, liveRoot);
        EditorSelection::Instance().SelectEntity(liveRoot);
        m_gizmoOperation = GizmoOperation::Translate;
        m_scenePickPending = false;
        m_scenePickBlockedByGizmo = true;
        RequestWindowFocus(WindowFocusTarget::SceneView);
    };

    if (ext == ".prefab") {
        EntitySnapshot::Snapshot snapshot;
        if (PrefabSystem::LoadPrefabSnapshot(assetPath, snapshot) && !snapshot.nodes.empty()) {
            ApplyPlacementToSnapshot(snapshot, placementPosition);
            auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), Entity::NULL_ID, "Place Prefab");
            auto* actionPtr = action.get();
            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
            if (!Entity::IsNull(actionPtr->GetLiveRoot())) {
                EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
            }
        }
    } else if (m_sceneViewMode == SceneViewMode::Mode2D && IsSupportedSpriteAsset(assetPath)) {
        DirectX::XMFLOAT2 size = { 128.0f, 128.0f };
        if (auto texture = ResourceManager::Instance().GetTexture(assetPath.string())) {
            size = { static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()) };
        }
        EntitySnapshot::Snapshot snapshot = BuildSingleSpriteSnapshot(
            assetPath.stem().string(),
            assetPath.string(),
            { placementPosition.x, placementPosition.y },
            size);
        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), Entity::NULL_ID, "Place Sprite");
        auto* actionPtr = action.get();
        UndoSystem::Instance().ExecuteAction(std::move(action), registry);
        selectCreated2DEntity(actionPtr->GetLiveRoot());
    } else if (m_sceneViewMode == SceneViewMode::Mode2D && IsSupportedFontAsset(assetPath)) {
        EntitySnapshot::Snapshot snapshot = BuildSingleTextSnapshot(
            assetPath.stem().string(),
            assetPath.string(),
            { placementPosition.x, placementPosition.y });
        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), Entity::NULL_ID, "Place Text");
        auto* actionPtr = action.get();
        UndoSystem::Instance().ExecuteAction(std::move(action), registry);
        selectCreated2DEntity(actionPtr->GetLiveRoot());
    } else if (IsSupportedModelAsset(assetPath)) {
        MeshComponent meshComp;
        meshComp.modelFilePath = assetPath.string();
        meshComp.model = ResourceManager::Instance().CreateModelInstance(meshComp.modelFilePath);
        placementPosition = AdjustPlacementForModelBounds(meshComp, placementPosition);
        EntitySnapshot::Snapshot snapshot = BuildSingleEntitySnapshot(assetPath.stem().string(), &meshComp);
        if (ApplyPlacementToSnapshot(snapshot, placementPosition)) {
            auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), Entity::NULL_ID, "Place Model");
            auto* actionPtr = action.get();
            UndoSystem::Instance().ExecuteAction(std::move(action), registry);
            if (!Entity::IsNull(actionPtr->GetLiveRoot())) {
                EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
            }
        }
    }

    ImGui::EndDragDropTarget();
}

void EditorLayer::FocusSelectedEntity()
{
    if (!m_gameLayer) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity) {
        return;
    }

    const EntityID entity = selection.GetEntity();
    if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
        return;
    }

    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        if (auto* rect = registry.GetComponent<RectTransformComponent>(entity)) {
            m_editor2DCenter = rect->anchoredPosition;
            const float extent = (std::max)(rect->sizeDelta.x * rect->scale2D.x, rect->sizeDelta.y * rect->scale2D.y) * 0.5f;
            m_editor2DZoom = (std::max)(5.0f, extent * 1.25f);
            return;
        }
    }

    if (auto* mesh = registry.GetComponent<MeshComponent>(entity); mesh && mesh->model) {
        const auto& bounds = mesh->model->GetWorldBounds();
        FocusEditorCameraOnTarget(bounds.Center, Max3(bounds.Extents.x, bounds.Extents.y, bounds.Extents.z));
        return;
    }

    if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
        FocusEditorCameraOnTarget(transform->worldPosition, 1.0f);
    }
}

void EditorLayer::FocusEditorCameraOnTarget(const DirectX::XMFLOAT3& target, float radius)
{
    const DirectX::XMFLOAT3 forward = GetEditorCameraDirection();
    const float distance = ComputeFocusDistance(radius, m_editorCameraFovY);
    DirectX::XMFLOAT3 position{
        target.x - forward.x * distance,
        target.y - forward.y * distance,
        target.z - forward.z * distance
    };

    m_editorCameraPosition = position;
    m_editorCameraUserOverride = true;
    m_editorCameraAutoFramed = true;
}

void EditorLayer::SetEditorCameraDirection(const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& target, float distance)
{
    using namespace DirectX;
    XMFLOAT3 normalized = forward;
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&normalized));
    XMStoreFloat3(&normalized, dir);

    m_editorCameraYaw = std::atan2(normalized.x, normalized.z);
    const float xzLen = std::sqrt(normalized.x * normalized.x + normalized.z * normalized.z);
    m_editorCameraPitch = std::atan2(normalized.y, xzLen);
    m_editorCameraPosition = {
        target.x - normalized.x * distance,
        target.y - normalized.y * distance,
        target.z - normalized.z * distance
    };
    m_editorCameraUserOverride = true;
}

