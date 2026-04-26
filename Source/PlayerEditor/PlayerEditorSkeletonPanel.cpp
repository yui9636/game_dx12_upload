// ============================================================================
// PlayerEditor — Skeleton panel (bone tree + sockets) and persistent collider
// authoring helpers. Sibling of PlayerEditorPanel.cpp; split out for readability.
// ============================================================================
#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cctype>

#include <imgui.h>

#include "Icon/IconsFontAwesome7.h"
#include "PlayerEditorPanelInternal.h"
#include "PlayerEditorSession.h"
#include "Collision/CollisionManager.h"
#include "Component/ColliderComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"

using namespace PlayerEditorInternal;

// ----------------------------------------------------------------------------
// Persistent collider authoring (used by Skeleton + Properties panels)
// ----------------------------------------------------------------------------

ColliderComponent* PlayerEditorPanel::GetPreviewColliderComponent(bool createIfMissing)
{
    if (!m_registry || !CanUsePreviewEntity()) {
        return nullptr;
    }

    ColliderComponent* collider = m_registry->GetComponent<ColliderComponent>(m_previewEntity);
    if (!collider && createIfMissing) {
        m_registry->AddComponent(m_previewEntity, ColliderComponent{});
        collider = m_registry->GetComponent<ColliderComponent>(m_previewEntity);
    }

    if (collider) {
        collider->enabled = true;
        collider->drawGizmo = true;
    }

    return collider;
}

bool PlayerEditorPanel::TryAssignSelectedBoneToPersistentCollider(int boneIndex)
{
    if (boneIndex < 0) {
        return false;
    }

    ColliderComponent* collider = GetPreviewColliderComponent(false);
    if (!collider) {
        return false;
    }

    if (m_selectedColliderIdx < 0 || m_selectedColliderIdx >= static_cast<int>(collider->elements.size())) {
        return false;
    }

    ColliderComponent::Element& element = collider->elements[m_selectedColliderIdx];
    if (element.runtimeTag != 0) {
        return false;
    }

    element.nodeIndex = boneIndex;
    element.offsetLocal = { 0.0f, 0.0f, 0.0f };
    collider->enabled = true;
    collider->drawGizmo = true;
    m_colliderDirty = true;
    m_selectionCtx = SelectionContext::PersistentCollider;
    return true;
}

void PlayerEditorPanel::SelectPersistentCollider(int colliderIndex)
{
    ColliderComponent* collider = GetPreviewColliderComponent(false);
    if (!collider) {
        m_selectedColliderIdx = -1;
        return;
    }

    if (colliderIndex < 0 || colliderIndex >= static_cast<int>(collider->elements.size())) {
        m_selectedColliderIdx = -1;
        return;
    }

    m_selectedColliderIdx = colliderIndex;
    m_selectionCtx = SelectionContext::PersistentCollider;
}

void PlayerEditorPanel::AddPersistentCollider(ColliderAttribute attribute)
{
    ColliderComponent* collider = GetPreviewColliderComponent(true);
    if (!collider) {
        return;
    }

    ColliderComponent::Element element;
    element.enabled = true;
    element.attribute = attribute;
    element.runtimeTag = 0;
    element.registeredId = 0;
    element.nodeIndex = m_selectedBoneIndex;
    element.offsetLocal = { 0.0f, 0.0f, 0.0f };

    if (attribute == ColliderAttribute::Body) {
        element.type = ColliderShape::Capsule;
        element.radius = 18.0f;
        element.height = 60.0f;
        element.color = { 0.20f, 0.90f, 0.35f, 0.35f };
    }
    else {
        element.type = ColliderShape::Sphere;
        element.radius = 20.0f;
        element.height = 20.0f;
        element.color = { 1.0f, 0.25f, 0.25f, 0.35f };
    }

    int insertIndex = static_cast<int>(collider->elements.size());
    for (int i = 0; i < static_cast<int>(collider->elements.size()); ++i) {
        if (collider->elements[i].runtimeTag != 0) {
            insertIndex = i;
            break;
        }
    }

    collider->elements.insert(collider->elements.begin() + insertIndex, element);
    collider->enabled = true;
    collider->drawGizmo = true;
    m_colliderDirty = true;
    SelectPersistentCollider(insertIndex);
}

