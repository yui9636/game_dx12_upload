#include "EditorLayer.h"
#include "Engine/EngineKernel.h" // カーネルの機能(Play/Stop)を呼ぶため
#include "Icon/IconFontManager.h"
#include "Engine/EditorSelection.h" // 選択状態の管理
#include "Inspector/InspectorECSUI.h" // インスペクターUI
#include "Component/NameComponent.h"  // 名前表示用
#include "Archetype/Archetype.h"      // エンティティ走査用
#include "System/UndoSystem.h"
#include "Undo/EntitySnapshot.h"
#include "Undo/EntityUndoActions.h"
#include "Undo/ComponentUndoAction.h"
#include "Asset/PrefabSystem.h"
#include "Component/HierarchyComponent.h"
#include "Component/TransformComponent.h"
#include "Component/MeshComponent.h"
#include "Component/ColliderComponent.h"
#include "Console/Logger.h"
#include "Hierarchy/HierarchySystem.h"
#include "Physics/PhysicsManager.h"
#include "System/Dialog.h"
#include "ImGuizmo.h"
#include "Component/LightComponent.h"
#include "Component/ReflectionProbeComponent.h"
#include "Component/CameraComponent.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "Hierarchy/HierarchyECSUI.h"
#include <Component\CameraBehaviorComponent.h>
#include "Generated/ComponentMeta.generated.h"
#include "Component/EnvironmentComponent.h"
#include "Component/PostEffectComponent.h"
#include "Console/Console.h"
#include "RHI/ITexture.h"
#include "ImGuiRenderer.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <cfloat>
#include <nlohmann/json.hpp>
#include <optional>

namespace {
    std::optional<EntitySnapshot::Snapshot> s_entityClipboard;
    constexpr float kSceneViewPickDragThreshold = 4.0f;
    constexpr float kEditorFocusMinDistance = 1.5f;
    constexpr float kMinScaleValue = 0.001f;
    constexpr const char* kDefaultSceneSavePath = "Data/Scene/EditorScene.scene";
    constexpr const char* kSceneDialogFilter = "Scene Files (*.scene)\0*.scene\0Legacy Scene Files (*.scene.json)\0*.scene.json\0All Files\0*.*\0\0";

    using json = nlohmann::json;

    template <typename T>
    bool DrawSettingWidget(std::string_view name, T& value) {
        bool changed = false;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", name.data());
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1);

        std::string id = std::string("##") + name.data();

