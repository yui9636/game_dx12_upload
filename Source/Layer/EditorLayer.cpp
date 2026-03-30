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
#include "Audio/AudioWorldSystem.h"
#include "Component/AudioEmitterComponent.h"
#include "Component/AudioListenerComponent.h"
#include "Component/AudioSettingsComponent.h"
#include "Component/Camera2DComponent.h"
#include "Component/CanvasItemComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Component/TransformComponent.h"
#include "Component/MeshComponent.h"
#include "Component/ColliderComponent.h"
#include "Console/Logger.h"
#include "Font/FontManager.h"
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
#include "Component/PrefabInstanceComponent.h"
#include "Console/Console.h"
#include "RHI/ITexture.h"
#include "ImGuiRenderer.h"
#include "UI/UI2DDrawSystem.h"
#include "UI/UIHitTestSystem.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <cfloat>
#include <nlohmann/json.hpp>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace {
    struct ClipboardEntry
    {
        EntitySnapshot::Snapshot snapshot;
        EntityID parentEntity = Entity::NULL_ID;
    };

    std::vector<ClipboardEntry> s_entityClipboard;
    constexpr float kSceneViewPickDragThreshold = 4.0f;
    constexpr float kEditorFocusMinDistance = 1.5f;
    constexpr float kMinScaleValue = 0.001f;
    constexpr const char* kDefaultSceneSavePath = "Data/Scene/EditorScene.scene";
    constexpr const char* kSceneDialogFilter = "Scene Files (*.scene)\0*.scene\0Legacy Scene Files (*.scene.json)\0*.scene.json\0All Files\0*.*\0\0";
    constexpr double kAutosaveIntervalSeconds = 120.0;
    constexpr size_t kAutosaveKeepGenerations = 5;
    constexpr const char* kSceneViewWindowTitle = "Scene View";
    constexpr const char* kGameViewWindowTitle = "Game View";
    constexpr const char* kHierarchyWindowTitle = ICON_FA_LIST " Hierarchy";
    constexpr const char* kInspectorWindowTitle = ICON_FA_CIRCLE_INFO " Inspector";
    constexpr const char* kAssetBrowserWindowTitle = ICON_FA_FOLDER_OPEN " Asset Browser";
    constexpr const char* kLightingWindowTitle = ICON_FA_SUN " Lighting Settings";
    constexpr const char* kAudioWindowTitle = ICON_FA_VOLUME_HIGH " Audio";
    constexpr const char* kRenderPassesWindowTitle = "Render Passes";
    constexpr const char* kGridSettingsWindowTitle = "Grid Settings";
    constexpr const char* kGBufferWindowTitle = ICON_FA_IMAGES " G-Buffer Debug";
    constexpr const char* kConsoleWindowTitle = "Console";

    const char* GetWindowTitleForFocus(EditorLayer::WindowFocusTarget target)
    {
        switch (target) {
        case EditorLayer::WindowFocusTarget::SceneView: return kSceneViewWindowTitle;
        case EditorLayer::WindowFocusTarget::GameView: return kGameViewWindowTitle;
        case EditorLayer::WindowFocusTarget::Hierarchy: return kHierarchyWindowTitle;
        case EditorLayer::WindowFocusTarget::Inspector: return kInspectorWindowTitle;
        case EditorLayer::WindowFocusTarget::AssetBrowser: return kAssetBrowserWindowTitle;
        case EditorLayer::WindowFocusTarget::Console: return kConsoleWindowTitle;
        case EditorLayer::WindowFocusTarget::Lighting: return kLightingWindowTitle;
        case EditorLayer::WindowFocusTarget::Audio: return kAudioWindowTitle;
        case EditorLayer::WindowFocusTarget::RenderPasses: return kRenderPassesWindowTitle;
        case EditorLayer::WindowFocusTarget::GridSettings: return kGridSettingsWindowTitle;
        case EditorLayer::WindowFocusTarget::GBufferDebug: return kGBufferWindowTitle;
        default: return nullptr;
        }
    }

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

    bool ProjectWorldToSceneScreen(const DirectX::XMFLOAT4& rect,
                                   const DirectX::XMFLOAT4X4& view,
                                   const DirectX::XMFLOAT4X4& projection,
                                   const DirectX::XMFLOAT3& worldPosition,
                                   ImVec2& outScreen)
    {
        using namespace DirectX;
        if (rect.z <= 1.0f || rect.w <= 1.0f) {
            return false;
        }

        const XMMATRIX viewMatrix = XMLoadFloat4x4(&view);
        const XMMATRIX projectionMatrix = XMLoadFloat4x4(&projection);
        const XMVECTOR world = XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
        const XMVECTOR viewPos = XMVector4Transform(world, viewMatrix);
        const float viewZ = XMVectorGetZ(viewPos);
        if (!std::isfinite(viewZ) || viewZ <= 0.01f) {
            return false;
        }

        const XMVECTOR clip = XMVector4Transform(viewPos, projectionMatrix);
        const float clipW = XMVectorGetW(clip);
        if (!std::isfinite(clipW) || clipW <= 0.0001f) {
            return false;
        }

        const XMVECTOR ndcVec = XMVectorScale(clip, 1.0f / clipW);
        XMFLOAT3 ndc{};
        XMStoreFloat3(&ndc, ndcVec);
        if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || !std::isfinite(ndc.z) || ndc.z < 0.0f || ndc.z > 1.0f) {
            return false;
        }

        outScreen.x = rect.x + ((ndc.x * 0.5f) + 0.5f) * rect.z;
        outScreen.y = rect.y + ((-ndc.y * 0.5f) + 0.5f) * rect.w;
        return true;
    }

    bool ProjectWorldToSceneScreenDepth(const DirectX::XMFLOAT4& rect,
                                        const DirectX::XMFLOAT4X4& view,
                                        const DirectX::XMFLOAT4X4& projection,
                                        const DirectX::XMFLOAT3& worldPosition,
                                        ImVec2& outScreen,
                                        float& outDepth)
    {
        using namespace DirectX;
        if (rect.z <= 1.0f || rect.w <= 1.0f) {
            return false;
        }

        const XMMATRIX viewMatrix = XMLoadFloat4x4(&view);
        const XMMATRIX projectionMatrix = XMLoadFloat4x4(&projection);
        const XMVECTOR world = XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
        const XMVECTOR viewPos = XMVector4Transform(world, viewMatrix);
        const float viewZ = XMVectorGetZ(viewPos);
        if (!std::isfinite(viewZ) || viewZ <= 0.01f) {
            return false;
        }

        const XMVECTOR clip = XMVector4Transform(viewPos, projectionMatrix);
        const float clipW = XMVectorGetW(clip);
        if (!std::isfinite(clipW) || clipW <= 0.0001f) {
            return false;
        }

        const XMVECTOR ndcVec = XMVectorScale(clip, 1.0f / clipW);
        XMFLOAT3 ndc{};
        XMStoreFloat3(&ndc, ndcVec);
        if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || !std::isfinite(ndc.z) || ndc.z < 0.0f || ndc.z > 1.0f) {
            return false;
        }

        outScreen.x = rect.x + ((ndc.x * 0.5f) + 0.5f) * rect.z;
        outScreen.y = rect.y + ((-ndc.y * 0.5f) + 0.5f) * rect.w;
        outDepth = ndc.z;
        return true;
    }

    struct GridOccluder
    {
        ImVec2 min;
        ImVec2 max;
        float minDepth = 1.0f;
    };

    bool IntersectRayWithGroundPlane(const DirectX::XMFLOAT3& origin,
                                     const DirectX::XMFLOAT3& direction,
                                     DirectX::XMFLOAT3& outPoint)
    {
        const float denom = direction.y;
        if (std::fabs(denom) < 0.0001f) {
            return false;
        }

        const float t = -origin.y / denom;
        if (!std::isfinite(t) || t <= 0.0f) {
            return false;
        }

        outPoint = {
            origin.x + direction.x * t,
            0.0f,
            origin.z + direction.z * t
        };
        return true;
    }

    bool IsPointInsideOccluder(const ImVec2& point, float depth, const std::vector<GridOccluder>& occluders)
    {
        constexpr float kGridDepthBias = 0.0005f;
        for (const GridOccluder& occluder : occluders) {
            if (point.x < occluder.min.x || point.x > occluder.max.x ||
                point.y < occluder.min.y || point.y > occluder.max.y) {
                continue;
            }

            if (depth >= occluder.minDepth - kGridDepthBias) {
                return true;
            }
        }
        return false;
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

    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    std::string SanitizeSceneDefaultPath(const std::string& path)
    {
        return path.empty() ? std::string(kDefaultSceneSavePath) : path;
    }

    const char* SceneViewModeToString(EditorLayer::SceneViewMode mode)
    {
        return mode == EditorLayer::SceneViewMode::Mode2D ? "2D" : "3D";
    }

    const char* SceneShadingModeToString(EditorLayer::SceneShadingMode mode)
    {
        switch (mode) {
        case EditorLayer::SceneShadingMode::Unlit:
            return "Unlit";
        case EditorLayer::SceneShadingMode::Wireframe:
            return "Wireframe";
        case EditorLayer::SceneShadingMode::Lit:
        default:
            return "Lit";
        }
    }

    EditorLayer::SceneViewMode SceneViewModeFromString(const std::string& value)
    {
        return ToLowerCopy(value) == "2d"
            ? EditorLayer::SceneViewMode::Mode2D
            : EditorLayer::SceneViewMode::Mode3D;
    }

    SceneFileMetadata BuildSceneMetadata(EditorLayer::SceneViewMode mode)
    {
        SceneFileMetadata metadata;
        metadata.sceneViewMode = SceneViewModeToString(mode);
        return metadata;
    }

    std::filesystem::path GetAutosaveRootDirectory()
    {
        return std::filesystem::path("Saved/Autosave");
    }

    std::string GetSceneAutosaveKey(const std::filesystem::path& scenePath)
    {
        const std::string stem = scenePath.stem().string();
        return stem.empty() ? "Untitled" : stem;
    }

    std::filesystem::path GetAutosaveDirectoryForScene(const std::filesystem::path& scenePath)
    {
        return GetAutosaveRootDirectory() / GetSceneAutosaveKey(scenePath);
    }

    void BuildBoundingBoxCorners(const DirectX::BoundingBox& bounds, DirectX::XMFLOAT3(&corners)[8])
    {
        const DirectX::XMFLOAT3& c = bounds.Center;
        const DirectX::XMFLOAT3& e = bounds.Extents;
        corners[0] = { c.x - e.x, c.y - e.y, c.z - e.z };
        corners[1] = { c.x + e.x, c.y - e.y, c.z - e.z };
        corners[2] = { c.x + e.x, c.y + e.y, c.z - e.z };
        corners[3] = { c.x - e.x, c.y + e.y, c.z - e.z };
        corners[4] = { c.x - e.x, c.y - e.y, c.z + e.z };
        corners[5] = { c.x + e.x, c.y - e.y, c.z + e.z };
        corners[6] = { c.x + e.x, c.y + e.y, c.z + e.z };
        corners[7] = { c.x - e.x, c.y + e.y, c.z + e.z };
    }

    bool TryGetLiveMeshWorldBounds(const MeshComponent& mesh,
                                   const TransformComponent* transform,
                                   DirectX::BoundingBox& outBounds)
    {
        if (!mesh.model) {
            return false;
        }

        if (transform) {
            if (const std::shared_ptr<ModelResource> modelResource = mesh.model->GetModelResource()) {
                const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&transform->worldMatrix);
                modelResource->GetLocalBounds().Transform(outBounds, world);
                return true;
            }
        }

        outBounds = mesh.model->GetWorldBounds();
        return true;
    }

    std::string GetEntityLabel(Registry& registry, EntityID entity)
    {
        if (auto* name = registry.GetComponent<NameComponent>(entity)) {
            return name->name;
        }
        return Entity::IsNull(entity) ? std::string("Entity") : ("Entity " + std::to_string(entity));
    }

    std::string BuildAutosaveTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
        localtime_s(&localTime, &tt);
        std::ostringstream oss;
        oss << std::put_time(&localTime, "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::filesystem::path BuildAutosaveScenePath(const std::filesystem::path& scenePath)
    {
        const std::string key = GetSceneAutosaveKey(scenePath);
        return GetAutosaveDirectoryForScene(scenePath) / (key + "_autosave_" + BuildAutosaveTimestamp() + ".scene");
    }

    void TrimAutosaveGenerations(const std::filesystem::path& autosaveDir)
    {
        std::error_code ec;
        std::vector<std::filesystem::directory_entry> entries;
        if (!std::filesystem::exists(autosaveDir, ec)) {
            return;
        }
        for (const auto& entry : std::filesystem::directory_iterator(autosaveDir, ec)) {
            if (ec) {
                break;
            }
            if (entry.is_regular_file(ec) && entry.path().extension() == ".scene") {
                entries.push_back(entry);
            }
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            std::error_code sortEcA, sortEcB;
            return a.last_write_time(sortEcA) > b.last_write_time(sortEcB);
        });
        for (size_t i = kAutosaveKeepGenerations; i < entries.size(); ++i) {
            std::filesystem::remove(entries[i].path(), ec);
        }
    }

    std::filesystem::path FindLatestAutosaveForScene(const std::filesystem::path& scenePath)
    {
        const std::filesystem::path autosaveDir = GetAutosaveDirectoryForScene(scenePath);
        std::error_code ec;
        if (!std::filesystem::exists(autosaveDir, ec)) {
            return {};
        }

        std::filesystem::path latestPath;
        std::filesystem::file_time_type latestTime{};
        bool hasLatest = false;
        for (const auto& entry : std::filesystem::directory_iterator(autosaveDir, ec)) {
            if (ec || !entry.is_regular_file(ec) || entry.path().extension() != ".scene") {
                continue;
            }
            std::error_code timeEc;
            const auto time = entry.last_write_time(timeEc);
            if (!hasLatest || (!timeEc && time > latestTime)) {
                latestPath = entry.path();
                latestTime = time;
                hasLatest = !timeEc;
            }
        }
        return latestPath;
    }

    bool IsAutosaveNewerThanScene(const std::filesystem::path& autosavePath, const std::filesystem::path& scenePath)
    {
        std::error_code ec;
        if (autosavePath.empty() || !std::filesystem::exists(autosavePath, ec)) {
            return false;
        }
        if (!std::filesystem::exists(scenePath, ec)) {
            return true;
        }
        const auto autosaveTime = std::filesystem::last_write_time(autosavePath, ec);
        if (ec) {
            return false;
        }
        const auto sceneTime = std::filesystem::last_write_time(scenePath, ec);
        if (ec) {
            return true;
        }
        return autosaveTime > sceneTime;
    }

    bool IsUtilityEntity(Registry& registry, EntityID entity)
    {
        return registry.GetComponent<EnvironmentComponent>(entity) ||
               registry.GetComponent<ReflectionProbeComponent>(entity) ||
               registry.GetComponent<AudioSettingsComponent>(entity);
    }

    EntityID FindEnvironmentEntity(Registry& registry)
    {
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<EnvironmentComponent>())) {
                continue;
            }
            const auto& entities = archetype->GetEntities();
            if (!entities.empty()) {
                return entities.front();
            }
        }
        return Entity::NULL_ID;
    }

    EnvironmentComponent& GetOrCreateEnvironmentComponent(Registry& registry)
    {
        EntityID environmentEntity = FindEnvironmentEntity(registry);
        if (Entity::IsNull(environmentEntity)) {
            environmentEntity = registry.CreateEntity();
            registry.AddComponent(environmentEntity, NameComponent{ "Environment" });
            registry.AddComponent(environmentEntity, EnvironmentComponent{});
        } else if (registry.GetComponent<HierarchyComponent>(environmentEntity)) {
            registry.RemoveComponent<HierarchyComponent>(environmentEntity);
        }
        return *registry.GetComponent<EnvironmentComponent>(environmentEntity);
    }

    EntityID FindReflectionProbeEntity(Registry& registry)
    {
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<ReflectionProbeComponent>())) {
                continue;
            }
            const auto& entities = archetype->GetEntities();
            if (!entities.empty()) {
                return entities.front();
            }
        }
        return Entity::NULL_ID;
    }

    ReflectionProbeComponent& GetOrCreateReflectionProbeComponent(Registry& registry)
    {
        EntityID probeEntity = FindReflectionProbeEntity(registry);
        if (Entity::IsNull(probeEntity)) {
            probeEntity = registry.CreateEntity();
            registry.AddComponent(probeEntity, NameComponent{ "Reflection Probe" });
            registry.AddComponent(probeEntity, ReflectionProbeComponent{});
        } else if (registry.GetComponent<HierarchyComponent>(probeEntity)) {
            registry.RemoveComponent<HierarchyComponent>(probeEntity);
        }
        return *registry.GetComponent<ReflectionProbeComponent>(probeEntity);
    }

    EntityID FindAudioSettingsEntity(Registry& registry)
    {
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<AudioSettingsComponent>())) {
                continue;
            }
            const auto& entities = archetype->GetEntities();
            if (!entities.empty()) {
                return entities.front();
            }
        }
        return Entity::NULL_ID;
    }

    AudioSettingsComponent& GetOrCreateAudioSettingsComponent(Registry& registry)
    {
        EntityID audioEntity = FindAudioSettingsEntity(registry);
        if (Entity::IsNull(audioEntity)) {
            audioEntity = registry.CreateEntity();
            registry.AddComponent(audioEntity, NameComponent{ "Audio Settings" });
            registry.AddComponent(audioEntity, AudioSettingsComponent{});
        } else if (registry.GetComponent<HierarchyComponent>(audioEntity)) {
            registry.RemoveComponent<HierarchyComponent>(audioEntity);
        }
        return *registry.GetComponent<AudioSettingsComponent>(audioEntity);
    }

    DirectX::XMUINT2 GetGameViewPresetResolution(EditorLayer::GameViewResolutionPreset preset)
    {
        using namespace DirectX;
        switch (preset) {
        case EditorLayer::GameViewResolutionPreset::HD1080:
            return XMUINT2(1920, 1080);
        case EditorLayer::GameViewResolutionPreset::HD720:
            return XMUINT2(1280, 720);
        case EditorLayer::GameViewResolutionPreset::Portrait1080x1920:
            return XMUINT2(1080, 1920);
        case EditorLayer::GameViewResolutionPreset::Portrait750x1334:
            return XMUINT2(750, 1334);
        case EditorLayer::GameViewResolutionPreset::Free:
        default:
            return XMUINT2(0, 0);
        }
    }

    const char* GetGameViewPresetLabel(EditorLayer::GameViewResolutionPreset preset)
    {
        switch (preset) {
        case EditorLayer::GameViewResolutionPreset::HD1080:
            return "1920x1080";
        case EditorLayer::GameViewResolutionPreset::HD720:
            return "1280x720";
        case EditorLayer::GameViewResolutionPreset::Portrait1080x1920:
            return "1080x1920";
        case EditorLayer::GameViewResolutionPreset::Portrait750x1334:
            return "750x1334";
        case EditorLayer::GameViewResolutionPreset::Free:
        default:
            return "Free";
        }
    }

    const char* GetGameViewAspectPolicyLabel(EditorLayer::GameViewAspectPolicy policy)
    {
        switch (policy) {
        case EditorLayer::GameViewAspectPolicy::Fill:
            return "Fill";
        case EditorLayer::GameViewAspectPolicy::PixelPerfect:
            return "1:1";
        case EditorLayer::GameViewAspectPolicy::Fit:
        default:
            return "Fit";
        }
    }

    const char* GetGameViewScalePolicyLabel(EditorLayer::GameViewScalePolicy policy)
    {
        switch (policy) {
        case EditorLayer::GameViewScalePolicy::Scale1x:
            return "1x";
        case EditorLayer::GameViewScalePolicy::Scale2x:
            return "2x";
        case EditorLayer::GameViewScalePolicy::Scale3x:
            return "3x";
        case EditorLayer::GameViewScalePolicy::AutoFit:
        default:
            return "Auto";
        }
    }

    ImVec2 FitSizeKeepingAspect(const ImVec2& available, float aspect)
    {
        if (available.x <= 0.0f || available.y <= 0.0f || aspect <= 0.0f) {
            return available;
        }

        ImVec2 fitted = available;
        const float currentAspect = available.x / available.y;
        if (currentAspect > aspect) {
            fitted.x = available.y * aspect;
        } else {
            fitted.y = available.x / aspect;
        }
        return fitted;
    }

    ImVec2 FillSizeKeepingAspect(const ImVec2& available, float aspect)
    {
        if (available.x <= 0.0f || available.y <= 0.0f || aspect <= 0.0f) {
            return available;
        }

        ImVec2 filled = available;
        const float currentAspect = available.x / available.y;
        if (currentAspect > aspect) {
            filled.y = available.x / aspect;
        } else {
            filled.x = available.y * aspect;
        }
        return filled;
    }

    bool HasSelectedAncestor(EntityID entity, Registry& registry, const EditorSelection& selection)
    {
        const HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        EntityID parent = hierarchy ? hierarchy->parent : Entity::NULL_ID;
        while (!Entity::IsNull(parent)) {
            if (selection.IsEntitySelected(parent)) {
                return true;
            }
            hierarchy = registry.GetComponent<HierarchyComponent>(parent);
            parent = hierarchy ? hierarchy->parent : Entity::NULL_ID;
        }
        return false;
    }

    std::vector<EntityID> GetSelectedRootEntities(Registry& registry, const EditorSelection& selection)
    {
        std::vector<EntityID> roots;
        for (EntityID entity : selection.GetSelectedEntities()) {
            if (!registry.IsAlive(entity)) {
                continue;
            }
            if (!HasSelectedAncestor(entity, registry, selection)) {
                roots.push_back(entity);
            }
        }
        return roots;
    }

    std::vector<EntityID> DuplicateSelectionRoots(Registry& registry, const EditorSelection& selection)
    {
        std::vector<EntityID> selectedRoots = GetSelectedRootEntities(registry, selection);
        auto composite = std::make_unique<CompositeUndoAction>("Alt Drag Duplicate");
        std::vector<DuplicateEntityAction*> duplicateActions;

        for (EntityID selectedRoot : selectedRoots) {
            if (!PrefabSystem::CanDuplicate(selectedRoot, registry)) {
                LOG_WARN("[Prefab] Prefab instance children cannot be duplicated. Duplicate the root instance or unpack it first.");
                continue;
            }

            EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedRoot, registry);
            if (snapshot.nodes.empty()) {
                continue;
            }

            EntityID parentEntity = Entity::NULL_ID;
            if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(selectedRoot)) {
                parentEntity = hierarchy->parent;
            }

            EntitySnapshot::AppendRootNameSuffix(snapshot, " (Clone)");
            auto action = std::make_unique<DuplicateEntityAction>(std::move(snapshot), parentEntity);
            auto* actionPtr = action.get();
            composite->Add(std::move(action));
            duplicateActions.push_back(actionPtr);
        }

        if (composite->Empty()) {
            return {};
        }

        UndoSystem::Instance().ExecuteAction(std::move(composite), registry);

        std::vector<EntityID> liveRoots;
        liveRoots.reserve(duplicateActions.size());
        for (DuplicateEntityAction* action : duplicateActions) {
            if (!Entity::IsNull(action->GetLiveRoot())) {
                liveRoots.push_back(action->GetLiveRoot());
            }
        }
        return liveRoots;
    }

    std::vector<ClipboardEntry> CopySelectionRoots(Registry& registry, const EditorSelection& selection)
    {
        std::vector<ClipboardEntry> clipboard;
        for (EntityID selectedRoot : GetSelectedRootEntities(registry, selection)) {
            EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedRoot, registry);
            if (snapshot.nodes.empty()) {
                continue;
            }
            EntityID parentEntity = Entity::NULL_ID;
            if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(selectedRoot)) {
                parentEntity = hierarchy->parent;
            }
            clipboard.push_back(ClipboardEntry{ std::move(snapshot), parentEntity });
        }
        return clipboard;
    }

    class CreateEmptyParentAction : public IUndoAction
    {
    public:
        CreateEmptyParentAction(std::vector<EntityID> children,
                                std::vector<EntityID> oldParents,
                                EntityID externalParent)
            : m_children(std::move(children))
            , m_oldParents(std::move(oldParents))
            , m_externalParent(externalParent)
        {
            m_parentSnapshot.rootLocalID = 0;
            EntitySnapshot::Node node;
            node.localID = 0;
            node.sourceEntity = Entity::NULL_ID;
            node.parentLocalID = EntitySnapshot::kInvalidLocalID;
            node.externalParent = m_externalParent;
            std::get<std::optional<NameComponent>>(node.components) = NameComponent{ "Empty Parent" };
            std::get<std::optional<TransformComponent>>(node.components) = TransformComponent{};
            std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
            m_parentSnapshot.nodes.push_back(std::move(node));
        }

        void Undo(Registry& registry) override
        {
            for (size_t i = 0; i < m_children.size(); ++i) {
                if (registry.IsAlive(m_children[i])) {
                    HierarchySystem::Reparent(m_children[i], m_oldParents[i], registry, true);
                }
            }
            if (!Entity::IsNull(m_parentEntity) && registry.IsAlive(m_parentEntity)) {
                EntitySnapshot::DestroySubtree(m_parentEntity, registry);
            }
            m_parentEntity = Entity::NULL_ID;
        }

        void Redo(Registry& registry) override
        {
            EntitySnapshot::Snapshot snapshot = m_parentSnapshot;
            for (auto& node : snapshot.nodes) {
                if (node.localID == snapshot.rootLocalID) {
                    node.externalParent = m_externalParent;
                    break;
                }
            }
            EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(snapshot, registry);
            m_parentEntity = restore.root;
            for (EntityID child : m_children) {
                if (registry.IsAlive(child)) {
                    HierarchySystem::Reparent(child, m_parentEntity, registry, true);
                }
            }
        }

        const char* GetName() const override { return "Create Empty Parent"; }
        EntityID GetLiveRoot() const { return m_parentEntity; }

    private:
        std::vector<EntityID> m_children;
        std::vector<EntityID> m_oldParents;
        EntitySnapshot::Snapshot m_parentSnapshot;
        EntityID m_externalParent = Entity::NULL_ID;
        EntityID m_parentEntity = Entity::NULL_ID;
    };

    bool IsSupportedModelAsset(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".gltf";
    }

    bool IsSupportedSpriteAsset(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds" || ext == ".bmp";
    }

    bool IsSupportedFontAsset(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".ttf" || ext == ".otf" || ext == ".fnt";
    }

    EntitySnapshot::Snapshot BuildSingleEntitySnapshot(const std::string& name,
                                                       const MeshComponent* meshComponent = nullptr)
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

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    EntitySnapshot::Snapshot BuildSingleSpriteSnapshot(const std::string& name,
                                                       const std::string& texturePath,
                                                       const DirectX::XMFLOAT2& anchoredPosition,
                                                       const DirectX::XMFLOAT2& size)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        TransformComponent transform{};
        transform.localPosition = { anchoredPosition.x, anchoredPosition.y, 0.0f };
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;

        RectTransformComponent rect{};
        rect.anchoredPosition = anchoredPosition;
        rect.sizeDelta = size;

        CanvasItemComponent canvas{};
        SpriteComponent sprite{};
        sprite.textureAssetPath = texturePath;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<RectTransformComponent>>(node.components) = rect;
        std::get<std::optional<CanvasItemComponent>>(node.components) = canvas;
        std::get<std::optional<SpriteComponent>>(node.components) = sprite;

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    EntitySnapshot::Snapshot BuildSingleTextSnapshot(const std::string& name,
                                                     const std::string& fontPath,
                                                     const DirectX::XMFLOAT2& anchoredPosition)
    {
        EntitySnapshot::Snapshot snapshot;
        snapshot.rootLocalID = 0;

        EntitySnapshot::Node node;
        node.localID = 0;
        node.sourceEntity = Entity::NULL_ID;
        node.parentLocalID = EntitySnapshot::kInvalidLocalID;
        node.externalParent = Entity::NULL_ID;

        TransformComponent transform{};
        transform.localPosition = { anchoredPosition.x, anchoredPosition.y, 0.0f };
        transform.localScale = { 1.0f, 1.0f, 1.0f };
        transform.isDirty = true;

        RectTransformComponent rect{};
        rect.anchoredPosition = anchoredPosition;
        rect.sizeDelta = { 320.0f, 80.0f };

        CanvasItemComponent canvas{};
        TextComponent text{};
        text.text = name;
        text.fontAssetPath = fontPath;

        std::get<std::optional<NameComponent>>(node.components) = NameComponent{ name };
        std::get<std::optional<TransformComponent>>(node.components) = transform;
        std::get<std::optional<HierarchyComponent>>(node.components) = HierarchyComponent{};
        std::get<std::optional<RectTransformComponent>>(node.components) = rect;
        std::get<std::optional<CanvasItemComponent>>(node.components) = canvas;
        std::get<std::optional<TextComponent>>(node.components) = text;

        snapshot.nodes.push_back(std::move(node));
        return snapshot;
    }

    void SyncRectTransformToTransform(RectTransformComponent& rect, TransformComponent& transform)
    {
        using namespace DirectX;
        transform.localPosition = { rect.anchoredPosition.x, rect.anchoredPosition.y, 0.0f };
        const XMVECTOR q = XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, XMConvertToRadians(rect.rotationZ));
        XMStoreFloat4(&transform.localRotation, q);
        transform.localScale = { rect.scale2D.x, rect.scale2D.y, 1.0f };
        transform.isDirty = true;
    }

    void SyncRectTransforms(Registry& registry)
    {
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& signature = archetype->GetSignature();
            if (!signature.test(TypeManager::GetComponentTypeID<RectTransformComponent>()) ||
                !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
                continue;
            }

            auto* rectColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<RectTransformComponent>());
            auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                auto* rect = static_cast<RectTransformComponent*>(rectColumn->Get(i));
                auto* transform = static_cast<TransformComponent*>(transformColumn->Get(i));
                if (!rect || !transform) {
                    continue;
                }
                SyncRectTransformToTransform(*rect, *transform);
            }
        }
    }

    bool ApplyPlacementToSnapshot(EntitySnapshot::Snapshot& snapshot, const DirectX::XMFLOAT3& worldPosition)
    {
        for (auto& node : snapshot.nodes) {
            if (node.localID == snapshot.rootLocalID) {
                auto& transform = std::get<std::optional<TransformComponent>>(node.components);
                if (!transform.has_value()) {
                    transform = TransformComponent{};
                }
                transform->localPosition = worldPosition;
                transform->isDirty = true;
                return true;
            }
        }
        return false;
    }

    DirectX::XMFLOAT3 AdjustPlacementForModelBounds(const MeshComponent& meshComponent,
                                                    const DirectX::XMFLOAT3& placementPosition)
    {
        DirectX::XMFLOAT3 adjusted = placementPosition;
        if (!meshComponent.model) {
            return adjusted;
        }

        const auto& bounds = meshComponent.model->GetWorldBounds();
        adjusted.x -= bounds.Center.x;
        adjusted.y -= (bounds.Center.y - bounds.Extents.y);
        adjusted.z -= bounds.Center.z;
        return adjusted;
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
        registry.AddComponent(cameraEntity, AudioListenerComponent{});

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

        EntityID environmentEntity = registry.CreateEntity();
        registry.AddComponent(environmentEntity, NameComponent{ "Environment" });
        registry.AddComponent(environmentEntity, EnvironmentComponent{});

        EntityID audioSettingsEntity = registry.CreateEntity();
        registry.AddComponent(audioSettingsEntity, NameComponent{ "Audio Settings" });
        registry.AddComponent(audioSettingsEntity, AudioSettingsComponent{});
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
    MarkSceneSaved();
    CheckRecoveryCandidate();
}

