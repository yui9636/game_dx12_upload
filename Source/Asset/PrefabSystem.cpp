#include "PrefabSystem.h"

#include <fstream>
#include <type_traits>
#include <unordered_map>

#include "Component/Camera2DComponent.h"
#include "Component/CanvasItemComponent.h"
#include "Component/CameraBehaviorComponent.h"
#include "Component/CameraComponent.h"
#include "Component/CameraEffectComponent.h"
#include "Component/EnvironmentComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/GridComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/LightComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NameComponent.h"
#include "Component/PrefabInstanceComponent.h"
#include "Component/PostEffectComponent.h"
#include "Component/ReflectionProbeComponent.h"
#include "Component/RectTransformComponent.h"
#include "Component/ShadowSettingsComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TextComponent.h"
#include "Component/TransformComponent.h"
#include "Console/Logger.h"
#include "JSONManager.h"
#include "Registry/Registry.h"
#include "Undo/EntitySnapshot.h"

namespace
{
    using json = nlohmann::json;

    template<typename T>
    void WritePrimitive(json& out, const char* key, const T& value)
    {
        out[key] = value;
    }

    template<typename T>
    bool TryReadPrimitive(const json& in, const char* key, T& value)
    {
        if (!in.contains(key)) {
            return false;
        }
        value = in.at(key).get<T>();
        return true;
    }