        if constexpr (std::is_same_v<T, float>) { changed = ImGui::DragFloat(id.c_str(), &value, 0.01f); }
        else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, uint32_t>) { changed = ImGui::DragInt(id.c_str(), (int*)&value, 1); }
        else if constexpr (std::is_same_v<T, bool>) { changed = ImGui::Checkbox(id.c_str(), &value); }
        else if constexpr (std::is_same_v<T, std::string>) {
            char buf[256]; strcpy_s(buf, value.c_str());
            if (ImGui::InputText(id.c_str(), buf, 256)) { value = buf; changed = true; }

            // ★ 文字列項目なら、自動的にアセットのドラッグ＆ドロップを受け付ける！
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
                    value = (const char*)payload->Data;
                    changed = true;
                }
                ImGui::EndDragDropTarget();
            }
        }
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT3>) { changed = ImGui::ColorEdit3(id.c_str(), &value.x); } // 3要素は色と仮定
        else if constexpr (std::is_same_v<T, DirectX::XMFLOAT4>) { changed = ImGui::ColorEdit4(id.c_str(), &value.x); }
        else if constexpr (std::is_enum_v<T>) {
            int enumVal = static_cast<int>(value);
            if (ImGui::DragInt(id.c_str(), &enumVal, 1)) { value = static_cast<T>(enumVal); changed = true; }
        }
        else { ImGui::TextDisabled("Unsupported Type"); }
        return changed;
    }

    // 構造体の全メンバ変数を展開してテーブル化する関数
    template <typename T>
    void DrawGlobalSettingsUI(T& comp) {
        constexpr auto& metaName = ComponentMeta<T>::Name;

        ImGui::PushID(metaName.data());
        if (ImGui::CollapsingHeader(metaName.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("CompTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

                std::apply([&](auto... fields) {
                    (DrawSettingWidget(fields.name, comp.*(fields.ptr)), ...);
                    }, ComponentMeta<T>::Fields);

                ImGui::EndTable();
            }
        }
        ImGui::PopID();
    }

    bool HasMeaningfulTransformChange(const TransformComponent& before, const TransformComponent& after)
    {
        auto diff3 = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
            return std::fabs(a.x - b.x) + std::fabs(a.y - b.y) + std::fabs(a.z - b.z);
        };
        auto diff4 = [](const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b) {
            return std::fabs(a.x - b.x) + std::fabs(a.y - b.y) + std::fabs(a.z - b.z) + std::fabs(a.w - b.w);
        };
        return diff3(before.localPosition, after.localPosition) > 0.0001f ||
               diff3(before.localScale, after.localScale) > 0.0001f ||
               diff4(before.localRotation, after.localRotation) > 0.0001f;
    }

    void NormalizeQuaternion(DirectX::XMFLOAT4& rotation)
    {
        using namespace DirectX;
        XMVECTOR q = XMLoadFloat4(&rotation);
        if (XMVector4Equal(q, XMVectorZero())) {
            rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
            return;
        }
        q = XMQuaternionNormalize(q);
        XMStoreFloat4(&rotation, q);
    }

    DirectX::XMFLOAT3 ClampScale(const DirectX::XMFLOAT3& scale)
    {
        DirectX::XMFLOAT3 out = scale;
        auto clampComponent = [](float value) {
            if (!std::isfinite(value)) {
                return 1.0f;
            }
            const float sign = value < 0.0f ? -1.0f : 1.0f;
            return sign * (std::max)(std::fabs(value), kMinScaleValue);
        };
        out.x = clampComponent(out.x);
        out.y = clampComponent(out.y);
        out.z = clampComponent(out.z);
        return out;
    }

    bool RayIntersectsBoundingBox(const DirectX::BoundingBox& box,
                                  const DirectX::XMFLOAT3& origin,
                                  const DirectX::XMFLOAT3& direction,
                                  float& distance)
    {
        using namespace DirectX;
        const XMVECTOR rayOrigin = XMLoadFloat3(&origin);
        const XMVECTOR rayDir = XMVector3Normalize(XMLoadFloat3(&direction));
        return box.Intersects(rayOrigin, rayDir, distance);
    }

    EntityID PickRenderableFallback(Registry& registry,
                                    const DirectX::XMFLOAT3& origin,
                                    const DirectX::XMFLOAT3& direction,
                                    float maxDistance)
    {
        EntityID bestEntity = Entity::NULL_ID;
        float bestDistance = maxDistance;

        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<MeshComponent>()) ||
                !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
                continue;
            }

            auto* meshColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<MeshComponent>());
            const auto& entities = archetype->GetEntities();
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                EntityID entity = entities[i];
                auto* mesh = static_cast<MeshComponent*>(meshColumn->Get(i));
                if (!mesh || !mesh->model || !mesh->isVisible) {
                    continue;
                }

                RaycastHit hit;
                if (mesh->model->Raycast(origin, direction, hit) && hit.distance < bestDistance) {
                    bestDistance = hit.distance;
                    bestEntity = entity;
                    continue;
                }

                float boundsDistance = FLT_MAX;
                if (RayIntersectsBoundingBox(mesh->model->GetWorldBounds(), origin, direction, boundsDistance) &&
                    boundsDistance < bestDistance) {
                    bestDistance = boundsDistance;
                    bestEntity = entity;
                }
            }
        }

        return bestEntity;
    }

    bool BuildWorldRay(const DirectX::XMFLOAT4& rect,
                       const DirectX::XMFLOAT4X4& view,
                       const DirectX::XMFLOAT4X4& projection,
                       const ImVec2& mousePos,
                       DirectX::XMFLOAT3& outOrigin,
                       DirectX::XMFLOAT3& outDirection)
    {
        using namespace DirectX;
        if (rect.z <= 1.0f || rect.w <= 1.0f) {
            return false;
        }

        const float localX = (mousePos.x - rect.x) / rect.z;
        const float localY = (mousePos.y - rect.y) / rect.w;
        if (localX < 0.0f || localX > 1.0f || localY < 0.0f || localY > 1.0f) {
            return false;
        }

        const float ndcX = localX * 2.0f - 1.0f;
        const float ndcY = 1.0f - localY * 2.0f;

        const XMMATRIX viewMatrix = XMLoadFloat4x4(&view);
        const XMMATRIX projectionMatrix = XMLoadFloat4x4(&projection);
        const XMMATRIX inverseViewProj = XMMatrixInverse(nullptr, viewMatrix * projectionMatrix);

        const XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverseViewProj);
        const XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inverseViewProj);
        const XMVECTOR direction = XMVector3Normalize(farPoint - nearPoint);

        XMStoreFloat3(&outOrigin, nearPoint);
        XMStoreFloat3(&outDirection, direction);
        return true;
    }

    float ComputeFocusDistance(float radius, float fovY)
    {
        const float safeRadius = (std::max)(radius, 0.5f);
        const float halfFov = (std::max)(fovY * 0.5f, 0.1f);
        return (std::max)(kEditorFocusMinDistance, safeRadius / std::tan(halfFov));
    }

    float Max3(float a, float b, float c)
    {
        return (std::max)((std::max)(a, b), c);
    }

    std::string SanitizeSceneDefaultPath(const std::string& path)
    {
        return path.empty() ? std::string(kDefaultSceneSavePath) : path;
    }

    void ClearRegistryEntities(Registry& registry)
    {
        std::vector<EntityID> entities;
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& archetypeEntities = archetype->GetEntities();
            entities.insert(entities.end(), archetypeEntities.begin(), archetypeEntities.end());
        }

        for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
            if (registry.IsAlive(*it)) {
                registry.DestroyEntity(*it);
            }
        }
    }

    void CreateDefaultSceneEntities(Registry& registry)
    {
        using namespace DirectX;

        EntityID cameraEntity = registry.CreateEntity();
        registry.AddComponent(cameraEntity, NameComponent{ "Main Camera" });

        TransformComponent camTrans;
        camTrans.localPosition = { 0.0f, 2.0f, -10.0f };
        registry.AddComponent(cameraEntity, camTrans);
        registry.AddComponent(cameraEntity, HierarchyComponent{});
        registry.AddComponent(cameraEntity, CameraFreeControlComponent{});
        registry.AddComponent(cameraEntity, CameraLensComponent{});
        registry.AddComponent(cameraEntity, CameraMatricesComponent{});
        registry.AddComponent(cameraEntity, CameraMainTagComponent{});

        EntityID lightEntity = registry.CreateEntity();
        registry.AddComponent(lightEntity, NameComponent{ "Directional Light" });
        TransformComponent lightTrans;
        XMVECTOR rot = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(45.0f), XMConvertToRadians(45.0f), 0.0f);
        XMStoreFloat4(&lightTrans.localRotation, rot);
        registry.AddComponent(lightEntity, lightTrans);
        registry.AddComponent(lightEntity, HierarchyComponent{});

        LightComponent lightComp;
        lightComp.type = LightType::Directional;
        lightComp.color = { 1.0f, 1.0f, 1.0f };
        lightComp.intensity = 1.0f;
        registry.AddComponent(lightEntity, lightComp);

        EntityID probeEntity = registry.CreateEntity();
        registry.AddComponent(probeEntity, NameComponent{ "Reflection Probe" });
        ReflectionProbeComponent probeComp;
        probeComp.position = { 0.0f, 1.5f, 0.0f };
        probeComp.radius = 20.0f;
        probeComp.needsBake = true;
        registry.AddComponent(probeEntity, probeComp);
    }
}
void ApplyUnityTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    // --- Unity 2022 Dark Theme Color Palette ---
    const ImVec4 gray_100 = ImVec4(0.82f, 0.82f, 0.82f, 1.00f); // Text
    const ImVec4 gray_070 = ImVec4(0.24f, 0.24f, 0.24f, 1.00f); // Header / Active
    const ImVec4 gray_050 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f); // Background
    const ImVec4 gray_030 = ImVec4(0.18f, 0.18f, 0.18f, 1.00f); // Input / Darker
    const ImVec4 unity_blue = ImVec4(0.17f, 0.36f, 0.53f, 1.00f); // Selection

    colors[ImGuiCol_Text] = gray_100;
    colors[ImGuiCol_WindowBg] = gray_050;
    colors[ImGuiCol_ChildBg] = gray_050;
    colors[ImGuiCol_PopupBg] = gray_030;
    colors[ImGuiCol_Border] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBg] = gray_030;
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBg] = gray_070;
    colors[ImGuiCol_TitleBgActive] = gray_070;
    colors[ImGuiCol_MenuBarBg] = gray_070;
    colors[ImGuiCol_Header] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = unity_blue;
    colors[ImGuiCol_Button] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = unity_blue;
    colors[ImGuiCol_Tab] = gray_050;
    colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_TabActive] = gray_070;
    colors[ImGuiCol_TabUnfocused] = gray_050;
    colors[ImGuiCol_TabUnfocusedActive] = gray_070;
    colors[ImGuiCol_DockingPreview] = ImVec4(0.17f, 0.36f, 0.53f, 0.70f);
}