// ----------------------------------------------------------------------------
// Socket import/export thin wrappers (delegate to PlayerEditorSession)
// ----------------------------------------------------------------------------

void PlayerEditorPanel::ImportSocketsFromPreviewEntity()
{
    PlayerEditorSession::ImportSocketsFromPreviewEntity(*this);
}

void PlayerEditorPanel::ExportSocketsToPreviewEntity()
{
    PlayerEditorSession::ExportSocketsToPreviewEntity(*this);
}

// ----------------------------------------------------------------------------
// Skeleton panel — bone tree + socket list
// ----------------------------------------------------------------------------

void PlayerEditorPanel::DrawSkeletonPanel()
{
    if (!ImGui::Begin(kPESkeletonTitle)) { ImGui::End(); return; }

    m_hoveredBoneIndex = -1;

    if (!m_model) {
        ImGui::TextDisabled("No model assigned.");
        ImGui::End();
        return;
    }

    const auto& nodes = m_model->GetNodes();

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##BoneSearch", ICON_FA_MAGNIFYING_GLASS " Search bones...",
        m_boneSearchFilter, sizeof(m_boneSearchFilter));

    ImGui::Separator();

    const float socketPaneHeight = 28.0f;
    const float treePaneHeight = (std::max)(0.0f, ImGui::GetContentRegionAvail().y - socketPaneHeight - ImGui::GetStyle().ItemSpacing.y);
    ImGui::BeginChild("BoneTree", ImVec2(0, treePaneHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (m_boneSearchFilter[0] == '\0') {
        for (int i = 0; i < (int)nodes.size(); ++i) {
            if (nodes[i].parentIndex < 0) {
                DrawBoneTreeNode(i);
            }
        }
    } else {
        std::string filterLower(m_boneSearchFilter);
        for (auto& c : filterLower) c = (char)tolower(c);

        for (int i = 0; i < (int)nodes.size(); ++i) {
            std::string nameLower = nodes[i].name;
            for (auto& c : nameLower) c = (char)tolower(c);

            if (nameLower.find(filterLower) != std::string::npos) {
                bool selected = (m_selectedBoneIndex == i);
                if (ImGui::Selectable(("[" + std::to_string(i) + "] " + nodes[i].name).c_str(), selected)) {
                    m_selectedBoneIndex = i;
                    m_selectedBoneName = nodes[i].name;
                    if (!TryAssignSelectedBoneToTimelineItem(i) && !TryAssignSelectedBoneToPersistentCollider(i)) {
                        m_selectionCtx = SelectionContext::Bone;
                    }
                }
            }
        }
    }

    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y * 0.1f));
    DrawSocketList(socketPaneHeight);

    ImGui::End();
}

void PlayerEditorPanel::DrawBoneTreeNode(int nodeIndex)
{
    if (!m_model) return;
    const auto& nodes = m_model->GetNodes();
    if (nodeIndex < 0 || nodeIndex >= (int)nodes.size()) return;

    const auto& node = nodes[nodeIndex];
    bool hasChildren = !node.children.empty();
    bool selected = (m_selectedBoneIndex == nodeIndex);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Icon: bone vs joint
    const char* icon = hasChildren ? ICON_FA_BONE : ICON_FA_CIRCLE;
    std::string label = std::string(icon) + " " + node.name;

    bool open = ImGui::TreeNodeEx(("##bone_" + std::to_string(nodeIndex)).c_str(), flags, "%s", label.c_str());

    if (ImGui::IsItemHovered()) {
        m_hoveredBoneIndex = nodeIndex;
    }

    // Click to select
    if (ImGui::IsItemClicked(0)) {
        m_selectedBoneIndex = nodeIndex;
        m_selectedBoneName = node.name;
        if (!TryAssignSelectedBoneToTimelineItem(nodeIndex) && !TryAssignSelectedBoneToPersistentCollider(nodeIndex)) {
            m_selectionCtx = SelectionContext::Bone;
        }
    }

    // Right-click: quick actions
    if (ImGui::IsItemClicked(1)) {
        m_selectedBoneIndex = nodeIndex;
        m_selectedBoneName = node.name;
        ImGui::OpenPopup("BoneCtx");
    }

    if (ImGui::BeginPopup("BoneCtx")) {
        ImGui::Text(ICON_FA_BONE " %s [%d]", m_selectedBoneName.c_str(), m_selectedBoneIndex);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_PLUG " Create Socket Here")) {
            NodeSocket sock;
            sock.name = "Socket_" + node.name;
            sock.parentBoneName = node.name;
            sock.cachedBoneIndex = nodeIndex;
            m_sockets.push_back(sock);
            m_socketDirty = true;
        }
        if (ImGui::MenuItem(ICON_FA_PLUS " Add Body Collider")) {
            AddPersistentCollider(ColliderAttribute::Body);
        }
        if (ImGui::MenuItem(ICON_FA_PLUS " Add Attack Collider")) {
            AddPersistentCollider(ColliderAttribute::Attack);
        }
        ImGui::EndPopup();
    }

    // Recurse children
    if (open && hasChildren) {
        for (auto* child : node.children) {
            // Find child index
            int childIdx = (int)(child - &nodes[0]);
            DrawBoneTreeNode(childIdx);
        }
        ImGui::TreePop();
    }
}