    json SerializeHierarchyNode(const EntitySnapshot::Node& node,
                                const std::unordered_map<EntityID, uint32_t>& sourceToLocal)
    {
        json out;
        out["id"] = node.localID;
        if (node.parentLocalID != EntitySnapshot::kInvalidLocalID) {
            out["parent"] = node.parentLocalID;
        }

        json components = json::object();

        auto writeComponent = [&](const char* name, const json& value) {
            components[name] = value;
        };

        if (const auto& hierarchy = std::get<std::optional<HierarchyComponent>>(node.components); hierarchy.has_value()) {
            writeComponent("HierarchyComponent", json{ {"isActive", hierarchy->isActive} });
        }

        if (const auto& name = std::get<std::optional<NameComponent>>(node.components); name.has_value()) {
            writeComponent("NameComponent", json{ {"name", name->name} });
        }

        if (const auto& transform = std::get<std::optional<TransformComponent>>(node.components); transform.has_value()) {
            writeComponent("TransformComponent", json{
                {"localPosition", transform->localPosition},
                {"localRotation", transform->localRotation},
                {"localScale", transform->localScale}
            });
        }

        if (const auto& rect = std::get<std::optional<RectTransformComponent>>(node.components); rect.has_value()) {
            writeComponent("RectTransformComponent", json{
                {"anchoredPosition", rect->anchoredPosition},
                {"sizeDelta", rect->sizeDelta},
                {"anchorMin", rect->anchorMin},
                {"anchorMax", rect->anchorMax},
                {"pivot", rect->pivot},
                {"rotationZ", rect->rotationZ},
                {"scale2D", rect->scale2D}
            });
        }

        if (const auto& canvas = std::get<std::optional<CanvasItemComponent>>(node.components); canvas.has_value()) {
            writeComponent("CanvasItemComponent", json{
                {"sortingLayer", canvas->sortingLayer},
                {"orderInLayer", canvas->orderInLayer},
                {"visible", canvas->visible},
                {"interactable", canvas->interactable},
                {"pixelSnap", canvas->pixelSnap},
                {"lockAspect", canvas->lockAspect}
            });
        }

        if (const auto& sprite = std::get<std::optional<SpriteComponent>>(node.components); sprite.has_value()) {
            writeComponent("SpriteComponent", json{
                {"textureAssetPath", sprite->textureAssetPath},
                {"tint", sprite->tint}
            });
        }

        if (const auto& text = std::get<std::optional<TextComponent>>(node.components); text.has_value()) {
            writeComponent("TextComponent", json{
                {"text", text->text},
                {"fontAssetPath", text->fontAssetPath},
                {"fontSize", text->fontSize},
                {"color", text->color},
                {"alignment", static_cast<int>(text->alignment)},
                {"lineSpacing", text->lineSpacing},
                {"wrapping", text->wrapping}
            });
        }

        if (const auto& camera2D = std::get<std::optional<Camera2DComponent>>(node.components); camera2D.has_value()) {
            writeComponent("Camera2DComponent", json{
                {"orthographicSize", camera2D->orthographicSize},
                {"zoom", camera2D->zoom},
                {"nearZ", camera2D->nearZ},
                {"farZ", camera2D->farZ},
                {"backgroundColor", camera2D->backgroundColor}
            });
        }

        if (const auto& mesh = std::get<std::optional<MeshComponent>>(node.components); mesh.has_value()) {
            writeComponent("MeshComponent", json{
                {"modelFilePath", mesh->modelFilePath},
                {"isVisible", mesh->isVisible},
                {"castShadow", mesh->castShadow},
                {"isDebugModel", mesh->isDebugModel}
            });
        }

        if (const auto& material = std::get<std::optional<MaterialComponent>>(node.components); material.has_value()) {
            writeComponent("MaterialComponent", json{
                {"materialAssetPath", material->materialAssetPath}
            });
        }

        if (const auto& light = std::get<std::optional<LightComponent>>(node.components); light.has_value()) {
            writeComponent("LightComponent", json{
                {"type", static_cast<int>(light->type)},
                {"color", light->color},
                {"intensity", light->intensity},
                {"range", light->range},
                {"castShadow", light->castShadow}
            });
        }

        if (const auto& environment = std::get<std::optional<EnvironmentComponent>>(node.components); environment.has_value()) {
            writeComponent("EnvironmentComponent", json{
                {"enableSkybox", environment->enableSkybox},
                {"skyboxPath", environment->skyboxPath},
                {"diffuseIBLPath", environment->diffuseIBLPath},
                {"specularIBLPath", environment->specularIBLPath}
            });
        }

        if (const auto& grid = std::get<std::optional<GridComponent>>(node.components); grid.has_value()) {
            writeComponent("GridComponent", json{
                {"subdivisions", grid->subdivisions},
                {"scale", grid->scale},
                {"color", grid->color},
                {"enabled", grid->enabled}
            });
        }

        if (const auto& gizmo = std::get<std::optional<GizmoComponent>>(node.components); gizmo.has_value()) {
            writeComponent("GizmoComponent", json{
                {"shape", static_cast<int>(gizmo->shape)},
                {"color", gizmo->color},
                {"offset", gizmo->offset},
                {"size", gizmo->size},
                {"radius", gizmo->radius},
                {"height", gizmo->height}
            });
        }

        if (const auto& shadow = std::get<std::optional<ShadowSettingsComponent>>(node.components); shadow.has_value()) {
            writeComponent("ShadowSettingsComponent", json{
                {"enableShadow", shadow->enableShadow},
                {"shadowColor", shadow->shadowColor}
            });
        }

        if (const auto& post = std::get<std::optional<PostEffectComponent>>(node.components); post.has_value()) {
            writeComponent("PostEffectComponent", json{
                {"luminanceLowerEdge", post->luminanceLowerEdge},
                {"luminanceHigherEdge", post->luminanceHigherEdge},
                {"bloomIntensity", post->bloomIntensity},
                {"gaussianSigma", post->gaussianSigma},
                {"exposure", post->exposure},
                {"monoBlend", post->monoBlend},
                {"hueShift", post->hueShift},
                {"flashAmount", post->flashAmount},
                {"vignetteAmount", post->vignetteAmount},
                {"enableDoF", post->enableDoF},
                {"focusDistance", post->focusDistance},
                {"focusRange", post->focusRange},
                {"bokehRadius", post->bokehRadius},
                {"motionBlurIntensity", post->motionBlurIntensity},
                {"motionBlurSamples", post->motionBlurSamples}
            });
        }

        if (const auto& probe = std::get<std::optional<ReflectionProbeComponent>>(node.components); probe.has_value()) {
            writeComponent("ReflectionProbeComponent", json{
                {"position", probe->position},
                {"radius", probe->radius}
            });
        }

        if (const auto& freeCam = std::get<std::optional<CameraFreeControlComponent>>(node.components); freeCam.has_value()) {
            writeComponent("CameraFreeControlComponent", json{
                {"moveSpeed", freeCam->moveSpeed},
                {"rotateSpeed", freeCam->rotateSpeed},
                {"pitch", freeCam->pitch},
                {"yaw", freeCam->yaw}
            });
        }

        if (const auto& tpv = std::get<std::optional<CameraTPVControlComponent>>(node.components); tpv.has_value()) {
            json value{
                {"distance", tpv->distance},
                {"heightOffset", tpv->heightOffset},
                {"smoothness", tpv->smoothness},
                {"pitch", tpv->pitch},
                {"yaw", tpv->yaw}
            };
            const auto localIt = sourceToLocal.find(tpv->target);
            if (localIt != sourceToLocal.end()) {
                value["targetLocalId"] = localIt->second;
            }
            writeComponent("CameraTPVControlComponent", value);
        }

        if (const auto& lookAt = std::get<std::optional<CameraLookAtComponent>>(node.components); lookAt.has_value()) {
            json value{
                {"up", lookAt->up}
            };
            const auto localIt = sourceToLocal.find(lookAt->target);
            if (localIt != sourceToLocal.end()) {
                value["targetLocalId"] = localIt->second;
            }
            writeComponent("CameraLookAtComponent", value);
        }

        if (const auto& lens = std::get<std::optional<CameraLensComponent>>(node.components); lens.has_value()) {
            writeComponent("CameraLensComponent", json{
                {"fovY", lens->fovY},
                {"nearZ", lens->nearZ},
                {"farZ", lens->farZ},
                {"aspect", lens->aspect}
            });
        }

        if (const auto& mainTag = std::get<std::optional<CameraMainTagComponent>>(node.components); mainTag.has_value()) {
            writeComponent("CameraMainTagComponent", json::object());
        }

        if (const auto& shake = std::get<std::optional<CameraShakeComponent>>(node.components); shake.has_value()) {
            writeComponent("CameraShakeComponent", json{
                {"amplitude", shake->amplitude},
                {"duration", shake->duration},
                {"frequency", shake->frequency},
                {"decay", shake->decay}
            });
        }

        if (const auto& collider = std::get<std::optional<ColliderComponent>>(node.components); collider.has_value()) {
            json colliderJson;
            colliderJson["enabled"] = collider->enabled;
            colliderJson["drawGizmo"] = collider->drawGizmo;
            colliderJson["elements"] = json::array();
            for (const auto& element : collider->elements) {
                colliderJson["elements"].push_back(json{
                    {"type", static_cast<int>(element.type)},
                    {"enabled", element.enabled},
                    {"nodeIndex", element.nodeIndex},
                    {"offsetLocal", DirectX::XMFLOAT3{ element.offsetLocal.x, element.offsetLocal.y, element.offsetLocal.z }},
                    {"radius", element.radius},
                    {"height", element.height},
                    {"size", DirectX::XMFLOAT3{ element.size.x, element.size.y, element.size.z }},
                    {"color", DirectX::XMFLOAT4{ element.color.x, element.color.y, element.color.z, element.color.w }},
                    {"attribute", static_cast<int>(element.attribute)}
                });
            }
            writeComponent("ColliderComponent", colliderJson);
        }

        out["components"] = std::move(components);
        return out;
    }

