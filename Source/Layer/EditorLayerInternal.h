#pragma once
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
#include "Component/EffectPreviewTagComponent.h"
#include "Component/SequencerPreviewCameraComponent.h"
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
#include "Input/InputDebugSystem.h"
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
#include <vector>

namespace {
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
    constexpr const char* kSerializerWindowTitle = "Serializer";
    constexpr const char* kLightingWindowTitle = ICON_FA_SUN " Lighting Settings";
    constexpr const char* kAudioWindowTitle = ICON_FA_VOLUME_HIGH " Audio";
    constexpr const char* kSequencerWindowTitle = ICON_FA_FILM " Sequencer";
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
        case EditorLayer::WindowFocusTarget::Serializer: return kSerializerWindowTitle;
        case EditorLayer::WindowFocusTarget::Console: return kConsoleWindowTitle;
        case EditorLayer::WindowFocusTarget::Sequencer: return kSequencerWindowTitle;
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
               registry.GetComponent<AudioSettingsComponent>(entity) ||
               registry.GetComponent<EffectPreviewTagComponent>(entity) ||
               registry.GetComponent<SequencerPreviewCameraComponent>(entity);
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
inline void ApplyUnityTheme()
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