void EditorLayer::Finalize()
{
}

void EditorLayer::ExecuteUndo()
{
    if (m_gameLayer) {
        UndoSystem::Instance().Undo(m_gameLayer->GetRegistry());
    }
}

void EditorLayer::ExecuteRedo()
{
    if (m_gameLayer) {
        UndoSystem::Instance().Redo(m_gameLayer->GetRegistry());
    }
}

void EditorLayer::ExecuteDuplicateSelection()
{
    if (!m_gameLayer) {
        return;
    }
    auto& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity || selection.GetSelectedEntityCount() == 0) {
        return;
    }
    const std::vector<EntityID> liveRoots = DuplicateSelectionRoots(registry, selection);
    if (!liveRoots.empty()) {
        selection.SetEntitySelection(liveRoots, liveRoots.back());
    }
}

void EditorLayer::ExecuteDeleteSelection()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity || selection.GetSelectedEntityCount() == 0) {
        return;
    }

    const std::vector<EntityID> selectedRoots = GetSelectedRootEntities(registry, selection);
    auto composite = std::make_unique<CompositeUndoAction>("Delete Entities");
    bool canDeleteAny = false;
    for (EntityID selectedRoot : selectedRoots) {
        if (!PrefabSystem::CanDelete(selectedRoot, registry)) {
            LOG_WARN("[Prefab] Prefab instance children cannot be deleted directly. Use Unpack first.");
            continue;
        }
        EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(selectedRoot, registry);
        if (!snapshot.nodes.empty()) {
            composite->Add(std::make_unique<DeleteEntityAction>(std::move(snapshot), selectedRoot));
            canDeleteAny = true;
        }
    }
    if (canDeleteAny) {
        UndoSystem::Instance().ExecuteAction(std::move(composite), registry);
        selection.Clear();
    }
}

