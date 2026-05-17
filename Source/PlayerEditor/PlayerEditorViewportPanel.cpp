// PlayerEditor の 3D モデルプレビュー用 Viewport パネル。
// PlayerEditorPanel.cpp から分離し、パネル単位で読みやすくしている。
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>

#include "ImGuiRenderer.h"
#include "PlayerEditorPanelInternal.h"
#include "Component/CameraBehaviorComponent.h"
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

    DrawViewportSurface();

    ImGui::End();
}

void PlayerEditorPanel::DrawViewportSurface()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

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

    DrawViewportOverlay(ImVec2(m_viewportRect.x, m_viewportRect.y), ImVec2(m_viewportRect.z, m_viewportRect.w));

    ImGui::PopStyleVar(2);
}

void PlayerEditorPanel::DrawViewportOverlay(const ImVec2& imageMin, const ImVec2& imageSize)
{
    if (imageSize.x <= 1.0f || imageSize.y <= 1.0f) {
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 bg = IM_COL32(18, 20, 23, 190);
    const ImU32 fg = IM_COL32(230, 234, 240, 255);

    const char* modeText = m_viewMode == PlayerEditorViewMode::Test ? "TEST" : "EDIT";
    const std::string title = std::string(modeText) + "  " + (HasOpenModel() ? GetActiveToolLabel() : "No Model");
    const ImVec2 textSize = ImGui::CalcTextSize(title.c_str());
    const ImVec2 badgeMin(imageMin.x + 12.0f, imageMin.y + 12.0f);
    const ImVec2 badgeMax(badgeMin.x + textSize.x + 20.0f, badgeMin.y + 28.0f);
    dl->AddRectFilled(badgeMin, badgeMax, bg, 4.0f);
    dl->AddText(ImVec2(badgeMin.x + 10.0f, badgeMin.y + 7.0f), fg, title.c_str());

    if (!HasOpenModel()) {
        const char* hint = "Drop a model or prefab here";
        const ImVec2 hintSize = ImGui::CalcTextSize(hint);
        dl->AddText(
            ImVec2(imageMin.x + imageSize.x * 0.5f - hintSize.x * 0.5f,
                   imageMin.y + imageSize.y * 0.5f - hintSize.y * 0.5f),
            IM_COL32(190, 198, 210, 255),
            hint);
        return;
    }

    if (m_viewMode == PlayerEditorViewMode::Test) {
        const char* testHint = "Live combat preview";
        const ImVec2 testSize = ImGui::CalcTextSize(testHint);
        dl->AddRectFilled(
            ImVec2(imageMin.x + 12.0f, imageMin.y + imageSize.y - 44.0f),
            ImVec2(imageMin.x + 12.0f + testSize.x + 20.0f, imageMin.y + imageSize.y - 14.0f),
            bg,
            4.0f);
        dl->AddText(
            ImVec2(imageMin.x + 22.0f, imageMin.y + imageSize.y - 36.0f),
            fg,
            testHint);
        return;
    }

    const float buttonW = 128.0f;
    const float buttonH = 34.0f;
    const float gap = 8.0f;
    ImVec2 cursor(imageMin.x + 16.0f, imageMin.y + 58.0f);
    const ImVec2 backup = ImGui::GetCursorScreenPos();

    const auto toolButton = [&](const char* label, PlayerEditorTool tool, const char* id) {
        ImGui::SetCursorScreenPos(cursor);
        const bool selected = m_activeTool == tool && m_toolPopoverOpen;
        const ImVec2 min = cursor;
        const ImVec2 rectMax(cursor.x + buttonW, cursor.y + buttonH);
        const std::string buttonId = std::string("##PlayerEditorRail") + id;
        ImGui::InvisibleButton(buttonId.c_str(), ImVec2(buttonW, buttonH));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            m_activeTool = tool;
            m_toolPopoverOpen = selected ? !m_toolPopoverOpen : true;
        }

        const ImU32 fill = selected
            ? IM_COL32(33, 96, 128, 218)
            : hovered
                ? IM_COL32(35, 42, 50, 220)
                : IM_COL32(18, 22, 28, 178);
        const ImU32 border = selected
            ? IM_COL32(78, 205, 255, 255)
            : hovered
                ? IM_COL32(115, 135, 150, 230)
                : IM_COL32(60, 70, 78, 180);
        dl->AddRectFilled(min, rectMax, fill, 8.0f);
        dl->AddRect(min, rectMax, border, 8.0f, 0, selected ? 2.0f : 1.0f);
        dl->AddRectFilled(
            ImVec2(min.x + 6.0f, min.y + 7.0f),
            ImVec2(min.x + 9.0f, rectMax.y - 7.0f),
            selected ? IM_COL32(78, 205, 255, 255) : IM_COL32(130, 145, 155, 180),
            2.0f);
        dl->AddText(ImVec2(min.x + 18.0f, min.y + 9.0f), fg, label);

        cursor.y += buttonH + gap;
    };

    toolButton("State", PlayerEditorTool::State, "State");
    toolButton("Timeline", PlayerEditorTool::Timeline, "Timeline");
    toolButton("Attack", PlayerEditorTool::Hitbox, "Hitbox");
    toolButton("Body", PlayerEditorTool::Body, "Body");
    toolButton("Input", PlayerEditorTool::Input, "Input");
    toolButton("Bones", PlayerEditorTool::Bone, "Bone");
    ImGui::SetCursorScreenPos(backup);
}

