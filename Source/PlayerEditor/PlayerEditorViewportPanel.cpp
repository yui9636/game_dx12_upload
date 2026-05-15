// PlayerEditor の 3D モデルプレビュー用 Viewport パネル。
// PlayerEditorPanel.cpp から分離し、パネル単位で読みやすくしている。
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

    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImVec2 avail = ImGui::GetContentRegionAvail();
    m_previewRenderSize = {
        (std::max)(avail.x, 0.0f),
        (std::max)(avail.y, 0.0f)
    };
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size((std::max)(avail.x, 1.0f), (std::max)(avail.y, 1.0f));
    m_viewportRect = { pos.x, pos.y, avail.x, avail.y };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    void* texId = m_viewportTexture ? ImGuiRenderer::GetTextureID(m_viewportTexture) : nullptr;
    if (texId) {
        ImGui::Image((ImTextureID)texId, size);
        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const ImVec2 imageMax = ImGui::GetItemRectMax();
        const ImVec2 imageSize(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
        m_viewportRect = { imageMin.x, imageMin.y, imageSize.x, imageSize.y };
        m_viewportHovered = ImGui::IsItemHovered();
    } else {
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(0, 0, 0, 255));
        ImGui::Dummy(size);
        m_viewportHovered = ImGui::IsItemHovered();
    }

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