void EditorLayer::ExecuteFrameSelected()
{
    FocusSelectedEntity();
}

void EditorLayer::ExecuteSelectAll()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    std::vector<EntityID> entities;
    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<HierarchyComponent>())) {
            continue;
        }
        const auto& archetypeEntities = archetype->GetEntities();
        for (EntityID entity : archetypeEntities) {
            if (!registry.IsAlive(entity) || IsUtilityEntity(registry, entity)) {
                continue;
            }
            entities.push_back(entity);
        }
    }
    if (!entities.empty()) {
        EditorSelection::Instance().SetEntitySelection(entities, entities.front());
    }
}

void EditorLayer::ExecuteDeselect()
{
    EditorSelection::Instance().Clear();
}

void EditorLayer::ExecuteRenameSelection()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    auto* name = registry.GetComponent<NameComponent>(entity);
    if (!name) {
        return;
    }
    strcpy_s(m_renameBuffer, name->name.c_str());
    m_requestRenamePopup = true;
}

void EditorLayer::ExecuteCopySelection()
{
    if (!m_gameLayer) {
        return;
    }
    auto& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity || selection.GetSelectedEntityCount() == 0) {
        return;
    }
    s_entityClipboard = CopySelectionRoots(registry, selection);
}

void EditorLayer::ExecutePasteSelection()
{
    if (!m_gameLayer || s_entityClipboard.empty()) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    EntityID explicitParent = Entity::NULL_ID;
    if (selection.GetType() == SelectionType::Entity) {
        const EntityID selectedEntity = selection.GetPrimaryEntity();
        if (!Entity::IsNull(selectedEntity) && registry.IsAlive(selectedEntity) &&
            PrefabSystem::CanCreateChild(selectedEntity, registry)) {
            explicitParent = selectedEntity;
        }
    }

    auto composite = std::make_unique<CompositeUndoAction>("Paste Entities");
    std::vector<CreateEntityAction*> actions;
    for (const ClipboardEntry& entry : s_entityClipboard) {
        EntitySnapshot::Snapshot snapshot = entry.snapshot;
        if (snapshot.nodes.empty()) {
            continue;
        }
        EntitySnapshot::AppendRootNameSuffix(snapshot, " (Copy)");
        EntityID parentEntity = explicitParent;
        if (Entity::IsNull(parentEntity) && !Entity::IsNull(entry.parentEntity) && registry.IsAlive(entry.parentEntity)) {
            parentEntity = entry.parentEntity;
        }
        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), parentEntity, "Paste Entity");
        actions.push_back(action.get());
        composite->Add(std::move(action));
    }

    if (composite->Empty()) {
        return;
    }

    UndoSystem::Instance().ExecuteAction(std::move(composite), registry);
    std::vector<EntityID> pastedEntities;
    pastedEntities.reserve(actions.size());
    for (CreateEntityAction* action : actions) {
        if (!Entity::IsNull(action->GetLiveRoot())) {
            pastedEntities.push_back(action->GetLiveRoot());
        }
    }
    if (!pastedEntities.empty()) {
        selection.SetEntitySelection(pastedEntities, pastedEntities.back());
    }
}

void EditorLayer::ExecuteResetTransform()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
        return;
    }

    auto composite = std::make_unique<CompositeUndoAction>("Reset Transform");
    bool changed = false;
    if (auto* rect = registry.GetComponent<RectTransformComponent>(entity)) {
        RectTransformComponent before = *rect;
        rect->anchoredPosition = { 0.0f, 0.0f };
        rect->sizeDelta = { 100.0f, 100.0f };
        rect->anchorMin = { 0.5f, 0.5f };
        rect->anchorMax = { 0.5f, 0.5f };
        rect->pivot = { 0.5f, 0.5f };
        rect->rotationZ = 0.0f;
        rect->scale2D = { 1.0f, 1.0f };
        composite->Add(std::make_unique<ComponentUndoAction<RectTransformComponent>>(entity, before, *rect));
        changed = true;
    }

    if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
        TransformComponent before = *transform;
        transform->localPosition = { 0.0f, 0.0f, 0.0f };
        transform->localRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        transform->localScale = { 1.0f, 1.0f, 1.0f };
        transform->isDirty = true;
        if (auto* rect = registry.GetComponent<RectTransformComponent>(entity)) {
            SyncRectTransformToTransform(*rect, *transform);
        }
        composite->Add(std::make_unique<ComponentUndoAction<TransformComponent>>(entity, before, *transform));
        changed = true;
    }

    if (changed && !composite->Empty()) {
        UndoSystem::Instance().RecordAction(std::move(composite));
        HierarchySystem::MarkDirtyRecursive(entity, registry);
        PrefabSystem::MarkPrefabOverride(entity, registry);
    }
}

void EditorLayer::ExecuteCreateEmptyParent()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    auto& selection = EditorSelection::Instance();
    if (selection.GetType() != SelectionType::Entity || selection.GetSelectedEntityCount() == 0) {
        return;
    }

    const std::vector<EntityID> selectedRoots = GetSelectedRootEntities(registry, selection);
    if (selectedRoots.empty()) {
        return;
    }

    std::vector<EntityID> oldParents;
    oldParents.reserve(selectedRoots.size());
    EntityID commonParent = Entity::NULL_ID;
    bool commonParentInitialized = false;
    for (EntityID root : selectedRoots) {
        EntityID parentEntity = Entity::NULL_ID;
        if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(root)) {
            parentEntity = hierarchy->parent;
        }
        oldParents.push_back(parentEntity);
        if (!commonParentInitialized) {
            commonParent = parentEntity;
            commonParentInitialized = true;
        } else if (commonParent != parentEntity) {
            commonParent = Entity::NULL_ID;
        }
    }

    auto action = std::make_unique<CreateEmptyParentAction>(selectedRoots, oldParents, commonParent);
    auto* actionPtr = action.get();
    UndoSystem::Instance().ExecuteAction(std::move(action), registry);
    if (!Entity::IsNull(actionPtr->GetLiveRoot())) {
        EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
    }
}

void EditorLayer::ExecuteUnpackPrefab()
{
    if (!m_gameLayer) {
        return;
    }
    Registry& registry = m_gameLayer->GetRegistry();
    EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    EntityID root = PrefabSystem::FindPrefabRoot(entity, registry);
    if (Entity::IsNull(root)) {
        return;
    }
    auto* prefabInstance = registry.GetComponent<PrefabInstanceComponent>(root);
    if (!prefabInstance) {
        return;
    }

    UndoSystem::Instance().ExecuteAction(
        std::make_unique<OptionalComponentUndoAction<PrefabInstanceComponent>>(
            root,
            std::optional<PrefabInstanceComponent>(*prefabInstance),
            std::nullopt,
            "Unpack Prefab"),
        registry);
    EditorSelection::Instance().SelectEntity(root);
}

void EditorLayer::ExecuteFocusSearch()
{
    m_showHierarchy = true;
    RequestWindowFocus(WindowFocusTarget::Hierarchy);
    HierarchyECSUI::RequestSearchFocus();
}

void EditorLayer::ExecuteResetView()
{
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        m_editor2DCenter = { 0.0f, 0.0f };
        m_editor2DZoom = 10.0f;
    } else {
        m_editorCameraPosition = { 0.0f, 12.0f, -80.0f };
        m_editorCameraYaw = 0.0f;
        m_editorCameraPitch = 0.0f;
        m_editorCameraUserOverride = false;
        m_editorCameraAutoFramed = false;
    }
}

void EditorLayer::ExecuteGamePlay()
{
    EngineKernel& kernel = EngineKernel::Instance();
    if (kernel.GetMode() == EngineMode::Editor || kernel.GetMode() == EngineMode::Pause) {
        kernel.Play();
    }
}

void EditorLayer::ExecuteGamePauseToggle()
{
    EngineKernel& kernel = EngineKernel::Instance();
    if (kernel.GetMode() == EngineMode::Play || kernel.GetMode() == EngineMode::Pause) {
        kernel.Pause();
    }
}

void EditorLayer::ExecuteGameStop()
{
    EngineKernel& kernel = EngineKernel::Instance();
    if (kernel.GetMode() != EngineMode::Editor) {
        kernel.Stop();
    }
}

void EditorLayer::ExecuteGameStep()
{
    EngineKernel& kernel = EngineKernel::Instance();
    if (kernel.GetMode() == EngineMode::Pause) {
        kernel.Step();
    }
}

void EditorLayer::ExecuteGameResetPreview()
{
    m_gameViewResolutionPreset = GameViewResolutionPreset::Free;
    m_gameViewAspectPolicy = GameViewAspectPolicy::Fit;
    m_gameViewScalePolicy = GameViewScalePolicy::AutoFit;
    m_gameViewShowSafeArea = false;
    m_gameViewShowPixelPreview = false;
    m_gameViewShowStatsOverlay = false;
    m_gameViewShowUIOverlay = true;
    m_gameViewShow2DOverlay = true;
}

void EditorLayer::ExecuteCloseSecondaryWindows()
{
    m_showLightingWindow = false;
    m_showAudioWindow = false;
    m_showRenderPassesWindow = false;
    m_showGridSettingsWindow = false;
    m_showGBufferDebug = false;
    if (m_maximizedWindow == WindowFocusTarget::Lighting ||
        m_maximizedWindow == WindowFocusTarget::Audio ||
        m_maximizedWindow == WindowFocusTarget::RenderPasses ||
        m_maximizedWindow == WindowFocusTarget::GridSettings ||
        m_maximizedWindow == WindowFocusTarget::GBufferDebug) {
        m_maximizedWindow = WindowFocusTarget::None;
    }
}

void EditorLayer::ExecuteResetLayout()
{
    m_showSceneView = true;
    m_showGameView = true;
    m_showHierarchy = true;
    m_showInspector = true;
    m_showAssetBrowser = true;
    m_showConsole = true;
    m_showLightingWindow = false;
    m_showAudioWindow = false;
    m_showRenderPassesWindow = false;
    m_showGridSettingsWindow = false;
    m_showGBufferDebug = false;
    m_showStatusBar = true;
    m_showMainToolbar = true;
    m_showSceneGrid = true;
    m_showSceneGizmo = true;
    m_showSceneStatsOverlay = false;
    m_showSceneSelectionOutline = false;
    m_showSceneLightIcons = true;
    m_showSceneCameraIcons = true;
    m_showSceneBounds = false;
    m_showSceneCollision = false;
    m_sceneShadingMode = SceneShadingMode::Lit;
    m_maximizedWindow = WindowFocusTarget::None;
    m_forceDockLayoutReset = true;
}

void EditorLayer::ExecuteMaximizeActivePanel()
{
    if (m_lastFocusedWindow == WindowFocusTarget::None) {
        return;
    }
    m_maximizedWindow = (m_maximizedWindow == m_lastFocusedWindow) ? WindowFocusTarget::None : m_lastFocusedWindow;
    m_forceDockLayoutReset = true;
}

void EditorLayer::RequestWindowFocus(WindowFocusTarget target)
{
    if (m_maximizedWindow != WindowFocusTarget::None && m_maximizedWindow != target) {
        m_maximizedWindow = WindowFocusTarget::None;
    }
    switch (target) {
    case WindowFocusTarget::SceneView: m_showSceneView = true; break;
    case WindowFocusTarget::GameView: m_showGameView = true; break;
    case WindowFocusTarget::Hierarchy: m_showHierarchy = true; break;
    case WindowFocusTarget::Inspector: m_showInspector = true; break;
    case WindowFocusTarget::AssetBrowser: m_showAssetBrowser = true; break;
    case WindowFocusTarget::Console: m_showConsole = true; break;
    case WindowFocusTarget::Lighting: m_showLightingWindow = true; break;
    case WindowFocusTarget::Audio: m_showAudioWindow = true; break;
    case WindowFocusTarget::RenderPasses: m_showRenderPassesWindow = true; break;
    case WindowFocusTarget::GridSettings: m_showGridSettingsWindow = true; break;
    case WindowFocusTarget::GBufferDebug: m_showGBufferDebug = true; break;
    default: break;
    }
    m_pendingWindowFocus = target;
}

void EditorLayer::ApplyPendingWindowFocus(WindowFocusTarget target)
{
    if (m_pendingWindowFocus == target) {
        ImGui::SetNextWindowFocus();
        m_pendingWindowFocus = WindowFocusTarget::None;
    }
}

void EditorLayer::SetLastFocusedWindow(WindowFocusTarget target, bool focused)
{
    if (focused) {
        m_lastFocusedWindow = target;
    }
}

void EditorLayer::SaveCameraBookmark(size_t slot)
{
    if (slot >= m_cameraBookmarks.size()) {
        return;
    }
    auto& bookmark = m_cameraBookmarks[slot];
    bookmark.valid = true;
    bookmark.mode = m_sceneViewMode;
    bookmark.cameraPosition = m_editorCameraPosition;
    bookmark.cameraYaw = m_editorCameraYaw;
    bookmark.cameraPitch = m_editorCameraPitch;
    bookmark.center2D = m_editor2DCenter;
    bookmark.zoom2D = m_editor2DZoom;
}

void EditorLayer::LoadCameraBookmark(size_t slot)
{
    if (slot >= m_cameraBookmarks.size()) {
        return;
    }
    const auto& bookmark = m_cameraBookmarks[slot];
    if (!bookmark.valid) {
        return;
    }
    m_sceneViewMode = bookmark.mode;
    m_editorCameraPosition = bookmark.cameraPosition;
    m_editorCameraYaw = bookmark.cameraYaw;
    m_editorCameraPitch = bookmark.cameraPitch;
    m_editor2DCenter = bookmark.center2D;
    m_editor2DZoom = bookmark.zoom2D;
    RequestWindowFocus(WindowFocusTarget::SceneView);
}