EditorLayer::EditorLayer(GameLayer* gameLayer)
    : m_gameLayer(gameLayer)
    , m_sceneSavePath(kDefaultSceneSavePath)
{
}

void EditorLayer::Initialize()
{

    m_assetBrowser = std::make_unique<AssetBrowser>();
    m_assetBrowser->Initialize();

    ApplyUnityTheme();
}

void EditorLayer::Finalize()
{
}

void EditorLayer::Update(const EngineTime& time)
{
    ImGuiIO& io = ImGui::GetIO();
    using namespace DirectX;

    HandleEditorShortcuts();
    ProcessDeferredEditorActions();

    if (!io.WantTextInput && m_gameLayer) {
        Registry& registry = m_gameLayer->GetRegistry();
        auto& selection = EditorSelection::Instance();

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (io.KeyShift) {
                UndoSystem::Instance().Redo(registry);
            } else {
                UndoSystem::Instance().Undo(registry);
            }
        } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            UndoSystem::Instance().Redo(registry);
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            if (selection.GetType() == SelectionType::Entity) {
                const EntityID selectedEntity = selection.GetEntity();
                if (!Entity::IsNull(selectedEntity) && registry.IsAlive(selectedEntity)) {
                    if (!PrefabSystem::CanDuplicate(selectedEntity, registry)) {
                        LOG_WARN("[Prefab] Prefab instance children cannot be duplicated. Duplicate the root instance or unpack it first.");
                        return;
                    }
                    EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedEntity, registry);
                    if (!snapshot.nodes.empty()) {
                        EntityID parentEntity = Entity::NULL_ID;
                        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(selectedEntity)) {
                            parentEntity = hierarchy->parent;
                        }

                        EntitySnapshot::AppendRootNameSuffix(snapshot, " (Clone)");
                        auto action = std::make_unique<DuplicateEntityAction>(std::move(snapshot), parentEntity);
                        auto* actionPtr = action.get();
                        UndoSystem::Instance().ExecuteAction(std::move(action), registry);
                        if (!Entity::IsNull(actionPtr->GetLiveRoot())) {
                            selection.SelectEntity(actionPtr->GetLiveRoot());
                        }
                    }
                }
            }
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            if (selection.GetType() == SelectionType::Entity) {
                const EntityID selectedEntity = selection.GetEntity();
                if (!Entity::IsNull(selectedEntity) && registry.IsAlive(selectedEntity)) {
                    s_entityClipboard = EntitySnapshot::CaptureSubtree(selectedEntity, registry);
                }
            }
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false) && s_entityClipboard.has_value()) {
            EntitySnapshot::Snapshot snapshot = *s_entityClipboard;
            if (!snapshot.nodes.empty()) {
                EntityID parentEntity = Entity::NULL_ID;
                if (selection.GetType() == SelectionType::Entity) {
                    const EntityID selectedEntity = selection.GetEntity();
                    if (!Entity::IsNull(selectedEntity) && registry.IsAlive(selectedEntity)) {
                        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(selectedEntity)) {
                            parentEntity = hierarchy->parent;
                        }
                    }
                }

                EntitySnapshot::AppendRootNameSuffix(snapshot, " (Copy)");
                auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parentEntity, "Paste Entity");
                auto* actionPtr = action.get();
                UndoSystem::Instance().ExecuteAction(std::move(action), registry);
                if (!Entity::IsNull(actionPtr->GetLiveRoot())) {
                    selection.SelectEntity(actionPtr->GetLiveRoot());
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && selection.GetType() == SelectionType::Entity) {
            const EntityID selectedEntity = selection.GetEntity();
            if (!Entity::IsNull(selectedEntity) && registry.IsAlive(selectedEntity)) {
                if (!PrefabSystem::CanDelete(selectedEntity, registry)) {
                    LOG_WARN("[Prefab] Prefab instance children cannot be deleted directly. Use Unpack first.");
                    return;
                }
                EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedEntity, registry);
                if (!snapshot.nodes.empty()) {
                    UndoSystem::Instance().ExecuteAction(
                        std::make_unique<DeleteEntityAction>(std::move(snapshot), selectedEntity),
                        registry);
                    selection.Clear();
                }
            }
        }
    }

    XMVECTOR pos = XMLoadFloat3(&m_editorCameraPosition);
    const XMVECTOR rot = XMQuaternionRotationRollPitchYaw(m_editorCameraPitch, m_editorCameraYaw, 0.0f);
    const XMVECTOR forward = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot));
    const XMVECTOR right = XMVector3Normalize(XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot));
    const XMVECTOR up = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot));

    if (m_sceneViewHovered && io.MouseDown[ImGuiMouseButton_Right] && !io.KeyAlt && !m_gizmoWasUsing) {
        m_editorCameraUserOverride = true;
        m_editorCameraYaw += io.MouseDelta.x * 0.005f;
        m_editorCameraPitch += io.MouseDelta.y * 0.005f;
        m_editorCameraPitch = std::clamp(m_editorCameraPitch, -1.55f, 1.55f);

        float speed = 20.0f * io.DeltaTime;
        if (io.KeyShift) speed *= 3.0f;

        const XMVECTOR moveRot = XMQuaternionRotationRollPitchYaw(m_editorCameraPitch, m_editorCameraYaw, 0.0f);
        const XMVECTOR moveForward = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 0, 1, 0), moveRot));
        const XMVECTOR moveRight = XMVector3Normalize(XMVector3Rotate(XMVectorSet(1, 0, 0, 0), moveRot));
        const XMVECTOR moveUp = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 1, 0, 0), moveRot));

        if (ImGui::IsKeyDown(ImGuiKey_W)) pos += moveForward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) pos -= moveForward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) pos += moveRight * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) pos -= moveRight * speed;
        if (ImGui::IsKeyDown(ImGuiKey_E)) pos += moveUp * speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) pos -= moveUp * speed;
    }

    if (m_sceneViewHovered && io.MouseDown[ImGuiMouseButton_Middle] && !m_gizmoWasUsing) {
        m_editorCameraUserOverride = true;
        const float panSpeed = 10.0f * io.DeltaTime;
        pos -= right * io.MouseDelta.x * panSpeed;
        pos += up * io.MouseDelta.y * panSpeed;
    }

    if (m_sceneViewHovered && io.MouseWheel != 0.0f && !m_gizmoWasUsing) {
        m_editorCameraUserOverride = true;
        pos += forward * (io.MouseWheel * 10.0f);
    }

    XMStoreFloat3(&m_editorCameraPosition, pos);
}

