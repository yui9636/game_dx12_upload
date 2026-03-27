#include "HierarchyECSUI.h"
#include "Registry/Registry.h"
#include "Engine/EditorSelection.h"
#include "Icon/IconFontManager.h"
#include "Hierarchy/HierarchySystem.h"
#include "Asset/PrefabSystem.h"
#include "Console/Logger.h"
#include "System/UndoSystem.h"
#include "Undo/ComponentUndoAction.h"
#include "Undo/EntityUndoActions.h"

#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"
#include "Component/LightComponent.h"
#include "Component/ReflectionProbeComponent.h"

#include "System/ResourceManager.h"
#include "Model/Model.h"

#include <imgui.h>
#include <string>
#include <filesystem>
#include <algorithm>
#include <Component\MaterialComponent.h>

namespace
{
    EntitySnapshot::Snapshot BuildSingleEntitySnapshot(const std::string& name,
                                                       const MeshComponent* meshComponent = nullptr,
                                                       const LightComponent* lightComponent = nullptr,
                                                       const ReflectionProbeComponent* probeComponent = nullptr)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = TransformComponent{};
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        if (meshComponent) {
            std::get<std::optional<MeshComponent>>(node.components) = *meshComponent;
        }
        if (lightComponent) {
            std::get<std::optional<LightComponent>>(node.components) = *lightComponent;
        }
        if (probeComponent) {
            std::get<std::optional<ReflectionProbeComponent>>(node.components) = *probeComponent;
        }

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    bool IsSupportedModelAsset(const std::string& path)
    {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf";
    }

    EntitySnapshot::Snapshot BuildDirectionalLightSnapshot()
    {
        LightComponent directionalLight;
        directionalLight.type = LightType::Directional;
        directionalLight.castShadow = true;

        auto snapshot = BuildSingleEntitySnapshot("Directional Light", nullptr, &directionalLight, nullptr);
        auto& transform = std::get<std::optional<TransformComponent>>(snapshot.nodes[0].components);
        if (transform.has_value()) {
            using namespace DirectX;
            XMVECTOR rot = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(45.0f), XMConvertToRadians(45.0f), 0.0f);
            XMStoreFloat4(&transform->localRotation, rot);
        }
        return snapshot;
    }

    bool CreateEntityFromSnapshot(Registry* registry,
                                  EntitySnapshot::Snapshot snapshot,
                                  EntityID parentEntity,
                                  const char* actionName)
    {
        if (!registry || snapshot.nodes.empty()) {
            return false;
        }
        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parentEntity, actionName);
        auto* actionPtr = action.get();
        UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
        EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
        return true;
    }
}