void EditorLayer::Update(const EngineTime& time)
{
    FontManager::Instance().ProcessEditorPreviewFonts();
    ImGuiIO& io = ImGui::GetIO();
    using namespace DirectX;

    HandleEditorShortcuts();
    ProcessDeferredEditorActions();
    UpdateAutosave(time.unscaledDt);

    if (!io.WantTextInput && m_gameLayer) {
        Registry& registry = m_gameLayer->GetRegistry();
        SyncRectTransforms(registry);

        if (m_sceneViewMode == SceneViewMode::Mode2D && m_sceneViewHovered && !m_sceneViewToolbarHovered) {
            if (std::fabs(io.MouseWheel) > 0.0001f) {
                const float zoomFactor = (io.MouseWheel > 0.0f) ? 0.9f : 1.1f;
                m_editor2DZoom = (std::clamp)(m_editor2DZoom * zoomFactor, 1.0f, 2000.0f);
            }

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
                const float aspect = (m_sceneViewRect.w > 0.0f) ? (m_sceneViewRect.z / m_sceneViewRect.w) : (16.0f / 9.0f);
                const float worldPerPixelX = ((m_editor2DZoom * 2.0f) * aspect) / (std::max)(m_sceneViewRect.z, 1.0f);
                const float worldPerPixelY = (m_editor2DZoom * 2.0f) / (std::max)(m_sceneViewRect.w, 1.0f);
                m_editor2DCenter.x -= mouseDelta.x * worldPerPixelX;
                m_editor2DCenter.y += mouseDelta.y * worldPerPixelY;
            }
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (io.KeyShift) {
                ExecuteRedo();
            } else {
                ExecuteUndo();
            }
        } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            ExecuteRedo();
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            ExecuteDuplicateSelection();
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
            ExecuteCopySelection();
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
            ExecutePasteSelection();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            ExecuteDeleteSelection();
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

        float speed = m_cameraMoveSpeed * io.DeltaTime;
        if (io.KeyShift) speed *= 3.0f;
        if (io.KeyCtrl) speed *= 0.25f;

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
        const float panSpeed = (m_cameraMoveSpeed * 0.5f) * io.DeltaTime;
        pos -= right * io.MouseDelta.x * panSpeed;
        pos += up * io.MouseDelta.y * panSpeed;
    }

    if (m_sceneViewHovered && io.MouseWheel != 0.0f && !m_gizmoWasUsing) {
        m_editorCameraUserOverride = true;
        pos += forward * (io.MouseWheel * (m_cameraMoveSpeed * 0.5f));
    }

    XMStoreFloat3(&m_editorCameraPosition, pos);
}

void EditorLayer::RenderUI()
{
    // カーネルから移動してきた大枠のUI描画
    DrawDockSpace();
    DrawMenuBar();
    if (m_showMainToolbar) {
        DrawMainToolbar();
    }

    const bool maximizeLeft = (m_maximizedWindow == WindowFocusTarget::Hierarchy);
    const bool maximizeRight = (m_maximizedWindow == WindowFocusTarget::Inspector);
    const bool maximizeBottomAsset = (m_maximizedWindow == WindowFocusTarget::AssetBrowser);
    const bool maximizeBottomConsole = (m_maximizedWindow == WindowFocusTarget::Console);
    const bool maximizeTool = (m_maximizedWindow == WindowFocusTarget::Lighting ||
        m_maximizedWindow == WindowFocusTarget::Audio ||
        m_maximizedWindow == WindowFocusTarget::RenderPasses ||
        m_maximizedWindow == WindowFocusTarget::GridSettings ||
        m_maximizedWindow == WindowFocusTarget::GBufferDebug);

    if (m_showSceneView && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::SceneView)) {
        DrawSceneView();
    }
    if (m_showGameView && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::GameView)) {
        DrawGameView();
    }
    if (m_showHierarchy && (m_maximizedWindow == WindowFocusTarget::None || maximizeLeft)) {
        DrawHierarchy();
    }
    if (m_showInspector && (m_maximizedWindow == WindowFocusTarget::None || maximizeRight)) {
        DrawInspector();
    }

    if (m_showLightingWindow && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::Lighting || maximizeTool)) {
        DrawLightingWindow();
    }
    if (m_showAudioWindow && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::Audio || maximizeTool)) {
        DrawAudioWindow();
    }
    if (m_showRenderPassesWindow && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::RenderPasses || maximizeTool)) {
        DrawRenderPassesWindow();
    }
    if (m_showGridSettingsWindow && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::GridSettings || maximizeTool)) {
        DrawGridSettingsWindow();
    }
    if (m_showGBufferDebug && (m_maximizedWindow == WindowFocusTarget::None || m_maximizedWindow == WindowFocusTarget::GBufferDebug || maximizeTool)) {
        DrawGBufferDebugWindow();
    }
    if (m_showStatusBar) {
        DrawStatusBar();
    }
    DrawUnsavedChangesPopup();
    DrawRecoveryPopup();
    DrawRenamePopup();
    if (m_showConsole && (m_maximizedWindow == WindowFocusTarget::None || maximizeBottomConsole)) {
        bool consoleFocused = false;
        ApplyPendingWindowFocus(WindowFocusTarget::Console);
        Console::Instance().Draw(kConsoleWindowTitle, &m_showConsole, &consoleFocused);
        SetLastFocusedWindow(WindowFocusTarget::Console, consoleFocused);
    }
    if (m_assetBrowser && m_showAssetBrowser && (m_maximizedWindow == WindowFocusTarget::None || maximizeBottomAsset)) {
        m_assetBrowser->SetRegistry(m_gameLayer ? &m_gameLayer->GetRegistry() : nullptr);
        bool assetFocused = false;
        ApplyPendingWindowFocus(WindowFocusTarget::AssetBrowser);
        m_assetBrowser->RenderUI(&m_showAssetBrowser, &assetFocused);
        SetLastFocusedWindow(WindowFocusTarget::AssetBrowser, assetFocused);
    }
}