void EditorLayer::RenderUI()
{
    // カーネルから移動してきた大枠のUI描画
    DrawDockSpace();
    DrawMenuBar();
    DrawMainToolbar();

    // 各種パネル
    DrawSceneView();
    DrawHierarchy();
    DrawInspector();

    DrawLightingWindow();
    DrawGBufferDebugWindow();
    Console::Instance().Draw();
    if (m_assetBrowser) {
        m_assetBrowser->SetRegistry(m_gameLayer ? &m_gameLayer->GetRegistry() : nullptr);
        m_assetBrowser->RenderUI();
    }
}

void EditorLayer::DrawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File(F)")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                m_requestNewScene = true;
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                m_requestOpenScene = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                SaveCurrentScene();
            }
            if (ImGui::MenuItem("Save As...")) {
                m_requestSaveSceneAs = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit(E)")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("View(V)")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Game(G)")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Window(W)")) {
            if (ImGui::MenuItem(ICON_FA_SUN " Lighting Settings")) {
                m_showLightingWindow = true;
            }
            if (ImGui::MenuItem(ICON_FA_IMAGES " G-Buffer Debug")) {


                m_showGBufferDebug = true;
            }

            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200.0f);
        ImGui::TextDisabled(ICON_FA_GEAR " %.1f FPS", ImGui::GetIO().Framerate);
        ImGui::SameLine();

        float totalTime = EngineKernel::Instance().GetTime().totalTime;
        ImGui::TextDisabled(ICON_FA_CLOCK " %.2f", totalTime);

        ImGui::EndMainMenuBar();
    }
}

