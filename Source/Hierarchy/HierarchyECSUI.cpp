#include "HierarchyECSUI.h"
#include "Registry/Registry.h"
#include "Engine/EditorSelection.h"
#include "Icon/IconFontManager.h"

// 必要なコンポーネント群
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/MeshComponent.h"

// リソースマネージャー
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
    // 1. ツリーの描画 (ルート直下のエンティティを探す)
    // ==========================================
    // ※今回は簡易的に全エンティティを回し、親がNULL_IDのものだけを描画の起点にします
    auto archetypes = registry->GetAllArchetypes();
    for (auto* archetype : archetypes) {
        const auto& entities = archetype->GetEntities();
        for (EntityID entity : entities) {
            HierarchyComponent* hier = registry->GetComponent<HierarchyComponent>(entity);
            // HierarchyComponent が無い、または親が無い（ルート）場合のみ描画起点とする
            if (!hier || Entity::IsNull(hier->parent)) {
                DrawEntityNode(registry, entity);
            }
        }
    }

    // ==========================================
    // 2. ウィンドウ全体の空きスペースに対するD&D受け皿 (ルート直下への追加)
    // ==========================================
    ImVec2 availSize = ImGui::GetContentRegionAvail();

    // std::maxを使わず、単純なif文で最低サイズを保証する
    if (availSize.x <= 0.0f) { availSize.x = 1.0f; }
    if (availSize.y < 50.0f) { availSize.y = 50.0f; } // ★最低50px確保

    ImGui::Dummy(availSize);
    HandleDragDropTarget(registry, Entity::NULL_ID);

    // 空きスペースでの右クリックメニュー
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

    // 子を持っているか判定
    bool hasChildren = (hierComp && !Entity::IsNull(hierComp->firstChild));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // 選択状態の反映
    if (EditorSelection::Instance().GetType() == SelectionType::Entity &&
        EditorSelection::Instance().GetEntity() == entity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // ツリーノードの描画
    bool isOpen = ImGui::TreeNodeEx((entityName + idStr).c_str(), flags, "%s %s", ICON_FA_CUBE, entityName.c_str());

    // クリックで選択
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        EditorSelection::Instance().SelectEntity(entity);
    }

    // 特定のエンティティへのD&D受け皿 (子として追加)
    HandleDragDropTarget(registry, entity);

    // 子を展開
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
            // ★ アセットの種類に応じてエンティティを生成
            // ==========================================
            if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf") {
                // 新しいエンティティの作成
                EntityID newEntity = registry->CreateEntity();

                // 基本コンポーネント
                registry->AddComponent(newEntity, NameComponent{ path.stem().string() });
                registry->AddComponent(newEntity, TransformComponent{});

                // 親子関係の構築
                HierarchyComponent newHier;
                newHier.parent = parentEntity;
                registry->AddComponent(newEntity, newHier);

                // 親がいる場合はリンクを繋ぐ（簡易実装）
                if (!Entity::IsNull(parentEntity)) {
                    auto* parentHier = registry->GetComponent<HierarchyComponent>(parentEntity);
                    if (parentHier) {
                        // ※本当は最後尾の兄弟の nextSibling に繋ぐ処理が必要です
                        parentHier->firstChild = newEntity;
                    }
                }

                // ★ メッシュコンポーネントの付与！
                MeshComponent meshComp;
                meshComp.modelFilePath = sourcePathStr;
                meshComp.model = ResourceManager::Instance().GetModel(sourcePathStr);
                registry->AddComponent(newEntity, meshComp);

           

                // 生成したものを即座に選択
                EditorSelection::Instance().SelectEntity(newEntity);
            }
        }
        ImGui::EndDragDropTarget();
    }
}