void EditorLayer::DrawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        Registry* registry = m_gameLayer ? &m_gameLayer->GetRegistry() : nullptr;
        auto& selection = EditorSelection::Instance();
        const bool hasEntitySelection = selection.GetType() == SelectionType::Entity && selection.GetSelectedEntityCount() > 0;
        const bool hasPrimarySelection = hasEntitySelection && !Entity::IsNull(selection.GetPrimaryEntity());
        const bool canPaste = !s_entityClipboard.empty();
        bool canUnpackPrefab = false;
        if (registry && hasPrimarySelection) {
            canUnpackPrefab = !Entity::IsNull(PrefabSystem::FindPrefabRoot(selection.GetPrimaryEntity(), *registry));
        }

        if (ImGui::BeginMenu("File(F)")) {
            if (ImGui::MenuItem(ICON_FA_FILE " New Scene", "Ctrl+N")) {
                m_requestNewScene = true;
            }
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Scene...", "Ctrl+O")) {
                m_requestOpenScene = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save", "Ctrl+S")) {
                SaveCurrentScene();
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save As...")) {
                m_requestSaveSceneAs = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit(E)")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, UndoSystem::Instance().CanUndoECS())) {
                ExecuteUndo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y / Ctrl+Shift+Z", false, UndoSystem::Instance().CanRedoECS())) {
                ExecuteRedo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasEntitySelection)) {
                ExecuteCopySelection();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, canPaste)) {
                ExecutePasteSelection();
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasEntitySelection)) {
                ExecuteDuplicateSelection();
            }
            if (ImGui::MenuItem("Delete", "Delete", false, hasEntitySelection)) {
                ExecuteDeleteSelection();
            }
            if (ImGui::MenuItem("Rename", "F2", false, hasPrimarySelection)) {
                ExecuteRenameSelection();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Frame Selected", "F", false, hasPrimarySelection)) {
                ExecuteFrameSelected();
            }
            if (ImGui::MenuItem("Select All", "Ctrl+A", false, registry != nullptr)) {
                ExecuteSelectAll();
            }
            if (ImGui::MenuItem("Deselect", "Esc", false, selection.GetType() != SelectionType::None)) {
                ExecuteDeselect();
            }
            if (ImGui::MenuItem("Focus Search", "Ctrl+K")) {
                ExecuteFocusSearch();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Transform", nullptr, false, hasPrimarySelection)) {
                ExecuteResetTransform();
            }
            if (ImGui::MenuItem("Create Empty Parent", nullptr, false, hasEntitySelection)) {
                ExecuteCreateEmptyParent();
            }
            if (ImGui::MenuItem("Unpack Prefab", nullptr, false, canUnpackPrefab)) {
                ExecuteUnpackPrefab();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View(V)")) {
            if (ImGui::MenuItem("3D Mode", nullptr, m_sceneViewMode == SceneViewMode::Mode3D)) {
                m_sceneViewMode = SceneViewMode::Mode3D;
            }
            if (ImGui::MenuItem("2D Mode", nullptr, m_sceneViewMode == SceneViewMode::Mode2D)) {
                m_sceneViewMode = SceneViewMode::Mode2D;
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Overlays")) {
                ImGui::MenuItem("Show Grid", nullptr, &m_showSceneGrid);
                ImGui::MenuItem("Show Gizmo", nullptr, &m_showSceneGizmo);
                ImGui::MenuItem("Show Stats Overlay", nullptr, &m_showSceneStatsOverlay);
                ImGui::MenuItem("Show Selection Outline", nullptr, &m_showSceneSelectionOutline);
                ImGui::MenuItem("Show Light Icons", nullptr, &m_showSceneLightIcons);
                ImGui::MenuItem("Show Camera Icons", nullptr, &m_showSceneCameraIcons);
                ImGui::MenuItem("Show Bounds", nullptr, &m_showSceneBounds);
                ImGui::MenuItem("Show Collision", nullptr, &m_showSceneCollision);
                ImGui::MenuItem("Show Safe Area", nullptr, &m_gameViewShowSafeArea);
                ImGui::MenuItem("Show Pixel Preview", nullptr, &m_gameViewShowPixelPreview);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Grid")) {
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::DragFloat("Cell Size", &m_sceneGridCellSize, 0.5f, 1.0f, 500.0f, "%.1f")) {
                    m_sceneGridCellSize = (std::clamp)(m_sceneGridCellSize, 1.0f, 500.0f);
                }
                if (ImGui::SliderInt("Half Line Count", &m_sceneGridHalfLineCount, 4, 128)) {
                    m_sceneGridHalfLineCount = (std::clamp)(m_sceneGridHalfLineCount, 4, 128);
                }
                if (ImGui::MenuItem("Open Grid Settings...")) {
                    m_showGridSettingsWindow = true;
                    RequestWindowFocus(WindowFocusTarget::GridSettings);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Shading")) {
                if (ImGui::MenuItem("Lit", nullptr, m_sceneShadingMode == SceneShadingMode::Lit)) {
                    m_sceneShadingMode = SceneShadingMode::Lit;
                }
                if (ImGui::MenuItem("Unlit", nullptr, m_sceneShadingMode == SceneShadingMode::Unlit)) {
                    m_sceneShadingMode = SceneShadingMode::Unlit;
                }
                if (ImGui::MenuItem("Wireframe", nullptr, m_sceneShadingMode == SceneShadingMode::Wireframe)) {
                    m_sceneShadingMode = SceneShadingMode::Wireframe;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Snap")) {
                ImGui::MenuItem("Translate", nullptr, &m_translateSnapEnabled);
                ImGui::MenuItem("Rotate", nullptr, &m_rotateSnapEnabled);
                ImGui::MenuItem("Scale", nullptr, &m_scaleSnapEnabled);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Camera Speed")) {
                if (ImGui::MenuItem("Slow", nullptr, m_cameraMoveSpeed == 5.0f)) m_cameraMoveSpeed = 5.0f;
                if (ImGui::MenuItem("Normal", nullptr, m_cameraMoveSpeed == 20.0f)) m_cameraMoveSpeed = 20.0f;
                if (ImGui::MenuItem("Fast", nullptr, m_cameraMoveSpeed == 50.0f)) m_cameraMoveSpeed = 50.0f;
                if (ImGui::MenuItem("Very Fast", nullptr, m_cameraMoveSpeed == 100.0f)) m_cameraMoveSpeed = 100.0f;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Bookmarks")) {
                for (size_t i = 0; i < m_cameraBookmarks.size(); ++i) {
                    std::string saveLabel = "Save Slot " + std::to_string(i + 1);
                    std::string recallLabel = "Recall Slot " + std::to_string(i + 1);
                    if (ImGui::MenuItem(saveLabel.c_str())) {
                        SaveCameraBookmark(i);
                    }
                    if (ImGui::MenuItem(recallLabel.c_str(), nullptr, false, m_cameraBookmarks[i].valid)) {
                        LoadCameraBookmark(i);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset View")) {
                ExecuteResetView();
            }
            if (ImGui::MenuItem("Focus Scene View")) {
                RequestWindowFocus(WindowFocusTarget::SceneView);
            }
            if (ImGui::MenuItem("Focus Game View")) {
                RequestWindowFocus(WindowFocusTarget::GameView);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Game(G)")) {
            const EngineMode mode = EngineKernel::Instance().GetMode();
            if (ImGui::MenuItem("Play", nullptr, mode == EngineMode::Play, mode == EngineMode::Editor || mode == EngineMode::Pause)) {
                ExecuteGamePlay();
            }
            if (ImGui::MenuItem("Pause", nullptr, mode == EngineMode::Pause, mode == EngineMode::Play || mode == EngineMode::Pause)) {
                ExecuteGamePauseToggle();
            }
            if (ImGui::MenuItem("Step", nullptr, false, mode == EngineMode::Pause)) {
                ExecuteGameStep();
            }
            if (ImGui::MenuItem("Stop", nullptr, false, mode != EngineMode::Editor)) {
                ExecuteGameStop();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Focus Game View")) {
                RequestWindowFocus(WindowFocusTarget::GameView);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Resolution Preset")) {
                const GameViewResolutionPreset presets[] = {
                    GameViewResolutionPreset::Free,
                    GameViewResolutionPreset::HD1080,
                    GameViewResolutionPreset::HD720,
                    GameViewResolutionPreset::Portrait1080x1920,
                    GameViewResolutionPreset::Portrait750x1334
                };
                for (GameViewResolutionPreset preset : presets) {
                    if (ImGui::MenuItem(GetGameViewPresetLabel(preset), nullptr, m_gameViewResolutionPreset == preset)) {
                        m_gameViewResolutionPreset = preset;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Aspect Policy")) {
                if (ImGui::MenuItem("Fit", nullptr, m_gameViewAspectPolicy == GameViewAspectPolicy::Fit)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::Fit;
                }
                if (ImGui::MenuItem("Fill", nullptr, m_gameViewAspectPolicy == GameViewAspectPolicy::Fill)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::Fill;
                }
                if (ImGui::MenuItem("1:1 Pixel", nullptr, m_gameViewAspectPolicy == GameViewAspectPolicy::PixelPerfect)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::PixelPerfect;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Scale")) {
                if (ImGui::MenuItem("Auto", nullptr, m_gameViewScalePolicy == GameViewScalePolicy::AutoFit)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::AutoFit;
                }
                if (ImGui::MenuItem("1x", nullptr, m_gameViewScalePolicy == GameViewScalePolicy::Scale1x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale1x;
                }
                if (ImGui::MenuItem("2x", nullptr, m_gameViewScalePolicy == GameViewScalePolicy::Scale2x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale2x;
                }
                if (ImGui::MenuItem("3x", nullptr, m_gameViewScalePolicy == GameViewScalePolicy::Scale3x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale3x;
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            ImGui::MenuItem("Safe Area", nullptr, &m_gameViewShowSafeArea);
            ImGui::MenuItem("Pixel Preview", nullptr, &m_gameViewShowPixelPreview);
            ImGui::MenuItem("Stats Overlay", nullptr, &m_gameViewShowStatsOverlay);
            ImGui::MenuItem("UI Overlay", nullptr, &m_gameViewShowUIOverlay);
            ImGui::MenuItem("2D Overlay", nullptr, &m_gameViewShow2DOverlay);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Preview")) {
                ExecuteGameResetPreview();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window(W)")) {
            ImGui::MenuItem(kSceneViewWindowTitle, nullptr, &m_showSceneView);
            ImGui::MenuItem(kGameViewWindowTitle, nullptr, &m_showGameView);
            ImGui::MenuItem(kHierarchyWindowTitle, nullptr, &m_showHierarchy);
            ImGui::MenuItem(kInspectorWindowTitle, nullptr, &m_showInspector);
            ImGui::MenuItem(kAssetBrowserWindowTitle, nullptr, &m_showAssetBrowser);
            ImGui::MenuItem(kConsoleWindowTitle, nullptr, &m_showConsole);
            ImGui::Separator();
            ImGui::MenuItem(ICON_FA_SUN " Lighting Settings", nullptr, &m_showLightingWindow);
            ImGui::MenuItem(kAudioWindowTitle, nullptr, &m_showAudioWindow);
            ImGui::MenuItem("Render Passes", nullptr, &m_showRenderPassesWindow);
            ImGui::MenuItem("Grid Settings", nullptr, &m_showGridSettingsWindow);
            ImGui::MenuItem(ICON_FA_IMAGES " G-Buffer Debug", nullptr, &m_showGBufferDebug);
            ImGui::Separator();
            if (ImGui::MenuItem("Focus Hierarchy")) RequestWindowFocus(WindowFocusTarget::Hierarchy);
            if (ImGui::MenuItem("Focus Inspector")) RequestWindowFocus(WindowFocusTarget::Inspector);
            if (ImGui::MenuItem("Focus Asset Browser")) RequestWindowFocus(WindowFocusTarget::AssetBrowser);
            if (ImGui::MenuItem("Focus Console")) RequestWindowFocus(WindowFocusTarget::Console);
            if (ImGui::MenuItem("Focus Audio", nullptr, false, m_showAudioWindow)) RequestWindowFocus(WindowFocusTarget::Audio);
            if (ImGui::MenuItem("Focus Render Passes", nullptr, false, m_showRenderPassesWindow)) RequestWindowFocus(WindowFocusTarget::RenderPasses);
            if (ImGui::MenuItem("Focus Grid Settings", nullptr, false, m_showGridSettingsWindow)) RequestWindowFocus(WindowFocusTarget::GridSettings);
            ImGui::Separator();
            if (ImGui::MenuItem("Maximize Active Panel", nullptr, false, m_lastFocusedWindow != WindowFocusTarget::None)) {
                ExecuteMaximizeActivePanel();
            }
            if (ImGui::MenuItem("Restore Panel Layout")) {
                ExecuteResetLayout();
            }
            if (ImGui::MenuItem("Close All Secondary Windows")) {
                ExecuteCloseSecondaryWindows();
            }
            ImGui::Separator();
            ImGui::MenuItem("Show Status Bar", nullptr, &m_showStatusBar);
            ImGui::MenuItem("Show Main Toolbar", nullptr, &m_showMainToolbar);
            if (ImGui::MenuItem("Reset Layout")) {
                ExecuteResetLayout();
            }
            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 300.0f);
        if (IsSceneDirty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.30f, 1.0f), ICON_FA_CIRCLE " Unsaved");
            ImGui::SameLine();
        }
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
            ExecuteUndo();
        }
        ImGui::SameLine();
        if (ifm.IconButton(ICON_FA_ARROW_ROTATE_RIGHT, IconSemantic::Default, IconFontSize::Medium, nullptr)) {
            ExecuteRedo();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // 中央: 再生コントロール
        float centerX = ImGui::GetWindowWidth() * 0.5f;
        ImGui::SetCursorPosX(centerX - 58.0f);

        // ★現在のモードをカーネルから取得
        EngineMode mode = EngineKernel::Instance().GetMode();

        // [Play]
        bool isPlaying = (mode == EngineMode::Play);
        ImVec4 playColor = isPlaying ? ImVec4(0.26f, 0.90f, 0.26f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, playColor);
        if (ifm.IconButton(ICON_FA_PLAY, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            ExecuteGamePlay();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // [Pause]
        bool isPaused = (mode == EngineMode::Pause);
        ImVec4 pauseColor = isPaused ? ImVec4(1.00f, 0.60f, 0.00f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, pauseColor);
        if (ifm.IconButton(ICON_FA_PAUSE, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            ExecuteGamePauseToggle();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        const bool canStep = (mode == EngineMode::Pause);
        ImVec4 stepColor = canStep ? ImVec4(0.70f, 0.85f, 1.00f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, stepColor);
        if (!canStep) {
            ImGui::BeginDisabled();
        }
        if (ifm.IconButton(ICON_FA_FORWARD_STEP, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            ExecuteGameStep();
        }
        if (!canStep) {
            ImGui::EndDisabled();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // [Stop]
        bool canStop = (mode != EngineMode::Editor);
        ImVec4 stopColor = canStop ? ImVec4(1.00f, 0.25f, 0.25f, 1.00f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, stopColor);
        if (ifm.IconButton(ICON_FA_SQUARE, IconSemantic::Default, IconFontSize::Medium, nullptr))
        {
            if (canStop) ExecuteGameStop();
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(2);
}

void EditorLayer::DrawStatusBar()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float statusBarHeight = 26.0f;

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - statusBarHeight));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, statusBarHeight));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    if (ImGui::Begin("##EditorStatusBar", nullptr, flags)) {
        const std::string sceneName = std::filesystem::path(m_sceneSavePath).filename().string();
        ImGui::TextDisabled("%s %s", ICON_FA_FILE, sceneName.empty() ? "Untitled.scene" : sceneName.c_str());
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextDisabled("%s %s", ICON_FA_FLOPPY_DISK, IsSceneDirty() ? "Unsaved" : "Saved");
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextDisabled("Speed %.1f", m_cameraMoveSpeed);
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::TextDisabled("Snap T:%s R:%s S:%s",
            m_translateSnapEnabled ? "On" : "Off",
            m_rotateSnapEnabled ? "On" : "Off",
            m_scaleSnapEnabled ? "On" : "Off");
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        auto& selection = EditorSelection::Instance();
        if (selection.GetType() == SelectionType::Entity && selection.GetSelectedEntityCount() > 0) {
            if (selection.GetSelectedEntityCount() == 1) {
                ImGui::TextDisabled("Entity %llu", static_cast<unsigned long long>(selection.GetPrimaryEntity()));
            } else {
                ImGui::TextDisabled("%zu Selected (Primary %llu)",
                    selection.GetSelectedEntityCount(),
                    static_cast<unsigned long long>(selection.GetPrimaryEntity()));
            }
        } else if (selection.GetType() == SelectionType::Asset) {
            ImGui::TextDisabled("%s Asset", ICON_FA_FOLDER_OPEN);
        } else {
            ImGui::TextDisabled("No Selection");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::DrawRenamePopup()
{
    if (m_requestRenamePopup) {
        ImGui::OpenPopup("Rename Entity");
        m_requestRenamePopup = false;
    }

    if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Enter a new name for the selected entity.");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(320.0f);
        ImGui::SetKeyboardFocusHere();
        const bool submit = ImGui::InputText("##RenameEntity", m_renameBuffer, sizeof(m_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        if (submit || ImGui::Button("OK", ImVec2(120, 0))) {
            if (m_gameLayer) {
                Registry& registry = m_gameLayer->GetRegistry();
                const EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
                if (auto* name = registry.GetComponent<NameComponent>(entity)) {
                    NameComponent before = *name;
                    NameComponent after = before;
                    after.name = m_renameBuffer;
                    if (after.name != before.name && !after.name.empty()) {
                        UndoSystem::Instance().ExecuteAction(
                            std::make_unique<ComponentUndoAction<NameComponent>>(entity, before, after),
                            registry);
                        PrefabSystem::MarkPrefabOverride(entity, registry);
                    }
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::DrawSceneGridOverlay(const DirectX::XMFLOAT4& viewRect,
                                       const DirectX::XMFLOAT4X4& view,
                                       const DirectX::XMFLOAT4X4& projection) const
{
    if (viewRect.z <= 1.0f || viewRect.w <= 1.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        const float aspect = viewRect.z / (std::max)(viewRect.w, 1.0f);
        const float worldHalfHeight = m_editor2DZoom;
        const float worldHalfWidth = m_editor2DZoom * aspect;
        const float left = m_editor2DCenter.x - worldHalfWidth;
        const float right = m_editor2DCenter.x + worldHalfWidth;
        const float bottom = m_editor2DCenter.y - worldHalfHeight;
        const float top = m_editor2DCenter.y + worldHalfHeight;

        const float approxStep = (std::max)(1.0f, m_editor2DZoom / 4.0f);
        const float stepBase = std::pow(10.0f, std::floor(std::log10(approxStep)));
        float step = stepBase;
        if (approxStep / stepBase >= 5.0f) step = stepBase * 5.0f;
        else if (approxStep / stepBase >= 2.0f) step = stepBase * 2.0f;

        auto worldToScreen = [&](float x, float y) {
            const float sx = viewRect.x + ((x - left) / (std::max)(right - left, 0.001f)) * viewRect.z;
            const float sy = viewRect.y + (1.0f - ((y - bottom) / (std::max)(top - bottom, 0.001f))) * viewRect.w;
            return ImVec2(sx, sy);
        };

        const ImU32 minorColor = IM_COL32(90, 98, 112, 110);
        const ImU32 axisColor = IM_COL32(140, 170, 220, 180);
        const float startX = std::floor(left / step) * step;
        const float startY = std::floor(bottom / step) * step;
        for (float x = startX; x <= right; x += step) {
            const ImU32 color = std::fabs(x) < 0.001f ? axisColor : minorColor;
            drawList->AddLine(worldToScreen(x, bottom), worldToScreen(x, top), color, std::fabs(x) < 0.001f ? 2.0f : 1.0f);
        }
        for (float y = startY; y <= top; y += step) {
            const ImU32 color = std::fabs(y) < 0.001f ? axisColor : minorColor;
            drawList->AddLine(worldToScreen(left, y), worldToScreen(right, y), color, std::fabs(y) < 0.001f ? 2.0f : 1.0f);
        }
        return;
    }

    const float step = 10.0f;
    const ImU32 minorColor = IM_COL32(90, 98, 112, 96);
    const ImU32 axisXColor = IM_COL32(210, 80, 80, 180);
    const ImU32 axisZColor = IM_COL32(80, 160, 220, 180);
    float minX = 0.0f;
    float maxX = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    bool hasPlaneCoverage = false;
    const std::array<ImVec2, 5> samplePoints = {
        ImVec2(viewRect.x, viewRect.y),
        ImVec2(viewRect.x + viewRect.z, viewRect.y),
        ImVec2(viewRect.x + viewRect.z, viewRect.y + viewRect.w),
        ImVec2(viewRect.x, viewRect.y + viewRect.w),
        ImVec2(viewRect.x + viewRect.z * 0.5f, viewRect.y + viewRect.w * 0.5f)
    };

    for (const ImVec2& samplePoint : samplePoints) {
        DirectX::XMFLOAT3 rayOrigin{};
        DirectX::XMFLOAT3 rayDirection{};
        DirectX::XMFLOAT3 planePoint{};
        if (!BuildWorldRay(viewRect, view, projection, samplePoint, rayOrigin, rayDirection) ||
            !IntersectRayWithGroundPlane(rayOrigin, rayDirection, planePoint)) {
            continue;
        }

        if (!hasPlaneCoverage) {
            minX = maxX = planePoint.x;
            minZ = maxZ = planePoint.z;
            hasPlaneCoverage = true;
        } else {
            minX = (std::min)(minX, planePoint.x);
            maxX = (std::max)(maxX, planePoint.x);
            minZ = (std::min)(minZ, planePoint.z);
            maxZ = (std::max)(maxZ, planePoint.z);
        }
    }

    if (!hasPlaneCoverage) {
        const float fallbackExtent = (std::max)(200.0f, std::fabs(m_editorCameraPosition.y) * 8.0f);
        minX = -fallbackExtent;
        maxX = fallbackExtent;
        minZ = -fallbackExtent;
        maxZ = fallbackExtent;
    } else {
        const float margin = step * 4.0f;
        minX -= margin;
        maxX += margin;
        minZ -= margin;
        maxZ += margin;
        const float minExtent = 120.0f;
        minX = (std::min)(minX, -minExtent);
        maxX = (std::max)(maxX, minExtent);
        minZ = (std::min)(minZ, -minExtent);
        maxZ = (std::max)(maxZ, minExtent);
    }

    minX = std::floor(minX / step) * step;
    maxX = std::ceil(maxX / step) * step;
    minZ = std::floor(minZ / step) * step;
    maxZ = std::ceil(maxZ / step) * step;

    Registry& registry = m_gameLayer->GetRegistry();
    std::vector<GridOccluder> occluders;
    occluders.reserve(32);
    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<MeshComponent>()) ||
            !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
            continue;
        }

        auto* meshColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<MeshComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            const auto* mesh = static_cast<MeshComponent*>(meshColumn->Get(i));
            const auto* transform = static_cast<TransformComponent*>(transformColumn->Get(i));
            if (!mesh || !mesh->model || !mesh->isVisible || !transform) {
                continue;
            }

            DirectX::BoundingBox bounds{};
            if (!TryGetLiveMeshWorldBounds(*mesh, transform, bounds)) {
                continue;
            }

            DirectX::XMFLOAT3 corners[8]{};
            BuildBoundingBoxCorners(bounds, corners);

            ImVec2 minCorner(viewRect.x + viewRect.z, viewRect.y + viewRect.w);
            ImVec2 maxCorner(viewRect.x, viewRect.y);
            float minDepth = 1.0f;
            bool anyProjected = false;
            for (const DirectX::XMFLOAT3& corner : corners) {
                ImVec2 screen{};
                float depth = 1.0f;
                if (!ProjectWorldToSceneScreenDepth(viewRect, view, projection, corner, screen, depth)) {
                    continue;
                }
                anyProjected = true;
                minCorner.x = (std::min)(minCorner.x, screen.x);
                minCorner.y = (std::min)(minCorner.y, screen.y);
                maxCorner.x = (std::max)(maxCorner.x, screen.x);
                maxCorner.y = (std::max)(maxCorner.y, screen.y);
                minDepth = (std::min)(minDepth, depth);
            }

            if (!anyProjected) {
                continue;
            }

            minCorner.x = (std::max)(minCorner.x, viewRect.x);
            minCorner.y = (std::max)(minCorner.y, viewRect.y);
            maxCorner.x = (std::min)(maxCorner.x, viewRect.x + viewRect.z);
            maxCorner.y = (std::min)(maxCorner.y, viewRect.y + viewRect.w);
            if (maxCorner.x <= minCorner.x || maxCorner.y <= minCorner.y) {
                continue;
            }

            occluders.push_back(GridOccluder{ minCorner, maxCorner, minDepth });
        }
    }

    auto drawWorldSegment = [&](const DirectX::XMFLOAT3& a,
                                const DirectX::XMFLOAT3& b,
                                const DirectX::XMFLOAT3& samplePoint,
                                ImU32 color,
                                float thickness) {
        ImVec2 screenA{};
        ImVec2 screenB{};
        ImVec2 sampleScreen{};
        float sampleDepth = 1.0f;
        if (!ProjectWorldToSceneScreen(viewRect, view, projection, a, screenA) ||
            !ProjectWorldToSceneScreen(viewRect, view, projection, b, screenB) ||
            !ProjectWorldToSceneScreenDepth(viewRect, view, projection, samplePoint, sampleScreen, sampleDepth)) {
            return;
        }

        if (IsPointInsideOccluder(sampleScreen, sampleDepth, occluders)) {
            return;
        }

        drawList->AddLine(screenA, screenB, color, thickness);
    };

    for (float x = minX; x <= maxX; x += step) {
        const ImU32 color = std::fabs(x) < 0.001f ? axisXColor : minorColor;
        const float thickness = std::fabs(x) < 0.001f ? 2.0f : 1.0f;
        for (float z = minZ; z < maxZ; z += step) {
            const float nextZ = (std::min)(z + step, maxZ);
            drawWorldSegment({ x, 0.0f, z }, { x, 0.0f, nextZ }, { x, 0.0f, (z + nextZ) * 0.5f }, color, thickness);
        }
    }
    for (float z = minZ; z <= maxZ; z += step) {
        const ImU32 color = std::fabs(z) < 0.001f ? axisZColor : minorColor;
        const float thickness = std::fabs(z) < 0.001f ? 2.0f : 1.0f;
        for (float x = minX; x < maxX; x += step) {
            const float nextX = (std::min)(x + step, maxX);
            drawWorldSegment({ x, 0.0f, z }, { nextX, 0.0f, z }, { (x + nextX) * 0.5f, 0.0f, z }, color, thickness);
        }
    }
}

void EditorLayer::DrawSelectionOutlineOverlay(const DirectX::XMFLOAT4& viewRect,
                                              const DirectX::XMFLOAT4X4& view,
                                              const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    const EntityID entity = EditorSelection::Instance().GetPrimaryEntity();
    if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
        return;
    }

    auto* mesh = registry.GetComponent<MeshComponent>(entity);
    auto* transform = registry.GetComponent<TransformComponent>(entity);
    if (!mesh || !mesh->model || !transform) {
        return;
    }

    DirectX::BoundingBox bounds{};
    if (!TryGetLiveMeshWorldBounds(*mesh, transform, bounds)) {
        return;
    }
    DirectX::XMFLOAT3 corners[8]{};
    BuildBoundingBoxCorners(bounds, corners);
    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 color = IM_COL32(80, 180, 255, 255);
    for (const auto& edge : edges) {
        ImVec2 a{};
        ImVec2 b{};
        if (ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[0]], a) &&
            ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[1]], b)) {
            drawList->AddLine(a, b, color, 2.0f);
        }
    }
}

void EditorLayer::DrawSceneIconOverlay(const DirectX::XMFLOAT4& viewRect,
                                       const DirectX::XMFLOAT4X4& view,
                                       const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const auto drawIcon = [&](const DirectX::XMFLOAT3& worldPos, ImU32 color, const char* text, const std::string& label) {
        ImVec2 screen{};
        if (!ProjectWorldToSceneScreen(viewRect, view, projection, worldPos, screen)) {
            return;
        }
        drawList->AddCircleFilled(screen, 7.0f, color, 12);
        drawList->AddCircle(screen, 7.0f, IM_COL32(12, 16, 24, 220), 12, 1.5f);
        drawList->AddText(ImVec2(screen.x + 12.0f, screen.y - 8.0f), IM_COL32(240, 244, 248, 255), label.c_str());
        drawList->AddText(ImVec2(screen.x - 3.0f, screen.y - 6.0f), IM_COL32(10, 12, 16, 255), text);
    };

    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        auto* transformColumn = signature.test(TypeManager::GetComponentTypeID<TransformComponent>())
            ? archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>())
            : nullptr;
        auto* nameColumn = signature.test(TypeManager::GetComponentTypeID<NameComponent>())
            ? archetype->GetColumn(TypeManager::GetComponentTypeID<NameComponent>())
            : nullptr;
        const auto& entities = archetype->GetEntities();

        if (m_showSceneLightIcons && transformColumn && signature.test(TypeManager::GetComponentTypeID<LightComponent>())) {
            auto* lightColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<LightComponent>());
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                const auto& transform = *static_cast<TransformComponent*>(transformColumn->Get(i));
                const auto& light = *static_cast<LightComponent*>(lightColumn->Get(i));
                const std::string label = nameColumn
                    ? static_cast<NameComponent*>(nameColumn->Get(i))->name
                    : GetEntityLabel(registry, entities[i]);
                const ImU32 color = (light.type == LightType::Directional)
                    ? IM_COL32(255, 210, 90, 220)
                    : IM_COL32(255, 160, 96, 220);
                const char* text = (light.type == LightType::Directional) ? "L" : "P";
                drawIcon(transform.worldPosition, color, text, label);
            }
        }

        if (m_showSceneCameraIcons && transformColumn &&
            (signature.test(TypeManager::GetComponentTypeID<CameraLensComponent>()) ||
             signature.test(TypeManager::GetComponentTypeID<Camera2DComponent>()))) {
            for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                const auto& transform = *static_cast<TransformComponent*>(transformColumn->Get(i));
                const std::string label = nameColumn
                    ? static_cast<NameComponent*>(nameColumn->Get(i))->name
                    : GetEntityLabel(registry, entities[i]);
                drawIcon(transform.worldPosition, IM_COL32(110, 190, 255, 220), "C", label);
            }
        }
    }
}

void EditorLayer::DrawSceneBoundsOverlay(const DirectX::XMFLOAT4& viewRect,
                                         const DirectX::XMFLOAT4X4& view,
                                         const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D) {
        return;
    }

    Registry& registry = m_gameLayer->GetRegistry();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImU32 color = (m_sceneShadingMode == SceneShadingMode::Wireframe)
        ? IM_COL32(120, 220, 255, 180)
        : IM_COL32(120, 180, 255, 120);
    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<MeshComponent>()) ||
            !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
            continue;
        }
        auto* meshColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<MeshComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            const auto* mesh = static_cast<MeshComponent*>(meshColumn->Get(i));
            const auto* transform = static_cast<TransformComponent*>(transformColumn->Get(i));
            if (!mesh || !mesh->model || !mesh->isVisible || !transform) {
                continue;
            }

            DirectX::BoundingBox bounds{};
            if (!TryGetLiveMeshWorldBounds(*mesh, transform, bounds)) {
                continue;
            }
            DirectX::XMFLOAT3 corners[8]{};
            BuildBoundingBoxCorners(bounds, corners);
            for (const auto& edge : edges) {
                ImVec2 a{};
                ImVec2 b{};
                if (ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[0]], a) &&
                    ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[1]], b)) {
                    drawList->AddLine(a, b, color, m_sceneShadingMode == SceneShadingMode::Wireframe ? 1.6f : 1.0f);
                }
            }
        }
    }
}