void HierarchyECSUI::Render(Registry* registry) {
    ImGui::Begin(ICON_FA_LIST " Hierarchy");

    if (!registry) {
        ImGui::TextDisabled("No Active Scene");
        ImGui::End();
        return;
    }

    // ==========================================
    // ==========================================
    auto archetypes = registry->GetAllArchetypes();
    for (auto* archetype : archetypes) {
        const auto& entities = archetype->GetEntities();
        for (EntityID entity : entities) {
            HierarchyComponent* hier = registry->GetComponent<HierarchyComponent>(entity);
            if (!hier || Entity::IsNull(hier->parent)) {
                DrawEntityNode(registry, entity);
            }
        }
    }

    // ==========================================
    // ==========================================
    ImVec2 availSize = ImGui::GetContentRegionAvail();

    if (availSize.x <= 0.0f) { availSize.x = 1.0f; }
    if (availSize.y < 50.0f) { availSize.y = 50.0f; }

    ImGui::Dummy(availSize);
    HandleDragDropTarget(registry, Entity::NULL_ID);

    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create Empty Entity")) {
            CreateEntityFromSnapshot(registry, BuildSingleEntitySnapshot("Empty Entity"), Entity::NULL_ID, "Create Entity");
        }

        LightComponent pointLight;
        pointLight.type = LightType::Point;
        pointLight.range = 10.0f;
        pointLight.castShadow = false;
        if (ImGui::MenuItem("Create Point Light")) {
            CreateEntityFromSnapshot(registry,
                                     BuildSingleEntitySnapshot("Point Light", nullptr, &pointLight, nullptr),
                                     Entity::NULL_ID,
                                     "Create Point Light");
        }

        if (ImGui::MenuItem("Create Directional Light")) {
            CreateEntityFromSnapshot(registry, BuildDirectionalLightSnapshot(), Entity::NULL_ID, "Create Directional Light");
        }

        ReflectionProbeComponent probe;
        probe.needsBake = true;
        if (ImGui::MenuItem("Create Reflection Probe")) {
            CreateEntityFromSnapshot(registry,
                                     BuildSingleEntitySnapshot("Reflection Probe", nullptr, nullptr, &probe),
                                     Entity::NULL_ID,
                                     "Create Reflection Probe");
        }

        const auto& selection = EditorSelection::Instance();
        if (selection.GetType() == SelectionType::Asset && IsSupportedModelAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create From Selected Model")) {
                MeshComponent meshComp;
                meshComp.modelFilePath = selection.GetAssetPath();
                meshComp.model = ResourceManager::Instance().CreateModelInstance(selection.GetAssetPath());
                const std::string name = std::filesystem::path(meshComp.modelFilePath).stem().string();
                CreateEntityFromSnapshot(registry,
                                         BuildSingleEntitySnapshot(name, &meshComp, nullptr, nullptr),
                                         Entity::NULL_ID,
                                         "Create Entity From Model");
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

void HierarchyECSUI::DrawEntityNode(Registry* registry, EntityID entity) {
    auto* nameComp = registry->GetComponent<NameComponent>(entity);
    auto* hierComp = registry->GetComponent<HierarchyComponent>(entity);

    std::string entityName = nameComp ? nameComp->name : "Entity_" + std::to_string(Entity::GetIndex(entity));
    std::string idStr = "##" + std::to_string(Entity::GetIndex(entity));

    bool hasChildren = (hierComp && !Entity::IsNull(hierComp->firstChild));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (EditorSelection::Instance().GetType() == SelectionType::Entity &&
        EditorSelection::Instance().GetEntity() == entity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool isOpen = ImGui::TreeNodeEx((entityName + idStr).c_str(), flags, "%s %s", ICON_FA_CUBE, entityName.c_str());

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        EditorSelection::Instance().SelectEntity(entity);
    }

    if (ImGui::BeginDragDropSource()) {
        EntityID payloadEntity = entity;
        ImGui::SetDragDropPayload("ENGINE_ENTITY", &payloadEntity, sizeof(payloadEntity));
        ImGui::Text("%s %s", ICON_FA_CUBE, entityName.c_str());
        ImGui::EndDragDropSource();
    }

    const std::string popupId = "HierarchyEntityContext##" + std::to_string(static_cast<unsigned long long>(entity));
    if (ImGui::BeginPopupContextItem(popupId.c_str())) {
        if (ImGui::MenuItem("Duplicate")) {
            if (!PrefabSystem::CanDuplicate(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance children cannot be duplicated. Duplicate the root instance or unpack it first.");
            } else {
                EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(entity, *registry);
                EntityID parentEntity = Entity::NULL_ID;
                if (auto* hierarchy = registry->GetComponent<HierarchyComponent>(entity)) {
                    parentEntity = hierarchy->parent;
                }
                EntitySnapshot::AppendRootNameSuffix(snapshot, " (Clone)");
                auto action = std::make_unique<DuplicateEntityAction>(std::move(snapshot), parentEntity);
                auto* actionPtr = action.get();
                UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
                EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
            }
        }

        if (ImGui::MenuItem("Delete")) {
            if (!PrefabSystem::CanDelete(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance children cannot be deleted directly. Use Unpack first.");
            } else {
                EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(entity, *registry);
                UndoSystem::Instance().ExecuteAction(
                    std::make_unique<DeleteEntityAction>(std::move(snapshot), entity),
                    *registry);
                EditorSelection::Instance().Clear();
            }
        }

        if (ImGui::MenuItem("Create Empty Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            } else {
                CreateEntityFromSnapshot(registry, BuildSingleEntitySnapshot("Empty Entity"), entity, "Create Child Entity");
            }
        }

        LightComponent pointLight;
        pointLight.type = LightType::Point;
        pointLight.range = 10.0f;
        pointLight.castShadow = false;
        if (ImGui::MenuItem("Create Point Light Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            } else {
                CreateEntityFromSnapshot(registry,
                                         BuildSingleEntitySnapshot("Point Light", nullptr, &pointLight, nullptr),
                                         entity,
                                         "Create Point Light Child");
            }
        }

        if (ImGui::MenuItem("Create Directional Light Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            } else {
                CreateEntityFromSnapshot(registry, BuildDirectionalLightSnapshot(), entity, "Create Directional Light Child");
            }
        }

        ReflectionProbeComponent probe;
        probe.needsBake = true;
        if (ImGui::MenuItem("Create Reflection Probe Child")) {
            if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
            } else {
                CreateEntityFromSnapshot(registry,
                                         BuildSingleEntitySnapshot("Reflection Probe", nullptr, nullptr, &probe),
                                         entity,
                                         "Create Reflection Probe Child");
            }
        }

        const auto& selection = EditorSelection::Instance();
        if (selection.GetType() == SelectionType::Asset && IsSupportedModelAsset(selection.GetAssetPath())) {
            if (ImGui::MenuItem("Create Model Child")) {
                if (!PrefabSystem::CanCreateChild(entity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                } else {
                    MeshComponent meshComp;
                    meshComp.modelFilePath = selection.GetAssetPath();
                    meshComp.model = ResourceManager::Instance().CreateModelInstance(selection.GetAssetPath());
                    const std::string name = std::filesystem::path(meshComp.modelFilePath).stem().string();
                    CreateEntityFromSnapshot(registry,
                                             BuildSingleEntitySnapshot(name, &meshComp, nullptr, nullptr),
                                             entity,
                                             "Create Model Child");
                }
            }
        }

        ImGui::EndPopup();
    }

    HandleDragDropTarget(registry, entity);

    if (isOpen && hasChildren) {
        EntityID currentChild = hierComp->firstChild;
        while (!Entity::IsNull(currentChild)) {
            DrawEntityNode(registry, currentChild);

            auto* childHier = registry->GetComponent<HierarchyComponent>(currentChild);
            currentChild = childHier ? childHier->nextSibling : Entity::NULL_ID;
        }
        ImGui::TreePop();
    }
}

void HierarchyECSUI::HandleDragDropTarget(Registry* registry, EntityID parentEntity) {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ENTITY")) {
            if (payload->DataSize == sizeof(EntityID)) {
                EntityID draggedEntity = *static_cast<const EntityID*>(payload->Data);
                if (registry->IsAlive(draggedEntity) &&
                    draggedEntity != parentEntity &&
                    !HierarchySystem::WouldCreateCycle(draggedEntity, parentEntity, *registry)) {
                    if (!PrefabSystem::CanReparent(draggedEntity, parentEntity, *registry)) {
                        LOG_WARN("[Prefab] Prefab instance internal hierarchy cannot be reparented. Use Unpack first.");
                        ImGui::EndDragDropTarget();
                        return;
                    }
                    EntityID oldParent = Entity::NULL_ID;
                    if (auto* hierarchy = registry->GetComponent<HierarchyComponent>(draggedEntity)) {
                        oldParent = hierarchy->parent;
                    }
                    UndoSystem::Instance().ExecuteAction(
                        std::make_unique<ReparentEntityAction>(draggedEntity, parentEntity, oldParent, true),
                        *registry);
                    EditorSelection::Instance().SelectEntity(draggedEntity);
                }
            }
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
            std::string sourcePathStr((const char*)payload->Data);
            std::filesystem::path path(sourcePathStr);
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            // ==========================================
            // ==========================================
            if (ext == ".mat") {
                if (!Entity::IsNull(parentEntity)) {
                    auto* matComp = registry->GetComponent<MaterialComponent>(parentEntity);
                    MaterialComponent before = matComp ? *matComp : MaterialComponent{};
                    const bool hadComponent = (matComp != nullptr);
                    if (!matComp) {
                        MaterialComponent newMatComp;
                        newMatComp.materialAssetPath = sourcePathStr;
                        newMatComp.materialAsset = ResourceManager::Instance().GetMaterial(sourcePathStr);
                        registry->AddComponent(parentEntity, newMatComp);
                    } else {
                        matComp->materialAssetPath = sourcePathStr;
                        matComp->materialAsset = ResourceManager::Instance().GetMaterial(sourcePathStr);
                    }
                    MaterialComponent after = *registry->GetComponent<MaterialComponent>(parentEntity);
                    UndoSystem::Instance().RecordAction(
                        std::make_unique<OptionalComponentUndoAction<MaterialComponent>>(
                            parentEntity,
                            hadComponent ? std::optional<MaterialComponent>(before) : std::nullopt,
                            std::optional<MaterialComponent>(after),
                            "Assign Material"));
                    PrefabSystem::MarkPrefabOverride(parentEntity, *registry);
                    EditorSelection::Instance().SelectEntity(parentEntity);
                }
            }
            else if (ext == ".prefab") {
                if (!PrefabSystem::CanCreateChild(parentEntity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                    ImGui::EndDragDropTarget();
                    return;
                }

                EntityID newEntity = PrefabSystem::InstantiatePrefab(sourcePathStr, *registry, parentEntity);
                if (!Entity::IsNull(newEntity)) {
                    EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(newEntity, *registry);
                    auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parentEntity, "Instantiate Prefab");
                    action->AdoptLiveRoot(newEntity);
                    UndoSystem::Instance().RecordAction(std::move(action));
                    EditorSelection::Instance().SelectEntity(newEntity);
                }
            }
            else if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf") {
                MeshComponent meshComp;
                meshComp.modelFilePath = sourcePathStr;
                meshComp.model = ResourceManager::Instance().CreateModelInstance(sourcePathStr);

                if (!PrefabSystem::CanCreateChild(parentEntity, *registry)) {
                    LOG_WARN("[Prefab] Prefab instance hierarchy is locked. Use Unpack before adding children.");
                    ImGui::EndDragDropTarget();
                    return;
                }

                auto action = std::make_unique<CreateEntityAction>(
                    BuildSingleEntitySnapshot(path.stem().string(), &meshComp),
                    parentEntity,
                    "Create Entity From Asset");
                auto* actionPtr = action.get();
                UndoSystem::Instance().ExecuteAction(std::move(action), *registry);
                EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
            }
        }
        ImGui::EndDragDropTarget();
    }
}