void EditorLayer::DrawMainToolbar()
{
    auto& ifm = IconFontManager::Instance();
    ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + ImGui::GetFrameHeight()));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, 32));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    if (ImGui::Begin("##MainToolbar", nullptr, flags))
    {
        // 左側: ファイル操作
        if (ifm.IconButton(ICON_FA_FLOPPY_DISK, IconSemantic::Default, IconFontSize::Medium, nullptr)) { SaveCurrentScene(); }
        ImGui::SameLine();
        if (ifm.IconButton(ICON_FA_ARROW_ROTATE_LEFT, IconSemantic::Default, IconFontSize::Medium, nullptr)) {
            if (m_gameLayer) {
                UndoSystem::Instance().Undo(m_gameLayer->GetRegistry());
            }
        }
        ImGui::SameLine();
        if (ifm.IconButton(ICON_FA_ARROW_ROTATE_RIGHT, IconSemantic::Default, IconFontSize::Medium, nullptr)) {
            if (m_gameLayer) {
                UndoSystem::Instance().Redo(m_gameLayer->GetRegistry());
            }
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // 中央: 再生コントロール
        float centerX = ImGui::GetWindowWidth() * 0.5f;
        ImGui::SetCursorPosX(centerX - 45.0f);

        // ★現在のモードをカーネルから取得
        EngineMode mode = EngineKernel::Instance().GetMode();

        // [Play]
        bool isPlaying = (mode == EngineMode::Play);
        ImVec4 playColor = isPlaying ? ImVec4(0.26f, 0.90f, 0.26f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, playColor);
        if (ifm.IconButton(ICON_FA_PLAY, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            // ★カーネルの関数を直接叩く
            if (mode == EngineMode::Editor || mode == EngineMode::Pause) EngineKernel::Instance().Play();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // [Pause]
        bool isPaused = (mode == EngineMode::Pause);
        ImVec4 pauseColor = isPaused ? ImVec4(1.00f, 0.60f, 0.00f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, pauseColor);
        if (ifm.IconButton(ICON_FA_PAUSE, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            // ★カーネルの関数を直接叩く
            if (mode == EngineMode::Play) EngineKernel::Instance().Pause();
            else if (mode == EngineMode::Pause) EngineKernel::Instance().Play();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // [Stop]
        bool canStop = (mode != EngineMode::Editor);
        ImVec4 stopColor = canStop ? ImVec4(1.00f, 0.25f, 0.25f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, stopColor);
        if (ifm.IconButton(ICON_FA_SQUARE, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            // ★カーネルの関数を直接叩く
            if (canStop) EngineKernel::Instance().Stop();
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);
}

void EditorLayer::DrawDockSpace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

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

    static bool isLayoutInitialized = false;
    if (!isLayoutInitialized)
    {
        isLayoutInitialized = true;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

        // 画面を分割していく
        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
        ImGuiID dock_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

        // 各ウィンドウを分割したエリアにはめ込む
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow(ICON_FA_CIRCLE_INFO " Inspector", dock_right);

        ImGui::DockBuilderDockWindow(ICON_FA_FOLDER_OPEN " Asset Browser", dock_down);
        ImGui::DockBuilderDockWindow("Console", dock_down); // ★ 追加：コンソールを下のエリアに重ねる（タブ化）

        ImGui::DockBuilderDockWindow("Scene View", dock_main_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }



    ImGui::End();
}

void EditorLayer::DrawSceneView()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Scene View", nullptr, window_flags))
    {
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
        if (m_sceneViewTexture) {
            if (void* sceneViewTexture = ImGuiRenderer::GetTextureID(m_sceneViewTexture)) {
                ImGui::Image((ImTextureID)sceneViewTexture, viewportSize);
            }
        } else if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
            Graphics& graphics = Graphics::Instance();
            FrameBuffer* sceneViewBuffer = graphics.GetFrameBuffer(FrameBufferId::Display);
            if (sceneViewBuffer) {
                if (ITexture* color = sceneViewBuffer->GetColorTexture(0)) {
                    if (void* sceneViewTexture = sceneViewBuffer->GetImGuiTextureID()) {
                        ImGui::Image((ImTextureID)sceneViewTexture, viewportSize);
                    }
                }
            }
        }

        ImGuizmo::BeginFrame();
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
    const XMVECTOR rot = XMQuaternionRotationRollPitchYaw(m_editorCameraPitch, m_editorCameraYaw, 0.0f);
    const XMVECTOR forward = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot));
    DirectX::XMFLOAT3 out{};
    XMStoreFloat3(&out, forward);
    return out;
}

DirectX::XMFLOAT4X4 EditorLayer::GetEditorViewMatrix() const
{
    using namespace DirectX;
    const XMVECTOR eye = XMLoadFloat3(&m_editorCameraPosition);
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
    const XMMATRIX proj = XMMatrixPerspectiveFovLH(m_editorCameraFovY, safeAspect, 0.1f, 100000.0f);
    DirectX::XMFLOAT4X4 out{};
    XMStoreFloat4x4(&out, proj);
    return out;
}

void EditorLayer::SetEditorCameraLookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target)
{
    using namespace DirectX;
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

void EditorLayer::ProcessDeferredEditorActions()
{
    const bool requestNewScene = m_requestNewScene;
    const bool requestOpenScene = m_requestOpenScene;
    const bool requestSaveSceneAs = m_requestSaveSceneAs;

    m_requestNewScene = false;
    m_requestOpenScene = false;
    m_requestSaveSceneAs = false;

    std::filesystem::path pendingScenePath;
    const bool hasPendingSceneFromAssetBrowser = m_assetBrowser && m_assetBrowser->ConsumePendingSceneLoad(pendingScenePath);

    if (requestNewScene) {
        NewScene();
    }
    if (requestOpenScene) {
        OpenScene();
    }
    if (requestSaveSceneAs) {
        SaveCurrentSceneAs();
    }
    if (hasPendingSceneFromAssetBrowser) {
        LoadSceneFromPath(pendingScenePath);
    }
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

    if (!m_sceneViewHovered || io.KeyCtrl || io.KeyAlt || io.MouseDown[ImGuiMouseButton_Right]) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        m_gizmoOperation = GizmoOperation::Translate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        m_gizmoOperation = GizmoOperation::Rotate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        m_gizmoOperation = GizmoOperation::Scale;
    } else if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        FocusSelectedEntity();
    }
}

void EditorLayer::DrawSceneViewToolbar()
{
    if (m_sceneViewRect.z <= 1.0f || m_sceneViewRect.w <= 1.0f) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.11f, 0.82f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.22f, 0.26f, 0.90f));

    ImGui::SetNextWindowPos(ImVec2(m_sceneViewRect.x + m_sceneViewRect.z - 12.0f, m_sceneViewRect.y + 12.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##SceneViewToolbarOverlay", nullptr, flags)) {
        m_sceneViewToolbarHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        auto drawOpButton = [&](const char* label, GizmoOperation op) {
            const bool selected = (m_gizmoOperation == op);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.21f, 0.47f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.53f, 0.90f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
            }
            if (ImGui::Button(label)) {
                m_gizmoOperation = op;
            }
            if (selected) {
                ImGui::PopStyleColor(3);
            }
            ImGui::SameLine();
        };

        ImGui::TextDisabled("Transform");
        ImGui::SameLine();
        drawOpButton("W", GizmoOperation::Translate);
        drawOpButton("E", GizmoOperation::Rotate);
        drawOpButton("R", GizmoOperation::Scale);

        if (ImGui::Button(m_gizmoSpace == GizmoSpace::Local ? "Local" : "World")) {
            m_gizmoSpace = (m_gizmoSpace == GizmoSpace::Local) ? GizmoSpace::World : GizmoSpace::Local;
        }
        ImGui::SameLine();

        if (ImGui::Button("F")) {
            FocusSelectedEntity();
        }
        ImGui::Separator();
        ImGui::TextDisabled("View");
        ImGui::SameLine();

        auto viewButton = [&](const char* label, const DirectX::XMFLOAT3& forward) {
            if (ImGui::Button(label)) {
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
            }
        };

        viewButton("Front", { 0.0f, 0.0f, 1.0f }); ImGui::SameLine();
        viewButton("Back", { 0.0f, 0.0f, -1.0f }); ImGui::SameLine();
        viewButton("Left", { -1.0f, 0.0f, 0.0f }); ImGui::SameLine();
        viewButton("Right", { 1.0f, 0.0f, 0.0f }); ImGui::SameLine();
        viewButton("Top", { 0.0f, -1.0f, 0.0f }); ImGui::SameLine();
        viewButton("Bottom", { 0.0f, 1.0f, 0.0f });
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

void EditorLayer::DrawTransformGizmo()
{
    static std::optional<TransformComponent> s_gizmoBeforeState;

    m_gizmoIsOver = false;

    if (!m_gameLayer || m_sceneViewRect.z <= 1.0f || m_sceneViewRect.w <= 1.0f) {
        m_gizmoWasUsing = false;
        m_hasGizmoBeforeTransform = false;
        m_gizmoUndoEntity = Entity::NULL_ID;
        s_gizmoBeforeState.reset();
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
    const EntityID entity = selection.GetEntity();
    auto* transform = registry.GetComponent<TransformComponent>(entity);
    if (!transform) {
        m_gizmoWasUsing = false;
        m_hasGizmoBeforeTransform = false;
        m_gizmoUndoEntity = Entity::NULL_ID;
        s_gizmoBeforeState.reset();
        return;
    }

    using namespace DirectX;
    XMFLOAT4X4 view = GetEditorViewMatrix();
    const float aspect = (m_sceneViewRect.w > 0.0f) ? (m_sceneViewRect.z / m_sceneViewRect.w) : (16.0f / 9.0f);
    XMFLOAT4X4 projection = BuildEditorProjectionMatrix(aspect);
    XMFLOAT4X4 world = transform->worldMatrix;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_sceneViewRect.x, m_sceneViewRect.y, m_sceneViewRect.z, m_sceneViewRect.w);

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    switch (m_gizmoOperation) {
    case GizmoOperation::Rotate: operation = ImGuizmo::ROTATE; break;
    case GizmoOperation::Scale: operation = ImGuizmo::SCALE; break;
    default: break;
    }

    ImGuizmo::MODE mode = (m_gizmoOperation == GizmoOperation::Scale)
        ? ImGuizmo::LOCAL
        : (m_gizmoSpace == GizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD);

    ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], operation, mode, &world.m[0][0]);
    const bool isUsing = ImGuizmo::IsUsing();
    m_gizmoIsOver = ImGuizmo::IsOver();

    if (isUsing && !m_gizmoWasUsing) {
        m_gizmoUndoEntity = entity;
        s_gizmoBeforeState = *transform;
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
            XMStoreFloat3(&transform->localPosition, translation);
            XMStoreFloat4(&transform->localRotation, rotation);
            NormalizeQuaternion(transform->localRotation);
            XMStoreFloat3(&transform->localScale, scale);
            transform->localScale = ClampScale(transform->localScale);
            transform->isDirty = true;
            HierarchySystem::MarkDirtyRecursive(entity, registry);
            PrefabSystem::MarkPrefabOverride(entity, registry);
        }
    }

    if (isUsing && !m_gizmoWasUsing) {
        m_hasGizmoBeforeTransform = true;
    }
    if (!isUsing && m_gizmoWasUsing) {
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
        s_gizmoBeforeState.reset();
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

    XMFLOAT3 rayOrigin{};
    XMFLOAT3 rayDirection{};
    if (!BuildWorldRay(m_sceneViewRect, view, projection, ImGui::GetMousePos(), rayOrigin, rayDirection)) {
        return;
    }

    EntityID selectedEntity = Entity::NULL_ID;
    float maxDistance = 100000.0f;

    PhysicsRaycastResult hit = PhysicsManager::Instance().CastRay(rayOrigin, rayDirection, maxDistance);
    if (hit.hasHit && registry.IsAlive(hit.entityID)) {
        selectedEntity = hit.entityID;
        maxDistance = hit.distance;
    }

    const EntityID fallbackEntity = PickRenderableFallback(registry, rayOrigin, rayDirection, maxDistance);
    if (!Entity::IsNull(fallbackEntity)) {
        selectedEntity = fallbackEntity;
    }

    if (!Entity::IsNull(selectedEntity)) {
        EditorSelection::Instance().SelectEntity(selectedEntity);
    } else {
        EditorSelection::Instance().Clear();
    }
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

void EditorLayer::NewScene()
{
    if (!m_gameLayer) {
        return;
    }

    EngineKernel::Instance().ResetRenderStateForSceneChange();

    Registry& registry = m_gameLayer->GetRegistry();
    EditorSelection::Instance().Clear();
    UndoSystem::Instance().ClearECSHistory();
    m_scenePickPending = false;
    m_scenePickBlockedByGizmo = false;
    m_gizmoWasUsing = false;
    m_gizmoIsOver = false;
    m_gizmoUndoEntity = Entity::NULL_ID;
    m_hasGizmoBeforeTransform = false;

    ClearRegistryEntities(registry);
    CreateDefaultSceneEntities(registry);

    m_sceneSavePath = kDefaultSceneSavePath;
    LOG_INFO("[Editor] New scene created.");
}

bool EditorLayer::LoadSceneFromPath(const std::filesystem::path& scenePath)
{
    if (!m_gameLayer) {
        return false;
    }

    EngineKernel::Instance().ResetRenderStateForSceneChange();
    if (!PrefabSystem::LoadSceneIntoRegistry(scenePath, m_gameLayer->GetRegistry())) {
        LOG_WARN("[Editor] Failed to load scene: %s", scenePath.string().c_str());
        return false;
    }

    UndoSystem::Instance().ClearECSHistory();
    EditorSelection::Instance().Clear();
    m_sceneSavePath = scenePath.string();
    LOG_INFO("[Editor] Scene loaded: %s", scenePath.string().c_str());
    return true;
}

bool EditorLayer::OpenScene()
{
    if (!m_gameLayer) {
        return false;
    }

    char filePath[MAX_PATH] = {};
    const std::filesystem::path initialPath = SanitizeSceneDefaultPath(m_sceneSavePath);
    strcpy_s(filePath, initialPath.string().c_str());
    if (Dialog::OpenFileName(filePath, MAX_PATH, kSceneDialogFilter, "Open Scene", nullptr) != DialogResult::OK) {
        return false;
    }

    return LoadSceneFromPath(std::filesystem::path(filePath));
}

bool EditorLayer::SaveCurrentScene()
{
    if (!m_gameLayer) {
        return false;
    }

    const std::filesystem::path scenePath = SanitizeSceneDefaultPath(m_sceneSavePath);
    const bool success = PrefabSystem::SaveRegistryAsScene(m_gameLayer->GetRegistry(), scenePath);
    if (success) {
        LOG_INFO("[Editor] Scene saved: %s", scenePath.string().c_str());
        m_sceneSavePath = scenePath.string();
    } else {
        LOG_WARN("[Editor] Failed to save scene: %s", scenePath.string().c_str());
    }
    return success;
}

bool EditorLayer::SaveCurrentSceneAs()
{
    if (!m_gameLayer) {
        return false;
    }

    char filePath[MAX_PATH] = {};
    strcpy_s(filePath, SanitizeSceneDefaultPath(m_sceneSavePath).c_str());
    if (Dialog::SaveFileName(filePath, MAX_PATH, kSceneDialogFilter, "Save Scene As", "scene", nullptr) != DialogResult::OK) {
        return false;
    }

    m_sceneSavePath = filePath;
    return SaveCurrentScene();
}

void EditorLayer::DrawGameView()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Game View", nullptr, window_flags))
    {
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
        if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
            Graphics& graphics = Graphics::Instance();
            FrameBuffer* gameViewBuffer = graphics.GetFrameBuffer(FrameBufferId::Display);
            if (gameViewBuffer) {
                if (ITexture* color = gameViewBuffer->GetColorTexture(0)) {
                    if (void* gameViewTexture = gameViewBuffer->GetImGuiTextureID()) {
                        ImGui::Image((ImTextureID)gameViewTexture, viewportSize);
                    }
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::DrawHierarchy()
{
    if (!m_gameLayer) return;

    HierarchyECSUI::Render(&m_gameLayer->GetRegistry());

}

void EditorLayer::DrawInspector()
{
    if (!m_gameLayer) return;  // InspectorECSUI::Render が Begin/End を管理
    InspectorECSUI::Render(&m_gameLayer->GetRegistry());
}

void EditorLayer::DrawLightingWindow()
{
    if (!m_showLightingWindow) return;

    // ウィンドウを描画
    if (ImGui::Begin(ICON_FA_SUN " Lighting Settings", &m_showLightingWindow))
    {
        // 1. Environment の自動生成UI
        auto& env = m_gameLayer->GetEnvironment();
        DrawGlobalSettingsUI(env);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 2. PostEffect の自動生成UI
        auto& post = m_gameLayer->GetPostEffect();
        DrawGlobalSettingsUI(post);
    }
    ImGui::End();
}

void EditorLayer::DrawGBufferDebugWindow()
{
    if (!m_showGBufferDebug) return;

    if (ImGui::Begin(ICON_FA_IMAGES " G-Buffer Debug", &m_showGBufferDebug))
    {
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

