#include "HierarchyECSUI.h"
#include "Registry/Registry.h"
#include "Engine/EditorSelection.h"
#include "Icon/IconFontManager.h"

#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"

#include "System/ResourceManager.h"
#include "Model/Model.h"

#include <imgui.h>
#include <string>
#include <filesystem>
#include <algorithm>
#include <Component\MaterialComponent.h>

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

    if (ImGui::BeginPopupContextItem("HierarchyContextMenu")) {
        if (ImGui::MenuItem("Create Empty Entity")) {
            EntityID newEntity = registry->CreateEntity();
            registry->AddComponent(newEntity, NameComponent{ "Empty Entity" });
            registry->AddComponent(newEntity, TransformComponent{});
            registry->AddComponent(newEntity, HierarchyComponent{});
            EditorSelection::Instance().SelectEntity(newEntity);
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
                    if (!matComp) {
                        MaterialComponent newMatComp;
                        newMatComp.materialAssetPath = sourcePathStr;
                        newMatComp.materialAsset = ResourceManager::Instance().GetMaterial(sourcePathStr);
                        registry->AddComponent(parentEntity, newMatComp);
                    } else {
                        matComp->materialAssetPath = sourcePathStr;
                        matComp->materialAsset = ResourceManager::Instance().GetMaterial(sourcePathStr);
                    }
                    EditorSelection::Instance().SelectEntity(parentEntity);
                }
            }
            else if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf") {
                EntityID newEntity = registry->CreateEntity();

                registry->AddComponent(newEntity, NameComponent{ path.stem().string() });
                registry->AddComponent(newEntity, TransformComponent{});

                HierarchyComponent newHier;
                newHier.parent = parentEntity;
                registry->AddComponent(newEntity, newHier);

                if (!Entity::IsNull(parentEntity)) {
                    auto* parentHier = registry->GetComponent<HierarchyComponent>(parentEntity);
                    if (parentHier) {
                        parentHier->firstChild = newEntity;
                    }
                }

                MeshComponent meshComp;
                meshComp.modelFilePath = sourcePathStr;
                meshComp.model = ResourceManager::Instance().GetModel(sourcePathStr);
                registry->AddComponent(newEntity, meshComp);

           

                EditorSelection::Instance().SelectEntity(newEntity);
            }
        }
        ImGui::EndDragDropTarget();
    }
}