void EditorLayer::DrawSceneCollisionOverlay(const DirectX::XMFLOAT4& viewRect,
                                            const DirectX::XMFLOAT4X4& view,
                                            const DirectX::XMFLOAT4X4& projection) const
{
    if (!m_gameLayer || m_sceneViewMode == SceneViewMode::Mode2D) {
        return;
    }

    using namespace DirectX;
    Registry& registry = m_gameLayer->GetRegistry();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    static constexpr int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (Archetype* archetype : registry.GetAllArchetypes()) {
        const auto& signature = archetype->GetSignature();
        if (!signature.test(TypeManager::GetComponentTypeID<ColliderComponent>()) ||
            !signature.test(TypeManager::GetComponentTypeID<TransformComponent>())) {
            continue;
        }

        auto* colliderColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<ColliderComponent>());
        auto* transformColumn = archetype->GetColumn(TypeManager::GetComponentTypeID<TransformComponent>());
        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
            const auto& collider = *static_cast<ColliderComponent*>(colliderColumn->Get(i));
            const auto& transform = *static_cast<TransformComponent*>(transformColumn->Get(i));
            if (!collider.enabled) {
                continue;
            }

            const XMMATRIX world = XMLoadFloat4x4(&transform.worldMatrix);
            for (const auto& element : collider.elements) {
                if (!element.enabled) {
                    continue;
                }

                BoundingBox localBounds{};
                switch (element.type) {
                case ColliderShape::Sphere:
                    localBounds.Center = { element.offsetLocal.x, element.offsetLocal.y, element.offsetLocal.z };
                    localBounds.Extents = { element.radius, element.radius, element.radius };
                    break;
                case ColliderShape::Capsule:
                    localBounds.Center = {
                        element.offsetLocal.x,
                        element.offsetLocal.y + element.height * 0.5f,
                        element.offsetLocal.z
                    };
                    localBounds.Extents = {
                        element.radius,
                        element.height * 0.5f + element.radius,
                        element.radius
                    };
                    break;
                case ColliderShape::Box:
                default:
                    localBounds.Center = { element.offsetLocal.x, element.offsetLocal.y, element.offsetLocal.z };
                    localBounds.Extents = { element.size.x * 0.5f, element.size.y * 0.5f, element.size.z * 0.5f };
                    break;
                }

                BoundingBox worldBounds{};
                localBounds.Transform(worldBounds, world);
                XMFLOAT3 corners[8]{};
                BuildBoundingBoxCorners(worldBounds, corners);
                const ImU32 color = IM_COL32(
                    static_cast<int>(std::clamp(element.color.x, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(element.color.y, 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(element.color.z, 0.0f, 1.0f) * 255.0f),
                    190);

                for (const auto& edge : edges) {
                    ImVec2 a{};
                    ImVec2 b{};
                    if (ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[0]], a) &&
                        ProjectWorldToSceneScreen(viewRect, view, projection, corners[edge[1]], b)) {
                        drawList->AddLine(a, b, color, 1.0f);
                    }
                }
            }
        }
    }
}

