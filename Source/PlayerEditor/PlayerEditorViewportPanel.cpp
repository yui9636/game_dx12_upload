// ============================================================================
// PlayerEditor — Viewport panel (3D model preview window)
// Sibling of PlayerEditorPanel.cpp; split out for readability.
// ============================================================================
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "ImGuiRenderer.h"
#include "PlayerEditorPanelInternal.h"
#include "Component/TransformComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"

using namespace PlayerEditorInternal;

void PlayerEditorPanel::DrawViewportPanel()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool open = ImGui::Begin(kPEViewportTitle);
    ImGui::PopStyleVar();
    if (!open) { ImGui::End(); return; }

    const float sharedDirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    const bool usingSharedSceneView = sharedDirLengthSq > 0.0001f;

    (void)usingSharedSceneView;
    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_previewRenderSize = {
        (std::max)(avail.x, 0.0f),
        (std::max)(avail.y, 0.0f)
    };
    m_viewportHovered = false;
    m_viewportRect = { 0.0f, 0.0f, avail.x, avail.y };

    if (!HasOpenModel() && !m_viewportTexture) {
        ImGui::SetCursorPos(ImVec2(12.0f, 12.0f));
        DrawEmptyState();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
                const std::string droppedPath(static_cast<const char*>(payload->Data));
                if (HasExtension(droppedPath, { ".prefab", ".fbx", ".gltf", ".glb", ".obj" })) {
                    OpenModelFromPath(droppedPath);
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::End();
        return;
    }

    if (m_viewportTexture) {
        // Display the dedicated render target
        void* texId = ImGuiRenderer::GetTextureID(m_viewportTexture);
        if (texId) {
            ImGui::Image((ImTextureID)texId, avail);

            const ImVec2 imageMin = ImGui::GetItemRectMin();
            const ImVec2 imageMax = ImGui::GetItemRectMax();
            const ImVec2 imageSize = ImVec2(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
            m_viewportRect = { imageMin.x, imageMin.y, imageSize.x, imageSize.y };
            m_viewportHovered = ImGui::IsItemHovered();
            const int markerBoneIndex = (m_hoveredBoneIndex >= 0) ? m_hoveredBoneIndex : m_selectedBoneIndex;
            if (m_model && markerBoneIndex >= 0) {
                ImVec2 markerPos{};
                if (ProjectBoneMarkerToViewport(
                    m_model,
                    markerBoneIndex,
                    usingSharedSceneView ? 1.0f : m_previewModelScale,
                    GetPreviewCameraPosition(),
                    GetPreviewCameraTarget(),
                    GetPreviewCameraFovY(),
                    GetPreviewNearZ(),
                    GetPreviewFarZ(),
                    imageMin,
                    imageSize,
                    markerPos))
                {
                    const auto& nodes = m_model->GetNodes();
                    const char* boneName = (markerBoneIndex >= 0 && markerBoneIndex < static_cast<int>(nodes.size()))
                        ? nodes[markerBoneIndex].name.c_str()
                        : "";

                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    dl->PushClipRect(imageMin, imageMax, true);
                    dl->AddCircleFilled(markerPos, 5.0f, IM_COL32(255, 230, 80, 255));
                    dl->AddCircle(markerPos, 9.0f, IM_COL32(255, 255, 255, 220), 0, 1.5f);
                    dl->AddLine(ImVec2(markerPos.x - 10.0f, markerPos.y), ImVec2(markerPos.x - 3.0f, markerPos.y), IM_COL32(255, 255, 255, 220), 2.0f);
                    dl->AddLine(ImVec2(markerPos.x + 3.0f, markerPos.y), ImVec2(markerPos.x + 10.0f, markerPos.y), IM_COL32(255, 255, 255, 220), 2.0f);
                    dl->AddLine(ImVec2(markerPos.x, markerPos.y - 10.0f), ImVec2(markerPos.x, markerPos.y - 3.0f), IM_COL32(255, 255, 255, 220), 2.0f);
                    dl->AddLine(ImVec2(markerPos.x, markerPos.y + 3.0f), ImVec2(markerPos.x, markerPos.y + 10.0f), IM_COL32(255, 255, 255, 220), 2.0f);
                    if (boneName && boneName[0] != '\0') {
                        ImVec2 labelSize = ImGui::CalcTextSize(boneName);
                        const float margin = 6.0f;
                        ImVec2 labelPos(markerPos.x + 12.0f, markerPos.y - labelSize.y - 8.0f);
                        if (labelPos.x + labelSize.x + 8.0f > imageMax.x - margin) {
                            labelPos.x = markerPos.x - labelSize.x - 16.0f;
                        }
                        if (labelPos.x < imageMin.x + margin) {
                            labelPos.x = imageMin.x + margin;
                        }
                        if (labelPos.y < imageMin.y + margin) {
                            labelPos.y = markerPos.y + 12.0f;
                        }
                        if (labelPos.y + labelSize.y + 4.0f > imageMax.y - margin) {
                            labelPos.y = imageMax.y - labelSize.y - 4.0f - margin;
                        }
                        dl->AddRectFilled(
                            ImVec2(labelPos.x - 4.0f, labelPos.y - 2.0f),
                            ImVec2(labelPos.x + labelSize.x + 4.0f, labelPos.y + labelSize.y + 2.0f),
                            IM_COL32(20, 20, 20, 220),
                            4.0f);
                        dl->AddText(labelPos, IM_COL32(255, 255, 255, 240), boneName);
                    }
                    dl->PopClipRect();
                }
            }
        }
    } else {
        // Placeholder: dark background with instructions
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        m_viewportRect = { pos.x, pos.y, avail.x, avail.y };
        dl->AddRectFilled(pos, ImVec2(pos.x + avail.x, pos.y + avail.y), IM_COL32(20, 20, 25, 255));

        // Center text
        const char* msg = "3D Model Viewport";
        ImVec2 textSize = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2(pos.x + (avail.x - textSize.x) * 0.5f, pos.y + avail.y * 0.4f),
            IM_COL32(120, 120, 120, 255), msg);

        const char* sub = "Set viewport texture via SetViewportTexture()";
        ImVec2 subSize = ImGui::CalcTextSize(sub);
        dl->AddText(ImVec2(pos.x + (avail.x - subSize.x) * 0.5f, pos.y + avail.y * 0.4f + 20),
            IM_COL32(80, 80, 80, 255), sub);

        ImGui::Dummy(avail);
    }

    // Orbit camera controls (mouse drag on viewport)
    if (ImGui::IsItemHovered() && !usingSharedSceneView && !m_viewportTexture) {
        // Right-drag: orbit
        if (ImGui::IsMouseDragging(1)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(1);
            m_vpCameraYaw   += delta.x * 0.005f;
            m_vpCameraPitch += delta.y * 0.005f;
            m_vpCameraPitch = (std::max)(-1.5f, (std::min)(1.5f, m_vpCameraPitch));
            ImGui::ResetMouseDragDelta(1);
        }
        // Scroll: zoom
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_vpCameraDist -= wheel * 0.5f;
            m_vpCameraDist = (std::max)(0.5f, (std::min)(50.0f, m_vpCameraDist));
        }
    }

    ImGui::End();
}