    template<typename T>
    void SetOptional(EntitySnapshot::ComponentStorage& storage, T value)
    {
        std::get<std::optional<T>>(storage) = std::move(value);
    }

    void DeserializeHierarchyNode(const json& in, EntitySnapshot::Node& node)
    {
        node.localID = in.at("id").get<uint32_t>();
        node.parentLocalID = in.contains("parent")
            ? in.at("parent").get<uint32_t>()
            : EntitySnapshot::kInvalidLocalID;
        node.sourceEntity = Entity::Create(node.localID, 0);
        node.externalParent = Entity::NULL_ID;

        const json components = in.value("components", json::object());

        HierarchyComponent hierarchyComponent{};
        if (components.contains("HierarchyComponent")) {
            hierarchyComponent.isActive = components["HierarchyComponent"].value("isActive", hierarchyComponent.isActive);
        }
        SetOptional(node.components, hierarchyComponent);

        if (components.contains("NameComponent")) {
            NameComponent component;
            component.name = components["NameComponent"].value("name", "New Entity");
            SetOptional(node.components, component);
        }

        if (components.contains("TransformComponent")) {
            TransformComponent component;
            const json& value = components["TransformComponent"];
            component.localPosition = value.value("localPosition", component.localPosition);
            component.localRotation = value.value("localRotation", component.localRotation);
            component.localScale = value.value("localScale", component.localScale);
            component.parent = 0;
            component.isDirty = true;
            SetOptional(node.components, component);
        }

        if (components.contains("RectTransformComponent")) {
            RectTransformComponent component;
            const json& value = components["RectTransformComponent"];
            component.anchoredPosition = value.value("anchoredPosition", component.anchoredPosition);
            component.sizeDelta = value.value("sizeDelta", component.sizeDelta);
            component.anchorMin = value.value("anchorMin", component.anchorMin);
            component.anchorMax = value.value("anchorMax", component.anchorMax);
            component.pivot = value.value("pivot", component.pivot);
            component.rotationZ = value.value("rotationZ", component.rotationZ);
            component.scale2D = value.value("scale2D", component.scale2D);
            SetOptional(node.components, component);
        }

        if (components.contains("CanvasItemComponent")) {
            CanvasItemComponent component;
            const json& value = components["CanvasItemComponent"];
            component.sortingLayer = value.value("sortingLayer", component.sortingLayer);
            component.orderInLayer = value.value("orderInLayer", component.orderInLayer);
            component.visible = value.value("visible", component.visible);
            component.interactable = value.value("interactable", component.interactable);
            component.pixelSnap = value.value("pixelSnap", component.pixelSnap);
            component.lockAspect = value.value("lockAspect", component.lockAspect);
            SetOptional(node.components, component);
        }

        if (components.contains("SpriteComponent")) {
            SpriteComponent component;
            const json& value = components["SpriteComponent"];
            component.textureAssetPath = value.value("textureAssetPath", component.textureAssetPath);
            component.tint = value.value("tint", component.tint);
            SetOptional(node.components, component);
        }

        if (components.contains("TextComponent")) {
            TextComponent component;
            const json& value = components["TextComponent"];
            component.text = value.value("text", component.text);
            component.fontAssetPath = value.value("fontAssetPath", component.fontAssetPath);
            component.fontSize = value.value("fontSize", component.fontSize);
            component.color = value.value("color", component.color);
            component.alignment = static_cast<TextAlignment>(value.value("alignment", static_cast<int>(component.alignment)));
            component.lineSpacing = value.value("lineSpacing", component.lineSpacing);
            component.wrapping = value.value("wrapping", component.wrapping);
            SetOptional(node.components, component);
        }

        if (components.contains("Camera2DComponent")) {
            Camera2DComponent component;
            const json& value = components["Camera2DComponent"];
            component.orthographicSize = value.value("orthographicSize", component.orthographicSize);
            component.zoom = value.value("zoom", component.zoom);
            component.nearZ = value.value("nearZ", component.nearZ);
            component.farZ = value.value("farZ", component.farZ);
            component.backgroundColor = value.value("backgroundColor", component.backgroundColor);
            SetOptional(node.components, component);
        }

        if (components.contains("MeshComponent")) {
            MeshComponent component;
            const json& value = components["MeshComponent"];
            component.modelFilePath = value.value("modelFilePath", component.modelFilePath);
            component.isVisible = value.value("isVisible", component.isVisible);
            component.castShadow = value.value("castShadow", component.castShadow);
            component.isDebugModel = value.value("isDebugModel", component.isDebugModel);
            SetOptional(node.components, component);
        }

        if (components.contains("MaterialComponent")) {
            MaterialComponent component;
            component.materialAssetPath = components["MaterialComponent"].value("materialAssetPath", component.materialAssetPath);
            SetOptional(node.components, component);
        }

        if (components.contains("LightComponent")) {
            LightComponent component;
            const json& value = components["LightComponent"];
            component.type = static_cast<LightType>(value.value("type", static_cast<int>(component.type)));
            component.color = value.value("color", component.color);
            component.intensity = value.value("intensity", component.intensity);
            component.range = value.value("range", component.range);
            component.castShadow = value.value("castShadow", component.castShadow);
            SetOptional(node.components, component);
        }

        if (components.contains("EnvironmentComponent")) {
            EnvironmentComponent component;
            const json& value = components["EnvironmentComponent"];
            component.enableSkybox = value.value("enableSkybox", component.enableSkybox);
            component.skyboxPath = value.value("skyboxPath", component.skyboxPath);
            component.diffuseIBLPath = value.value("diffuseIBLPath", component.diffuseIBLPath);
            component.specularIBLPath = value.value("specularIBLPath", component.specularIBLPath);
            SetOptional(node.components, component);
        }

        if (components.contains("GridComponent")) {
            GridComponent component;
            const json& value = components["GridComponent"];
            component.subdivisions = value.value("subdivisions", component.subdivisions);
            component.scale = value.value("scale", component.scale);
            component.color = value.value("color", component.color);
            component.enabled = value.value("enabled", component.enabled);
            SetOptional(node.components, component);
        }

        if (components.contains("GizmoComponent")) {
            GizmoComponent component;
            const json& value = components["GizmoComponent"];
            component.shape = static_cast<GizmoComponent::Shape>(value.value("shape", static_cast<int>(component.shape)));
            component.color = value.value("color", component.color);
            component.offset = value.value("offset", component.offset);
            component.size = value.value("size", component.size);
            component.radius = value.value("radius", component.radius);
            component.height = value.value("height", component.height);
            SetOptional(node.components, component);
        }

        if (components.contains("ShadowSettingsComponent")) {
            ShadowSettingsComponent component;
            const json& value = components["ShadowSettingsComponent"];
            component.enableShadow = value.value("enableShadow", component.enableShadow);
            component.shadowColor = value.value("shadowColor", component.shadowColor);
            SetOptional(node.components, component);
        }

        if (components.contains("PostEffectComponent")) {
            PostEffectComponent component;
            const json& value = components["PostEffectComponent"];
            component.luminanceLowerEdge = value.value("luminanceLowerEdge", component.luminanceLowerEdge);
            component.luminanceHigherEdge = value.value("luminanceHigherEdge", component.luminanceHigherEdge);
            component.bloomIntensity = value.value("bloomIntensity", component.bloomIntensity);
            component.gaussianSigma = value.value("gaussianSigma", component.gaussianSigma);
            component.exposure = value.value("exposure", component.exposure);
            component.monoBlend = value.value("monoBlend", component.monoBlend);
            component.hueShift = value.value("hueShift", component.hueShift);
            component.flashAmount = value.value("flashAmount", component.flashAmount);
            component.vignetteAmount = value.value("vignetteAmount", component.vignetteAmount);
            component.enableDoF = value.value("enableDoF", component.enableDoF);
            component.focusDistance = value.value("focusDistance", component.focusDistance);
            component.focusRange = value.value("focusRange", component.focusRange);
            component.bokehRadius = value.value("bokehRadius", component.bokehRadius);
            component.motionBlurIntensity = value.value("motionBlurIntensity", component.motionBlurIntensity);
            component.motionBlurSamples = value.value("motionBlurSamples", component.motionBlurSamples);
            SetOptional(node.components, component);
        }

        if (components.contains("ReflectionProbeComponent")) {
            ReflectionProbeComponent component;
            const json& value = components["ReflectionProbeComponent"];
            component.position = value.value("position", component.position);
            component.radius = value.value("radius", component.radius);
            component.needsBake = true;
            SetOptional(node.components, component);
        }

        if (components.contains("CameraFreeControlComponent")) {
            CameraFreeControlComponent component;
            const json& value = components["CameraFreeControlComponent"];
            component.moveSpeed = value.value("moveSpeed", component.moveSpeed);
            component.rotateSpeed = value.value("rotateSpeed", component.rotateSpeed);
            component.pitch = value.value("pitch", component.pitch);
            component.yaw = value.value("yaw", component.yaw);
            component.isHovered = false;
            SetOptional(node.components, component);
        }

        if (components.contains("CameraTPVControlComponent")) {
            CameraTPVControlComponent component;
            const json& value = components["CameraTPVControlComponent"];
            component.distance = value.value("distance", component.distance);
            component.heightOffset = value.value("heightOffset", component.heightOffset);
            component.smoothness = value.value("smoothness", component.smoothness);
            component.pitch = value.value("pitch", component.pitch);
            component.yaw = value.value("yaw", component.yaw);
            component.target = value.contains("targetLocalId")
                ? Entity::Create(value["targetLocalId"].get<uint32_t>(), 0)
                : Entity::NULL_ID;
            SetOptional(node.components, component);
        }

        if (components.contains("CameraLookAtComponent")) {
            CameraLookAtComponent component;
            const json& value = components["CameraLookAtComponent"];
            component.up = value.value("up", component.up);
            component.target = value.contains("targetLocalId")
                ? Entity::Create(value["targetLocalId"].get<uint32_t>(), 0)
                : Entity::NULL_ID;
            SetOptional(node.components, component);
        }

        if (components.contains("CameraLensComponent")) {
            CameraLensComponent component;
            const json& value = components["CameraLensComponent"];
            component.fovY = value.value("fovY", component.fovY);
            component.nearZ = value.value("nearZ", component.nearZ);
            component.farZ = value.value("farZ", component.farZ);
            component.aspect = value.value("aspect", component.aspect);
            SetOptional(node.components, component);
        }

        if (components.contains("CameraMainTagComponent")) {
            SetOptional(node.components, CameraMainTagComponent{});
        }

        if (components.contains("CameraShakeComponent")) {
            CameraShakeComponent component;
            const json& value = components["CameraShakeComponent"];
            component.amplitude = value.value("amplitude", component.amplitude);
            component.duration = value.value("duration", component.duration);
            component.frequency = value.value("frequency", component.frequency);
            component.decay = value.value("decay", component.decay);
            component.timer = 0.0f;
            component.currentOffset = { 0.0f, 0.0f, 0.0f };
            SetOptional(node.components, component);
        }

        if (components.contains("ColliderComponent")) {
            ColliderComponent component;
            const json& value = components["ColliderComponent"];
            component.enabled = value.value("enabled", component.enabled);
            component.drawGizmo = value.value("drawGizmo", component.drawGizmo);
            if (value.contains("elements") && value["elements"].is_array()) {
                component.elements.clear();
                for (const auto& elemJson : value["elements"]) {
                    ColliderComponent::Element element;
                    element.type = static_cast<ColliderShape>(elemJson.value("type", static_cast<int>(element.type)));
                    element.enabled = elemJson.value("enabled", element.enabled);
                    element.nodeIndex = elemJson.value("nodeIndex", element.nodeIndex);
                    const DirectX::XMFLOAT3 offset = elemJson.value("offsetLocal", DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f });
                    element.offsetLocal = { offset.x, offset.y, offset.z };
                    element.radius = elemJson.value("radius", element.radius);
                    element.height = elemJson.value("height", element.height);
                    const DirectX::XMFLOAT3 size = elemJson.value("size", DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f });
                    element.size = { size.x, size.y, size.z };
                    const DirectX::XMFLOAT4 color = elemJson.value("color", DirectX::XMFLOAT4{ 1.0f, 0.0f, 0.0f, 0.35f });
                    element.color = { color.x, color.y, color.z, color.w };
                    element.attribute = static_cast<ColliderAttribute>(elemJson.value("attribute", static_cast<int>(element.attribute)));
                    element.registeredId = 0;
                    element.runtimeTag = 0;
                    component.elements.push_back(std::move(element));
                }
            }
            SetOptional(node.components, component);
        }
    }

    void StripPrefabMetadata(EntitySnapshot::Snapshot& snapshot)
    {
        for (auto& node : snapshot.nodes) {
            std::get<std::optional<PrefabInstanceComponent>>(node.components).reset();
        }
    }

    void AttachPrefabMetadata(EntitySnapshot::Snapshot& snapshot, const std::filesystem::path& prefabPath)
    {
        for (auto& node : snapshot.nodes) {
            if (node.localID != snapshot.rootLocalID) {
                continue;
            }

            PrefabInstanceComponent prefabInstance;
            prefabInstance.prefabAssetPath = prefabPath.generic_string();
            prefabInstance.hasOverrides = false;
            SetOptional(node.components, prefabInstance);
            break;
        }
    }

    std::filesystem::path MakeUniquePrefabPath(const std::filesystem::path& directory, const std::string& baseName)
    {
        std::error_code ec;
        std::string safeName = baseName.empty() ? "NewPrefab" : baseName;
        std::filesystem::path path = directory / (safeName + ".prefab");
        int suffix = 1;
        while (std::filesystem::exists(path, ec)) {
            path = directory / (safeName + " (" + std::to_string(suffix++) + ").prefab");
        }
        return path;
    }

    std::vector<EntityID> GatherSceneRoots(Registry& registry)
    {
        std::vector<EntityID> roots;
        for (Archetype* archetype : registry.GetAllArchetypes()) {
            const auto& entities = archetype->GetEntities();
            for (EntityID entity : entities) {
                EntityID parent = Entity::NULL_ID;
                if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(entity)) {
                    parent = hierarchy->parent;
                } else if (auto* transform = registry.GetComponent<TransformComponent>(entity)) {
                    parent = transform->parent == 0 ? Entity::NULL_ID : transform->parent;
                }

                if (Entity::IsNull(parent)) {
                    roots.push_back(entity);
                }
            }
        }
        return roots;
    }
}