bool PlayerEditorPanel::TryBuildThirdPersonPreviewCamera(
    DirectX::XMFLOAT3& outPosition,
    DirectX::XMFLOAT3& outTarget,
    DirectX::XMFLOAT3& outDirection) const
{
    if (m_viewMode != PlayerEditorViewMode::Test || !m_registry || Entity::IsNull(m_previewEntity)) {
        return false;
    }

    const auto* transform = m_registry->GetComponent<TransformComponent>(m_previewEntity);
    const auto* tpv = m_registry->GetComponent<CameraTPVControlComponent>(m_previewEntity);
    if (!transform || !tpv) {
        return false;
    }

    using namespace DirectX;
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR forward = XMVectorSet(std::sin(tpv->yaw), 0.0f, std::cos(tpv->yaw), 0.0f);
    if (tpv->followTargetFacing) {
        forward = XMVector3Rotate(
            XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
            XMLoadFloat4(&transform->localRotation));
        forward = XMVectorSetY(forward, 0.0f);
    }
    if (XMVectorGetX(XMVector3LengthSq(forward)) <= 0.0001f) {
        forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }
    forward = XMVector3Normalize(forward);

    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
    const XMVECTOR playerPosition = XMLoadFloat3(&transform->worldPosition);
    const float pitch = std::clamp(tpv->pitch, -1.1f, 1.1f);
    const float horizontalDistance = (std::max)(tpv->distance, 1.0f) * std::cos(pitch);
    const float verticalOffset = tpv->heightOffset + (std::max)(tpv->distance, 1.0f) * std::sin(pitch);

    const XMVECTOR target =
        playerPosition
        + up * (std::max)(tpv->lookAtHeight, 0.25f)
        + forward * tpv->lookAheadDistance;
    const XMVECTOR position =
        playerPosition
        - forward * horizontalDistance
        + right * tpv->shoulderOffset
        + up * verticalOffset
        + forward * tpv->forwardOffset;

    XMVECTOR direction = target - position;
    if (XMVectorGetX(XMVector3LengthSq(direction)) <= 0.0001f) {
        direction = forward;
    }
    direction = XMVector3Normalize(direction);

    XMStoreFloat3(&outPosition, position);
    XMStoreFloat3(&outTarget, target);
    XMStoreFloat3(&outDirection, direction);
    return true;
}

DirectX::XMFLOAT3 PlayerEditorPanel::GetPreviewCameraTarget() const
{
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 target{};
    DirectX::XMFLOAT3 direction{};
    if (TryBuildThirdPersonPreviewCamera(position, target, direction)) {
        return target;
    }

    if (m_model) {
        const auto bounds = m_model->GetWorldBounds();
        DirectX::XMFLOAT3 target = bounds.Center;
        if (m_registry && !Entity::IsNull(m_previewEntity) && !m_previewEntityOwned) {
            if (const auto* transform = m_registry->GetComponent<TransformComponent>(m_previewEntity)) {
                target.x += transform->worldPosition.x;
                target.y += transform->worldPosition.y;
                target.z += transform->worldPosition.z;
            }
        }
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
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 target{};
    DirectX::XMFLOAT3 direction{};
    if (TryBuildThirdPersonPreviewCamera(position, target, direction)) {
        return direction;
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
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 testTarget{};
    DirectX::XMFLOAT3 direction{};
    if (TryBuildThirdPersonPreviewCamera(position, testTarget, direction)) {
        return position;
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
    return 0.785398f;
}