void PlayerEditorPanel::DrawSocketList(float height)
{
    ImGui::BeginChild("SocketList", ImVec2(0, height), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));

    ImGui::TextDisabled(ICON_FA_PLUG " Sockets (%d)", (int)m_sockets.size());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_PLUS " Add")) {
        NodeSocket sock;
        sock.name = "NewSocket";
        if (m_selectedBoneIndex >= 0 && !m_selectedBoneName.empty()) {
            sock.parentBoneName = m_selectedBoneName;
            sock.cachedBoneIndex = m_selectedBoneIndex;
            sock.name = "Socket_" + m_selectedBoneName;
        }
        m_sockets.push_back(sock);
        m_socketDirty = true;
    }

    ImGui::Separator();

    for (int si = 0; si < (int)m_sockets.size(); ++si) {
        auto& sock = m_sockets[si];
        ImGui::PushID(si);

        bool selected = (m_selectedSocketIdx == si);
        std::string label = sock.name + " -> " + sock.parentBoneName;
        if (ImGui::Selectable(label.c_str(), selected)) {
            m_selectedSocketIdx = si;
            m_selectionCtx = SelectionContext::Socket;
        }

        ImGui::PopID();
    }

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
}

const char* PlayerEditorPanel::GetBoneNameByIndex(int boneIndex) const
{
    if (!m_model) {
        return "(none)";
    }

    const auto& nodes = m_model->GetNodes();
    if (boneIndex < 0 || boneIndex >= static_cast<int>(nodes.size())) {
        return "(none)";
    }

    return nodes[boneIndex].name.c_str();
}

// ----------------------------------------------------------------------------
// Persistent collider drawing (Properties panel sub-sections)
// ----------------------------------------------------------------------------

void PlayerEditorPanel::DrawPersistentColliderSection()
{
    ColliderComponent* collider = GetPreviewColliderComponent(false);

    int persistentCount = 0;
    if (collider) {
        for (const auto& element : collider->elements) {
            if (element.runtimeTag == 0) {
                ++persistentCount;
            }
        }
    }

    ImGui::Text("Persistent Colliders (%d)", persistentCount);

    const bool canAdd = HasOpenModel() && CanUsePreviewEntity();
    if (!canAdd) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("+ Body")) {
        AddPersistentCollider(ColliderAttribute::Body);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Attack")) {
        AddPersistentCollider(ColliderAttribute::Attack);
    }
    if (!canAdd) {
        ImGui::EndDisabled();
    }

    if (!collider || persistentCount == 0) {
        return;
    }

    for (int i = 0; i < static_cast<int>(collider->elements.size()); ++i) {
        const auto& element = collider->elements[i];
        if (element.runtimeTag != 0) {
            continue;
        }

        std::string label =
            std::string(GetColliderAttributeLabel(element.attribute)) +
            " " +
            GetColliderShapeLabel(element.type) +
            " -> " +
            GetBoneNameByIndex(element.nodeIndex);

        if (ImGui::Selectable(label.c_str(), m_selectedColliderIdx == i)) {
            SelectPersistentCollider(i);
        }
    }
}