DirectX::XMFLOAT3 PlayerEditorPanel::GetPreviewCameraTarget() const
{
    const float dirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    if (dirLengthSq > 0.0001f) {
        return {
            m_sharedSceneCameraPosition.x + m_sharedSceneCameraDirection.x,
            m_sharedSceneCameraPosition.y + m_sharedSceneCameraDirection.y,
            m_sharedSceneCameraPosition.z + m_sharedSceneCameraDirection.z
        };
    }

    if (m_model) {
        const auto bounds = m_model->GetWorldBounds();
        DirectX::XMFLOAT3 target = bounds.Center;
        target.y += bounds.Extents.y * 0.12f;
        return target;
    }

    if (m_registry && !Entity::IsNull(m_previewEntity)) {
        if (const auto* transform = m_registry->GetComponent<TransformComponent>(m_previewEntity)) {
            return transform->worldPosition;
        }
    }

    return { 0.0f, 1.0f, 0.0f };
}

DirectX::XMFLOAT3 PlayerEditorPanel::GetPreviewCameraDirection() const
{
    const float sharedDirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    if (sharedDirLengthSq > 0.0001f) {
        return m_sharedSceneCameraDirection;
    }

    const float cosPitch = std::cos(m_vpCameraPitch);
    DirectX::XMFLOAT3 dir = {
        std::sin(m_vpCameraYaw) * cosPitch,
        std::sin(m_vpCameraPitch),
        std::cos(m_vpCameraYaw) * cosPitch
    };

    const float lenSq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
    if (lenSq > 0.0001f) {
        const float invLen = 1.0f / std::sqrt(lenSq);
        dir.x *= invLen;
        dir.y *= invLen;
        dir.z *= invLen;
    } else {
        dir = { 0.0f, 0.0f, 1.0f };
    }
    return dir;
}

DirectX::XMFLOAT3 PlayerEditorPanel::GetPreviewCameraPosition() const
{
    const float sharedDirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    if (sharedDirLengthSq > 0.0001f) {
        return m_sharedSceneCameraPosition;
    }

    const DirectX::XMFLOAT3 target = GetPreviewCameraTarget();
    const DirectX::XMFLOAT3 dir = GetPreviewCameraDirection();
    return {
        target.x - dir.x * m_vpCameraDist,
        target.y - dir.y * m_vpCameraDist,
        target.z - dir.z * m_vpCameraDist
    };
}

float PlayerEditorPanel::GetPreviewCameraFovY() const
{
    const float sharedDirLengthSq =
        m_sharedSceneCameraDirection.x * m_sharedSceneCameraDirection.x +
        m_sharedSceneCameraDirection.y * m_sharedSceneCameraDirection.y +
        m_sharedSceneCameraDirection.z * m_sharedSceneCameraDirection.z;
    if (sharedDirLengthSq > 0.0001f) {
        return m_sharedSceneCameraFovY;
    }
    return 0.785398f;
}
