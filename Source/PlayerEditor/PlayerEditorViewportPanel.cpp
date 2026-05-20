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

#include "Render/ImGuiRenderer.h"
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

    // フリーカメラ: hover 中の右ドラッグ=回転 / middle ドラッグ=パン / ホイール=ズーム
    // Test モードでも操作可能 (TPV component が無ければそのまま、あっても override 可)
    if (m_viewportHovered) {
        ImGuiIO& io = ImGui::GetIO();

        // 右ドラッグ: orbit (yaw + pitch)
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
            const ImVec2 delta = io.MouseDelta;
            constexpr float kSensitivity = 0.008f;
            m_vpCameraYaw   -= delta.x * kSensitivity;
            m_vpCameraPitch -= delta.y * kSensitivity;
            m_vpCameraPitch = std::clamp(m_vpCameraPitch, -1.45f, 1.45f);
        }

        // middle ドラッグ: pan (target offset)
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            const ImVec2 delta = io.MouseDelta;
            const float panScale = m_vpCameraDist * 0.0018f;
            // camera right / up を計算
            const float cy = std::cos(m_vpCameraYaw);
            const float sy = std::sin(m_vpCameraYaw);
            const float cp = std::cos(m_vpCameraPitch);
            const float sp = std::sin(m_vpCameraPitch);
            // forward = (sin yaw cos pitch, sin pitch, cos yaw cos pitch)
            // right   = cross(up, forward) = (cos yaw, 0, -sin yaw)
            // up_cam  = cross(forward, right) = (-sin yaw sin pitch, cos pitch, -cos yaw sin pitch)
            const DirectX::XMFLOAT3 right{ cy, 0.0f, -sy };
            const DirectX::XMFLOAT3 upCam{ -sy * sp, cp, -cy * sp };
            m_vpCameraPanOffset.x -= (right.x * delta.x - upCam.x * delta.y) * panScale;
            m_vpCameraPanOffset.y -= (right.y * delta.x - upCam.y * delta.y) * panScale;
            m_vpCameraPanOffset.z -= (right.z * delta.x - upCam.z * delta.y) * panScale;
        }

        // ホイール: zoom (引きの上限は FarZ 近くまで、感度も上げる)
        if (io.MouseWheel != 0.0f) {
            const float zoomFactor = std::exp(-io.MouseWheel * 0.22f);
            m_vpCameraDist = std::clamp(m_vpCameraDist * zoomFactor, 0.05f, 2000.0f);
        }
    }

    DrawViewportOverlay(ImVec2(m_viewportRect.x, m_viewportRect.y), ImVec2(m_viewportRect.z, m_viewportRect.w));

    ImGui::PopStyleVar(2);
}

// コライダ Gizmo は PlayerModelPreviewStudio が DebugRenderSystem + Gizmos
// 経路で 3D ワイヤメッシュとして RT に焼き込む。ImGui 側での偽実装は廃止。

void PlayerEditorPanel::DrawViewportOverlay(const ImVec2& imageMin, const ImVec2& imageSize)
{
    if (imageSize.x <= 1.0f || imageSize.y <= 1.0f) return;
    if (m_drawingPipViewport) return;  // PiP 表示時は EDIT/TEST バッジ等を出さない

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 bg     = IM_COL32(10, 10, 10, 200);
    const ImU32 fg     = IM_COL32(255, 255, 255, 230);
    const ImU32 border = IM_COL32(56, 56, 56, 220);

    const char* modeText = m_viewMode == PlayerEditorViewMode::Test ? "TEST" : "EDIT";
    const std::string title = HasOpenModel() ? modeText : std::string(modeText) + "  No Model";
    const ImVec2 textSize = ImGui::CalcTextSize(title.c_str());
    const ImVec2 badgeMin(imageMin.x + 8.0f, imageMin.y + 8.0f);
    const ImVec2 badgeMax(badgeMin.x + textSize.x + 16.0f, badgeMin.y + 22.0f);
    dl->AddRectFilled(badgeMin, badgeMax, bg, 2.0f);
    dl->AddRect(badgeMin, badgeMax, border, 2.0f);
    dl->AddText(ImVec2(badgeMin.x + 8.0f, badgeMin.y + 4.0f), fg, title.c_str());

    if (!HasOpenModel()) {
        const char* hint = "Drop a model or prefab here";
        const ImVec2 hintSize = ImGui::CalcTextSize(hint);
        dl->AddText(
            ImVec2(imageMin.x + imageSize.x * 0.5f - hintSize.x * 0.5f,
                   imageMin.y + imageSize.y * 0.5f - hintSize.y * 0.5f),
            IM_COL32(255, 255, 255, 110),
            hint);
    }
}

bool PlayerEditorPanel::TryBuildThirdPersonPreviewCamera(
    DirectX::XMFLOAT3& outPosition,
    DirectX::XMFLOAT3& outTarget,
    DirectX::XMFLOAT3& outDirection) const
{
    // PlayerEditor では TPV 自動カメラを無効化し、常に手動軌道カメラを使う
    // (Test/Edit 両モードで Fit + ドラッグ + ホイール操作が一貫して効くため)
    (void)outPosition; (void)outTarget; (void)outDirection;
    return false;

    // 旧経路は参考用に下に残す (実行されない)
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

    // Fit でスナップショットされた軌道中心を優先使用 (アニメで動かない)
    if (m_orbitCenterValid) {
        return {
            m_orbitCenter.x + m_vpCameraPanOffset.x,
            m_orbitCenter.y + m_vpCameraPanOffset.y,
            m_orbitCenter.z + m_vpCameraPanOffset.z,
        };
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
        target.x += m_vpCameraPanOffset.x;
        target.y += m_vpCameraPanOffset.y;
        target.z += m_vpCameraPanOffset.z;
        return target;
    }

    if (m_registry && !Entity::IsNull(m_previewEntity)) {
        if (const auto* transform = m_registry->GetComponent<TransformComponent>(m_previewEntity)) {
            DirectX::XMFLOAT3 t = transform->worldPosition;
            t.x += m_vpCameraPanOffset.x;
            t.y += m_vpCameraPanOffset.y;
            t.z += m_vpCameraPanOffset.z;
            return t;
        }
    }

    return {
        m_vpCameraPanOffset.x,
        1.0f + m_vpCameraPanOffset.y,
        m_vpCameraPanOffset.z
    };
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