void PlayerEditorPanel::DrawPersistentColliderInspector()
{
    ColliderComponent* collider = GetPreviewColliderComponent(false);
    if (!collider ||
        m_selectedColliderIdx < 0 ||
        m_selectedColliderIdx >= static_cast<int>(collider->elements.size()) ||
        collider->elements[m_selectedColliderIdx].runtimeTag != 0) {
        ImGui::TextDisabled("Persistent collider not selected.");
        return;
    }

    ColliderComponent::Element& element = collider->elements[m_selectedColliderIdx];
    collider->enabled = true;
    collider->drawGizmo = true;

    ImGui::Text("Persistent Collider");
    ImGui::Separator();

    if (ImGui::Checkbox("Enabled", &element.enabled)) {
        m_colliderDirty = true;
    }

    int attributeIndex = static_cast<int>(element.attribute);
    const char* attributeLabels[] = { "Body", "Attack" };
    if (ImGui::Combo("Kind", &attributeIndex, attributeLabels, IM_ARRAYSIZE(attributeLabels))) {
        element.attribute = static_cast<ColliderAttribute>(attributeIndex);
        m_colliderDirty = true;
    }

    int shapeIndex = static_cast<int>(element.type);
    const char* shapeLabels[] = { "Sphere", "Capsule", "Box" };
    if (ImGui::Combo("Shape", &shapeIndex, shapeLabels, IM_ARRAYSIZE(shapeLabels))) {
        element.type = static_cast<ColliderShape>(shapeIndex);
        if (element.type == ColliderShape::Box && element.size.LengthSquared() <= 0.0001f) {
            element.size = { 20.0f, 20.0f, 20.0f };
        }
        m_colliderDirty = true;
    }

    ImGui::Text("Bone: %s", GetBoneNameByIndex(element.nodeIndex));
    if (m_selectedBoneIndex >= 0) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Use Selected Bone")) {
            element.nodeIndex = m_selectedBoneIndex;
            element.offsetLocal = { 0.0f, 0.0f, 0.0f };
            m_colliderDirty = true;
        }
    }

    if (m_selectedSocketIdx >= 0 && m_selectedSocketIdx < static_cast<int>(m_sockets.size())) {
        if (ImGui::SmallButton("Use Selected Socket")) {
            const NodeSocket& socket = m_sockets[m_selectedSocketIdx];
            element.nodeIndex = socket.cachedBoneIndex;
            element.offsetLocal = socket.offsetPos;
            m_colliderDirty = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_sockets[m_selectedSocketIdx].name.c_str());
    }

    if (ImGui::DragFloat3("Offset", &element.offsetLocal.x, 0.1f)) {
        m_colliderDirty = true;
    }

    switch (element.type) {
    case ColliderShape::Sphere:
        if (ImGui::DragFloat("Radius", &element.radius, 0.1f, 0.0f, 100.0f)) {
            m_colliderDirty = true;
        }
        break;

    case ColliderShape::Capsule:
        if (ImGui::DragFloat("Radius", &element.radius, 0.1f, 0.0f, 100.0f)) {
            m_colliderDirty = true;
        }
        if (ImGui::DragFloat("Height", &element.height, 0.1f, 0.0f, 200.0f)) {
            m_colliderDirty = true;
        }
        break;

    case ColliderShape::Box:
        if (ImGui::DragFloat3("Size", &element.size.x, 0.1f, 0.0f, 200.0f)) {
            m_colliderDirty = true;
        }
        break;
    }

    float rgba[4] = {
        element.color.x,
        element.color.y,
        element.color.z,
        element.color.w
    };
    if (ImGui::ColorEdit4("Color", rgba)) {
        element.color = { rgba[0], rgba[1], rgba[2], rgba[3] };
        m_colliderDirty = true;
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH " Delete Collider")) {
        if (element.registeredId != 0) {
            CollisionManager::Instance().Remove(element.registeredId);
        }
        collider->elements.erase(collider->elements.begin() + m_selectedColliderIdx);
        m_selectedColliderIdx = -1;
        m_selectionCtx = SelectionContext::Bone;
        m_colliderDirty = true;
    }
    ImGui::PopStyleColor();
}