bool PrefabSystem::SaveEntityAsPrefab(EntityID root,
                                      Registry& registry,
                                      const std::filesystem::path& destinationDir,
                                      std::filesystem::path* outPath)
{
    if (Entity::IsNull(root) || !registry.IsAlive(root)) {
        return false;
    }

    std::string prefabName = "NewPrefab";
    if (auto* name = registry.GetComponent<NameComponent>(root)) {
        prefabName = name->name;
    }

    const std::filesystem::path finalPath = MakeUniquePrefabPath(destinationDir, prefabName);
    if (!SaveEntityToPrefabPath(root, registry, finalPath)) {
        return false;
    }

    if (outPath) {
        *outPath = finalPath;
    }
    return true;
}

bool PrefabSystem::SaveEntityToPrefabPath(EntityID root,
                                          Registry& registry,
                                          const std::filesystem::path& prefabPath)
{
    if (Entity::IsNull(root) || !registry.IsAlive(root)) {
        return false;
    }

    EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(root, registry);
    if (snapshot.nodes.empty()) {
        return false;
    }

    StripPrefabMetadata(snapshot);

    for (auto& node : snapshot.nodes) {
        if (node.localID == snapshot.rootLocalID) {
            node.parentLocalID = EntitySnapshot::kInvalidLocalID;
            node.externalParent = Entity::NULL_ID;
            break;
        }
    }

    std::unordered_map<EntityID, uint32_t> sourceToLocal;
    for (const auto& node : snapshot.nodes) {
        sourceToLocal[node.sourceEntity] = node.localID;
    }

    std::error_code ec;
    std::filesystem::create_directories(prefabPath.parent_path(), ec);

    json document;
    document["version"] = 1;
    document["rootLocalId"] = snapshot.rootLocalID;
    document["nodes"] = json::array();

    for (const auto& node : snapshot.nodes) {
        document["nodes"].push_back(SerializeHierarchyNode(node, sourceToLocal));
    }

    std::ofstream ofs(prefabPath);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << document.dump(2);
    return true;
}