void EditorLayer::DrawStatsOverlay(const DirectX::XMFLOAT4& viewRect, const char* label) const
{
    if (viewRect.z <= 1.0f || viewRect.w <= 1.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 boxMin(viewRect.x + 10.0f, viewRect.y + 10.0f);
    const ImVec2 boxMax(viewRect.x + 190.0f, viewRect.y + 64.0f);
    drawList->AddRectFilled(boxMin, boxMax, IM_COL32(10, 12, 16, 150), 6.0f);
    drawList->AddRect(boxMin, boxMax, IM_COL32(70, 78, 92, 180), 6.0f);

    const auto& selection = EditorSelection::Instance();
    std::string line1 = std::string(label) + " View  |  " + SceneViewModeToString(m_sceneViewMode);
    if (std::strcmp(label, "Scene") == 0) {
        line1 += "  |  ";
        line1 += SceneShadingModeToString(m_sceneShadingMode);
    }
    std::ostringstream line2;
    line2 << "FPS " << std::fixed << std::setprecision(1) << ImGui::GetIO().Framerate
          << "  |  Frame " << std::setprecision(2) << (1000.0f / (std::max)(ImGui::GetIO().Framerate, 0.01f)) << " ms";
    std::ostringstream line3;
    line3 << "Selected " << selection.GetSelectedEntityCount() << "  |  Cam " << std::setprecision(1) << m_cameraMoveSpeed;

    drawList->AddText(ImVec2(boxMin.x + 10.0f, boxMin.y + 8.0f), IM_COL32(230, 235, 245, 255), line1.c_str());
    drawList->AddText(ImVec2(boxMin.x + 10.0f, boxMin.y + 24.0f), IM_COL32(210, 216, 226, 240), line2.str().c_str());
    drawList->AddText(ImVec2(boxMin.x + 10.0f, boxMin.y + 40.0f), IM_COL32(210, 216, 226, 240), line3.str().c_str());
}

void EditorLayer::DrawUnsavedChangesPopup()
{
    if (m_openUnsavedChangesPopup) {
        ImGui::OpenPopup("Unsaved Scene Changes");
        m_openUnsavedChangesPopup = false;
    }

    if (ImGui::BeginPopupModal("Unsaved Scene Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s Current scene has unsaved changes.", ICON_FA_TRIANGLE_EXCLAMATION);
        ImGui::Text("Save before continuing?");
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            bool saved = SaveCurrentScene();
            if (!saved) {
                saved = SaveCurrentSceneAs();
            }
            if (saved) {
                ExecutePendingSceneAction();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
            ExecutePendingSceneAction();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_pendingSceneAction = PendingSceneAction::None;
            m_pendingSceneLoadPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::DrawRecoveryPopup()
{
    if (m_openRecoveryPopup) {
        ImGui::OpenPopup("Autosave Recovery");
        m_openRecoveryPopup = false;
    }

    if (ImGui::BeginPopupModal("Autosave Recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s A newer autosave was found for this scene.", ICON_FA_TRIANGLE_EXCLAMATION);
        if (!m_pendingRecoveryScenePath.empty()) {
            ImGui::TextWrapped("Scene: %s", m_pendingRecoveryScenePath.string().c_str());
        }
        if (!m_pendingRecoveryAutosavePath.empty()) {
            ImGui::TextWrapped("Autosave: %s", m_pendingRecoveryAutosavePath.string().c_str());
        }
        ImGui::Separator();

        if (ImGui::Button("Recover", ImVec2(140, 0))) {
            const std::filesystem::path autosavePath = m_pendingRecoveryAutosavePath;
            const std::filesystem::path scenePath = m_pendingRecoveryScenePath;
            const bool recovered = !autosavePath.empty() && LoadSceneFromPath(autosavePath);
            if (recovered) {
                m_sceneSavePath = scenePath.empty() ? autosavePath.string() : scenePath.string();
                const uint64_t revision = UndoSystem::Instance().GetECSRevision();
                m_savedSceneRevision = (revision == (std::numeric_limits<uint64_t>::max)()) ? revision : revision + 1;
                LOG_INFO("[Editor] Recovered autosave: %s", autosavePath.string().c_str());
            }
            m_pendingRecoveryAutosavePath.clear();
            m_pendingRecoveryScenePath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(140, 0))) {
            m_pendingRecoveryAutosavePath.clear();
            m_pendingRecoveryScenePath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::DrawDockSpace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float toolbarHeight = m_showMainToolbar ? 32.0f : 0.0f;
    const float statusBarHeight = m_showStatusBar ? 26.0f : 0.0f;
    const ImVec2 dockspacePos(viewport->WorkPos.x, viewport->WorkPos.y + toolbarHeight);
    const ImVec2 dockspaceSize(
        viewport->WorkSize.x,
        (std::max)(1.0f, viewport->WorkSize.y - toolbarHeight - statusBarHeight));

    ImGui::SetNextWindowPos(dockspacePos);
    ImGui::SetNextWindowSize(dockspaceSize);

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

    static int s_layoutVersionApplied = 0;
    const int kDockLayoutVersion = 3 + static_cast<int>(m_maximizedWindow) * 100;
    if (m_forceDockLayoutReset) {
        s_layoutVersionApplied = 0;
        m_forceDockLayoutReset = false;
    }
    if (s_layoutVersionApplied != kDockLayoutVersion)
    {
        s_layoutVersionApplied = kDockLayoutVersion;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, dockspaceSize);

        if (m_maximizedWindow != WindowFocusTarget::None) {
            ImGui::DockBuilderDockWindow(GetWindowTitleForFocus(m_maximizedWindow), dockspace_id);
            ImGui::DockBuilderFinish(dockspace_id);
            ImGui::End();
            return;
        }

        // 画面を分割していく
        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
        ImGuiID dock_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

        // 各ウィンドウを分割したエリアにはめ込む
        ImGui::DockBuilderDockWindow(kHierarchyWindowTitle, dock_left);
        ImGui::DockBuilderDockWindow(kInspectorWindowTitle, dock_right);

        ImGui::DockBuilderDockWindow(kConsoleWindowTitle, dock_down);
        ImGui::DockBuilderDockWindow(kAssetBrowserWindowTitle, dock_down);

        ImGui::DockBuilderDockWindow(kSceneViewWindowTitle, dock_main_id);
        ImGui::DockBuilderDockWindow(kGameViewWindowTitle, dock_main_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }



    ImGui::End();
}

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
    if (m_sceneViewMode == SceneViewMode::Mode2D) {
        const XMVECTOR eye = XMVectorSet(m_editor2DCenter.x, m_editor2DCenter.y, -100.0f, 1.0f);
        const XMMATRIX view = XMMatrixLookToLH(eye, XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0));
        DirectX::XMFLOAT4X4 out{};
        XMStoreFloat4x4(&out, view);
        return out;
    }
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
        RequestSceneAction(PendingSceneAction::NewScene);
    }
    if (requestOpenScene) {
        RequestSceneAction(PendingSceneAction::OpenSceneDialog);
    }
    if (requestSaveSceneAs) {
        SaveCurrentSceneAs();
    }
    if (hasPendingSceneFromAssetBrowser) {
        RequestSceneAction(PendingSceneAction::LoadScenePath, pendingScenePath);
    }
}

void EditorLayer::UpdateAutosave(float deltaSeconds)
{
    if (!m_gameLayer || deltaSeconds <= 0.0f) {
        return;
    }

    if (!IsSceneDirty()) {
        m_autosaveAccumulator = 0.0;
        return;
    }

    m_autosaveAccumulator += static_cast<double>(deltaSeconds);
    if (m_autosaveAccumulator < kAutosaveIntervalSeconds) {
        return;
    }

    const std::filesystem::path scenePath = SanitizeSceneDefaultPath(m_sceneSavePath);
    const std::filesystem::path autosavePath = BuildAutosaveScenePath(scenePath);
    std::error_code ec;
    std::filesystem::create_directories(autosavePath.parent_path(), ec);
    if (ec) {
        LOG_WARN("[Editor] Failed to create autosave directory: %s", autosavePath.parent_path().string().c_str());
        m_autosaveAccumulator = 0.0;
        return;
    }

    const SceneFileMetadata metadata = BuildSceneMetadata(m_sceneViewMode);
    if (PrefabSystem::SaveRegistryAsScene(m_gameLayer->GetRegistry(), autosavePath, &metadata)) {
        TrimAutosaveGenerations(autosavePath.parent_path());
        LOG_INFO("[Editor] Autosaved scene: %s", autosavePath.string().c_str());
    } else {
        LOG_WARN("[Editor] Failed to autosave scene: %s", autosavePath.string().c_str());
    }

    m_autosaveAccumulator = 0.0;
}

void EditorLayer::CheckRecoveryCandidate()
{
    if (m_hasCheckedRecovery) {
        return;
    }
    m_hasCheckedRecovery = true;

    const std::filesystem::path scenePath = SanitizeSceneDefaultPath(m_sceneSavePath);
    const std::filesystem::path autosavePath = FindLatestAutosaveForScene(scenePath);
    if (autosavePath.empty() || !IsAutosaveNewerThanScene(autosavePath, scenePath)) {
        return;
    }

    m_pendingRecoveryAutosavePath = autosavePath;
    m_pendingRecoveryScenePath = scenePath;
    m_openRecoveryPopup = true;
}

bool EditorLayer::IsSceneDirty() const
{
    return UndoSystem::Instance().GetECSRevision() != m_savedSceneRevision;
}

void EditorLayer::MarkSceneSaved()
{
    m_savedSceneRevision = UndoSystem::Instance().GetECSRevision();
}

void EditorLayer::RequestSceneAction(PendingSceneAction action, std::filesystem::path scenePath)
{
    if (action == PendingSceneAction::None) {
        return;
    }

    m_pendingSceneAction = action;
    m_pendingSceneLoadPath = std::move(scenePath);

    if (IsSceneDirty()) {
        m_openUnsavedChangesPopup = true;
        return;
    }

    ExecutePendingSceneAction();
}

bool EditorLayer::ExecutePendingSceneAction()
{
    const PendingSceneAction action = m_pendingSceneAction;
    const std::filesystem::path scenePath = m_pendingSceneLoadPath;
    m_pendingSceneAction = PendingSceneAction::None;
    m_pendingSceneLoadPath.clear();

    switch (action) {
    case PendingSceneAction::NewScene:
        NewScene();
        return true;
    case PendingSceneAction::OpenSceneDialog:
        return OpenScene();
    case PendingSceneAction::LoadScenePath:
        return !scenePath.empty() && LoadSceneFromPath(scenePath);
    default:
        return false;
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
        if (!Entity::IsNull(actionPtr->GetLiveRoot())) {
            EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
        }
    } else if (m_sceneViewMode == SceneViewMode::Mode2D && IsSupportedFontAsset(assetPath)) {
        EntitySnapshot::Snapshot snapshot = BuildSingleTextSnapshot(
            assetPath.stem().string(),
            assetPath.string(),
            { placementPosition.x, placementPosition.y });
        auto action = std::make_unique<CreateEntityAction>(std::move(snapshot), Entity::NULL_ID, "Place Text");
        auto* actionPtr = action.get();
        UndoSystem::Instance().ExecuteAction(std::move(action), registry);
        if (!Entity::IsNull(actionPtr->GetLiveRoot())) {
            EditorSelection::Instance().SelectEntity(actionPtr->GetLiveRoot());
        }
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
    m_pendingRecoveryAutosavePath.clear();
    m_pendingRecoveryScenePath.clear();
    m_openRecoveryPopup = false;
    m_autosaveAccumulator = 0.0;
    m_hasCheckedRecovery = true;

    ClearRegistryEntities(registry);
    CreateDefaultSceneEntities(registry);

    m_sceneViewMode = SceneViewMode::Mode3D;
    m_sceneSavePath = kDefaultSceneSavePath;
    MarkSceneSaved();
    LOG_INFO("[Editor] New scene created.");
}

bool EditorLayer::LoadSceneFromPath(const std::filesystem::path& scenePath)
{
    if (!m_gameLayer) {
        return false;
    }

    EngineKernel::Instance().ResetRenderStateForSceneChange();
    SceneFileMetadata metadata;
    if (!PrefabSystem::LoadSceneIntoRegistry(scenePath, m_gameLayer->GetRegistry(), &metadata)) {
        LOG_WARN("[Editor] Failed to load scene: %s", scenePath.string().c_str());
        return false;
    }

    UndoSystem::Instance().ClearECSHistory();
    EditorSelection::Instance().Clear();
    m_sceneSavePath = scenePath.string();
    m_pendingRecoveryAutosavePath.clear();
    m_pendingRecoveryScenePath.clear();
    m_openRecoveryPopup = false;
    m_autosaveAccumulator = 0.0;
    m_hasCheckedRecovery = true;
    m_sceneViewMode = SceneViewModeFromString(metadata.sceneViewMode);
    MarkSceneSaved();
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
    const SceneFileMetadata metadata = BuildSceneMetadata(m_sceneViewMode);
    const bool success = PrefabSystem::SaveRegistryAsScene(m_gameLayer->GetRegistry(), scenePath, &metadata);
    if (success) {
        LOG_INFO("[Editor] Scene saved: %s", scenePath.string().c_str());
        m_sceneSavePath = scenePath.string();
        m_pendingRecoveryAutosavePath.clear();
        m_pendingRecoveryScenePath.clear();
        m_autosaveAccumulator = 0.0;
        MarkSceneSaved();
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

    ApplyPendingWindowFocus(WindowFocusTarget::GameView);
    if (ImGui::Begin(kGameViewWindowTitle, &m_showGameView, window_flags))
    {
        SetLastFocusedWindow(WindowFocusTarget::GameView, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
        if (ImGui::BeginChild("##GameViewToolbar", ImVec2(0.0f, 34.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            auto tooltip = [](const char* text) {
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    ImGui::SetTooltip("%s", text);
                }
            };

            auto drawMenuLikeToggle = [&](const char* label, bool* value, const char* help) {
                if (*value) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.21f, 0.47f, 0.82f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.53f, 0.90f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
                }
                if (ImGui::Button(label)) {
                    *value = !*value;
                }
                tooltip(help);
                if (*value) {
                    ImGui::PopStyleColor(3);
                }
            };

            if (ImGui::BeginCombo("##GameViewResolutionPreset", GetGameViewPresetLabel(m_gameViewResolutionPreset)))
            {
                const GameViewResolutionPreset presets[] = {
                    GameViewResolutionPreset::Free,
                    GameViewResolutionPreset::HD1080,
                    GameViewResolutionPreset::HD720,
                    GameViewResolutionPreset::Portrait1080x1920,
                    GameViewResolutionPreset::Portrait750x1334
                };
                for (GameViewResolutionPreset preset : presets) {
                    const bool selected = (m_gameViewResolutionPreset == preset);
                    if (ImGui::Selectable(GetGameViewPresetLabel(preset), selected)) {
                        m_gameViewResolutionPreset = preset;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            tooltip("Resolution preset");
            ImGui::SameLine();

            if (ImGui::BeginCombo("##GameViewAspectPolicy", GetGameViewAspectPolicyLabel(m_gameViewAspectPolicy))) {
                if (ImGui::Selectable("Fit", m_gameViewAspectPolicy == GameViewAspectPolicy::Fit)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::Fit;
                }
                if (ImGui::Selectable("Fill", m_gameViewAspectPolicy == GameViewAspectPolicy::Fill)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::Fill;
                }
                if (ImGui::Selectable("1:1 Pixel", m_gameViewAspectPolicy == GameViewAspectPolicy::PixelPerfect)) {
                    m_gameViewAspectPolicy = GameViewAspectPolicy::PixelPerfect;
                }
                ImGui::EndCombo();
            }
            tooltip("Aspect policy");
            ImGui::SameLine();

            if (ImGui::BeginCombo("##GameViewScalePolicy", GetGameViewScalePolicyLabel(m_gameViewScalePolicy))) {
                if (ImGui::Selectable("Auto", m_gameViewScalePolicy == GameViewScalePolicy::AutoFit)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::AutoFit;
                }
                if (ImGui::Selectable("1x", m_gameViewScalePolicy == GameViewScalePolicy::Scale1x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale1x;
                }
                if (ImGui::Selectable("2x", m_gameViewScalePolicy == GameViewScalePolicy::Scale2x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale2x;
                }
                if (ImGui::Selectable("3x", m_gameViewScalePolicy == GameViewScalePolicy::Scale3x)) {
                    m_gameViewScalePolicy = GameViewScalePolicy::Scale3x;
                }
                ImGui::EndCombo();
            }
            tooltip("Preview scale");
            ImGui::SameLine(0.0f, 8.0f);

            drawMenuLikeToggle("Safe", &m_gameViewShowSafeArea, "Show safe area");
            ImGui::SameLine();
            drawMenuLikeToggle("Pixel", &m_gameViewShowPixelPreview, "Show pixel preview");
            ImGui::SameLine();
            drawMenuLikeToggle("Stats", &m_gameViewShowStatsOverlay, "Show stats overlay");
            ImGui::SameLine();
            drawMenuLikeToggle("UI", &m_gameViewShowUIOverlay, "Show UI overlay");
            ImGui::SameLine();
            drawMenuLikeToggle("2D", &m_gameViewShow2DOverlay, "Show 2D overlay");
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                ExecuteGameResetPreview();
            }
            tooltip("Reset preview");

            if (m_gameLayer) {
                Registry& registry = m_gameLayer->GetRegistry();
                std::string cameraLabel = "No Camera";
                if (m_sceneViewMode == SceneViewMode::Mode2D) {
                    for (Archetype* archetype : registry.GetAllArchetypes()) {
                        if (!archetype->GetSignature().test(TypeManager::GetComponentTypeID<Camera2DComponent>())) {
                            continue;
                        }
                        const auto& entities = archetype->GetEntities();
                        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                            cameraLabel = "2D";
                            if (auto* name = registry.GetComponent<NameComponent>(entities[i])) {
                                cameraLabel = name->name;
                            }
                            break;
                        }
                        break;
                    }
                } else {
                    for (Archetype* archetype : registry.GetAllArchetypes()) {
                        const Signature signature = archetype->GetSignature();
                        if (!signature.test(TypeManager::GetComponentTypeID<CameraMainTagComponent>()) &&
                            !signature.test(TypeManager::GetComponentTypeID<CameraLensComponent>())) {
                            continue;
                        }
                        const auto& entities = archetype->GetEntities();
                        for (size_t i = 0; i < archetype->GetEntityCount(); ++i) {
                            cameraLabel = "Camera";
                            if (auto* name = registry.GetComponent<NameComponent>(entities[i])) {
                                cameraLabel = name->name;
                            }
                            break;
                        }
                        break;
                    }
                }
                ImGui::SameLine(0.0f, 8.0f);
                ImGui::TextDisabled("%s", cameraLabel.c_str());
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);

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

        ITexture* gameTexture = m_gameViewTexture ? m_gameViewTexture : m_sceneViewTexture;
        void* gameTextureId = nullptr;
        if (gameTexture) {
            gameTextureId = ImGuiRenderer::GetTextureID(gameTexture);
        } else if (Graphics::Instance().GetAPI() != GraphicsAPI::DX12) {
            Graphics& graphics = Graphics::Instance();
            FrameBuffer* gameViewBuffer = graphics.GetFrameBuffer(FrameBufferId::Scene);
            if ((!gameViewBuffer || !gameViewBuffer->GetColorTexture(0))) {
                gameViewBuffer = graphics.GetFrameBuffer(FrameBufferId::Display);
            }
            if (gameViewBuffer && gameViewBuffer->GetColorTexture(0)) {
                gameTextureId = gameViewBuffer->GetImGuiTextureID();
                gameTexture = gameViewBuffer->GetColorTexture(0);
            }
        }

        const DirectX::XMUINT2 presetResolution = GetGameViewPresetResolution(m_gameViewResolutionPreset);
        ImVec2 imageSize = viewportSize;
        if (presetResolution.x > 0 && presetResolution.y > 0) {
            const float presetAspect = static_cast<float>(presetResolution.x) / static_cast<float>(presetResolution.y);
            if (m_gameViewScalePolicy == GameViewScalePolicy::AutoFit) {
                switch (m_gameViewAspectPolicy) {
                case GameViewAspectPolicy::Fill:
                    imageSize = FillSizeKeepingAspect(viewportSize, presetAspect);
                    break;
                case GameViewAspectPolicy::PixelPerfect:
                    imageSize = ImVec2(static_cast<float>(presetResolution.x), static_cast<float>(presetResolution.y));
                    if (imageSize.x > viewportSize.x || imageSize.y > viewportSize.y) {
                        imageSize = FitSizeKeepingAspect(viewportSize, presetAspect);
                    }
                    break;
                case GameViewAspectPolicy::Fit:
                default:
                    imageSize = FitSizeKeepingAspect(viewportSize, presetAspect);
                    break;
                }
            } else {
                float scale = 1.0f;
                if (m_gameViewScalePolicy == GameViewScalePolicy::Scale2x) scale = 2.0f;
                else if (m_gameViewScalePolicy == GameViewScalePolicy::Scale3x) scale = 3.0f;
                imageSize = ImVec2(static_cast<float>(presetResolution.x) * scale, static_cast<float>(presetResolution.y) * scale);
                if (imageSize.x > viewportSize.x || imageSize.y > viewportSize.y) {
                    imageSize = FitSizeKeepingAspect(viewportSize, presetAspect);
                }
            }
        }
        const ImVec2 imageCursor(
            ImGui::GetCursorPosX() + (viewportSize.x - imageSize.x) * 0.5f,
            ImGui::GetCursorPosY() + (viewportSize.y - imageSize.y) * 0.5f);
        if (imageSize.x > 0.0f && imageSize.y > 0.0f) {
            ImGui::SetCursorPos(imageCursor);
        }

        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
        m_gameViewRect = { imageMin.x, imageMin.y, imageSize.x, imageSize.y };

        if (gameTextureId) {
            ImGui::Image((ImTextureID)gameTextureId, imageSize);
        } else {
            ImGui::Dummy(imageSize);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(ImVec2(imageMin.x, imageMin.y), ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), IM_COL32(80, 80, 80, 255));
            drawList->AddText(ImVec2(imageMin.x + 16.0f, imageMin.y + 16.0f), IM_COL32(220, 220, 220, 255), "No preview texture");
        }

        if (m_sceneViewMode == SceneViewMode::Mode2D) {
            DirectX::XMFLOAT4X4 gameView{};
            DirectX::XMFLOAT4X4 gameProjection{};
            if (TryBuildGameView2DViewProjection(gameView, gameProjection)) {
                if (m_gameViewShowUIOverlay) {
                    Draw2DOverlayForRect(m_gameViewRect, gameView, gameProjection, false);
                }
                if (m_gameViewShow2DOverlay) {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddText(ImVec2(imageMin.x + 12.0f, imageMin.y + 12.0f), IM_COL32(190, 220, 255, 220), "2D Preview");
                }
            } else if (m_gameViewShow2DOverlay) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddText(ImVec2(imageMin.x + 16.0f, imageMin.y + 16.0f), IM_COL32(255, 220, 160, 255), "No active 2D camera");
            }
        }

        if (m_gameViewShowSafeArea && imageSize.x > 8.0f && imageSize.y > 8.0f) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float insetX = imageSize.x * 0.05f;
            const float insetY = imageSize.y * 0.05f;
            const ImVec2 safeMin(imageMin.x + insetX, imageMin.y + insetY);
            const ImVec2 safeMax(imageMin.x + imageSize.x - insetX, imageMin.y + imageSize.y - insetY);
            drawList->AddRect(safeMin, safeMax, IM_COL32(255, 214, 102, 220), 0.0f, 0, 2.0f);
            drawList->AddText(ImVec2(safeMin.x + 6.0f, safeMin.y + 6.0f), IM_COL32(255, 214, 102, 230), "Safe Area");
        }

        if (m_gameViewShowPixelPreview && presetResolution.x > 0 && presetResolution.y > 0) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            std::string label = std::string("Preview ") + GetGameViewPresetLabel(m_gameViewResolutionPreset);
            drawList->AddText(ImVec2(imageMin.x + 8.0f, imageMin.y + imageSize.y - 22.0f), IM_COL32(200, 220, 255, 220), label.c_str());
        }

        if (m_gameViewShowStatsOverlay) {
            DrawStatsOverlay(m_gameViewRect, "Game");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorLayer::DrawHierarchy()
{
    if (!m_gameLayer) return;
    bool focused = false;
    ApplyPendingWindowFocus(WindowFocusTarget::Hierarchy);
    HierarchyECSUI::Render(&m_gameLayer->GetRegistry(), &m_showHierarchy, &focused);
    SetLastFocusedWindow(WindowFocusTarget::Hierarchy, focused);
}

void EditorLayer::DrawInspector()
{
    if (!m_gameLayer) return;  // InspectorECSUI::Render が Begin/End を管理
    bool focused = false;
    ApplyPendingWindowFocus(WindowFocusTarget::Inspector);
    InspectorECSUI::Render(&m_gameLayer->GetRegistry(), &m_showInspector, &focused);
    SetLastFocusedWindow(WindowFocusTarget::Inspector, focused);
}

void EditorLayer::DrawLightingWindow()
{
    if (!m_showLightingWindow) return;

    // ウィンドウを描画
    ApplyPendingWindowFocus(WindowFocusTarget::Lighting);
    if (ImGui::Begin(kLightingWindowTitle, &m_showLightingWindow))
    {
        SetLastFocusedWindow(WindowFocusTarget::Lighting, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
        // 1. Environment の自動生成UI
        auto& env = GetOrCreateEnvironmentComponent(m_gameLayer->GetRegistry());
        DrawGlobalSettingsUI(env);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader(ICON_FA_CUBE " Reflection Probe", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& probe = GetOrCreateReflectionProbeComponent(m_gameLayer->GetRegistry());
            bool changed = false;

            if (ImGui::BeginTable("ReflectionProbeTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("position");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.0f);
                changed |= ImGui::DragFloat3("##ReflectionProbePosition", &probe.position.x, 0.05f);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("radius");
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.0f);
                changed |= ImGui::DragFloat("##ReflectionProbeRadius", &probe.radius, 0.1f, 0.1f, 10000.0f, "%.2f");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("status");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(probe.cubemapTexture ? "Cubemap Ready" : "No Cubemap");

                ImGui::EndTable();
            }

            if (changed) {
                probe.needsBake = true;
            }

            if (ImGui::Button(ICON_FA_ROTATE " Rebake Probe")) {
                probe.needsBake = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Dirty: %s", probe.needsBake ? "Yes" : "No");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // 2. PostEffect の自動生成UI
        auto& post = m_gameLayer->GetPostEffect();
        DrawGlobalSettingsUI(post);
    }
    ImGui::End();
}

void EditorLayer::DrawAudioWindow()
{
    if (!m_showAudioWindow || !m_gameLayer) {
        return;
    }

    ApplyPendingWindowFocus(WindowFocusTarget::Audio);
    if (!ImGui::Begin(kAudioWindowTitle, &m_showAudioWindow)) {
        ImGui::End();
        return;
    }

    SetLastFocusedWindow(WindowFocusTarget::Audio, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

    Registry& registry = m_gameLayer->GetRegistry();
    AudioSettingsComponent& settings = GetOrCreateAudioSettingsComponent(registry);
    auto& audio = EngineKernel::Instance().GetAudioWorld();
    const EntityID listenerEntity = audio.GetActiveListenerEntity();
    const std::string previewPath = audio.GetPreviewClipPath();

    auto voices = audio.GetDebugVoices();
    auto buses = audio.GetDebugBuses();
    auto cachedClips = audio.GetCachedClips();

    if (ImGui::BeginTabBar("AudioDebugTabs")) {
        if (ImGui::BeginTabItem("Mixer")) {
            DrawGlobalSettingsUI(settings);

            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_STOP " Stop All Voices")) {
                audio.StopAllVoices();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_TRASH " Clear Clip Cache")) {
                audio.ClearClipCache();
                cachedClips.clear();
            }
            if (!previewPath.empty()) {
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_VOLUME_XMARK " Stop Preview")) {
                    audio.StopPreview();
                }
            }

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::TextDisabled("Active Listener:");
            ImGui::SameLine();
            if (Entity::IsNull(listenerEntity)) {
                ImGui::TextUnformatted("None");
            } else {
                const std::string listenerLabel = GetEntityLabel(registry, listenerEntity);
                ImGui::TextWrapped("%s", listenerLabel.c_str());
            }

            ImGui::TextDisabled("Preview:");
            ImGui::SameLine();
            if (previewPath.empty()) {
                ImGui::TextUnformatted("None");
            } else {
                const std::string previewName = std::filesystem::path(previewPath).filename().string();
                ImGui::TextWrapped("%s", previewName.c_str());
            }

            int streamingVoiceCount = 0;
            for (const auto& bus : buses) {
                streamingVoiceCount += bus.streamingVoiceCount;
            }
            ImGui::TextDisabled("Voices: %d  |  Streams: %d  |  Cache: %zu", static_cast<int>(voices.size()), streamingVoiceCount, cachedClips.size());

            if (ImGui::BeginTable("AudioBusTable", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Bus", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Effective", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Voices", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Streams", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Mute", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupColumn("Solo", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableHeadersRow();

                const auto activeSoloBus = audio.GetSoloBus();
                for (const auto& bus : buses) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(GetAudioBusTypeLabel(bus.bus));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f", bus.baseVolume);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.2f", bus.effectiveVolume);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%d", bus.activeVoiceCount);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%d", bus.streamingVoiceCount);
                    ImGui::TableSetColumnIndex(5);
                    bool muted = bus.muted;
                    std::string muteId = std::string("##MuteBus") + GetAudioBusTypeLabel(bus.bus);
                    if (ImGui::Checkbox(muteId.c_str(), &muted)) {
                        audio.SetBusMuted(bus.bus, muted);
                    }
                    ImGui::TableSetColumnIndex(6);
                    bool solo = activeSoloBus.has_value() && *activeSoloBus == bus.bus;
                    std::string soloId = std::string("##SoloBus") + GetAudioBusTypeLabel(bus.bus);
                    if (ImGui::Checkbox(soloId.c_str(), &solo)) {
                        audio.SetSoloBus(solo ? std::optional<AudioBusType>(bus.bus) : std::nullopt);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Voices")) {
            ImGui::TextDisabled("Active Voices");
            if (voices.empty()) {
                ImGui::TextDisabled("No active voices.");
            } else if (ImGui::BeginTable("AudioVoices", 10, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 300.0f))) {
                ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Clip");
                ImGui::TableSetupColumn("Bus", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("3D", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed, 45.0f);
                ImGui::TableSetupColumn("Vol", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableSetupColumn("Pitch", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();

                for (const auto& voice : voices) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%llu", static_cast<unsigned long long>(voice.handle));
                    ImGui::TableSetColumnIndex(1);
                    const std::string clipName = std::filesystem::path(voice.clipPath).filename().string();
                    ImGui::TextWrapped("%s", clipName.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(GetAudioBusTypeLabel(voice.bus));
                    ImGui::TableSetColumnIndex(3);
                    if (Entity::IsNull(voice.entity)) {
                        ImGui::TextDisabled("Transient");
                    } else {
                        const std::string entityLabel = GetEntityLabel(registry, voice.entity);
                        ImGui::TextWrapped("%s", entityLabel.c_str());
                    }
                    ImGui::TableSetColumnIndex(4);
                    if (voice.preview) {
                        ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "Preview");
                    } else if (voice.playing) {
                        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Playing");
                    } else {
                        ImGui::TextDisabled("Stopped");
                    }
                    ImGui::TableSetColumnIndex(5);
                    ImGui::TextUnformatted(voice.is3D ? "Yes" : "No");
                    ImGui::TableSetColumnIndex(6);
                    ImGui::TextUnformatted(voice.loop ? "Yes" : "No");
                    ImGui::TableSetColumnIndex(7);
                    ImGui::Text("%.2f", voice.volume);
                    ImGui::TableSetColumnIndex(8);
                    ImGui::Text("%.2f", voice.pitch);
                    ImGui::TableSetColumnIndex(9);
                    ImGui::Text("%.2f / %.2f", voice.cursorSeconds, voice.lengthSeconds);
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Cache")) {
            ImGui::TextDisabled("Cached Audio Clips: %zu", cachedClips.size());
            if (cachedClips.empty()) {
                ImGui::TextDisabled("No cached clip metadata.");
            } else if (ImGui::BeginTable("AudioClipCache", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 260.0f))) {
                ImGui::TableSetupColumn("Clip");
                ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Sample Rate", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Streaming", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Size KB", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                for (const auto& clip : cachedClips) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextWrapped("%s", std::filesystem::path(clip.sourcePath).filename().string().c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f", clip.lengthSec);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%u", clip.channelCount);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%u", clip.sampleRate);
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(clip.streaming ? "Yes" : "No");
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%.1f", static_cast<double>(clip.fileSizeBytes) / 1024.0);
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void EditorLayer::DrawRenderPassesWindow()
{
    if (!m_showRenderPassesWindow || !m_gameLayer) {
        return;
    }

    auto resetPassDefaults = [](PostEffectComponent& post) {
        post.enableComputeCulling = true;
        post.enableAsyncCompute = true;
        post.enableGTAO = true;
        post.enableSSGI = false;
        post.enableVolumetricFog = false;
        post.enableSSR = false;
        post.enableBloom = true;
        post.enableColorFilter = true;
        post.enableDoF = false;
        post.enableMotionBlur = true;
    };

    auto disableOptionalPasses = [](PostEffectComponent& post) {
        post.enableComputeCulling = false;
        post.enableAsyncCompute = false;
        post.enableGTAO = false;
        post.enableSSGI = false;
        post.enableVolumetricFog = false;
        post.enableSSR = false;
        post.enableBloom = false;
        post.enableColorFilter = false;
        post.enableDoF = false;
        post.enableMotionBlur = false;
    };

    PostEffectComponent& post = m_gameLayer->GetPostEffect();

    ApplyPendingWindowFocus(WindowFocusTarget::RenderPasses);
    if (ImGui::Begin(kRenderPassesWindowTitle, &m_showRenderPassesWindow)) {
        SetLastFocusedWindow(WindowFocusTarget::RenderPasses, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

        ImGui::TextDisabled("Heavy render features");
        ImGui::Separator();

        if (ImGui::BeginTable("RenderPassToggleTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 170.0f);
            ImGui::TableSetupColumn("Toggle", ImGuiTableColumnFlags_WidthStretch);

            auto drawBoolRow = [](const char* label, bool* value, const char* help) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", label);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    ImGui::SetTooltip("%s", help);
                }
                ImGui::TableSetColumnIndex(1);
                std::string id = std::string("##") + label;
                ImGui::Checkbox(id.c_str(), value);
            };

            drawBoolRow("Compute Culling", &post.enableComputeCulling, "GPU driven visible instance culling");
            drawBoolRow("Async Compute", &post.enableAsyncCompute, "Use async compute queue when compute culling is enabled");
            drawBoolRow("GTAO", &post.enableGTAO, "Ground-truth ambient occlusion pass");
            drawBoolRow("SSGI", &post.enableSSGI, "Screen space global illumination");
            drawBoolRow("SSR", &post.enableSSR, "Screen space reflections");
            drawBoolRow("Volumetric Fog", &post.enableVolumetricFog, "Half resolution volumetric fog");
            drawBoolRow("Bloom", &post.enableBloom, "Bloom extraction and bloom blend");
            drawBoolRow("Color Filter", &post.enableColorFilter, "Exposure / hue / vignette / flash adjustments");
            drawBoolRow("Depth of Field", &post.enableDoF, "Depth of field blur");
            drawBoolRow("Motion Blur", &post.enableMotionBlur, "Velocity-based motion blur");

            ImGui::EndTable();
        }

        if (!post.enableComputeCulling) {
            post.enableAsyncCompute = false;
        }

        ImGui::Spacing();
        if (ImGui::Button("Disable All Optional Passes")) {
            disableOptionalPasses(post);
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore Defaults")) {
            resetPassDefaults(post);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Main GBuffer / Lighting / Shadow / PostProcess output are always required and are not disabled here.");
    }
    ImGui::End();
}

void EditorLayer::DrawGridSettingsWindow()
{
    if (!m_showGridSettingsWindow) {
        return;
    }

    ApplyPendingWindowFocus(WindowFocusTarget::GridSettings);
    if (ImGui::Begin(kGridSettingsWindowTitle, &m_showGridSettingsWindow)) {
        SetLastFocusedWindow(WindowFocusTarget::GridSettings, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));

        ImGui::TextDisabled("Scene View 3D grid");
        ImGui::Spacing();

        if (ImGui::BeginTable("GridSettingsTable", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("showGrid");
            ImGui::TableSetColumnIndex(1);
            ImGui::Checkbox("##ShowSceneGrid", &m_showSceneGrid);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("cellSize");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::DragFloat("##SceneGridCellSize", &m_sceneGridCellSize, 0.5f, 1.0f, 500.0f, "%.1f")) {
                m_sceneGridCellSize = (std::clamp)(m_sceneGridCellSize, 1.0f, 500.0f);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("halfLineCount");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderInt("##SceneGridHalfLineCount", &m_sceneGridHalfLineCount, 4, 128)) {
                m_sceneGridHalfLineCount = (std::clamp)(m_sceneGridHalfLineCount, 4, 128);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("totalSpan");
            ImGui::TableSetColumnIndex(1);
            const float totalSpan = static_cast<float>(m_sceneGridHalfLineCount * 2) * m_sceneGridCellSize;
            ImGui::Text("%.1f units", totalSpan);

            ImGui::EndTable();
        }

        if (ImGui::Button("Compact Preset")) {
            m_sceneGridCellSize = 20.0f;
            m_sceneGridHalfLineCount = 24;
        }
        ImGui::SameLine();
        if (ImGui::Button("Default Preset")) {
            m_sceneGridCellSize = 20.0f;
            m_sceneGridHalfLineCount = 32;
        }
    }
    ImGui::End();
}

void EditorLayer::DrawGBufferDebugWindow()
{
    if (!m_showGBufferDebug) return;

    ApplyPendingWindowFocus(WindowFocusTarget::GBufferDebug);
    if (ImGui::Begin(kGBufferWindowTitle, &m_showGBufferDebug))
    {
        SetLastFocusedWindow(WindowFocusTarget::GBufferDebug, ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
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