bool PrefabSystem::SaveRegistryAsScene(Registry& registry,
                                       const std::filesystem::path& scenePath,
                                       const SceneFileMetadata* metadata)
{
    std::vector<EntityID> roots = GatherSceneRoots(registry);
    if (roots.empty()) {
        return false;
    }

    EntitySnapshot::Snapshot mergedSnapshot;
    mergedSnapshot.rootLocalID = EntitySnapshot::kInvalidLocalID;
    std::vector<uint32_t> rootLocalIDs;
    uint32_t localIdOffset = 0;

    for (EntityID root : roots) {
        EntitySnapshot::Snapshot snapshot = EntitySnapshot::CaptureSubtree(root, registry);
        if (snapshot.nodes.empty()) {
            continue;
        }

        rootLocalIDs.push_back(snapshot.rootLocalID + localIdOffset);
        if (mergedSnapshot.rootLocalID == EntitySnapshot::kInvalidLocalID) {
            mergedSnapshot.rootLocalID = snapshot.rootLocalID + localIdOffset;
        }

        for (auto& node : snapshot.nodes) {
            node.localID += localIdOffset;
            if (node.parentLocalID != EntitySnapshot::kInvalidLocalID) {
                node.parentLocalID += localIdOffset;
            }
            node.externalParent = Entity::NULL_ID;
            std::get<std::optional<PrefabInstanceComponent>>(node.components).reset();
            mergedSnapshot.nodes.push_back(std::move(node));
        }

        localIdOffset += static_cast<uint32_t>(snapshot.nodes.size());
    }

    if (mergedSnapshot.nodes.empty()) {
        return false;
    }

    std::unordered_map<EntityID, uint32_t> sourceToLocal;
    for (const auto& node : mergedSnapshot.nodes) {
        sourceToLocal[node.sourceEntity] = node.localID;
    }

    std::error_code ec;
    std::filesystem::create_directories(scenePath.parent_path(), ec);

    json document;
    document["version"] = 1;
    document["type"] = "scene";
    document["editor"] = {
        { "sceneViewMode", metadata ? metadata->sceneViewMode : "3D" }
    };
    document["rootLocalIds"] = rootLocalIDs;
    document["nodes"] = json::array();

    for (const auto& node : mergedSnapshot.nodes) {
        document["nodes"].push_back(SerializeHierarchyNode(node, sourceToLocal));
    }

    std::ofstream ofs(scenePath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << document.dump(2);
    return true;
}

bool PrefabSystem::LoadSceneIntoRegistry(const std::filesystem::path& scenePath,
                                         Registry& registry,
                                         SceneFileMetadata* outMetadata)
{
    std::ifstream ifs(scenePath, std::ios::binary);
    if (!ifs.is_open()) {
        LOG_WARN("[Scene] Failed to open scene '%s'", scenePath.string().c_str());
        return false;
    }

    json document;
    try {
        ifs >> document;
    } catch (const std::exception& e) {
        LOG_WARN("[Scene] Failed to parse scene '%s' what='%s'", scenePath.string().c_str(), e.what());
        return false;
    }

    if (outMetadata) {
        outMetadata->sceneViewMode = "3D";
        if (document.contains("editor") && document["editor"].is_object()) {
            outMetadata->sceneViewMode = document["editor"].value("sceneViewMode", outMetadata->sceneViewMode);
        }
    }

    EntitySnapshot::Snapshot snapshot;
    const json nodes = document.value("nodes", json::array());
    for (const auto& nodeJson : nodes) {
        EntitySnapshot::Node node;
        DeserializeHierarchyNode(nodeJson, node);
        snapshot.nodes.push_back(std::move(node));
    }

    if (snapshot.nodes.empty()) {
        LOG_WARN("[Scene] Scene '%s' has no nodes", scenePath.string().c_str());
        return false;
    }

    if (document.contains("rootLocalIds") && document["rootLocalIds"].is_array() && !document["rootLocalIds"].empty()) {
        snapshot.rootLocalID = document["rootLocalIds"][0].get<uint32_t>();
    } else {
        snapshot.rootLocalID = document.value("rootLocalId", snapshot.nodes.front().localID);
    }

    std::vector<EntityID> existingRoots = GatherSceneRoots(registry);
    for (auto it = existingRoots.rbegin(); it != existingRoots.rend(); ++it) {
        if (registry.IsAlive(*it)) {
            EntitySnapshot::DestroySubtree(*it, registry);
        }
    }

    EntitySnapshot::RestoreSubtree(snapshot, registry);
    return true;
}

bool PrefabSystem::LoadPrefabSnapshot(const std::filesystem::path& prefabPath,
                                      EntitySnapshot::Snapshot& outSnapshot)
{
    std::ifstream ifs(prefabPath);
    if (!ifs.is_open()) {
        LOG_WARN("[Prefab] Failed to open prefab '%s'", prefabPath.string().c_str());
        return false;
    }

    json document;
    try {
        ifs >> document;
    } catch (const std::exception& e) {
        LOG_WARN("[Prefab] Failed to parse prefab '%s' what='%s'", prefabPath.string().c_str(), e.what());
        return false;
    }

    EntitySnapshot::Snapshot snapshot;
    snapshot.rootLocalID = document.value("rootLocalId", EntitySnapshot::kInvalidLocalID);

    const json nodes = document.value("nodes", json::array());
    for (const auto& nodeJson : nodes) {
        EntitySnapshot::Node node;
        DeserializeHierarchyNode(nodeJson, node);
        snapshot.nodes.push_back(std::move(node));
    }

    if (snapshot.nodes.empty()) {
        return false;
    }

    outSnapshot = std::move(snapshot);
    return true;
}

EntityID PrefabSystem::InstantiatePrefab(const std::filesystem::path& prefabPath,
                                         Registry& registry,
                                         EntityID parentEntity)
{
    EntitySnapshot::Snapshot snapshot;
    if (!LoadPrefabSnapshot(prefabPath, snapshot)) {
        return Entity::NULL_ID;
    }

    AttachPrefabMetadata(snapshot, prefabPath);

    for (auto& node : snapshot.nodes) {
        if (node.localID == snapshot.rootLocalID) {
            node.externalParent = parentEntity;
            break;
        }
    }

    EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(snapshot, registry);
    return restore.root;
}

bool PrefabSystem::ApplyPrefab(EntityID root, Registry& registry)
{
    auto* prefabInstance = registry.GetComponent<PrefabInstanceComponent>(root);
    if (!prefabInstance || prefabInstance->prefabAssetPath.empty()) {
        return false;
    }

    if (!SaveEntityToPrefabPath(root, registry, prefabInstance->prefabAssetPath)) {
        return false;
    }

    prefabInstance->hasOverrides = false;
    return true;
}

EntityID PrefabSystem::RevertPrefab(EntityID root, Registry& registry)
{
    auto* prefabInstance = registry.GetComponent<PrefabInstanceComponent>(root);
    if (!prefabInstance || prefabInstance->prefabAssetPath.empty()) {
        return Entity::NULL_ID;
    }

    EntityID parent = Entity::NULL_ID;
    if (auto* hierarchy = registry.GetComponent<HierarchyComponent>(root)) {
        parent = hierarchy->parent;
    }

    const std::string prefabPath = prefabInstance->prefabAssetPath;
    EntitySnapshot::DestroySubtree(root, registry);
    return InstantiatePrefab(prefabPath, registry, parent);
}

bool PrefabSystem::UnpackPrefab(EntityID root, Registry& registry)
{
    if (auto* prefabInstance = registry.GetComponent<PrefabInstanceComponent>(root)) {
        registry.RemoveComponent<PrefabInstanceComponent>(root);
        return true;
    }
    return false;
}

EntityID PrefabSystem::FindPrefabRoot(EntityID entity, Registry& registry)
{
    EntityID current = entity;
    while (!Entity::IsNull(current) && registry.IsAlive(current)) {
        if (registry.GetComponent<PrefabInstanceComponent>(current)) {
            return current;
        }

        auto* hierarchy = registry.GetComponent<HierarchyComponent>(current);
        if (!hierarchy) {
            break;
        }
        current = hierarchy->parent;
    }

    return Entity::NULL_ID;
}

void PrefabSystem::MarkPrefabOverride(EntityID entity, Registry& registry)
{
    EntityID root = FindPrefabRoot(entity, registry);
    if (Entity::IsNull(root)) {
        return;
    }

    if (auto* prefabInstance = registry.GetComponent<PrefabInstanceComponent>(root)) {
        prefabInstance->hasOverrides = true;
    }
}

bool PrefabSystem::CanReparent(EntityID entity, EntityID newParent, Registry& registry)
{
    EntityID entityPrefabRoot = FindPrefabRoot(entity, registry);
    if (!Entity::IsNull(entityPrefabRoot) && entityPrefabRoot != entity) {
        return false;
    }

    EntityID parentPrefabRoot = FindPrefabRoot(newParent, registry);
    return Entity::IsNull(parentPrefabRoot);
}

bool PrefabSystem::CanCreateChild(EntityID parentEntity, Registry& registry)
{
    return Entity::IsNull(FindPrefabRoot(parentEntity, registry));
}

bool PrefabSystem::CanDelete(EntityID entity, Registry& registry)
{
    EntityID prefabRoot = FindPrefabRoot(entity, registry);
    return Entity::IsNull(prefabRoot) || prefabRoot == entity;
}

bool PrefabSystem::CanDuplicate(EntityID entity, Registry& registry)
{
    EntityID prefabRoot = FindPrefabRoot(entity, registry);
    return Entity::IsNull(prefabRoot) || prefabRoot == entity;
}


