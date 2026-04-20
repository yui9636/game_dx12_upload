#include "EffectEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>

#include "Asset/ThumbnailGenerator.h"
#include "Console/Logger.h"
#include "Component/EffectAssetComponent.h"
#include "Component/EffectParameterOverrideComponent.h"
#include "Component/EffectPlaybackComponent.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/EffectSpawnRequestComponent.h"
#include "Component/HierarchyComponent.h"
#include "Component/NameComponent.h"
#include "Component/TransformComponent.h"
#include "EffectRuntime/EffectCompiler.h"
#include "EffectRuntime/EffectParameterBindings.h"
#include "EffectRuntime/EffectGraphSerializer.h"
#include "EffectRuntime/EffectRuntimeRegistry.h"
#include "Icon/IconsFontAwesome7.h"
#include "ImGuiRenderer.h"
#include "Registry/Registry.h"
#include "System/ResourceManager.h"

namespace ed = ax::NodeEditor;

namespace
{
    constexpr const char* kGraphWindowTitle = ICON_FA_DIAGRAM_PROJECT " System Overview##EffectEditor";
    constexpr const char* kPreviewWindowTitle = ICON_FA_CUBE " Preview##EffectEditor";
    constexpr const char* kTimelineWindowTitle = ICON_FA_CLOCK " Timeline##EffectEditor";
    constexpr const char* kDetailsWindowTitle = ICON_FA_SLIDERS " Inspector##EffectEditor";
    constexpr const char* kAssetWindowTitle = "Blackboard##EffectEditor";
    constexpr float kPreviewMinSize = 96.0f;
    constexpr float kPreviewToolbarHeight = 28.0f;
    constexpr uint32_t kSystemVisualNodeId = 0x70000001u;
    constexpr uint32_t kEmitterVisualNodeId = 0x70000002u;

    ImColor GetNodeColor(EffectGraphNodeType type)
    {
        switch (type) {
        case EffectGraphNodeType::Spawn:           return ImColor(70, 140, 220);
        case EffectGraphNodeType::Lifetime:        return ImColor(90, 180, 120);
        case EffectGraphNodeType::MeshSource:      return ImColor(180, 120, 80);
        case EffectGraphNodeType::MeshRenderer:    return ImColor(220, 160, 80);
        case EffectGraphNodeType::ParticleEmitter: return ImColor(140, 90, 210);
        case EffectGraphNodeType::SpriteRenderer:  return ImColor(200, 100, 180);
        case EffectGraphNodeType::Float:
        case EffectGraphNodeType::Vec3:
        case EffectGraphNodeType::Color:           return ImColor(90, 90, 90);
        case EffectGraphNodeType::Output:          return ImColor(240, 200, 80);
        default:                              return ImColor(80, 80, 80);
        }
    }

    ImColor GetValueTypeColor(EffectValueType type)
    {
        switch (type) {
        case EffectValueType::Flow:     return ImColor(238, 181, 58);
        case EffectValueType::Float:    return ImColor(104, 178, 255);
        case EffectValueType::Vec3:     return ImColor(109, 215, 179);
        case EffectValueType::Color:    return ImColor(232, 120, 120);
        case EffectValueType::Mesh:     return ImColor(208, 156, 94);
        case EffectValueType::Particle: return ImColor(126, 208, 95);
        default:                        return ImColor(140, 140, 140);
        }
    }

    const char* EffectValueTypeLabel(EffectValueType type)
    {
        switch (type) {
        case EffectValueType::Flow:     return "Flow";
        case EffectValueType::Float:    return "Float";
        case EffectValueType::Vec3:     return "Vec3";
        case EffectValueType::Color:    return "Color";
        case EffectValueType::Mesh:     return "Mesh";
        case EffectValueType::Particle: return "Particle";
        default:                        return "Unknown";
        }
    }

    const char* BlendStateLabel(BlendState blendState)
    {
        switch (blendState) {
        case BlendState::Opaque:       return "Opaque";
        case BlendState::Transparency: return "Transparency";
        case BlendState::Additive:     return "Additive";
        case BlendState::Subtraction:  return "Subtraction";
        case BlendState::Multiply:     return "Multiply";
        case BlendState::Alpha:        return "Alpha";
        default:                       return "Unknown";
        }
    }

    bool InputTextString(const char* label, std::string& value)
    {
        char buffer[512] = {};
        strcpy_s(buffer, value.c_str());
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            value = buffer;
            return true;
        }
        return false;
    }

    float SafeDuration(float duration)
    {
        return (duration > 0.01f) ? duration : 0.01f;
    }

    bool HasModelExtension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb";
    }

    bool HasTextureExtension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext == ".png" || ext == ".dds" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
    }

    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool ContainsInsensitive(const std::string& value, const char* needle)
    {
        if (!needle || needle[0] == '\0') {
            return true;
        }

        const std::string loweredValue = ToLowerCopy(value);
        const std::string loweredNeedle = ToLowerCopy(needle);
        return loweredValue.find(loweredNeedle) != std::string::npos;
    }

    void PushRecentPath(std::vector<std::string>& paths, const std::string& path, size_t maxCount = 10)
    {
        if (path.empty()) {
            return;
        }

        paths.erase(std::remove(paths.begin(), paths.end(), path), paths.end());
        paths.insert(paths.begin(), path);
        if (paths.size() > maxCount) {
            paths.resize(maxCount);
        }
    }

    bool ContainsExactPath(const std::vector<std::string>& paths, const std::string& path)
    {
        return std::find(paths.begin(), paths.end(), path) != paths.end();
    }

    void TogglePath(std::vector<std::string>& paths, const std::string& path)
    {
        if (path.empty()) {
            return;
        }

        auto it = std::find(paths.begin(), paths.end(), path);
        if (it != paths.end()) {
            paths.erase(it);
            return;
        }

        paths.push_back(path);
    }

    std::vector<std::filesystem::path> CollectAssets(const std::filesystem::path& root, bool wantModels)
    {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) {
            return result;
        }

        for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec)) {
                continue;
            }

            const auto& path = it->path();
            if (wantModels ? HasModelExtension(path) : HasTextureExtension(path)) {
                result.push_back(path);
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    ImVec4 NiagaraSectionColor(EffectGraphNodeType type)
    {
        switch (type) {
        case EffectGraphNodeType::Spawn:           return ImVec4(0.19f, 0.64f, 0.89f, 1.0f);
        case EffectGraphNodeType::Lifetime:        return ImVec4(0.31f, 0.82f, 0.39f, 1.0f);
        case EffectGraphNodeType::ParticleEmitter: return ImVec4(0.97f, 0.48f, 0.11f, 1.0f);
        case EffectGraphNodeType::SpriteRenderer:
        case EffectGraphNodeType::MeshRenderer:    return ImVec4(0.98f, 0.36f, 0.23f, 1.0f);
        case EffectGraphNodeType::Color:           return ImVec4(0.17f, 0.48f, 0.98f, 1.0f);
        case EffectGraphNodeType::MeshSource:      return ImVec4(0.51f, 0.80f, 0.42f, 1.0f);
        default:                                   return ImVec4(0.55f, 0.55f, 0.60f, 1.0f);
        }
    }

    void SanitizeGraphAsset(EffectGraphAsset& asset)
    {
        asset.pins.erase(
            std::remove_if(
                asset.pins.begin(),
                asset.pins.end(),
                [&](const EffectGraphPin& pin) {
                    return asset.FindNode(pin.nodeId) == nullptr;
                }),
            asset.pins.end());

        asset.links.erase(
            std::remove_if(
                asset.links.begin(),
                asset.links.end(),
                [&](const EffectGraphLink& link) {
                    return asset.FindPin(link.startPinId) == nullptr || asset.FindPin(link.endPinId) == nullptr;
                }),
            asset.links.end());

        // Defensive: side-effect nodes (Spawn/Lifetime/MeshRenderer/ParticleEmitter/SpriteRenderer)
        // may drive at most ONE Flow output. Left-over links from prior template switches can
        // otherwise surface as "Side-effect nodes may drive only one flow output." at compile.
        // Keep the first Flow output per node and drop the rest, logging what we removed.
        std::unordered_map<uint32_t, uint32_t> flowOutCount; // nodeId -> seen so far
        std::vector<uint32_t> removedLinkIds;
        for (auto it = asset.links.begin(); it != asset.links.end();) {
            const EffectGraphPin* startPin = asset.FindPin(it->startPinId);
            if (!startPin || startPin->valueType != EffectValueType::Flow) {
                ++it;
                continue;
            }
            const EffectGraphNode* startNode = asset.FindNode(startPin->nodeId);
            if (!startNode || !IsEffectSideEffectNode(startNode->type)) {
                ++it;
                continue;
            }
            uint32_t& seen = flowOutCount[startNode->id];
            if (seen >= 1) {
                removedLinkIds.push_back(it->id);
                it = asset.links.erase(it);
                continue;
            }
            ++seen;
            ++it;
        }
        if (!removedLinkIds.empty()) {
            LOG_WARN("[EffectSanitize] Pruned %zu stray flow links from side-effect nodes (fan-out>1).",
                removedLinkIds.size());
        }
    }

    void LogGraphStructure(const EffectGraphAsset& asset, const char* tag)
    {
        LOG_INFO("[%s] nodes=%zu pins=%zu links=%zu", tag, asset.nodes.size(), asset.pins.size(), asset.links.size());
        for (const auto& node : asset.nodes) {
            const char* typeName = EffectGraphNodeTypeToString(node.type);
            LOG_INFO("[%s]   node id=%u type=%s", tag, node.id, typeName ? typeName : "?");
        }
        std::unordered_map<uint32_t, uint32_t> flowFan;
        for (const auto& link : asset.links) {
            const EffectGraphPin* startPin = asset.FindPin(link.startPinId);
            const EffectGraphPin* endPin = asset.FindPin(link.endPinId);
            if (!startPin || !endPin) continue;
            const EffectGraphNode* startNode = asset.FindNode(startPin->nodeId);
            const EffectGraphNode* endNode = asset.FindNode(endPin->nodeId);
            if (!startNode || !endNode) continue;
            if (startPin->valueType == EffectValueType::Flow) {
                ++flowFan[startNode->id];
            }
            LOG_INFO("[%s]   link %u: %s(%u)->%s(%u) type=%d",
                tag,
                link.id,
                EffectGraphNodeTypeToString(startNode->type),
                startNode->id,
                EffectGraphNodeTypeToString(endNode->type),
                endNode->id,
                static_cast<int>(startPin->valueType));
        }
        for (const auto& [nodeId, fan] : flowFan) {
            if (fan > 1) {
                const EffectGraphNode* n = asset.FindNode(nodeId);
                LOG_ERROR("[%s] side-effect flow fan-out>1: node id=%u type=%s fan=%u",
                    tag,
                    nodeId,
                    n ? EffectGraphNodeTypeToString(n->type) : "?",
                    fan);
            }
        }
    }

    EffectGraphNode* FindNodeOfType(EffectGraphAsset& asset, EffectGraphNodeType type)
    {
        for (auto& node : asset.nodes) {
            if (node.type == type) {
                return &node;
            }
        }
        return nullptr;
    }

    const EffectGraphNode* FindNodeOfType(const EffectGraphAsset& asset, EffectGraphNodeType type)
    {
        for (const auto& node : asset.nodes) {
            if (node.type == type) {
                return &node;
            }
        }
        return nullptr;
    }

    EffectGraphPin* FindNodePin(EffectGraphAsset& asset, uint32_t nodeId, EffectPinKind kind, EffectValueType valueType)
    {
        for (auto& pin : asset.pins) {
            if (pin.nodeId == nodeId && pin.kind == kind && pin.valueType == valueType) {
                return &pin;
            }
        }
        return nullptr;
    }

    bool HasLink(const EffectGraphAsset& asset, uint32_t startPinId, uint32_t endPinId)
    {
        return std::any_of(
            asset.links.begin(),
            asset.links.end(),
            [&](const EffectGraphLink& link) {
                return link.startPinId == startPinId && link.endPinId == endPinId;
            });
    }

    void EnsureLink(EffectGraphAsset& asset, uint32_t startPinId, uint32_t endPinId)
    {
        if (startPinId == 0 || endPinId == 0 || HasLink(asset, startPinId, endPinId)) {
            return;
        }

        EffectGraphLink link;
        link.id = asset.nextLinkId++;
        link.startPinId = startPinId;
        link.endPinId = endPinId;
        asset.links.push_back(link);
    }

    void EnsureGuiAuthoringLinks(EffectGraphAsset& asset)
    {
        auto* spawnNode        = FindNodeOfType(asset, EffectGraphNodeType::Spawn);
        auto* lifetimeNode     = FindNodeOfType(asset, EffectGraphNodeType::Lifetime);
        auto* emitterNode      = FindNodeOfType(asset, EffectGraphNodeType::ParticleEmitter);
        auto* spriteNode       = FindNodeOfType(asset, EffectGraphNodeType::SpriteRenderer);
        auto* colorNode        = FindNodeOfType(asset, EffectGraphNodeType::Color);
        auto* meshSourceNode   = FindNodeOfType(asset, EffectGraphNodeType::MeshSource);
        auto* meshRendererNode = FindNodeOfType(asset, EffectGraphNodeType::MeshRenderer);
        auto* outputNode       = FindNodeOfType(asset, EffectGraphNodeType::Output);

        if (spawnNode && lifetimeNode) {
            if (auto* startPin = FindNodePin(asset, spawnNode->id, EffectPinKind::Output, EffectValueType::Flow)) {
                if (auto* endPin = FindNodePin(asset, lifetimeNode->id, EffectPinKind::Input, EffectValueType::Flow)) {
                    EnsureLink(asset, startPin->id, endPin->id);
                }
            }
        }

        if (lifetimeNode && emitterNode) {
            if (auto* startPin = FindNodePin(asset, lifetimeNode->id, EffectPinKind::Output, EffectValueType::Flow)) {
                if (auto* endPin = FindNodePin(asset, emitterNode->id, EffectPinKind::Input, EffectValueType::Flow)) {
                    EnsureLink(asset, startPin->id, endPin->id);
                }
            }
        }

        // Mesh-only: Lifetime drives MeshRenderer directly (no particle emitter)
        if (lifetimeNode && meshRendererNode && !emitterNode) {
            if (auto* startPin = FindNodePin(asset, lifetimeNode->id, EffectPinKind::Output, EffectValueType::Flow)) {
                if (auto* endPin = FindNodePin(asset, meshRendererNode->id, EffectPinKind::Input, EffectValueType::Flow)) {
                    EnsureLink(asset, startPin->id, endPin->id);
                }
            }
        }

        if (emitterNode && spriteNode) {
            if (auto* startPin = FindNodePin(asset, emitterNode->id, EffectPinKind::Output, EffectValueType::Particle)) {
                if (auto* endPin = FindNodePin(asset, spriteNode->id, EffectPinKind::Input, EffectValueType::Particle)) {
                    EnsureLink(asset, startPin->id, endPin->id);
                }
            }
        }

        if (colorNode && spriteNode) {
            if (auto* startPin = FindNodePin(asset, colorNode->id, EffectPinKind::Output, EffectValueType::Color)) {
                if (auto* endPin = FindNodePin(asset, spriteNode->id, EffectPinKind::Input, EffectValueType::Color)) {
                    EnsureLink(asset, startPin->id, endPin->id);
                }
            }
        }

        if (meshSourceNode && meshRendererNode) {
            if (auto* startPin = FindNodePin(asset, meshSourceNode->id, EffectPinKind::Output, EffectValueType::Mesh)) {
                if (auto* endPin = FindNodePin(asset, meshRendererNode->id, EffectPinKind::Input, EffectValueType::Mesh)) {
                    EnsureLink(asset, startPin->id, endPin->id);
                }
            }
        }

        if (spriteNode && outputNode) {
            if (auto* startPin = FindNodePin(asset, spriteNode->id, EffectPinKind::Output, EffectValueType::Flow)) {
                if (auto* endPin = FindNodePin(asset, outputNode->id, EffectPinKind::Input, EffectValueType::Flow)) {
                    EnsureLink(asset, startPin->id, endPin->id);
                }
            }
        }

        if (meshRendererNode && outputNode) {
            if (auto* startPin = FindNodePin(asset, meshRendererNode->id, EffectPinKind::Output, EffectValueType::Flow)) {
                if (auto* endPin = FindNodePin(asset, outputNode->id, EffectPinKind::Input, EffectValueType::Flow)) {
                    EnsureLink(asset, startPin->id, endPin->id);
                }
            }
        }
    }
}

EffectEditorPanel::EffectEditorPanel()
{
    m_asset = CreateDefaultEffectGraphAsset();
    strcpy_s(m_documentPathBuffer, m_documentPath.c_str());

    ed::Config config;
    config.SettingsFile = nullptr;
    m_nodeEditorContext = ed::CreateEditor(&config);
}

EffectEditorPanel::~EffectEditorPanel()
{
    if (m_nodeEditorContext) {
        ed::DestroyEditor(m_nodeEditorContext);
        m_nodeEditorContext = nullptr;
    }
}

void EffectEditorPanel::SetSelectedContext(EntityID entity, const std::string& meshPath)
{
    m_selectedEntity = entity;
    m_selectedMeshPath = meshPath;
}

DirectX::XMFLOAT3 EffectEditorPanel::GetPreviewCameraPosition() const
{
    const float cosPitch = std::cos(m_previewPitch);
    const float sinPitch = std::sin(m_previewPitch);
    const float cosYaw = std::cos(m_previewYaw);
    const float sinYaw = std::sin(m_previewYaw);

    return {
        m_previewAnchor.x + cosPitch * sinYaw * m_previewDistance,
        m_previewAnchor.y + sinPitch * m_previewDistance,
        m_previewAnchor.z - cosPitch * cosYaw * m_previewDistance
    };
}

DirectX::XMFLOAT3 EffectEditorPanel::GetPreviewCameraDirection() const
{
    const DirectX::XMFLOAT3 position = GetPreviewCameraPosition();
    DirectX::XMFLOAT3 direction = {
        m_previewAnchor.x - position.x,
        m_previewAnchor.y - position.y,
        m_previewAnchor.z - position.z
    };

    const float lengthSq =
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z;
    if (lengthSq <= 0.000001f) {
        return { 0.0f, 0.0f, 1.0f };
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    direction.x *= invLength;
    direction.y *= invLength;
    direction.z *= invLength;
    return direction;
}

void EffectEditorPanel::ResetPreviewCamera()
{
    m_previewYaw = 0.85f;
    m_previewPitch = -0.18f;
    m_previewDistance = 4.5f;
}

void EffectEditorPanel::DrawWorkspace(Registry* registry, bool* outFocused)
{
    m_registry = registry;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    const ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    const bool hostOpen = ImGui::Begin("##EffectEditorWorkspaceRoot", nullptr, hostFlags);
    ImGui::PopStyleVar(3);
    if (!hostOpen) {
        ImGui::End();
        if (outFocused) {
            *outFocused = false;
        }
        return;
    }

    if (outFocused) {
        *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    DrawToolbar();

    ImGuiID dockId = ImGui::GetID("EffectEditorWorkspaceDock");
    ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    if (m_needsLayoutRebuild) {
        BuildDockLayout(dockId);
        m_needsLayoutRebuild = false;
    }

    ImGui::End();

    DrawGraphPanel();
    DrawPreviewPanel();
    if (m_showBlackboard) {
        DrawAssetPanel();
    }
    DrawTimelinePanel();
    DrawDetailsPanel();
    DrawAssetPickerPopup();
}

void EffectEditorPanel::BuildDockLayout(unsigned int dockId)
{
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImVec2(1620.0f, 920.0f));

    ImGuiID topId = dockId;
    ImGuiID bottomId = ImGui::DockBuilderSplitNode(topId, ImGuiDir_Down, 0.30f, nullptr, &topId);
    ImGuiID leftColumnId = ImGui::DockBuilderSplitNode(topId, ImGuiDir_Left, m_showBlackboard ? 0.31f : 0.26f, nullptr, &topId);
    ImGuiID rightId = ImGui::DockBuilderSplitNode(topId, ImGuiDir_Right, 0.27f, nullptr, &topId);

    ImGuiID previewId = leftColumnId;
    if (m_showBlackboard) {
        ImGuiID blackboardId = ImGui::DockBuilderSplitNode(previewId, ImGuiDir_Down, 0.42f, nullptr, &previewId);
        ImGui::DockBuilderDockWindow(kAssetWindowTitle, blackboardId);
    }

    ImGui::DockBuilderDockWindow(kPreviewWindowTitle, previewId);
    ImGui::DockBuilderDockWindow(kGraphWindowTitle, topId);
    ImGui::DockBuilderDockWindow(kDetailsWindowTitle, rightId);
    ImGui::DockBuilderDockWindow(kTimelineWindowTitle, bottomId);
    ImGui::DockBuilderFinish(dockId);
}

void EffectEditorPanel::DrawToolbar()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    if (ImGui::BeginChild("##EffectEditorToolbar", ImVec2(0.0f, 42.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
            SaveDocument();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse")) {
            LoadDocument();
        }
        ImGui::SameLine();
        if (ImGui::Button("Templates")) {
            ImGui::OpenPopup("##EffectTemplatePopup");
        }
        ImGui::SameLine();
        if (ImGui::Button(m_showBlackboard ? "Hide Blackboard" : "Blackboard")) {
            m_showBlackboard = !m_showBlackboard;
            m_needsLayoutRebuild = true;
        }
        if (ImGui::BeginPopup("##EffectTemplatePopup")) {
            const auto applyTemplate = [&](const char* effectName,
                                           float duration,
                                           float spawnRate,
                                           uint32_t burstCount,
                                           float particleLifetime,
                                           float startSize,
                                           float endSize,
                                           float speed,
                                           EffectSpawnShapeType shapeType,
                                           const DirectX::XMFLOAT3& acceleration,
                                           float drag,
                                           const DirectX::XMFLOAT3& shapeParams,
                                           float spinRate,
                                           EffectParticleDrawMode drawMode,
                                           EffectParticleSortMode sortMode,
                                           float ribbonWidth,
                                           float ribbonStretch,
                                           const char* texturePath,
                                           uint32_t subUvColumns,
                                           uint32_t subUvRows,
                                           float subUvFrameRate,
                                           float curlNoiseStrength,
                                           float curlNoiseScale,
                                           float curlNoiseScrollSpeed,
                                           float vortexStrength,
                                           const DirectX::XMFLOAT4& startColor,
                                           const DirectX::XMFLOAT4& endColor) {
                const auto resolveTemplateMaxParticles = [](float duration, float spawnRate, uint32_t burstCount, float particleLifetime, EffectParticleDrawMode /*drawMode*/) {
                    return static_cast<int>(ResolveEffectParticleMaxParticles(
                        0, spawnRate, burstCount, particleLifetime, duration));
                };

                m_asset.name = effectName;
                m_asset.previewDefaults.duration = duration;

                // Remove mesh-only nodes so particle graph has no side-effect fan-out on Lifetime.
                // Loops while any node of the type exists to defensively clear duplicates.
                const auto removeNodeIfExists = [&](EffectGraphNodeType type) {
                    while (EffectGraphNode* n = FindNodeByType(type)) {
                        const uint32_t nid = n->id;
                        std::vector<uint32_t> pinIds;
                        for (const auto& p : m_asset.pins)
                            if (p.nodeId == nid) pinIds.push_back(p.id);
                        m_asset.links.erase(std::remove_if(m_asset.links.begin(), m_asset.links.end(),
                            [&](const EffectGraphLink& l) {
                                return std::find(pinIds.begin(), pinIds.end(), l.startPinId) != pinIds.end() ||
                                       std::find(pinIds.begin(), pinIds.end(), l.endPinId) != pinIds.end();
                            }), m_asset.links.end());
                        m_asset.pins.erase(std::remove_if(m_asset.pins.begin(), m_asset.pins.end(),
                            [nid](const EffectGraphPin& p) { return p.nodeId == nid; }), m_asset.pins.end());
                        m_asset.nodes.erase(std::remove_if(m_asset.nodes.begin(), m_asset.nodes.end(),
                            [nid](const EffectGraphNode& nd) { return nd.id == nid; }), m_asset.nodes.end());
                    }
                };
                removeNodeIfExists(EffectGraphNodeType::MeshRenderer);
                removeNodeIfExists(EffectGraphNodeType::MeshSource);
                // Ensure all nodes exist first. Pointers returned by EnsureNodeByType
                // can be invalidated by subsequent push_back into m_asset.nodes, so we
                // re-query via FindNodeByType after every node has been created.
                EnsureNodeByType(EffectGraphNodeType::Spawn);
                EnsureNodeByType(EffectGraphNodeType::Output);
                EnsureNodeByType(EffectGraphNodeType::Lifetime);
                EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                EnsureNodeByType(EffectGraphNodeType::Color);
                EnsureNodeByType(EffectGraphNodeType::SpriteRenderer);

                EffectGraphNode* lifetimeNode = FindNodeByType(EffectGraphNodeType::Lifetime);
                EffectGraphNode* emitterNode  = FindNodeByType(EffectGraphNodeType::ParticleEmitter);
                EffectGraphNode* colorNode    = FindNodeByType(EffectGraphNodeType::Color);
                EffectGraphNode* spriteNode   = FindNodeByType(EffectGraphNodeType::SpriteRenderer);
                if (!lifetimeNode || !emitterNode || !colorNode || !spriteNode) {
                    return;
                }

                lifetimeNode->scalar = duration;
                emitterNode->scalar = spawnRate;
                emitterNode->scalar2 = static_cast<float>(burstCount);
                emitterNode->intValue = resolveTemplateMaxParticles(duration, spawnRate, burstCount, particleLifetime, drawMode);
                emitterNode->vectorValue = { particleLifetime, startSize, endSize, speed };
                emitterNode->vectorValue2 = { acceleration.x, acceleration.y, acceleration.z, drag };
                emitterNode->vectorValue3 = { shapeParams.x, shapeParams.y, shapeParams.z, spinRate };
                emitterNode->vectorValue4 = { curlNoiseStrength, curlNoiseScale, curlNoiseScrollSpeed, vortexStrength };
                emitterNode->intValue2 = static_cast<int>(shapeType);

                colorNode->vectorValue = startColor;
                colorNode->vectorValue2 = endColor;

                spriteNode->intValue = static_cast<int>(drawMode);
                spriteNode->intValue2 = static_cast<int>(sortMode);
                spriteNode->vectorValue = startColor;
                spriteNode->vectorValue2 = { ribbonWidth, ribbonStretch, 1.0f, subUvFrameRate };
                spriteNode->vectorValue3.z = static_cast<float>((std::max)(subUvColumns, 1u));
                spriteNode->vectorValue3.w = static_cast<float>((std::max)(subUvRows, 1u));
                spriteNode->stringValue = texturePath && texturePath[0] != '\0'
                    ? texturePath
                    : "Data/Effect/particle/particle.png";

                EnsureGuiAuthoringLinks(m_asset);
                SanitizeGraphAsset(m_asset);
                LogGraphStructure(m_asset, "applyTemplate");
                m_compileDirty = true;
                m_syncNodePositions = true;
                // Stop any running preview so the previous template's runtime
                // is torn down before the user starts the new one. Keeps
                // stale particles/meshes from bleeding between templates.
                StopPreview();
                ImGui::CloseCurrentPopup();
            };

            if (ImGui::MenuItem("Spark Fountain")) {
                applyTemplate(
            "Spark Fountain", 3.2f, 70000.0f, 0u, 1.45f, 0.08f, 0.025f, 4.2f,
                    EffectSpawnShapeType::Sphere, { 0.0f, -2.2f, 0.0f }, 0.02f, { 0.26f, 0.26f, 0.26f }, 13.0f,
                    EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront, 0.08f, 0.35f,
                    "Data/Effect/particle/spark_03.png", 1u, 1u, 0.0f,
                    0.30f, 0.35f, 0.30f, 0.0f,
                    { 0.65f, 0.95f, 1.00f, 1.0f }, { 0.15f, 0.75f, 1.00f, 0.0f });
            }
            if (ImGui::MenuItem("Smoke Plume")) {
                applyTemplate(
            "Smoke Plume", 6.0f, 24000.0f, 0u, 4.2f, 0.18f, 1.55f, 0.95f,
                    EffectSpawnShapeType::Sphere, { 0.0f, 0.18f, 0.0f }, 0.12f, { 0.65f, 0.65f, 0.65f }, 1.2f,
                    EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront, 0.14f, 0.28f,
                    "Data/Effect/particle/smoke_03.png", 1u, 1u, 0.0f,
                    0.75f, 0.10f, 0.07f, 0.28f,
                    { 0.42f, 0.43f, 0.45f, 0.82f }, { 0.10f, 0.10f, 0.10f, 0.0f });
            }
            if (ImGui::MenuItem("Magic Burst")) {
                applyTemplate(
            "Magic Burst", 2.1f, 90000.0f, 0u, 1.10f, 0.14f, 0.028f, 7.8f,
                    EffectSpawnShapeType::Sphere, { 0.0f, -0.55f, 0.0f }, 0.01f, { 0.22f, 0.22f, 0.22f }, 22.0f,
                    EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront, 0.08f, 0.30f,
                    "Data/Effect/particle/magic_03.png", 1u, 1u, 0.0f,
                    0.55f, 0.26f, 0.40f, 1.65f,
                    { 0.95f, 0.45f, 1.00f, 1.0f }, { 0.35f, 0.15f, 1.00f, 0.0f });
            }
            if (ImGui::MenuItem("Ribbon Trail")) {
                applyTemplate(
            "Ribbon Trail", 2.8f, 3000.0f, 0u, 1.20f, 0.08f, 0.025f, 1.9f,
                    EffectSpawnShapeType::Line, { 0.0f, 0.0f, 0.0f }, 0.03f, { 0.40f, 0.0f, 0.0f }, 3.0f,
                    EffectParticleDrawMode::Ribbon, EffectParticleSortMode::BackToFront, 0.12f, 1.75f,
                    "Data/Effect/particle/trace_03.png", 1u, 1u, 0.0f,
                    0.16f, 0.22f, 0.24f, 2.10f,
                    { 0.30f, 0.95f, 1.00f, 0.95f }, { 0.08f, 0.28f, 1.00f, 0.0f });
            }

            ImGui::Separator();
            ImGui::TextDisabled("-- Mesh Effects --");

            // applyMeshTemplate: sets up MeshSource + MeshRenderer nodes
            const auto applyMeshTemplate = [&](
                const char* effectName,
                float duration,
                const char* meshPath,
                int blendState,          // 2=Additive
                int shaderFlags,
                const DirectX::XMFLOAT4& tint,
                // vectorValue2: {dissolveAmount, dissolveEdge, fresnelPower, flowStrength}
                float dissolveAmount, float dissolveEdge, float fresnelPower, float flowStrength,
                // vectorValue3: {flowSpeedX, flowSpeedY, scrollSpeedX, scrollSpeedY}
                float flowSpeedX, float flowSpeedY, float scrollSpeedX, float scrollSpeedY,
                // colors
                const DirectX::XMFLOAT4& dissolveGlowColor,
                const DirectX::XMFLOAT4& fresnelColor,
                // textures
                const char* baseTexPath,
                const char* maskTexPath,
                const char* flowMapPath)
            {
                m_asset.name = effectName;
                m_asset.previewDefaults.duration = duration;
                m_asset.previewDefaults.previewMeshPath = meshPath ? meshPath : "";

                // Remove particle-only nodes so mesh graph has no side-effect fan-out on Lifetime.
                // Loops while any node of the type exists to defensively clear duplicates.
                const auto removeNodeIfExists = [&](EffectGraphNodeType type) {
                    while (EffectGraphNode* n = FindNodeByType(type)) {
                        const uint32_t nid = n->id;
                        std::vector<uint32_t> pinIds;
                        for (const auto& p : m_asset.pins)
                            if (p.nodeId == nid) pinIds.push_back(p.id);
                        m_asset.links.erase(std::remove_if(m_asset.links.begin(), m_asset.links.end(),
                            [&](const EffectGraphLink& l) {
                                return std::find(pinIds.begin(), pinIds.end(), l.startPinId) != pinIds.end() ||
                                       std::find(pinIds.begin(), pinIds.end(), l.endPinId) != pinIds.end();
                            }), m_asset.links.end());
                        m_asset.pins.erase(std::remove_if(m_asset.pins.begin(), m_asset.pins.end(),
                            [nid](const EffectGraphPin& p) { return p.nodeId == nid; }), m_asset.pins.end());
                        m_asset.nodes.erase(std::remove_if(m_asset.nodes.begin(), m_asset.nodes.end(),
                            [nid](const EffectGraphNode& nd) { return nd.id == nid; }), m_asset.nodes.end());
                    }
                };
                removeNodeIfExists(EffectGraphNodeType::ParticleEmitter);
                removeNodeIfExists(EffectGraphNodeType::SpriteRenderer);
                removeNodeIfExists(EffectGraphNodeType::Color);
                // Ensure all nodes exist first. Pointers returned by EnsureNodeByType
                // can be invalidated by subsequent push_back into m_asset.nodes, so we
                // re-query via FindNodeByType after every node has been created.
                EnsureNodeByType(EffectGraphNodeType::Spawn);
                EnsureNodeByType(EffectGraphNodeType::Output);
                EnsureNodeByType(EffectGraphNodeType::Lifetime);
                EnsureNodeByType(EffectGraphNodeType::MeshSource);
                EnsureNodeByType(EffectGraphNodeType::MeshRenderer);

                EffectGraphNode* lifetimeNode = FindNodeByType(EffectGraphNodeType::Lifetime);
                EffectGraphNode* meshSrcNode  = FindNodeByType(EffectGraphNodeType::MeshSource);
                EffectGraphNode* meshRendNode = FindNodeByType(EffectGraphNodeType::MeshRenderer);
                if (!lifetimeNode || !meshSrcNode || !meshRendNode) {
                    return;
                }

                lifetimeNode->scalar = duration;

                meshSrcNode->stringValue = meshPath ? meshPath : "";

                meshRendNode->intValue  = blendState;
                meshRendNode->intValue2 = shaderFlags;
                meshRendNode->vectorValue  = tint;
                meshRendNode->vectorValue2 = { dissolveAmount, dissolveEdge, fresnelPower, flowStrength };
                meshRendNode->vectorValue3 = { flowSpeedX, flowSpeedY, scrollSpeedX, scrollSpeedY };
                meshRendNode->vectorValue4 = { 0.0f, 0.0f, 0.0f, 0.0f };
                meshRendNode->vectorValue5 = dissolveGlowColor;
                meshRendNode->vectorValue6 = fresnelColor;
                meshRendNode->vectorValue7 = { 0.0f, 0.0f, 0.0f, 0.0f };
                meshRendNode->vectorValue8 = { 0.0f, 0.0f, 0.0f, 0.0f };
                meshRendNode->stringValue  = baseTexPath  ? baseTexPath  : "";
                meshRendNode->stringValue2 = maskTexPath  ? maskTexPath  : "";
                meshRendNode->stringValue3.clear();
                meshRendNode->stringValue4 = flowMapPath  ? flowMapPath  : "";
                meshRendNode->stringValue5.clear();
                meshRendNode->stringValue6.clear();

                EnsureGuiAuthoringLinks(m_asset);
                SanitizeGraphAsset(m_asset);
                LogGraphStructure(m_asset, "applyMeshTemplate");
                m_compileDirty = true;
                m_syncNodePositions = true;
                // Stop any running preview so the previous template's runtime
                // (and its GPU particles) are torn down. Otherwise old particles
                // from an earlier particle template keep emitting until the user
                // presses Play, which looks like mystery particles appearing on
                // template switch.
                StopPreview();
                ImGui::CloseCurrentPopup();
            };

            // Texture | Dissolve | DissolveGlow | AlphaFade | Scroll | FlowMap
            // Scroll drives UV by time for the "streaking" motion of the slash.
            // FlowMap adds subtle UV wobble via Flow.png on top of the scroll.
            static constexpr int kMeshFlag_SlashGlow  = 0x001 | 0x002 | 0x200 | 0x4000 | 0x100000 | 0x1000; // 0x105203
            // Texture | FlowMap | Scroll | AlphaFade
            static constexpr int kMeshFlag_MagicCircle = 0x001 | 0x1000 | 0x100000 | 0x4000; // 0x105001
            // Texture | Dissolve | AlphaFade
            static constexpr int kMeshFlag_Shockwave  = 0x001 | 0x002 | 0x4000; // 0x4003
            // Texture | Fresnel | FlowMap | AlphaFade
            static constexpr int kMeshFlag_TornadoAura = 0x001 | 0x020 | 0x1000 | 0x4000; // 0x5021

            if (ImGui::MenuItem("Sword Slash Glow")) {
                applyMeshTemplate(
                    "Sword Slash Glow", 1.2f,
                    "Data/Model/Slash/fbx_slash_001_1.fbx",
                    2, kMeshFlag_SlashGlow,
                    { 1.0f, 0.9f, 0.35f, 1.0f },
                    // dissolveAmount is lifetime-driven in EffectSystems, so
                    // the static value here only sets the initial frame state
                    // (0 = fully visible at t=0). dissolveEdge 0.10 gives the
                    // dissolve boundary a visible glow band. flowStrength 0.20
                    // adds a moderate FlowMap-driven UV wobble on top of scroll.
                    0.0f, 0.10f, 1.0f, 0.20f,
                    // gFlowSpeed is not referenced by the current PS, so leave
                    // at 0. scrollSpeedX = 2.5 streaks the UV along the slash
                    // arc at a believable speed; scrollSpeedY stays 0 so the
                    // streak only runs along the blade direction.
                    0.0f, 0.0f, 2.5f, 0.0f,
                    { 1.0f, 0.5f, 0.1f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },
                    "Data/Effect/Effect/Aura01_T.png",
                    "Data/Effect/Mask/dissolve_animation.png",
                    "Data/Effect/Flow/Flow.png");
            }
            if (ImGui::MenuItem("Magic Circle")) {
                applyMeshTemplate(
                    "Magic Circle", 3.0f,
                    "Data/Model/ring/fbx_ring_002.fbx",
                    2, kMeshFlag_MagicCircle,
                    { 0.55f, 0.25f, 1.0f, 1.0f },
                    0.0f, 0.05f, 1.0f, 0.3f,
                    0.2f, 0.1f, 0.15f, 0.0f,
                    { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.6f, 0.3f, 1.0f, 1.0f },
                    "Data/Effect/Effect/AuroraRing.png",
                    nullptr,
                    "Data/Effect/Flow/Flow.png");
            }
            if (ImGui::MenuItem("Shockwave")) {
                applyMeshTemplate(
                    "Shockwave", 0.6f,
                    "Data/Model/shockwave/fbx_shockwave_001.fbx",
                    2, kMeshFlag_Shockwave,
                    { 0.75f, 0.95f, 1.0f, 1.0f },
                    0.0f, 0.05f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f,
                    { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },
                    "Data/Effect/Effect/Burst01.png",
                    "Data/Effect/Mask/dissolve_animation.png",
                    nullptr);
            }
            if (ImGui::MenuItem("Tornado Aura")) {
                applyMeshTemplate(
                    "Tornado Aura", 4.0f,
                    "Data/Model/cylinderTornade/fbx_cylinderTornade_001.fbx",
                    2, kMeshFlag_TornadoAura,
                    { 0.25f, 1.0f, 0.45f, 1.0f },
                    0.0f, 0.05f, 2.5f, 0.4f,
                    0.1f, 0.3f, 0.0f, 0.0f,
                    { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.2f, 1.0f, 0.5f, 1.0f },
                    "Data/Effect/Effect/Aura.png",
                    nullptr,
                    "Data/Effect/Flow/Flow.png");
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(360.0f);
        if (ImGui::InputText("##EffectGraphDocumentPath", m_documentPathBuffer, sizeof(m_documentPathBuffer))) {
            m_documentPath = m_documentPathBuffer;
        }
        ImGui::SameLine();
        const bool hasErrors = m_compiled && !m_compiled->errors.empty();
        if (hasErrors) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " Compile Errors");
        } else if (m_compileDirty) {
            ImGui::TextDisabled(ICON_FA_BROOM " Compile Dirty");
        } else {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.0f), ICON_FA_CIRCLE_CHECK " Compiled");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

bool EffectEditorPanel::CanCreateLink(uint32_t startPinId, uint32_t endPinId, std::string& reason) const
{
    const EffectGraphPin* startPin = m_asset.FindPin(startPinId);
    const EffectGraphPin* endPin = m_asset.FindPin(endPinId);
    if (!startPin || !endPin) {
        reason = "Invalid pin";
        return false;
    }
    if (startPin->nodeId == endPin->nodeId) {
        reason = "Same node";
        return false;
    }
    if (startPin->kind != EffectPinKind::Output || endPin->kind != EffectPinKind::Input) {
        reason = "Output -> Input only";
        return false;
    }
    if (startPin->valueType != endPin->valueType) {
        reason = "Type mismatch";
        return false;
    }
    for (const auto& link : m_asset.links) {
        if (link.startPinId == startPinId && link.endPinId == endPinId) {
            reason = "Duplicate link";
            return false;
        }
        if (link.endPinId == endPinId) {
            reason = "Input already connected";
            return false;
        }
    }
    return true;
}

void EffectEditorPanel::RemoveNode(uint32_t nodeId)
{
    std::vector<uint32_t> pinsToRemove;
    for (const auto& pin : m_asset.pins) {
        if (pin.nodeId == nodeId) {
            pinsToRemove.push_back(pin.id);
        }
    }

    m_asset.links.erase(
        std::remove_if(
            m_asset.links.begin(),
            m_asset.links.end(),
            [&](const EffectGraphLink& link) {
                return std::find(pinsToRemove.begin(), pinsToRemove.end(), link.startPinId) != pinsToRemove.end() ||
                    std::find(pinsToRemove.begin(), pinsToRemove.end(), link.endPinId) != pinsToRemove.end();
            }),
        m_asset.links.end());

    m_asset.pins.erase(
        std::remove_if(
            m_asset.pins.begin(),
            m_asset.pins.end(),
            [nodeId](const EffectGraphPin& pin) { return pin.nodeId == nodeId; }),
        m_asset.pins.end());

    m_asset.nodes.erase(
        std::remove_if(
            m_asset.nodes.begin(),
            m_asset.nodes.end(),
            [nodeId](const EffectGraphNode& node) { return node.id == nodeId; }),
        m_asset.nodes.end());

    if (m_selectedNodeId == nodeId) {
        m_selectedNodeId = 0;
    }
    m_compileDirty = true;
}

EffectGraphNode* EffectEditorPanel::FindNodeByType(EffectGraphNodeType type)
{
    for (auto& node : m_asset.nodes) {
        if (node.type == type) {
            return &node;
        }
    }
    return nullptr;
}

EffectGraphNode* EffectEditorPanel::EnsureNodeByType(EffectGraphNodeType type)
{
    if (EffectGraphNode* node = FindNodeByType(type)) {
        return node;
    }

    EffectGraphNode& addedNode = AddEffectGraphNode(m_asset, type, { 0.0f, 0.0f });
    m_compileDirty = true;
    return m_asset.FindNode(addedNode.id);
}

void EffectEditorPanel::OpenAssetPicker(AssetPickerKind kind, uint32_t nodeId, bool targetPreviewMesh)
{
    m_assetPickerKind = kind;
    m_assetPickerNodeId = nodeId;
    m_assetPickerTargetsPreviewMesh = targetPreviewMesh;
    m_assetPickerView = AssetPickerView::All;
    m_assetPickerSearch[0] = '\0';
    m_assetPickerOpenRequested = true;
}

bool EffectEditorPanel::DrawAssetSlotControl(const char* label, std::string& path, AssetPickerKind kind, uint32_t nodeId, bool targetPreviewMesh)
{
    bool changed = false;

    ImGui::TextUnformatted(label);
    ImGui::PushID(label);
    ImGui::BeginChild("##AssetSlot", ImVec2(0.0f, 92.0f), true);

    void* textureId = nullptr;
    if (!path.empty()) {
        if (kind == AssetPickerKind::Mesh) {
            ThumbnailGenerator::Instance().Request(path);
            if (auto thumb = ThumbnailGenerator::Instance().Get(path)) {
                textureId = ImGuiRenderer::GetTextureID(thumb.get());
            }
        } else if (kind == AssetPickerKind::Texture) {
            if (auto texture = ResourceManager::Instance().GetTexture(path)) {
                textureId = ImGuiRenderer::GetTextureID(texture.get());
            }
        }
    }

    const ImVec2 previewButtonSize(72.0f, 72.0f);
    bool openPicker = false;
    if (textureId) {
        openPicker = ImGui::ImageButton("##Thumb", textureId, previewButtonSize);
    } else {
        openPicker = ImGui::Button(kind == AssetPickerKind::Mesh ? ICON_FA_CUBE : ICON_FA_IMAGE, previewButtonSize);
    }
    changed |= AcceptAssetDropPayload(path, kind);
    if (openPicker) {
        OpenAssetPicker(kind, nodeId, targetPreviewMesh);
    }

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextWrapped("%s", path.empty() ? "No asset assigned" : path.c_str());
    if (!path.empty()) {
        if (ImGui::Button(IsFavoriteAsset(path, kind) ? ICON_FA_STAR " Favorite" : ICON_FA_STAR_HALF_STROKE " Favorite")) {
            ToggleFavoriteAsset(path, kind);
        }
        ImGui::SameLine();
    }
    if (ImGui::Button("Browse")) {
        OpenAssetPicker(kind, nodeId, targetPreviewMesh);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        path.clear();
        changed = true;
    }
    ImGui::EndGroup();
    ImGui::EndChild();
    ImGui::PopID();

    if (changed) {
        m_compileDirty = true;
    }
    return changed;
}

std::string* EffectEditorPanel::GetAssetPickerTargetPath()
{
    if (m_assetPickerTargetsPreviewMesh && m_assetPickerKind == AssetPickerKind::Mesh) {
        return &m_asset.previewDefaults.previewMeshPath;
    }

    if (EffectGraphNode* targetNode = m_asset.FindNode(m_assetPickerNodeId)) {
        return &targetNode->stringValue;
    }

    if (m_assetPickerKind == AssetPickerKind::Mesh) {
        return &m_asset.previewDefaults.previewMeshPath;
    }

    return nullptr;
}

void EffectEditorPanel::AssignPickedAsset(const std::string& path)
{
    if (!IsCompatibleAssetPath(path, m_assetPickerKind)) {
        return;
    }

    if (std::string* targetPath = GetAssetPickerTargetPath()) {
        *targetPath = path;
        m_compileDirty = true;
        TouchRecentAsset(path, m_assetPickerKind);
    }

    if (EffectGraphNode* targetNode = m_asset.FindNode(m_assetPickerNodeId)) {
        m_selectedNodeId = targetNode->id;
    }
}

void EffectEditorPanel::TouchRecentAsset(const std::string& path, AssetPickerKind kind)
{
    if (kind == AssetPickerKind::Mesh) {
        PushRecentPath(m_recentMeshAssets, path);
    } else if (kind == AssetPickerKind::Texture) {
        PushRecentPath(m_recentTextureAssets, path);
    }
}

bool EffectEditorPanel::IsFavoriteAsset(const std::string& path, AssetPickerKind kind) const
{
    if (kind == AssetPickerKind::Mesh) {
        return ContainsExactPath(m_favoriteMeshAssets, path);
    }
    if (kind == AssetPickerKind::Texture) {
        return ContainsExactPath(m_favoriteTextureAssets, path);
    }
    return false;
}

void EffectEditorPanel::ToggleFavoriteAsset(const std::string& path, AssetPickerKind kind)
{
    if (kind == AssetPickerKind::Mesh) {
        TogglePath(m_favoriteMeshAssets, path);
    } else if (kind == AssetPickerKind::Texture) {
        TogglePath(m_favoriteTextureAssets, path);
    }
}

bool EffectEditorPanel::IsCompatibleAssetPath(const std::string& path, AssetPickerKind kind) const
{
    const std::filesystem::path fsPath(path);
    if (kind == AssetPickerKind::Mesh) {
        return HasModelExtension(fsPath);
    }
    if (kind == AssetPickerKind::Texture) {
        return HasTextureExtension(fsPath);
    }
    return false;
}

bool EffectEditorPanel::AcceptAssetDropPayload(std::string& path, AssetPickerKind kind)
{
    bool changed = false;
    if (!ImGui::BeginDragDropTarget()) {
        return false;
    }

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENGINE_ASSET")) {
        std::string droppedPath(static_cast<const char*>(payload->Data));
        if (IsCompatibleAssetPath(droppedPath, kind)) {
            path = droppedPath;
            TouchRecentAsset(droppedPath, kind);
            m_compileDirty = true;
            changed = true;
        }
    }

    ImGui::EndDragDropTarget();
    return changed;
}

EffectParameterOverrideComponent EffectEditorPanel::BuildSuggestedOverrideComponent() const
{
    EffectParameterOverrideComponent component;
    component.enabled = false;

    for (const auto& parameter : m_asset.exposedParameters) {
        if (parameter.valueType == EffectValueType::Float && component.scalarParameter.empty()) {
            component.scalarParameter = parameter.name;
            component.scalarValue = parameter.defaultValue.x;
        } else if (parameter.valueType == EffectValueType::Color && component.colorParameter.empty()) {
            component.colorParameter = parameter.name;
            component.colorValue = parameter.defaultValue;
        }
    }

    return component;
}

void EffectEditorPanel::EnsureRuntimeOverrideComponent(EntityID entity)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        return;
    }

    EffectParameterOverrideComponent suggested = BuildSuggestedOverrideComponent();
    if (suggested.scalarParameter.empty() && suggested.colorParameter.empty()) {
        return;
    }

    auto* existing = m_registry->GetComponent<EffectParameterOverrideComponent>(entity);
    if (!existing) {
        m_registry->AddComponent(entity, suggested);
        return;
    }

    if (existing->scalarParameter.empty() && !suggested.scalarParameter.empty()) {
        existing->scalarParameter = suggested.scalarParameter;
        existing->scalarValue = suggested.scalarValue;
    }
    if (existing->colorParameter.empty() && !suggested.colorParameter.empty()) {
        existing->colorParameter = suggested.colorParameter;
        existing->colorValue = suggested.colorValue;
    }
}

void EffectEditorPanel::DrawRuntimeOverrideSection(const char* label, EntityID entity, bool autoAttach)
{
    if (!m_registry || Entity::IsNull(entity) || !m_registry->IsAlive(entity)) {
        ImGui::TextDisabled("%s unavailable.", label);
        return;
    }

    if (autoAttach) {
        EnsureRuntimeOverrideComponent(entity);
    }

    auto* overrideComponent = m_registry->GetComponent<EffectParameterOverrideComponent>(entity);
    if (!overrideComponent) {
        ImGui::TextDisabled("%s has no runtime override component.", label);
        if (ImGui::Button((std::string("Add Override##") + label).c_str())) {
            EnsureRuntimeOverrideComponent(entity);
        }
        return;
    }

    auto drawParameterCombo = [&](const char* comboLabel, EffectValueType type, std::string& currentName) {
        std::vector<const EffectExposedParameter*> candidates;
        for (const auto& parameter : m_asset.exposedParameters) {
            if (parameter.valueType == type) {
                candidates.push_back(&parameter);
            }
        }

        if (candidates.empty()) {
            ImGui::TextDisabled("No %s parameters.", type == EffectValueType::Float ? "float" : "color");
            return;
        }

        const char* previewValue = currentName.empty() ? "(None)" : currentName.c_str();
        if (ImGui::BeginCombo(comboLabel, previewValue)) {
            for (const auto* candidate : candidates) {
                const bool selected = currentName == candidate->name;
                if (ImGui::Selectable(candidate->name.c_str(), selected)) {
                    currentName = candidate->name;
                    if (type == EffectValueType::Float) {
                        overrideComponent->scalarValue = candidate->defaultValue.x;
                    } else {
                        overrideComponent->colorValue = candidate->defaultValue;
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    };

    ImGui::TextUnformatted(label);
    ImGui::Checkbox((std::string("Enabled##") + label).c_str(), &overrideComponent->enabled);
    ImGui::Separator();

    if (!overrideComponent->scalarParameter.empty()) {
        drawParameterCombo((std::string("Float Parameter##") + label).c_str(), EffectValueType::Float, overrideComponent->scalarParameter);
        ImGui::DragFloat((std::string("Float Value##") + label).c_str(), &overrideComponent->scalarValue, 0.01f);
        if (const char* binding = EffectParameterBindings::DescribeBinding(EffectValueType::Float, overrideComponent->scalarParameter)) {
            ImGui::TextDisabled("Binding: %s", binding);
        } else {
            ImGui::TextDisabled("Binding: custom / inactive");
        }
    } else {
        ImGui::TextDisabled("No float override selected.");
    }

    ImGui::Spacing();

    if (!overrideComponent->colorParameter.empty()) {
        drawParameterCombo((std::string("Color Parameter##") + label).c_str(), EffectValueType::Color, overrideComponent->colorParameter);
        ImGui::ColorEdit4((std::string("Color Value##") + label).c_str(), &overrideComponent->colorValue.x);
        if (const char* binding = EffectParameterBindings::DescribeBinding(EffectValueType::Color, overrideComponent->colorParameter)) {
            ImGui::TextDisabled("Binding: %s", binding);
        } else {
            ImGui::TextDisabled("Binding: custom / inactive");
        }
    } else {
        ImGui::TextDisabled("No color override selected.");
    }
}

void EffectEditorPanel::DrawGraphPanel()
{
    if (!ImGui::Begin(kGraphWindowTitle)) {
        ImGui::End();
        return;
    }
    std::vector<EffectGraphNodeType> pendingAdds;
    const auto queueNodeAdd = [&](EffectGraphNodeType type) {
        pendingAdds.push_back(type);
    };

    auto findNodeOfType = [&](EffectGraphNodeType type) -> EffectGraphNode* {
        for (auto& node : m_asset.nodes) {
            if (node.type == type) {
                return &node;
            }
        }
        return nullptr;
    };

    EffectGraphNode* spawnNode = findNodeOfType(EffectGraphNodeType::Spawn);
    EffectGraphNode* lifetimeNode = findNodeOfType(EffectGraphNodeType::Lifetime);
    EffectGraphNode* emitterNode = findNodeOfType(EffectGraphNodeType::ParticleEmitter);
    EffectGraphNode* spriteNode = findNodeOfType(EffectGraphNodeType::SpriteRenderer);
    EffectGraphNode* meshRendererNode = findNodeOfType(EffectGraphNodeType::MeshRenderer);
    EffectGraphNode* meshSourceNode = findNodeOfType(EffectGraphNodeType::MeshSource);
    EffectGraphNode* colorNode = findNodeOfType(EffectGraphNodeType::Color);
    EffectGraphNode* outputNode = findNodeOfType(EffectGraphNodeType::Output);

    const auto hasGraphInput = [&](const EffectGraphNode& node) {
        for (const auto& pin : m_asset.pins) {
            if (pin.nodeId != node.id || pin.kind != EffectPinKind::Input) {
                continue;
            }
            const auto linked = std::find_if(
                m_asset.links.begin(),
                m_asset.links.end(),
                [&](const EffectGraphLink& link) { return link.endPinId == pin.id; });
            if (linked != m_asset.links.end()) {
                return true;
            }
        }
        return false;
    };

    const auto hasParameterBinding = [&](const EffectGraphNode& node) {
        switch (node.type) {
        case EffectGraphNodeType::ParticleEmitter:
            return std::any_of(
                m_asset.exposedParameters.begin(),
                m_asset.exposedParameters.end(),
                [](const EffectExposedParameter& parameter) {
                    return parameter.name == "SpawnRate" ||
                        parameter.name == "Lifetime" ||
                        parameter.name == "Size" ||
                        parameter.name == "Speed";
                });
        case EffectGraphNodeType::Color:
        case EffectGraphNodeType::SpriteRenderer:
        case EffectGraphNodeType::MeshRenderer:
            return std::any_of(
                m_asset.exposedParameters.begin(),
                m_asset.exposedParameters.end(),
                [](const EffectExposedParameter& parameter) {
                    return parameter.name == "Tint" || parameter.name == "Color";
                });
        default:
            return false;
        }
    };

    const auto bindingLabelForNode = [&](const EffectGraphNode& node) -> const char* {
        if (hasGraphInput(node)) {
            return "Graph";
        }
        if (hasParameterBinding(node)) {
            return "Param";
        }
        return "Const";
    };

    const auto warningLabelForNode = [&](const EffectGraphNode& node) -> const char* {
        switch (node.type) {
        case EffectGraphNodeType::MeshSource:
            return node.stringValue.empty() ? "Missing Mesh" : nullptr;
        case EffectGraphNodeType::MeshRenderer:
            return node.stringValue.empty() ? "Missing Material" : nullptr;
        case EffectGraphNodeType::ParticleEmitter:
            if (node.intValue <= 0) {
                return "Zero Max";
            }
            if (IsEffectParticleMaxParticlesTooLow(
                node.intValue,
                node.scalar,
                node.scalar2 > 0.0f ? static_cast<uint32_t>(node.scalar2) : 0u,
                node.vectorValue.x,
                lifetimeNode ? lifetimeNode->scalar : m_asset.previewDefaults.duration)) {
                return "Cap Low";
            }
            return nullptr;
        default:
            return nullptr;
        }
    };

    const auto summaryForNode = [&](const EffectGraphNode& node) {
        std::string summary;
        char buffer[128] = {};
        switch (node.type) {
        case EffectGraphNodeType::Spawn:
            summary = "System entry";
            break;
        case EffectGraphNodeType::Lifetime:
            sprintf_s(buffer, "Duration %.2fs", node.scalar);
            summary = buffer;
            break;
        case EffectGraphNodeType::ParticleEmitter:
            if (node.vectorValue4.x > 0.01f || std::abs(node.vectorValue4.w) > 0.01f) {
                sprintf_s(buffer, "Spawn %.0f  Life %.2f  Curl %.2f  Vortex %.2f", node.scalar, node.vectorValue.x, node.vectorValue4.x, node.vectorValue4.w);
            } else {
                sprintf_s(buffer, "Spawn %.0f  Life %.2f  Max %d", node.scalar, node.vectorValue.x, node.intValue);
            }
            summary = buffer;
            break;
        case EffectGraphNodeType::Color:
            summary = "Start/End color";
            break;
        case EffectGraphNodeType::SpriteRenderer: {
            const char* drawModes[] = { "Billboard", "Mesh", "Ribbon" };
            const int modeIndex = std::clamp(node.intValue, 0, 2);
            sprintf_s(buffer, "%s  %s", drawModes[modeIndex], node.stringValue.empty() ? "No Texture" : "Texture");
            summary = buffer;
            break;
        }
        case EffectGraphNodeType::MeshRenderer:
            summary = node.stringValue.empty() ? "No material assigned" : "Material assigned";
            break;
        case EffectGraphNodeType::MeshSource:
            summary = node.stringValue.empty() ? "No mesh assigned" : std::filesystem::path(node.stringValue).filename().string();
            break;
        case EffectGraphNodeType::Output:
            summary = "System output";
            break;
        default:
            summary = EffectGraphNodeTypeToString(node.type);
            break;
        }
        return summary;
    };

    auto moduleRow = [&](const char* label, const char* value, EffectGraphNode* backingNode, const ImVec4& accent, EffectGraphNodeType addType, float rowWidth) {
        ImGui::PushID(label);

        const ImVec2 rowPos = ImGui::GetCursorScreenPos();
        const float clampedWidth = (std::max)(rowWidth, 120.0f);
        const float rowHeight = 42.0f;
        const bool selected = backingNode && m_selectedNodeId == backingNode->id;
        const char* bindingLabel = backingNode ? bindingLabelForNode(*backingNode) : nullptr;
        const char* warningLabel = backingNode ? warningLabelForNode(*backingNode) : nullptr;
        const std::string summary = backingNode ? summaryForNode(*backingNode) : value;
        const bool hasPlusMinusQuickEdit =
            backingNode && (backingNode->type == EffectGraphNodeType::ParticleEmitter || backingNode->type == EffectGraphNodeType::Lifetime);
        const bool hasCycleQuickEdit =
            backingNode && (backingNode->type == EffectGraphNodeType::SpriteRenderer || backingNode->type == EffectGraphNodeType::MeshRenderer);

        const ImVec2 rowMax(rowPos.x + clampedWidth, rowPos.y + rowHeight);
        const ImVec2 minusMin(rowMax.x - 38.0f, rowPos.y + 8.0f);
        const ImVec2 minusMax(rowMax.x - 22.0f, rowPos.y + 24.0f);
        const ImVec2 plusMin(rowMax.x - 20.0f, rowPos.y + 8.0f);
        const ImVec2 plusMax(rowMax.x - 4.0f, rowPos.y + 24.0f);

        ImGui::InvisibleButton("##ModuleRow", ImVec2(clampedWidth, rowHeight));
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 mousePos = ImGui::GetMousePos();
        if (ImGui::IsItemClicked()) {
            if (backingNode) {
                const bool inMinus =
                    hasPlusMinusQuickEdit &&
                    mousePos.x >= minusMin.x && mousePos.x <= minusMax.x &&
                    mousePos.y >= minusMin.y && mousePos.y <= minusMax.y;
                const bool inPlus =
                    (hasPlusMinusQuickEdit || hasCycleQuickEdit) &&
                    mousePos.x >= plusMin.x && mousePos.x <= plusMax.x &&
                    mousePos.y >= plusMin.y && mousePos.y <= plusMax.y;

                if (inMinus && backingNode->type == EffectGraphNodeType::ParticleEmitter) {
                    backingNode->scalar = (std::max)(0.0f, backingNode->scalar - 4.0f);
                    m_compileDirty = true;
                } else if (inMinus && backingNode->type == EffectGraphNodeType::Lifetime) {
                    backingNode->scalar = (std::max)(0.10f, backingNode->scalar - 0.25f);
                    m_asset.previewDefaults.duration = backingNode->scalar;
                    m_compileDirty = true;
                } else if (inPlus && backingNode->type == EffectGraphNodeType::ParticleEmitter) {
                    backingNode->scalar += 4.0f;
                    m_compileDirty = true;
                } else if (inPlus && backingNode->type == EffectGraphNodeType::Lifetime) {
                    backingNode->scalar += 0.25f;
                    m_asset.previewDefaults.duration = backingNode->scalar;
                    m_compileDirty = true;
                } else if (inPlus && backingNode->type == EffectGraphNodeType::SpriteRenderer) {
                    backingNode->intValue = (backingNode->intValue + 1) % 3;
                    m_compileDirty = true;
                } else if (inPlus && backingNode->type == EffectGraphNodeType::MeshRenderer) {
                    backingNode->intValue = (backingNode->intValue + 1) % 6;
                    m_compileDirty = true;
                } else {
                    m_selectedNodeId = backingNode->id;
                    m_selectedLinkId = 0;
                }
            } else {
                queueNodeAdd(addType);
            }
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 bgColor = IM_COL32(42, 44, 48, 255);
        if (hovered) {
            bgColor = IM_COL32(50, 53, 58, 255);
        }
        if (selected) {
            bgColor = IM_COL32(27, 78, 146, 255);
        }
        drawList->AddRectFilled(rowPos, rowMax, bgColor, 3.0f);
        drawList->AddRectFilled(rowPos, ImVec2(rowPos.x + 4.0f, rowMax.y), ImColor(accent), 3.0f, ImDrawFlags_RoundCornersLeft);

        const float labelY = rowPos.y + 4.0f;
        const float summaryY = rowPos.y + 22.0f;
        drawList->AddText(ImVec2(rowPos.x + 10.0f, labelY), IM_COL32(226, 229, 235, 255), label);
        drawList->AddText(ImVec2(rowPos.x + 10.0f, summaryY), IM_COL32(150, 155, 165, 255), summary.c_str());

        if (backingNode) {
            if (bindingLabel) {
                const ImVec2 bindSize = ImGui::CalcTextSize(bindingLabel);
                const ImVec2 bindMin(rowMax.x - bindSize.x - 88.0f, rowPos.y + 4.0f);
                const ImVec2 bindMax(bindMin.x + bindSize.x + 10.0f, bindMin.y + 16.0f);
                drawList->AddRectFilled(bindMin, bindMax, IM_COL32(33, 54, 88, 255), 4.0f);
                drawList->AddText(ImVec2(bindMin.x + 5.0f, bindMin.y + 2.0f), IM_COL32(170, 210, 255, 255), bindingLabel);
            }

            if (warningLabel) {
                const ImVec2 warnSize = ImGui::CalcTextSize(warningLabel);
                const ImVec2 warnMin(rowMax.x - warnSize.x - 88.0f, rowPos.y + 22.0f);
                const ImVec2 warnMax(warnMin.x + warnSize.x + 10.0f, warnMin.y + 16.0f);
                drawList->AddRectFilled(warnMin, warnMax, IM_COL32(96, 50, 28, 255), 4.0f);
                drawList->AddText(ImVec2(warnMin.x + 5.0f, warnMin.y + 2.0f), IM_COL32(255, 190, 120, 255), warningLabel);
            }

            if (hasPlusMinusQuickEdit) {
                drawList->AddRectFilled(minusMin, minusMax, IM_COL32(58, 60, 66, 255), 3.0f);
                drawList->AddRectFilled(plusMin, plusMax, IM_COL32(58, 60, 66, 255), 3.0f);
                drawList->AddText(ImVec2(minusMin.x + 4.0f, minusMin.y + 1.0f), IM_COL32(220, 224, 232, 255), "-");
                drawList->AddText(ImVec2(plusMin.x + 4.0f, plusMin.y + 1.0f), IM_COL32(220, 224, 232, 255), "+");
            } else if (hasCycleQuickEdit) {
                drawList->AddRectFilled(plusMin, plusMax, IM_COL32(58, 60, 66, 255), 3.0f);
                drawList->AddText(ImVec2(plusMin.x + 2.0f, plusMin.y + 1.0f), IM_COL32(220, 224, 232, 255), ">");
            }
        } else {
            drawList->AddText(
                ImVec2(rowMax.x - 16.0f, labelY),
                IM_COL32(105, 220, 105, 255),
                "+");
        }

        ImGui::PopID();
    };

    auto beginSection = [&](const char* label, const ImVec4& accent, const char* actionLabel, EffectGraphNodeType addType, float rowWidth) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(accent.x, accent.y, accent.z, 0.22f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accent.x, accent.y, accent.z, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accent.x, accent.y, accent.z, 0.34f));
        const float startX = ImGui::GetCursorPosX();
        const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor(3);
        ImGui::SameLine(startX + rowWidth - 24.0f);
        if (ImGui::SmallButton(actionLabel)) {
            queueNodeAdd(addType);
        }
        return open;
    };

    if (ImGui::BeginChild("##SystemOverviewHeader", ImVec2(0.0f, 28.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::TextDisabled("System Overview");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.74f, 0.76f, 0.82f, 1.0f), "%s", m_asset.name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add Module")) {
            ImGui::OpenPopup("##EffectOverviewAddModule");
        }
        if (ImGui::BeginPopup("##EffectOverviewAddModule")) {
            const auto addMenuItem = [&](const char* label, EffectGraphNodeType type) {
                if (ImGui::MenuItem(label)) {
                    queueNodeAdd(type);
                }
            };
            addMenuItem("Spawn", EffectGraphNodeType::Spawn);
            addMenuItem("Lifetime", EffectGraphNodeType::Lifetime);
            addMenuItem("Particle Emitter", EffectGraphNodeType::ParticleEmitter);
            addMenuItem("Color", EffectGraphNodeType::Color);
            addMenuItem("Sprite Renderer", EffectGraphNodeType::SpriteRenderer);
            addMenuItem("Mesh Source", EffectGraphNodeType::MeshSource);
            addMenuItem("Mesh Renderer", EffectGraphNodeType::MeshRenderer);
            addMenuItem("Float", EffectGraphNodeType::Float);
            addMenuItem("Vec3", EffectGraphNodeType::Vec3);
            addMenuItem("Output", EffectGraphNodeType::Output);
            ImGui::EndPopup();
        }
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 72.0f);
        ImGui::TextDisabled(m_authoringMode == AuthoringMode::Stack ? "Stack" : "Node");
    }
    ImGui::EndChild();

    if (ImGui::BeginTabBar("##EffectAuthoringModes")) {
        if (ImGui::BeginTabItem("Stack", nullptr, m_authoringMode == AuthoringMode::Stack ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
            m_authoringMode = AuthoringMode::Stack;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Node", nullptr, m_authoringMode == AuthoringMode::Node ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
            m_authoringMode = AuthoringMode::Node;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();

    if (m_authoringMode == AuthoringMode::Node) {
        DrawNodeModePanel();
        ImGui::End();
        return;
    }

    const ImVec2 graphAvail = ImGui::GetContentRegionAvail();
    const float systemCardWidth = 250.0f;
    const float emitterCardWidth = 320.0f;

    ed::SetCurrentEditor(m_nodeEditorContext);
    ed::Begin("##NiagaraOverviewCanvas");

    if (m_syncNodePositions) {
        const float padding = 32.0f;
        const float spacing = 36.0f;
        const float systemX = padding;
        const float systemY = padding;
        const float maxEmitterX = (std::max)(padding, graphAvail.x - emitterCardWidth - padding);
        const float emitterX = (std::min)(systemX + systemCardWidth + spacing, maxEmitterX);
        const float emitterY = padding;

        ed::SetNodePosition(ed::NodeId(kSystemVisualNodeId), ImVec2(systemX, systemY));
        ed::SetNodePosition(ed::NodeId(kEmitterVisualNodeId), ImVec2(emitterX, emitterY));
        ed::NavigateToContent(0.0f);
        m_syncNodePositions = false;
    }

    constexpr float kSystemCardWidth = 250.0f;
    constexpr float kEmitterCardWidth = 320.0f;

    ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(35, 39, 44));
    ed::BeginNode(ed::NodeId(kSystemVisualNodeId));
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.24f, 0.76f, 0.96f, 1.0f), "NewNiagaraSystem");
    ImGui::TextDisabled(m_compileDirty ? "Draft" : "Compiled");
    ImGui::Separator();
    moduleRow("Properties", "Effect Asset", outputNode, ImVec4(0.65f, 0.65f, 0.70f, 1.0f), EffectGraphNodeType::Output, kSystemCardWidth);
    ImGui::TextDisabled("Blackboard");
    ImGui::TextWrapped("Open Blackboard only when you need to define parameters.");
    ImGui::Spacing();
    moduleRow("System Spawn", spawnNode ? "Spawn" : "Add module", spawnNode, NiagaraSectionColor(EffectGraphNodeType::Spawn), EffectGraphNodeType::Spawn, kSystemCardWidth);
    moduleRow("System Update", lifetimeNode ? "Lifetime" : "Add module", lifetimeNode, NiagaraSectionColor(EffectGraphNodeType::Lifetime), EffectGraphNodeType::Lifetime, kSystemCardWidth);
    moduleRow("System State", outputNode ? "Output" : "Add module", outputNode, ImVec4(0.90f, 0.90f, 0.36f, 1.0f), EffectGraphNodeType::Output, kSystemCardWidth);
    ImGui::EndGroup();
    ed::EndNode();
    ed::PopStyleColor();

    ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(40, 35, 27));
    ed::BeginNode(ed::NodeId(kEmitterVisualNodeId));
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.98f, 0.56f, 0.15f, 1.0f), "Fountain");
    if (emitterNode) {
        ImGui::TextDisabled("Spawn %.0f  Max %d", emitterNode->scalar, emitterNode->intValue);
    } else {
        ImGui::TextDisabled("Niagara Emitter");
    }
    ImGui::Separator();

    if (beginSection("Properties", NiagaraSectionColor(EffectGraphNodeType::ParticleEmitter), "+", EffectGraphNodeType::ParticleEmitter, kEmitterCardWidth)) {
        moduleRow("Emitter Properties", emitterNode ? "Spawn / Max count" : "Add emitter", emitterNode, NiagaraSectionColor(EffectGraphNodeType::ParticleEmitter), EffectGraphNodeType::ParticleEmitter, kEmitterCardWidth);
    }
    if (beginSection("Emitter Spawn", NiagaraSectionColor(EffectGraphNodeType::ParticleEmitter), "+", EffectGraphNodeType::ParticleEmitter, kEmitterCardWidth)) {
        char spawnRateLabel[64] = {};
        if (emitterNode) {
            sprintf_s(spawnRateLabel, "Spawn Rate %.0f", emitterNode->scalar);
        }
        moduleRow("Spawn Rate", emitterNode ? spawnRateLabel : "Add spawn module", emitterNode, NiagaraSectionColor(EffectGraphNodeType::ParticleEmitter), EffectGraphNodeType::ParticleEmitter, kEmitterCardWidth);
    }
    if (beginSection("Emitter Update", NiagaraSectionColor(EffectGraphNodeType::ParticleEmitter), "+", EffectGraphNodeType::ParticleEmitter, kEmitterCardWidth)) {
        moduleRow("Emitter State", emitterNode ? "Loop / playback" : "Add update module", emitterNode, NiagaraSectionColor(EffectGraphNodeType::ParticleEmitter), EffectGraphNodeType::ParticleEmitter, kEmitterCardWidth);
    }
    if (beginSection("Particle Spawn", ImVec4(0.48f, 0.86f, 0.22f, 1.0f), "+", EffectGraphNodeType::Color, kEmitterCardWidth)) {
        moduleRow("Initialize Particle", emitterNode ? "Lifetime / size" : "Add initializer", emitterNode, ImVec4(0.48f, 0.86f, 0.22f, 1.0f), EffectGraphNodeType::ParticleEmitter, kEmitterCardWidth);
        moduleRow("Color", colorNode ? "Dynamic input" : "Add color module", colorNode, NiagaraSectionColor(EffectGraphNodeType::Color), EffectGraphNodeType::Color, kEmitterCardWidth);
    }
    if (beginSection("Particle Update", ImVec4(0.13f, 0.52f, 0.95f, 1.0f), "+", EffectGraphNodeType::Lifetime, kEmitterCardWidth)) {
        moduleRow("Particle State", lifetimeNode ? "Age / duration" : "Add state module", lifetimeNode, NiagaraSectionColor(EffectGraphNodeType::Lifetime), EffectGraphNodeType::Lifetime, kEmitterCardWidth);
        moduleRow("Scale Color", colorNode ? "Tint curve" : "Add color module", colorNode, NiagaraSectionColor(EffectGraphNodeType::Color), EffectGraphNodeType::Color, kEmitterCardWidth);
    }
    if (beginSection("Render", ImVec4(0.95f, 0.35f, 0.20f, 1.0f), "+", EffectGraphNodeType::SpriteRenderer, kEmitterCardWidth)) {
        moduleRow("Sprite Renderer", spriteNode ? "Texture / mode" : "Add sprite renderer", spriteNode, NiagaraSectionColor(EffectGraphNodeType::SpriteRenderer), EffectGraphNodeType::SpriteRenderer, kEmitterCardWidth);
        moduleRow("Mesh Renderer", meshRendererNode ? "Material / blend" : "Add mesh renderer", meshRendererNode, NiagaraSectionColor(EffectGraphNodeType::MeshRenderer), EffectGraphNodeType::MeshRenderer, kEmitterCardWidth);
        moduleRow("Mesh Source", meshSourceNode ? "Model asset" : "Add mesh source", meshSourceNode, NiagaraSectionColor(EffectGraphNodeType::MeshSource), EffectGraphNodeType::MeshSource, kEmitterCardWidth);
    }

    ImGui::EndGroup();
    ed::EndNode();
    ed::PopStyleColor();

    ed::End();
    ed::SetCurrentEditor(nullptr);

    size_t addIndex = 0;
    for (EffectGraphNodeType pendingType : pendingAdds) {
        const DirectX::XMFLOAT2 spawnPos = {
            80.0f + static_cast<float>(addIndex) * 38.0f,
            80.0f + static_cast<float>(addIndex) * 26.0f
        };
        EffectGraphNode& addedNode = AddEffectGraphNode(m_asset, pendingType, spawnPos);
        m_selectedNodeId = addedNode.id;
        m_compileDirty = true;
        ++addIndex;
    }
    if (!pendingAdds.empty()) {
        EnsureGuiAuthoringLinks(m_asset);
        m_syncNodePositions = true;
    }

    ImGui::End();
}

void EffectEditorPanel::DrawGuiPanel()
{
    EffectGraphNode* lifetimeNode = FindNodeByType(EffectGraphNodeType::Lifetime);
    EffectGraphNode* emitterNode = FindNodeByType(EffectGraphNodeType::ParticleEmitter);
    EffectGraphNode* colorNode = FindNodeByType(EffectGraphNodeType::Color);
    EffectGraphNode* spriteNode = FindNodeByType(EffectGraphNodeType::SpriteRenderer);
    EffectGraphNode* meshRendererNode = FindNodeByType(EffectGraphNodeType::MeshRenderer);
    EffectGraphNode* meshSourceNode = FindNodeByType(EffectGraphNodeType::MeshSource);

    if (ImGui::BeginChild("##EffectGuiMode", ImVec2(0.0f, 0.0f), false)) {
        ImGui::TextDisabled("Inspector-style authoring");
        ImGui::Spacing();

        const char* shapeItems[] = { "Point", "Sphere", "Box", "Cone", "Circle", "Line" };
        const char* drawModes[] = { "Billboard", "Mesh", "Ribbon" };
        const char* sortModes[] = { "None", "Back To Front", "Front To Back" };
        const char* blendItems[] = { "Opaque", "Transparency", "Additive", "Subtraction", "Multiply", "Alpha" };
        const auto refreshEmitterCapacity = [&](bool aggressiveTemplateBudget = false) {
            if (!emitterNode) {
                return;
            }

            const uint32_t burstCount = emitterNode->scalar2 > 0.0f
                ? static_cast<uint32_t>(emitterNode->scalar2)
                : 0u;
            const float duration = lifetimeNode ? lifetimeNode->scalar : m_asset.previewDefaults.duration;
            if (aggressiveTemplateBudget) {
                emitterNode->intValue = static_cast<int>(kEffectParticleDefaultMaxParticles);
            } else {
                emitterNode->intValue = static_cast<int>(ResolveEffectParticleMaxParticles(
                    0,
                    emitterNode->scalar,
                    burstCount,
                    emitterNode->vectorValue.x,
                    duration));
            }
        };
        const auto applyForcePreset = [&](const DirectX::XMFLOAT3& accel, float drag, float speed) {
            emitterNode = EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
            emitterNode->vectorValue2 = { accel.x, accel.y, accel.z, drag };
            emitterNode->vectorValue.w = speed;
            m_compileDirty = true;
        };
        const auto applyColorPreset = [&](const DirectX::XMFLOAT4& startColor, const DirectX::XMFLOAT4& endColor) {
            colorNode = EnsureNodeByType(EffectGraphNodeType::Color);
            spriteNode = EnsureNodeByType(EffectGraphNodeType::SpriteRenderer);
            colorNode->vectorValue = startColor;
            colorNode->vectorValue2 = endColor;
            spriteNode->vectorValue = startColor;
            m_compileDirty = true;
        };
        const auto applyRenderPreset = [&](EffectParticleDrawMode drawMode, EffectParticleSortMode sortMode, float ribbonWidth, float stretch, float alphaScale) {
            spriteNode = EnsureNodeByType(EffectGraphNodeType::SpriteRenderer);
            spriteNode->intValue = static_cast<int>(drawMode);
            spriteNode->intValue2 = static_cast<int>(sortMode);
            spriteNode->vectorValue2.x = ribbonWidth;
            spriteNode->vectorValue2.y = stretch;
            spriteNode->vectorValue2.z = alphaScale;
            m_compileDirty = true;
        };

        if (ImGui::CollapsingHeader("Quick Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("One-click authoring helpers");
            if (ImGui::Button("Ambient Loop")) {
                emitterNode = EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                lifetimeNode = EnsureNodeByType(EffectGraphNodeType::Lifetime);
                emitterNode->scalar = 12.0f;
                emitterNode->scalar2 = 0.0f;
                emitterNode->vectorValue = { 3.0f, 0.18f, 0.42f, 0.25f };
                lifetimeNode->scalar = 6.0f;
                m_asset.previewDefaults.duration = 6.0f;
                refreshEmitterCapacity(true);
                applyForcePreset({ 0.0f, 0.08f, 0.0f }, 0.10f, 0.25f);
                applyColorPreset({ 0.60f, 0.70f, 1.00f, 0.60f }, { 0.15f, 0.18f, 0.26f, 0.0f });
            }
            ImGui::SameLine();
            if (ImGui::Button("Burst Pop")) {
                emitterNode = EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                lifetimeNode = EnsureNodeByType(EffectGraphNodeType::Lifetime);
                emitterNode->scalar = 0.0f;
                emitterNode->scalar2 = 96.0f;
                emitterNode->vectorValue = { 0.9f, 0.20f, 0.05f, 5.5f };
                lifetimeNode->scalar = 1.4f;
                m_asset.previewDefaults.duration = 1.4f;
                refreshEmitterCapacity(true);
                applyForcePreset({ 0.0f, -0.8f, 0.0f }, 0.01f, 5.5f);
                applyColorPreset({ 1.00f, 0.55f, 0.20f, 1.0f }, { 0.25f, 0.05f, 0.02f, 0.0f });
            }
            ImGui::SameLine();
            if (ImGui::Button("Ribbon Trail")) {
                emitterNode = EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                lifetimeNode = EnsureNodeByType(EffectGraphNodeType::Lifetime);
                emitterNode->scalar = 96.0f;
                emitterNode->scalar2 = 0.0f;
                emitterNode->vectorValue = { 0.85f, 0.06f, 0.02f, 1.1f };
                lifetimeNode->scalar = 2.0f;
                m_asset.previewDefaults.duration = 2.0f;
                refreshEmitterCapacity(true);
                applyForcePreset({ 0.0f, 0.0f, 0.0f }, 0.04f, 1.1f);
                applyColorPreset({ 0.30f, 0.95f, 1.00f, 0.95f }, { 0.08f, 0.28f, 1.00f, 0.0f });
                applyRenderPreset(EffectParticleDrawMode::Ribbon, EffectParticleSortMode::BackToFront, 0.08f, 1.45f, 1.0f);
            }
        }

        if (ImGui::CollapsingHeader("System", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!lifetimeNode) {
                if (ImGui::Button("+ Add Lifetime Module")) {
                    lifetimeNode = EnsureNodeByType(EffectGraphNodeType::Lifetime);
                }
            } else {
                m_compileDirty |= ImGui::DragFloat("Duration", &lifetimeNode->scalar, 0.01f, 0.10f, 120.0f, "%.2f s");
                m_asset.previewDefaults.duration = lifetimeNode->scalar;
            }

            int seed = static_cast<int>(m_asset.previewDefaults.seed);
            if (ImGui::DragInt("Seed", &seed, 1.0f, 1, 1000000)) {
                m_asset.previewDefaults.seed = static_cast<uint32_t>(seed);
                m_compileDirty = true;
            }
        }

        if (ImGui::CollapsingHeader("Emission & Life", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!emitterNode) {
                if (ImGui::Button("+ Add Particle Emitter")) {
                    emitterNode = EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                }
            } else {
                m_compileDirty |= ImGui::DragFloat("Spawn Rate", &emitterNode->scalar, 1.0f, 1.0f, 10000.0f);
                int burstCount = static_cast<int>(emitterNode->scalar2);
                if (ImGui::DragInt("Burst Count", &burstCount, 1.0f, 0, 5000000)) {
                    emitterNode->scalar2 = static_cast<float>(burstCount);
                    m_compileDirty = true;
                }
                m_compileDirty |= ImGui::DragInt("Max Particles", &emitterNode->intValue, 1.0f, 1, 5000000);
                m_compileDirty |= ImGui::DragFloat("Particle Lifetime", &emitterNode->vectorValue.x, 0.01f, 0.05f, 30.0f, "%.2f s");
                m_compileDirty |= ImGui::DragFloat("Start Size", &emitterNode->vectorValue.y, 0.01f, 0.01f, 10.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("End Size", &emitterNode->vectorValue.z, 0.01f, 0.00f, 10.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Speed", &emitterNode->vectorValue.w, 0.01f, 0.00f, 20.0f, "%.2f");
            }
        }

        if (ImGui::CollapsingHeader("Shape & Spawn Location", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!emitterNode) {
                if (ImGui::Button("+ Add Particle Emitter##ShapeSection")) {
                    emitterNode = EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                }
            } else {
                if (ImGui::Button("Point")) {
                    emitterNode->intValue2 = static_cast<int>(EffectSpawnShapeType::Point);
                    m_compileDirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Sphere")) {
                    emitterNode->intValue2 = static_cast<int>(EffectSpawnShapeType::Sphere);
                    emitterNode->vectorValue3.x = 0.35f;
                    m_compileDirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cone")) {
                    emitterNode->intValue2 = static_cast<int>(EffectSpawnShapeType::Cone);
                    emitterNode->vectorValue3 = { 22.0f, 0.20f, 0.65f, emitterNode->vectorValue3.w };
                    m_compileDirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Line")) {
                    emitterNode->intValue2 = static_cast<int>(EffectSpawnShapeType::Line);
                    emitterNode->vectorValue3.x = 0.35f;
                    m_compileDirty = true;
                }

                int shapeType = emitterNode->intValue2;
                if (ImGui::Combo("Shape", &shapeType, shapeItems, IM_ARRAYSIZE(shapeItems))) {
                    emitterNode->intValue2 = shapeType;
                    m_compileDirty = true;
                }

                switch (static_cast<EffectSpawnShapeType>(emitterNode->intValue2)) {
                case EffectSpawnShapeType::Point:
                    ImGui::TextDisabled("Point emitters spawn at the anchor.");
                    break;
                case EffectSpawnShapeType::Sphere:
                    m_compileDirty |= ImGui::DragFloat("Radius", &emitterNode->vectorValue3.x, 0.01f, 0.01f, 50.0f, "%.2f");
                    break;
                case EffectSpawnShapeType::Box:
                    m_compileDirty |= ImGui::DragFloat3("Extents", &emitterNode->vectorValue3.x, 0.01f, 0.01f, 50.0f, "%.2f");
                    break;
                case EffectSpawnShapeType::Cone:
                    m_compileDirty |= ImGui::DragFloat("Angle", &emitterNode->vectorValue3.x, 0.25f, 1.0f, 89.0f, "%.1f deg");
                    m_compileDirty |= ImGui::DragFloat("Base Radius", &emitterNode->vectorValue3.y, 0.01f, 0.01f, 50.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat("Height", &emitterNode->vectorValue3.z, 0.01f, 0.01f, 50.0f, "%.2f");
                    break;
                case EffectSpawnShapeType::Circle:
                    m_compileDirty |= ImGui::DragFloat("Circle Radius", &emitterNode->vectorValue3.x, 0.01f, 0.01f, 50.0f, "%.2f");
                    break;
                case EffectSpawnShapeType::Line:
                    m_compileDirty |= ImGui::DragFloat("Half Length", &emitterNode->vectorValue3.x, 0.01f, 0.01f, 50.0f, "%.2f");
                    break;
                default:
                    break;
                }
            }
        }

        if (ImGui::CollapsingHeader("Velocity & Forces", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!emitterNode) {
                if (ImGui::Button("+ Add Particle Emitter##ForceSection")) {
                    emitterNode = EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                }
            } else {
                if (ImGui::Button("Gravity")) {
                    applyForcePreset({ 0.0f, -1.6f, 0.0f }, 0.02f, emitterNode->vectorValue.w);
                }
                ImGui::SameLine();
                if (ImGui::Button("Float Up")) {
                    applyForcePreset({ 0.0f, 0.22f, 0.0f }, 0.18f, emitterNode->vectorValue.w);
                }
                ImGui::SameLine();
                if (ImGui::Button("Explode")) {
                    applyForcePreset({ 0.0f, -0.10f, 0.0f }, 0.00f, 6.5f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Curl Smoke")) {
                    emitterNode->vectorValue4 = { 0.55f, 0.12f, 0.08f, 0.12f };
                    m_compileDirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Magic Swirl")) {
                    emitterNode->vectorValue4 = { 0.40f, 0.28f, 0.35f, 1.10f };
                    m_compileDirty = true;
                }
                m_compileDirty |= ImGui::DragFloat3("Acceleration", &emitterNode->vectorValue2.x, 0.01f, -100.0f, 100.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Drag", &emitterNode->vectorValue2.w, 0.01f, 0.0f, 20.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Launch Speed", &emitterNode->vectorValue.w, 0.01f, 0.0f, 50.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Curl Noise Strength", &emitterNode->vectorValue4.x, 0.01f, 0.0f, 8.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Curl Noise Scale", &emitterNode->vectorValue4.y, 0.01f, 0.01f, 4.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Curl Scroll", &emitterNode->vectorValue4.z, 0.01f, 0.0f, 8.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Vortex Strength", &emitterNode->vectorValue4.w, 0.01f, -12.0f, 12.0f, "%.2f");
            }
        }

        if (ImGui::CollapsingHeader("Scale & Rotation", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!emitterNode) {
                if (ImGui::Button("+ Add Particle Emitter##ScaleSection")) {
                    emitterNode = EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                }
            } else {
                m_compileDirty |= ImGui::DragFloat("Start Size##ScaleSection", &emitterNode->vectorValue.y, 0.01f, 0.01f, 25.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("End Size##ScaleSection", &emitterNode->vectorValue.z, 0.01f, 0.00f, 25.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Spin Rate", &emitterNode->vectorValue3.w, 0.05f, -64.0f, 64.0f, "%.2f");
            }
        }

        if (ImGui::CollapsingHeader("Color & Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!colorNode) {
                if (ImGui::Button("+ Add Color Module")) {
                    colorNode = EnsureNodeByType(EffectGraphNodeType::Color);
                }
            } else {
                if (ImGui::Button("Fire")) {
                    applyColorPreset({ 1.00f, 0.58f, 0.16f, 1.0f }, { 0.22f, 0.02f, 0.02f, 0.0f });
                }
                ImGui::SameLine();
                if (ImGui::Button("Ice")) {
                    applyColorPreset({ 0.65f, 0.95f, 1.00f, 1.0f }, { 0.18f, 0.52f, 1.00f, 0.0f });
                }
                ImGui::SameLine();
                if (ImGui::Button("Smoke")) {
                    applyColorPreset({ 0.38f, 0.38f, 0.40f, 0.75f }, { 0.10f, 0.10f, 0.10f, 0.0f });
                }
                ImGui::SameLine();
                if (ImGui::Button("Magic")) {
                    applyColorPreset({ 0.90f, 0.35f, 1.00f, 1.0f }, { 0.22f, 0.08f, 1.00f, 0.0f });
                }
                m_compileDirty |= ImGui::ColorEdit4("Start Color", &colorNode->vectorValue.x);
                m_compileDirty |= ImGui::ColorEdit4("End Color", &colorNode->vectorValue2.x);
            }

            if (!spriteNode) {
                if (ImGui::Button("+ Add Sprite Renderer")) {
                    spriteNode = EnsureNodeByType(EffectGraphNodeType::SpriteRenderer);
                }
            } else {
                int drawMode = spriteNode->intValue;
                if (ImGui::Combo("Draw Mode", &drawMode, drawModes, IM_ARRAYSIZE(drawModes))) {
                    spriteNode->intValue = drawMode;
                    m_compileDirty = true;
                }
                m_compileDirty |= ImGui::ColorEdit4("Sprite Tint", &spriteNode->vectorValue.x);
                DrawAssetSlotControl("Sprite Texture", spriteNode->stringValue, AssetPickerKind::Texture, spriteNode->id, false);
            }
        }

        if (ImGui::CollapsingHeader("Rendering & Blending", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!spriteNode) {
                if (ImGui::Button("+ Add Sprite Renderer##RenderSection")) {
                    spriteNode = EnsureNodeByType(EffectGraphNodeType::SpriteRenderer);
                }
            } else {
                if (ImGui::Button("Soft Add")) {
                    applyRenderPreset(EffectParticleDrawMode::Billboard, EffectParticleSortMode::BackToFront, 0.08f, 0.30f, 1.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Mesh FX")) {
                    applyRenderPreset(EffectParticleDrawMode::Mesh, EffectParticleSortMode::BackToFront, 0.10f, 0.20f, 1.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("Ribbon FX")) {
                    applyRenderPreset(EffectParticleDrawMode::Ribbon, EffectParticleSortMode::BackToFront, 0.08f, 1.30f, 1.0f);
                }

                int drawMode = spriteNode->intValue;
                if (ImGui::Combo("Particle Draw Mode", &drawMode, drawModes, IM_ARRAYSIZE(drawModes))) {
                    spriteNode->intValue = drawMode;
                    m_compileDirty = true;
                }

                int sortMode = spriteNode->intValue2;
                if (ImGui::Combo("Sort Mode", &sortMode, sortModes, IM_ARRAYSIZE(sortModes))) {
                    spriteNode->intValue2 = sortMode;
                    m_compileDirty = true;
                }

                const char* particleBlendModes[] = { "Premultiplied Alpha", "Additive", "Alpha Blend", "Multiply", "Soft Additive" };
                int blendMode = static_cast<int>(spriteNode->scalar2);
                if (blendMode < 0 || blendMode >= IM_ARRAYSIZE(particleBlendModes)) blendMode = 0;
                if (ImGui::Combo("Blend Mode", &blendMode, particleBlendModes, IM_ARRAYSIZE(particleBlendModes))) {
                    spriteNode->scalar2 = static_cast<float>(blendMode);
                    m_compileDirty = true;
                }

                m_compileDirty |= ImGui::DragFloat("Ribbon Width", &spriteNode->vectorValue2.x, 0.005f, 0.01f, 10.0f, "%.3f");
                m_compileDirty |= ImGui::DragFloat("Velocity Stretch", &spriteNode->vectorValue2.y, 0.01f, 0.05f, 20.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Alpha Scale", &spriteNode->vectorValue2.z, 0.01f, 0.01f, 4.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Flipbook FPS", &spriteNode->vectorValue2.w, 0.1f, 0.0f, 120.0f, "%.1f");
                m_compileDirty |= ImGui::DragFloat("Size Curve Bias", &spriteNode->vectorValue3.x, 0.01f, 0.05f, 8.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Alpha Curve Bias", &spriteNode->vectorValue3.y, 0.01f, 0.05f, 8.0f, "%.2f");
                if (ImGui::Checkbox("Soft Particle", &spriteNode->boolValue)) {
                    m_compileDirty = true;
                }
                if (spriteNode->boolValue) {
                    m_compileDirty |= ImGui::DragFloat("Soft Fade Scale", &spriteNode->scalar, 1.0f, 1.0f, 512.0f, "%.1f");
                }
                ImGui::Separator();
                ImGui::TextDisabled("Per-Particle Randomization");
                m_compileDirty |= ImGui::SliderFloat("Speed Random", &spriteNode->vectorValue4.x, 0.0f, 1.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
                m_compileDirty |= ImGui::SliderFloat("Size Random", &spriteNode->vectorValue4.y, 0.0f, 1.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
                m_compileDirty |= ImGui::SliderFloat("Life Random", &spriteNode->vectorValue4.z, 0.0f, 1.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
                ImGui::Separator();
                ImGui::TextDisabled("Wind");
                m_compileDirty |= ImGui::DragFloat("Wind Strength", &spriteNode->vectorValue4.w, 0.1f, 0.0f, 50.0f, "%.1f");
                int subUvColumns = (std::max)(1, static_cast<int>(std::round(spriteNode->vectorValue3.z > 0.0f ? spriteNode->vectorValue3.z : 1.0f)));
                int subUvRows = (std::max)(1, static_cast<int>(std::round(spriteNode->vectorValue3.w > 0.0f ? spriteNode->vectorValue3.w : 1.0f)));
                if (ImGui::DragInt("Flipbook Columns", &subUvColumns, 1.0f, 1, 32)) {
                    spriteNode->vectorValue3.z = static_cast<float>(subUvColumns);
                    m_compileDirty = true;
                }
                if (ImGui::DragInt("Flipbook Rows", &subUvRows, 1.0f, 1, 32)) {
                    spriteNode->vectorValue3.w = static_cast<float>(subUvRows);
                    m_compileDirty = true;
                }
            }

            if (!meshRendererNode) {
                if (ImGui::Button("+ Add Mesh Renderer##BlendSection")) {
                    meshRendererNode = EnsureNodeByType(EffectGraphNodeType::MeshRenderer);
                }
            } else {
                int blendState = meshRendererNode->intValue;
                if (ImGui::Combo("Mesh Blend", &blendState, blendItems, IM_ARRAYSIZE(blendItems))) {
                    meshRendererNode->intValue = blendState;
                    m_compileDirty = true;
                }
                m_compileDirty |= ImGui::ColorEdit4("Mesh Tint##BlendSection", &meshRendererNode->vectorValue.x);
            }
        }

        if (ImGui::CollapsingHeader("Mesh & Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawAssetSlotControl("Preview Mesh", m_asset.previewDefaults.previewMeshPath, AssetPickerKind::Mesh, 0, true);

            if (!meshSourceNode) {
                if (ImGui::Button("+ Add Mesh Source")) {
                    meshSourceNode = EnsureNodeByType(EffectGraphNodeType::MeshSource);
                }
            } else {
                DrawAssetSlotControl("Mesh Asset", meshSourceNode->stringValue, AssetPickerKind::Mesh, meshSourceNode->id, false);
            }

            if (!meshRendererNode) {
                if (ImGui::Button("+ Add Mesh Renderer")) {
                    meshRendererNode = EnsureNodeByType(EffectGraphNodeType::MeshRenderer);
                }
            } else {
                m_compileDirty |= ImGui::ColorEdit4("Mesh Tint", &meshRendererNode->vectorValue.x);
                m_compileDirty |= InputTextString("Material Asset", meshRendererNode->stringValue);
            }
        }

        ImGui::Separator();
        DrawCompiledPanel();
    }
    ImGui::EndChild();
}

void EffectEditorPanel::DrawNodeModePanel()
{
    if (!ImGui::BeginChild("##EffectNodeMode", ImVec2(0.0f, 0.0f), false)) {
        ImGui::EndChild();
        return;
    }

    if (ImGui::BeginChild("##EffectNodeModeToolbar", ImVec2(0.0f, 28.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::Button("Frame All")) {
            m_syncNodePositions = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Node Mode is for dynamic inputs, bindings, and value graphs.");
    }
    ImGui::EndChild();
    ImGui::Separator();

    auto drawPin = [&](const EffectGraphPin& pin) {
        const ImColor pinColor = GetValueTypeColor(pin.valueType);
        ed::BeginPin(ed::PinId(pin.id), pin.kind == EffectPinKind::Input ? ed::PinKind::Input : ed::PinKind::Output);
        if (pin.kind == EffectPinKind::Input) {
            ImGui::TextColored(pinColor, ICON_FA_CIRCLE);
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextUnformatted(pin.name.c_str());
        } else {
            const ImVec2 labelSize = ImGui::CalcTextSize(pin.name.c_str());
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            if (contentWidth > labelSize.x + 20.0f) {
                ImGui::Dummy(ImVec2(contentWidth - labelSize.x - 20.0f, 0.0f));
                ImGui::SameLine(0.0f, 0.0f);
            }
            ImGui::TextUnformatted(pin.name.c_str());
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(pinColor, ICON_FA_CIRCLE);
        }
        ed::EndPin();
    };

    auto drawNodeSummary = [&](const EffectGraphNode& node) {
        switch (node.type) {
        case EffectGraphNodeType::ParticleEmitter:
            ImGui::TextDisabled("Spawn %.0f  Burst %.0f", node.scalar, node.scalar2);
            ImGui::TextDisabled("Lifetime %.2f  Speed %.2f", node.vectorValue.x, node.vectorValue.w);
            break;
        case EffectGraphNodeType::Lifetime:
            ImGui::TextDisabled("Duration %.2f s", node.scalar);
            break;
        case EffectGraphNodeType::Color:
            ImGui::TextDisabled("Color over life");
            break;
        case EffectGraphNodeType::SpriteRenderer:
            ImGui::TextDisabled("Texture / draw mode");
            break;
        case EffectGraphNodeType::MeshSource:
            ImGui::TextDisabled("%s", node.stringValue.empty() ? "No mesh asset" : node.stringValue.c_str());
            break;
        case EffectGraphNodeType::MeshRenderer:
            ImGui::TextDisabled("%s", node.stringValue.empty() ? "No material asset" : node.stringValue.c_str());
            break;
        default:
            ImGui::TextDisabled("%s", EffectGraphNodeTypeToString(node.type));
            break;
        }
    };

    bool openAddPopup = false;
    ImVec2 addPopupPos = ImGui::GetMousePos();

    ed::SetCurrentEditor(m_nodeEditorContext);
    ed::Begin("##EffectNodeModeCanvas");

    if (m_syncNodePositions) {
        for (const auto& node : m_asset.nodes) {
            ed::SetNodePosition(ed::NodeId(node.id), ImVec2(node.position.x, node.position.y));
        }
        ed::NavigateToContent(0.0f);
        m_syncNodePositions = false;
    }

    for (auto& node : m_asset.nodes) {
        ed::BeginNode(ed::NodeId(node.id));
        ImGui::PushID(static_cast<int>(node.id));
        ImGui::BeginGroup();
        ImGui::TextColored(NiagaraSectionColor(node.type), "%s", node.title.c_str());
        ImGui::TextDisabled("%s", EffectGraphNodeTypeToString(node.type));
        ImGui::Separator();
        drawNodeSummary(node);
        ImGui::Spacing();

        std::vector<const EffectGraphPin*> inputPins;
        std::vector<const EffectGraphPin*> outputPins;
        for (const auto& pin : m_asset.pins) {
            if (pin.nodeId != node.id) {
                continue;
            }
            if (pin.kind == EffectPinKind::Input) {
                inputPins.push_back(&pin);
            } else {
                outputPins.push_back(&pin);
            }
        }

        const size_t rowCount = (std::max)(inputPins.size(), outputPins.size());
        for (size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
            if (rowIndex < inputPins.size()) {
                drawPin(*inputPins[rowIndex]);
            } else {
                ImGui::Dummy(ImVec2(84.0f, ImGui::GetTextLineHeight()));
            }

            if (rowIndex < outputPins.size()) {
                ImGui::SameLine();
                ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), 176.0f));
                drawPin(*outputPins[rowIndex]);
            }
        }

        ImGui::EndGroup();
        ImGui::PopID();
        ed::EndNode();
    }

    for (const auto& link : m_asset.links) {
        const EffectGraphPin* startPin = m_asset.FindPin(link.startPinId);
        const EffectGraphPin* endPin = m_asset.FindPin(link.endPinId);
        if (!startPin || !endPin) {
            continue;
        }

        ed::Link(
            ed::LinkId(link.id),
            ed::PinId(link.startPinId),
            ed::PinId(link.endPinId),
            GetValueTypeColor(startPin->valueType),
            2.0f);
    }

    if (ed::ShowBackgroundContextMenu()) {
        addPopupPos = ImGui::GetMousePos();
        openAddPopup = true;
    }

    if (ed::BeginCreate(ImVec4(0.65f, 0.85f, 1.0f, 1.0f), 2.0f)) {
        ed::PinId startPinId;
        ed::PinId endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId)) {
            uint32_t start = static_cast<uint32_t>(startPinId.Get());
            uint32_t end = static_cast<uint32_t>(endPinId.Get());
            const EffectGraphPin* firstPin = m_asset.FindPin(start);
            const EffectGraphPin* secondPin = m_asset.FindPin(end);
            if (firstPin && secondPin && firstPin->kind == EffectPinKind::Input && secondPin->kind == EffectPinKind::Output) {
                std::swap(start, end);
            }

            std::string reason;
            if (CanCreateLink(start, end, reason)) {
                if (ed::AcceptNewItem(ImVec4(0.38f, 0.80f, 0.48f, 1.0f), 2.0f)) {
                    EffectGraphLink link;
                    link.id = m_asset.nextLinkId++;
                    link.startPinId = start;
                    link.endPinId = end;
                    m_asset.links.push_back(std::move(link));
                    m_selectedLinkId = link.id;
                    m_compileDirty = true;
                }
            } else {
                ed::RejectNewItem(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), 2.0f);
                ed::Suspend();
                ImGui::SetTooltip("%s", reason.c_str());
                ed::Resume();
            }
        }
    }
    ed::EndCreate();

    if (ed::BeginDelete()) {
        ed::LinkId deletedLinkId;
        while (ed::QueryDeletedLink(&deletedLinkId)) {
            if (ed::AcceptDeletedItem()) {
                const uint32_t linkId = static_cast<uint32_t>(deletedLinkId.Get());
                m_asset.links.erase(
                    std::remove_if(
                        m_asset.links.begin(),
                        m_asset.links.end(),
                        [linkId](const EffectGraphLink& link) { return link.id == linkId; }),
                    m_asset.links.end());
                if (m_selectedLinkId == linkId) {
                    m_selectedLinkId = 0;
                }
                m_compileDirty = true;
            }
        }

        ed::NodeId deletedNodeId;
        while (ed::QueryDeletedNode(&deletedNodeId)) {
            if (ed::AcceptDeletedItem()) {
                RemoveNode(static_cast<uint32_t>(deletedNodeId.Get()));
            }
        }
    }
    ed::EndDelete();

    if (ed::GetSelectedObjectCount() > 0) {
        ed::NodeId selectedNodes[8] = {};
        const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes, IM_ARRAYSIZE(selectedNodes));
        if (selectedNodeCount > 0) {
            m_selectedNodeId = static_cast<uint32_t>(selectedNodes[0].Get());
            m_selectedLinkId = 0;
        } else {
            ed::LinkId selectedLinks[8] = {};
            const int selectedLinkCount = ed::GetSelectedLinks(selectedLinks, IM_ARRAYSIZE(selectedLinks));
            if (selectedLinkCount > 0) {
                m_selectedLinkId = static_cast<uint32_t>(selectedLinks[0].Get());
            }
        }
    }

    for (auto& node : m_asset.nodes) {
        const ImVec2 position = ed::GetNodePosition(ed::NodeId(node.id));
        node.position = { position.x, position.y };
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);

    if (openAddPopup) {
        m_pendingNodePopupPos = { addPopupPos.x, addPopupPos.y };
        ImGui::OpenPopup("##EffectNodeModeAddPopup");
    }

    ImGui::SetNextWindowPos(ImVec2(m_pendingNodePopupPos.x, m_pendingNodePopupPos.y), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##EffectNodeModeAddPopup")) {
        const auto addNodeItem = [&](const char* label, EffectGraphNodeType type) {
            if (ImGui::MenuItem(label)) {
                EffectGraphNode& addedNode = AddEffectGraphNode(m_asset, type, m_pendingNodePopupPos);
                EnsureGuiAuthoringLinks(m_asset);
                m_selectedNodeId = addedNode.id;
                m_compileDirty = true;
            }
        };

        addNodeItem("Spawn", EffectGraphNodeType::Spawn);
        addNodeItem("Lifetime", EffectGraphNodeType::Lifetime);
        addNodeItem("Particle Emitter", EffectGraphNodeType::ParticleEmitter);
        addNodeItem("Color", EffectGraphNodeType::Color);
        addNodeItem("Sprite Renderer", EffectGraphNodeType::SpriteRenderer);
        addNodeItem("Mesh Source", EffectGraphNodeType::MeshSource);
        addNodeItem("Mesh Renderer", EffectGraphNodeType::MeshRenderer);
        addNodeItem("Float", EffectGraphNodeType::Float);
        addNodeItem("Vec3", EffectGraphNodeType::Vec3);
        addNodeItem("Output", EffectGraphNodeType::Output);
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

void EffectEditorPanel::DrawPreviewPanel()
{
    if (!ImGui::Begin(kPreviewWindowTitle)) {
        ImGui::End();
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 2.0f));
    if (ImGui::BeginChild("##EffectPreviewToolbar", ImVec2(0.0f, kPreviewToolbarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::Button("Perspective");
        ImGui::SameLine();
        ImGui::Button(m_previewUseSkybox ? "Lighting On" : "Lighting Off");
        ImGui::SameLine();
        ImGui::Button("Show");
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            ResetPreviewCamera();
        }
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 74.0f);
        ImGui::Checkbox("Sky", &m_previewUseSkybox);
        ImGui::SameLine();
        ImGui::TextDisabled("Orbit: RMB  Zoom: Wheel");
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::Separator();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 previewSize(
        (std::max)(avail.x, 1.0f),
        (std::max)(avail.y, 1.0f));

    const ImVec2 surfaceMin = ImGui::GetCursorScreenPos();
    const ImVec2 surfaceMax(surfaceMin.x + previewSize.x, surfaceMin.y + previewSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(surfaceMin, surfaceMax, IM_COL32(54, 54, 58, 255), 5.0f);
    drawList->AddRect(surfaceMin, surfaceMax, IM_COL32(80, 82, 88, 255), 5.0f);

    bool hasTexture = false;
    if (m_viewportTexture) {
        if (void* textureId = ImGuiRenderer::GetTextureID(m_viewportTexture)) {
            ImGui::Image(reinterpret_cast<ImTextureID>(textureId), previewSize);
            hasTexture = true;
        }
    }

    if (!hasTexture) {
        ImGui::InvisibleButton("##EffectPreviewSurface", previewSize);
        const ImVec2 center(
            surfaceMin.x + previewSize.x * 0.5f,
            surfaceMin.y + previewSize.y * 0.5f);
        const char* title = "Preview Scene";
        const char* subtitle = "Compile and simulate to inspect the effect";
        const ImVec2 titleSize = ImGui::CalcTextSize(title);
        const ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);
        drawList->AddText(
            ImVec2(center.x - titleSize.x * 0.5f, center.y - 18.0f),
            IM_COL32(210, 214, 224, 255),
            title);
        drawList->AddText(
            ImVec2(center.x - subtitleSize.x * 0.5f, center.y + 4.0f),
            IM_COL32(150, 154, 164, 255),
            subtitle);
    }

    m_previewHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (m_previewHovered) {
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            m_previewYaw += io.MouseDelta.x * 0.0125f;
            m_previewPitch = std::clamp(m_previewPitch + io.MouseDelta.y * 0.0100f, -1.25f, 1.10f);
        }
        if (std::fabs(io.MouseWheel) > 0.0001f) {
            const float zoomFactor = (io.MouseWheel > 0.0f) ? 0.90f : 1.10f;
            m_previewDistance = std::clamp(m_previewDistance * zoomFactor, 1.25f, 18.0f);
        }
    }

    ImGui::End();
}

void EffectEditorPanel::DrawTimelinePanel()
{
    if (!ImGui::Begin(kTimelineWindowTitle)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##EffectBottomTabs")) {
        if (ImGui::BeginTabItem("Curves")) {
            EffectPlaybackComponent* playback = nullptr;
            if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
                playback = m_registry->GetComponent<EffectPlaybackComponent>(m_previewEntity);
            }

            const float duration = SafeDuration(m_asset.previewDefaults.duration);
            const float currentTime = playback ? std::clamp(playback->currentTime, 0.0f, duration) : 0.0f;
            const float sampleT = std::clamp(currentTime / duration, 0.0f, 1.0f);

            EffectGraphNode* emitterNode = FindNodeByType(EffectGraphNodeType::ParticleEmitter);
            EffectGraphNode* colorNode = FindNodeByType(EffectGraphNodeType::Color);
            EffectGraphNode* spriteNode = FindNodeByType(EffectGraphNodeType::SpriteRenderer);

            const auto easeSample = [](float t, float bias) {
                return std::pow((std::max)(0.0f, (std::min)(1.0f, t)), (std::max)(0.05f, bias));
            };
            const auto lerpSample = [](float a, float b, float t) {
                return a + (b - a) * t;
            };

            if (!emitterNode || !colorNode || !spriteNode) {
                ImGui::TextDisabled("Curves require Particle Emitter, Color, and Sprite Renderer modules.");
                if (ImGui::Button("Build Missing Modules")) {
                    EnsureNodeByType(EffectGraphNodeType::ParticleEmitter);
                    EnsureNodeByType(EffectGraphNodeType::Color);
                    EnsureNodeByType(EffectGraphNodeType::SpriteRenderer);
                    EnsureGuiAuthoringLinks(m_asset);
                    m_compileDirty = true;
                }
                ImGui::EndTabItem();
                ImGui::EndTabBar();
                ImGui::End();
                return;
            }

            ImGui::TextDisabled("Life curves preview and edit");
            ImGui::SameLine();
            ImGui::Text("Time %.2f / %.2f", currentTime, duration);
            ImGui::Separator();

            if (ImGui::BeginChild("##EffectCurveControls", ImVec2(330.0f, 0.0f), true)) {
                ImGui::TextUnformatted("Size Over Life");
                m_compileDirty |= ImGui::DragFloat("Start Size##Curve", &emitterNode->vectorValue.y, 0.01f, 0.01f, 25.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("End Size##Curve", &emitterNode->vectorValue.z, 0.01f, 0.00f, 25.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Size Bias##Curve", &spriteNode->vectorValue3.x, 0.01f, 0.05f, 8.0f, "%.2f");
                ImGui::Spacing();
                ImGui::TextUnformatted("Color / Alpha Over Life");
                m_compileDirty |= ImGui::ColorEdit4("Start Color##Curve", &colorNode->vectorValue.x);
                m_compileDirty |= ImGui::ColorEdit4("End Color##Curve", &colorNode->vectorValue2.x);
                m_compileDirty |= ImGui::DragFloat("Alpha Scale##Curve", &spriteNode->vectorValue2.z, 0.01f, 0.01f, 4.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Alpha Bias##Curve", &spriteNode->vectorValue3.y, 0.01f, 0.05f, 8.0f, "%.2f");
                ImGui::Spacing();
                const float sizeValue = lerpSample(emitterNode->vectorValue.y, emitterNode->vectorValue.z, easeSample(sampleT, spriteNode->vectorValue3.x));
                const float alphaScale = (spriteNode->vectorValue2.z > 0.0f) ? spriteNode->vectorValue2.z : 1.0f;
                const float alphaT = easeSample(sampleT, spriteNode->vectorValue3.y);
                const float alphaValue = lerpSample(colorNode->vectorValue.w, colorNode->vectorValue2.w, alphaT) * alphaScale;
                ImGui::Text("Sample Size: %.3f", sizeValue);
                ImGui::Text("Sample Alpha: %.3f", alphaValue);

                // Phase 1C: Size Curve Multi-Key
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextUnformatted("Size Curve (Multi-Key)");
                int sizeKeyCount = static_cast<int>(spriteNode->vectorValue6.w);
                if (sizeKeyCount < 2) sizeKeyCount = 2;
                if (ImGui::SliderInt("Key Count##SizeCurve", &sizeKeyCount, 2, 4)) {
                    spriteNode->vectorValue6.w = static_cast<float>(sizeKeyCount);
                    if (sizeKeyCount >= 3) {
                        if (spriteNode->vectorValue5.x <= 0.0f) spriteNode->vectorValue5.x = emitterNode->vectorValue.y;
                        if (spriteNode->vectorValue5.y <= 0.0f) spriteNode->vectorValue5.y = (emitterNode->vectorValue.y + emitterNode->vectorValue.z) * 0.5f;
                        if (spriteNode->vectorValue5.z <= 0.0f) spriteNode->vectorValue5.z = emitterNode->vectorValue.z;
                        if (spriteNode->vectorValue5.w <= 0.0f) spriteNode->vectorValue5.w = emitterNode->vectorValue.z;
                        if (spriteNode->vectorValue6.x <= 0.0f) spriteNode->vectorValue6.x = 0.33f;
                        if (spriteNode->vectorValue6.y <= 0.0f) spriteNode->vectorValue6.y = 0.66f;
                    }
                    m_compileDirty = true;
                }
                if (sizeKeyCount >= 3) {
                    m_compileDirty |= ImGui::DragFloat("S0 (t=0)##SC", &spriteNode->vectorValue5.x, 0.01f, 0.0f, 25.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat("S1##SC", &spriteNode->vectorValue5.y, 0.01f, 0.0f, 25.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat("T1##SC", &spriteNode->vectorValue6.x, 0.005f, 0.01f, 0.99f, "%.2f");
                    if (sizeKeyCount >= 4) {
                        m_compileDirty |= ImGui::DragFloat("S2##SC", &spriteNode->vectorValue5.z, 0.01f, 0.0f, 25.0f, "%.2f");
                        m_compileDirty |= ImGui::DragFloat("T2##SC", &spriteNode->vectorValue6.y, 0.005f, 0.01f, 0.99f, "%.2f");
                    }
                    m_compileDirty |= ImGui::DragFloat("S3 (t=1)##SC", &spriteNode->vectorValue5.w, 0.01f, 0.0f, 25.0f, "%.2f");
                }

                // Phase 1C: Color Gradient Multi-Key
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextUnformatted("Color Gradient (Multi-Key)");
                int gradKeyCount = colorNode->intValue;
                if (gradKeyCount < 2) gradKeyCount = 2;
                if (ImGui::SliderInt("Key Count##Gradient", &gradKeyCount, 2, 4)) {
                    colorNode->intValue = gradKeyCount;
                    if (gradKeyCount >= 3) {
                        if (colorNode->scalar <= 0.0f) colorNode->scalar = 0.33f;
                        if (colorNode->scalar2 <= 0.0f) colorNode->scalar2 = 0.66f;
                    }
                    m_compileDirty = true;
                }
                if (gradKeyCount >= 3) {
                    m_compileDirty |= ImGui::ColorEdit4("Mid Color 1##GC", &colorNode->vectorValue3.x);
                    m_compileDirty |= ImGui::DragFloat("Time 1##GC", &colorNode->scalar, 0.005f, 0.01f, 0.99f, "%.2f");
                    if (gradKeyCount >= 4) {
                        m_compileDirty |= ImGui::ColorEdit4("Mid Color 2##GC", &colorNode->vectorValue4.x);
                        m_compileDirty |= ImGui::DragFloat("Time 2##GC", &colorNode->scalar2, 0.005f, 0.01f, 0.99f, "%.2f");
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("##EffectCurvePreview", ImVec2(0.0f, 0.0f), true)) {
                const auto drawCurvePanel = [&](const char* label,
                                                float startValue,
                                                float endValue,
                                                float bias,
                                                ImU32 curveColor,
                                                float minValue,
                                                float maxValue,
                                                float indicatorValue) {
                    ImGui::TextUnformatted(label);
                    const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 148.0f);
                    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    ImGui::InvisibleButton(label, canvasSize);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const ImVec2 canvasMax(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
                    drawList->AddRectFilled(canvasPos, canvasMax, IM_COL32(24, 26, 31, 255), 4.0f);
                    drawList->AddRect(canvasPos, canvasMax, IM_COL32(58, 64, 76, 255), 4.0f);

                    for (int grid = 1; grid < 4; ++grid) {
                        const float gx = canvasPos.x + (canvasSize.x * grid / 4.0f);
                        const float gy = canvasPos.y + (canvasSize.y * grid / 4.0f);
                        drawList->AddLine(ImVec2(gx, canvasPos.y), ImVec2(gx, canvasMax.y), IM_COL32(40, 44, 52, 255));
                        drawList->AddLine(ImVec2(canvasPos.x, gy), ImVec2(canvasMax.x, gy), IM_COL32(40, 44, 52, 255));
                    }

                    ImVec2 prev = canvasPos;
                    for (int i = 0; i <= 64; ++i) {
                        const float t = static_cast<float>(i) / 64.0f;
                        const float eased = easeSample(t, bias);
                        const float value = lerpSample(startValue, endValue, eased);
                        const float normalized = (maxValue - minValue) > 0.0001f
                            ? std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f)
                            : 0.0f;
                        const ImVec2 point(
                            canvasPos.x + t * canvasSize.x,
                            canvasMax.y - normalized * canvasSize.y);
                        if (i > 0) {
                            drawList->AddLine(prev, point, curveColor, 2.0f);
                        }
                        prev = point;
                    }

                    const float indicatorX = canvasPos.x + sampleT * canvasSize.x;
                    drawList->AddLine(ImVec2(indicatorX, canvasPos.y), ImVec2(indicatorX, canvasMax.y), IM_COL32(255, 120, 70, 255), 2.0f);
                    const float indicatorNorm = (maxValue - minValue) > 0.0001f
                        ? std::clamp((indicatorValue - minValue) / (maxValue - minValue), 0.0f, 1.0f)
                        : 0.0f;
                    const ImVec2 indicatorPos(indicatorX, canvasMax.y - indicatorNorm * canvasSize.y);
                    drawList->AddCircleFilled(indicatorPos, 4.0f, IM_COL32(255, 220, 120, 255));
                };

                const float sizeSample = lerpSample(emitterNode->vectorValue.y, emitterNode->vectorValue.z, easeSample(sampleT, spriteNode->vectorValue3.x));
                drawCurvePanel(
                    "Size Curve",
                    emitterNode->vectorValue.y,
                    emitterNode->vectorValue.z,
                    spriteNode->vectorValue3.x,
                    IM_COL32(98, 194, 255, 255),
                    0.0f,
                    (std::max)(emitterNode->vectorValue.y, emitterNode->vectorValue.z) * 1.15f + 0.05f,
                    sizeSample);

                ImGui::Spacing();
                ImGui::TextUnformatted("Color Gradient");
                const ImVec2 gradientSize(ImGui::GetContentRegionAvail().x, 40.0f);
                const ImVec2 gradientPos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##EffectColorGradient", gradientSize);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 gradientMax(gradientPos.x + gradientSize.x, gradientPos.y + gradientSize.y);
                drawList->AddRectFilledMultiColor(
                    gradientPos,
                    gradientMax,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(colorNode->vectorValue.x, colorNode->vectorValue.y, colorNode->vectorValue.z, 1.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(colorNode->vectorValue2.x, colorNode->vectorValue2.y, colorNode->vectorValue2.z, 1.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(colorNode->vectorValue2.x, colorNode->vectorValue2.y, colorNode->vectorValue2.z, 1.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(colorNode->vectorValue.x, colorNode->vectorValue.y, colorNode->vectorValue.z, 1.0f)));
                drawList->AddRect(gradientPos, gradientMax, IM_COL32(64, 68, 78, 255), 4.0f);
                const float gradientX = gradientPos.x + sampleT * gradientSize.x;
                drawList->AddLine(ImVec2(gradientX, gradientPos.y), ImVec2(gradientX, gradientMax.y), IM_COL32(255, 120, 70, 255), 2.0f);

                ImGui::Spacing();
                const float alphaScale = (spriteNode->vectorValue2.z > 0.0f) ? spriteNode->vectorValue2.z : 1.0f;
                const float alphaSample = lerpSample(colorNode->vectorValue.w, colorNode->vectorValue2.w, easeSample(sampleT, spriteNode->vectorValue3.y)) * alphaScale;
                drawCurvePanel(
                    "Alpha Curve",
                    colorNode->vectorValue.w * alphaScale,
                    colorNode->vectorValue2.w * alphaScale,
                    spriteNode->vectorValue3.y,
                    IM_COL32(255, 196, 96, 255),
                    0.0f,
                    1.1f,
                    alphaSample);
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_SLIDERS " Parameters")) {
            if (m_asset.exposedParameters.empty()) {
                ImGui::TextDisabled("No exposed parameters. Add parameters in the Blackboard.");
            } else {
                auto* overrideComponent = (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity))
                    ? m_registry->GetComponent<EffectParameterOverrideComponent>(m_previewEntity)
                    : nullptr;

                if (overrideComponent) {
                    if (overrideComponent->scalarNames.size() != m_asset.exposedParameters.size()) {
                        overrideComponent->scalarNames.clear();
                        overrideComponent->scalarValues.clear();
                        overrideComponent->colorNames.clear();
                        overrideComponent->colorValues.clear();
                        for (const auto& param : m_asset.exposedParameters) {
                            if (param.valueType == EffectValueType::Float) {
                                overrideComponent->scalarNames.push_back(param.name);
                                overrideComponent->scalarValues.push_back(param.defaultValue.x);
                            } else if (param.valueType == EffectValueType::Color) {
                                overrideComponent->colorNames.push_back(param.name);
                                overrideComponent->colorValues.push_back(param.defaultValue);
                            }
                        }
                        overrideComponent->enabled = true;
                    }

                    ImGui::TextDisabled("Drag sliders to adjust parameters in real-time");
                    ImGui::Separator();

                    for (size_t i = 0; i < overrideComponent->scalarNames.size() && i < overrideComponent->scalarValues.size(); ++i) {
                        const std::string& name = overrideComponent->scalarNames[i];
                        ImGui::DragFloat(name.c_str(), &overrideComponent->scalarValues[i], 0.01f, 0.0f, 100.0f, "%.3f");
                        if (const char* desc = EffectParameterBindings::DescribeBinding(EffectValueType::Float, name)) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%s)", desc);
                        }
                    }
                    if (!overrideComponent->scalarNames.empty() && !overrideComponent->colorNames.empty()) {
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                    }
                    for (size_t i = 0; i < overrideComponent->colorNames.size() && i < overrideComponent->colorValues.size(); ++i) {
                        const std::string& name = overrideComponent->colorNames[i];
                        ImGui::ColorEdit4(name.c_str(), &overrideComponent->colorValues[i].x);
                        if (const char* desc = EffectParameterBindings::DescribeBinding(EffectValueType::Color, name)) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%s)", desc);
                        }
                    }
                } else {
                    ImGui::TextDisabled("Start preview to enable live parameter editing.");
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_FOLDER_OPEN " Presets")) {
            ImGui::TextDisabled("Save / Load effect presets");
            ImGui::Separator();

            if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save as Preset")) {
                std::filesystem::path presetDir = "Data/Effect/Presets";
                std::error_code ec;
                std::filesystem::create_directories(presetDir, ec);
                std::string presetName = m_asset.name.empty() ? "Untitled" : m_asset.name;
                std::string presetPath = (presetDir / (presetName + ".effectgraph.json")).string();
                EffectGraphSerializer::Save(presetPath, m_asset);
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_ARROWS_ROTATE " Refresh")) {
                // Trigger rescan on next frame
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Available presets:");

            std::error_code ec;
            const std::filesystem::path presetDir = "Data/Effect/Presets";
            if (std::filesystem::exists(presetDir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(presetDir, ec)) {
                    if (!entry.is_regular_file()) continue;
                    const auto& path = entry.path();
                    if (path.extension() != ".json") continue;
                    std::string filename = path.stem().string();
                    // Remove .effectgraph suffix if present
                    if (filename.size() > 12 && filename.substr(filename.size() - 12) == ".effectgraph") {
                        filename = filename.substr(0, filename.size() - 12);
                    }
                    if (ImGui::Selectable(filename.c_str())) {
                        EffectGraphAsset loaded;
                        if (EffectGraphSerializer::Load(path.string(), loaded)) {
                            m_asset = std::move(loaded);
                            m_compileDirty = true;
                            m_syncNodePositions = true;
                            CompileDocument();
                            QueuePreviewSpawn();
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("No presets directory found. Save a preset to create it.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Niagara Log")) {
            DrawCompiledPanel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(ICON_FA_CLOCK " Timeline")) {
            EffectPlaybackComponent* playback = nullptr;
            if (m_registry && !Entity::IsNull(m_previewEntity) && m_registry->IsAlive(m_previewEntity)) {
                playback = m_registry->GetComponent<EffectPlaybackComponent>(m_previewEntity);
            }

            const bool hasPreviewPlayback = playback != nullptr;
            const float duration = SafeDuration(m_asset.previewDefaults.duration);
            float currentTime = hasPreviewPlayback ? playback->currentTime : 0.0f;
            currentTime = std::clamp(currentTime, 0.0f, duration);

            if (ImGui::Button(ICON_FA_BACKWARD_STEP " Restart")) {
                CompileDocument();
                QueuePreviewSpawn();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_PLAY " Play")) {
                if (!hasPreviewPlayback || m_compileDirty) {
                    CompileDocument();
                    QueuePreviewSpawn();
                } else {
                    playback->isPlaying = true;
                    playback->stopRequested = false;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_PAUSE " Pause")) {
                if (hasPreviewPlayback) {
                    playback->isPlaying = false;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_STOP " Stop")) {
                StopPreview();
                playback = nullptr;
                currentTime = 0.0f;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::DragFloat("Duration", &m_asset.previewDefaults.duration, 0.01f, 0.10f, 120.0f, "%.2f s")) {
                m_compileDirty = true;
                if (playback) {
                    playback->duration = m_asset.previewDefaults.duration;
                }
            }

            float scrubTime = currentTime;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::SliderFloat("##EffectTimelineScrub", &scrubTime, 0.0f, duration, "%.2f s")) {
                if (playback) {
                    playback->currentTime = scrubTime;
                    playback->isPlaying = false;
                    if (playback->runtimeInstanceId != 0) {
                        if (auto* runtime = EffectRuntimeRegistry::Instance().GetRuntimeInstance(playback->runtimeInstanceId)) {
                            runtime->time = scrubTime;
                        }
                    }
                }
            }

            const float leftPaneWidth = 220.0f;
            const float rowHeight = 28.0f;
            const char* tracks[] = {
                "System Spawn",
                "Emitter Update",
                "Render Output"
            };

            ImGui::BeginChild("##EffectTimelineTrackLabels", ImVec2(leftPaneWidth, 0.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextDisabled("Tracks");
            ImGui::Separator();
            ImDrawList* trackDrawList = ImGui::GetWindowDrawList();
            for (int trackIndex = 0; trackIndex < static_cast<int>(IM_ARRAYSIZE(tracks)); ++trackIndex) {
                const ImVec2 rowPos = ImGui::GetCursorScreenPos();
                const ImVec2 rowSize(ImGui::GetContentRegionAvail().x, rowHeight);
                const ImVec2 rowMax(rowPos.x + rowSize.x, rowPos.y + rowSize.y);
                const ImU32 fill = (trackIndex % 2 == 0) ? IM_COL32(48, 50, 56, 255) : IM_COL32(42, 44, 49, 255);
                trackDrawList->AddRectFilled(rowPos, rowMax, fill);
                trackDrawList->AddText(ImVec2(rowPos.x + 10.0f, rowPos.y + 6.0f), IM_COL32(208, 212, 220, 255), tracks[trackIndex]);
                ImGui::Dummy(rowSize);
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("##EffectTimelineCanvas", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
            const ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
            const ImVec2 canvasMax(canvasMin.x + canvasAvail.x, canvasMin.y + canvasAvail.y);
            const float rulerHeight = 28.0f;
            const float usableHeight = (canvasAvail.y > rulerHeight) ? (canvasAvail.y - rulerHeight) : 0.0f;
            const float tickCount = 12.0f;

            drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(23, 25, 30, 255), 4.0f);
            drawList->AddRect(canvasMin, canvasMax, IM_COL32(62, 68, 80, 255), 4.0f);

            for (int tick = 0; tick <= static_cast<int>(tickCount); ++tick) {
                const float t = static_cast<float>(tick) / tickCount;
                const float x = canvasMin.x + t * canvasAvail.x;
                drawList->AddLine(ImVec2(x, canvasMin.y), ImVec2(x, canvasMax.y), IM_COL32(42, 46, 56, 255), 1.0f);

                char label[32];
                sprintf_s(label, "%.2f", duration * t);
                drawList->AddText(ImVec2(x + 4.0f, canvasMin.y + 4.0f), IM_COL32(180, 186, 198, 255), label);
            }

            for (int row = 0; row < static_cast<int>(IM_ARRAYSIZE(tracks)); ++row) {
                const float y0 = canvasMin.y + rulerHeight + row * rowHeight;
                const float y1 = y0 + rowHeight;
                const ImU32 fill = (row % 2 == 0) ? IM_COL32(24, 27, 33, 255) : IM_COL32(28, 31, 38, 255);
                drawList->AddRectFilled(ImVec2(canvasMin.x, y0), ImVec2(canvasMax.x, y1), fill);
                drawList->AddLine(ImVec2(canvasMin.x, y1), ImVec2(canvasMax.x, y1), IM_COL32(45, 50, 60, 255), 1.0f);
            }

            const float playheadRatio = scrubTime / duration;
            const float playheadX = canvasMin.x + playheadRatio * canvasAvail.x;
            drawList->AddLine(ImVec2(playheadX, canvasMin.y), ImVec2(playheadX, canvasMax.y), IM_COL32(255, 110, 60, 255), 2.0f);

            const float rangeStartX = canvasMin.x;
            const float rangeEndX = canvasMax.x;
            drawList->AddRectFilled(
                ImVec2(rangeStartX, canvasMin.y + rulerHeight + 1.0f),
                ImVec2(rangeEndX, canvasMin.y + rulerHeight + rowHeight - 2.0f),
                IM_COL32(60, 130, 220, 80));
            drawList->AddRectFilled(
                ImVec2(rangeStartX, canvasMin.y + rulerHeight + rowHeight + 1.0f),
                ImVec2(rangeEndX, canvasMin.y + rulerHeight + rowHeight * 2.0f - 2.0f),
                IM_COL32(60, 200, 120, 72));

            if (m_compiled && (m_compiled->meshRenderer.enabled || m_compiled->particleRenderer.enabled)) {
                const ImU32 renderColor = m_compiled->particleRenderer.enabled ? IM_COL32(214, 126, 255, 80) : IM_COL32(255, 196, 60, 80);
                drawList->AddRectFilled(
                    ImVec2(rangeStartX, canvasMin.y + rulerHeight + rowHeight * 2.0f + 1.0f),
                    ImVec2(rangeEndX, canvasMin.y + rulerHeight + rowHeight * 3.0f - 2.0f),
                    renderColor);
            }

            const float canvasHeight = (usableHeight > rowHeight * 3.0f) ? usableHeight : (rowHeight * 3.0f);
            ImGui::Dummy(ImVec2(canvasAvail.x, canvasHeight));
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void EffectEditorPanel::DrawDetailsPanel()
{
    if (!ImGui::Begin(kDetailsWindowTitle)) {
        ImGui::End();
        return;
    }

    EffectGraphNode* node = m_asset.FindNode(m_selectedNodeId);
    const auto drawSelectedNodeInspector = [&]() {
        if (!node) {
            ImGui::TextDisabled("Select a module from System Overview.");
            return;
        }

        ImGui::TextColored(NiagaraSectionColor(node->type), "%s", node->title.c_str());
        ImGui::TextDisabled("Node #%u", node->id);
        ImGui::Separator();
        m_compileDirty |= InputTextString("Display Name", node->title);

        switch (node->type) {
        case EffectGraphNodeType::ParticleEmitter:
            if (ImGui::CollapsingHeader("Emitter Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
                const char* shapeItems[] = { "Point", "Sphere", "Box", "Cone", "Circle", "Line" };
                m_compileDirty |= ImGui::DragFloat("Spawn Rate", &node->scalar, 1.0f, 1.0f, 10000.0f);
                int burstCount = static_cast<int>(node->scalar2);
            if (ImGui::DragInt("Burst Count", &burstCount, 1.0f, 0, 5000000)) {
                    node->scalar2 = static_cast<float>(burstCount);
                    m_compileDirty = true;
                }
            m_compileDirty |= ImGui::DragInt("Max Particles", &node->intValue, 1.0f, 1, 5000000);
                m_compileDirty |= ImGui::DragFloat("Particle Lifetime", &node->vectorValue.x, 0.01f, 0.05f, 30.0f, "%.2f s");
                m_compileDirty |= ImGui::DragFloat("Start Size", &node->vectorValue.y, 0.01f, 0.01f, 10.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("End Size", &node->vectorValue.z, 0.01f, 0.00f, 10.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Speed", &node->vectorValue.w, 0.01f, 0.00f, 20.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat3("Acceleration", &node->vectorValue2.x, 0.01f, -100.0f, 100.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Drag", &node->vectorValue2.w, 0.01f, 0.0f, 20.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Curl Noise Strength", &node->vectorValue4.x, 0.01f, 0.0f, 8.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Curl Noise Scale", &node->vectorValue4.y, 0.01f, 0.01f, 4.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Curl Scroll", &node->vectorValue4.z, 0.01f, 0.0f, 8.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Vortex Strength", &node->vectorValue4.w, 0.01f, -12.0f, 12.0f, "%.2f");
                int shapeType = node->intValue2;
                if (ImGui::Combo("Shape", &shapeType, shapeItems, IM_ARRAYSIZE(shapeItems))) {
                    node->intValue2 = shapeType;
                    m_compileDirty = true;
                }
                m_compileDirty |= ImGui::DragFloat3("Shape Params", &node->vectorValue3.x, 0.01f, -50.0f, 50.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Spin Rate", &node->vectorValue3.w, 0.05f, -64.0f, 64.0f, "%.2f");
            }
            if (ImGui::CollapsingHeader("Attractor / Repeller")) {
                ImGui::TextDisabled("Strength > 0 = attract, < 0 = repel");
                m_compileDirty |= ImGui::DragFloat3("Attractor 0 Pos", &node->vectorValue5.x, 0.1f);
                m_compileDirty |= ImGui::DragFloat("Strength 0", &node->vectorValue5.w, 0.1f, -50.0f, 50.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Radius 0", &node->vectorValue7.x, 0.1f, 0.1f, 100.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Falloff 0", &node->vectorValue7.z, 0.1f, 0.0f, 2.0f, "%.1f");
                ImGui::Spacing();
                m_compileDirty |= ImGui::DragFloat3("Attractor 1 Pos", &node->vectorValue6.x, 0.1f);
                m_compileDirty |= ImGui::DragFloat("Strength 1", &node->vectorValue6.w, 0.1f, -50.0f, 50.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Radius 1", &node->vectorValue7.y, 0.1f, 0.1f, 100.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Falloff 1", &node->vectorValue7.w, 0.1f, 0.0f, 2.0f, "%.1f");
            }
            if (ImGui::CollapsingHeader("Collision")) {
                ImGui::TextDisabled("Ground plane collision");
                m_compileDirty |= ImGui::DragFloat3("Plane Normal", &node->vectorValue8.x, 0.01f, -1.0f, 1.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Plane D", &node->vectorValue8.w, 0.1f, -100.0f, 100.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Restitution", &node->vectorValue9.x, 0.01f, 0.0f, 1.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Friction", &node->vectorValue9.y, 0.01f, 0.0f, 1.0f, "%.2f");
            }
            break;
        case EffectGraphNodeType::Lifetime:
            if (ImGui::CollapsingHeader("System State", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_compileDirty |= ImGui::DragFloat("Duration", &node->scalar, 0.01f, 0.10f, 120.0f, "%.2f s");
                m_asset.previewDefaults.duration = node->scalar;
            }
            break;
        case EffectGraphNodeType::Output:
            if (ImGui::CollapsingHeader("Effect Asset", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_compileDirty |= InputTextString("Effect Name", m_asset.name);
                int seed = static_cast<int>(m_asset.previewDefaults.seed);
                if (ImGui::DragInt("Preview Seed", &seed, 1.0f, 1, 1000000)) {
                    m_asset.previewDefaults.seed = static_cast<uint32_t>(seed);
                    m_compileDirty = true;
                }
                DrawAssetSlotControl("Preview Mesh", m_asset.previewDefaults.previewMeshPath, AssetPickerKind::Mesh, 0, true);
            }
            break;
        case EffectGraphNodeType::Color:
            if (ImGui::CollapsingHeader("Color Module", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_compileDirty |= ImGui::ColorEdit4("Start Color", &node->vectorValue.x);
                m_compileDirty |= ImGui::ColorEdit4("End Color", &node->vectorValue2.x);
            }
            break;
        case EffectGraphNodeType::MeshSource:
            if (ImGui::CollapsingHeader("Mesh Source", ImGuiTreeNodeFlags_DefaultOpen)) {
                DrawAssetSlotControl("Mesh Asset", node->stringValue, AssetPickerKind::Mesh, node->id, false);
            }
            break;
        case EffectGraphNodeType::MeshRenderer: {
            // Core: tint, blend mode. Variant flags (intValue2) are picked via the
            // "Effect Presets" section further down; the current value is shown there.
            if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_compileDirty |= ImGui::ColorEdit4("Tint", &node->vectorValue.x);
                int blendState = node->intValue;
                const char* blendItems[] = { "Opaque", "Transparency", "Additive", "Subtraction", "Multiply", "Alpha" };
                if (ImGui::Combo("Blend", &blendState, blendItems, IM_ARRAYSIZE(blendItems))) {
                    node->intValue = blendState;
                    m_compileDirty = true;
                }
            }

            // Textures. Slot->stringValueN mapping matches EffectCompiler.cpp:
            //   stringValue  : base albedo (t0, MeshFlag_Texture)
            //   stringValue2 : mask / dissolve noise (t1, MeshFlag_Mask / Dissolve)
            //   stringValue3 : tangent-space normal map (t2, MeshFlag_NormalMap)
            //   stringValue4 : flow map / gradient LUT (t3, MeshFlag_FlowMap / GradientMap)
            //   stringValue5 : sub texture / distort noise (t4, MeshFlag_SubTexture / Distort)
            //   stringValue6 : emission mask (t5, MeshFlag_Emission)
            if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_compileDirty |= DrawAssetSlotControl("Base Texture",  node->stringValue,  AssetPickerKind::Texture, node->id, false);
                m_compileDirty |= DrawAssetSlotControl("Mask / Dissolve Noise", node->stringValue2, AssetPickerKind::Texture, node->id, false);
                m_compileDirty |= DrawAssetSlotControl("Normal Map",    node->stringValue3, AssetPickerKind::Texture, node->id, false);
                m_compileDirty |= DrawAssetSlotControl("Flow Map / Gradient", node->stringValue4, AssetPickerKind::Texture, node->id, false);
                m_compileDirty |= DrawAssetSlotControl("Sub / Distort Texture", node->stringValue5, AssetPickerKind::Texture, node->id, false);
                m_compileDirty |= DrawAssetSlotControl("Emission Tex",  node->stringValue6, AssetPickerKind::Texture, node->id, false);
            }

            // UV Animation. Controls map to variantParams.constants in the compiler:
            //   vectorValue3.zw -> scrollSpeed (MeshFlag_Scroll)
            //   vectorValue2.w  -> flowStrength (MeshFlag_FlowMap)
            //   vectorValue4.z  -> distortStrength (MeshFlag_Distort / ChromaticAberration)
            if (ImGui::CollapsingHeader("UV Animation", ImGuiTreeNodeFlags_None)) {
                m_compileDirty |= ImGui::DragFloat2("Scroll Speed (XY)", &node->vectorValue3.z, 0.05f, -10.0f, 10.0f);
                m_compileDirty |= ImGui::DragFloat("Flow Strength",    &node->vectorValue2.w, 0.01f, 0.0f, 5.0f);
                m_compileDirty |= ImGui::DragFloat("Distort Strength", &node->vectorValue4.z, 0.01f, 0.0f, 5.0f);
                ImGui::TextDisabled("Requires MeshFlag_Scroll / FlowMap / Distort");
            }

            // Dissolve. When MeshFlag_Dissolve is on, EffectSystems overrides
            // dissolveAmount with (1 - lifetimeFade), so the static value below
            // becomes the t=0 starting point only.
            if (ImGui::CollapsingHeader("Dissolve", ImGuiTreeNodeFlags_None)) {
                m_compileDirty |= ImGui::DragFloat("Dissolve Amount", &node->vectorValue2.x, 0.005f, 0.0f, 1.0f);
                ImGui::TextDisabled("(runtime drives 0->1 over lifetime when MeshFlag_Dissolve is set)");
                m_compileDirty |= ImGui::DragFloat("Dissolve Edge",  &node->vectorValue2.y, 0.005f, 0.001f, 1.0f);
                m_compileDirty |= ImGui::ColorEdit4("Dissolve Glow Color", &node->vectorValue5.x);
            }

            // Fresnel (view-angle rim, blown-out silhouette) and RimLight
            // (stylized back-light rim). Both use the same pow(1 - NdotV, k)
            // but with separate power/color so they can coexist.
            if (ImGui::CollapsingHeader("Fresnel & Rim", ImGuiTreeNodeFlags_None)) {
                m_compileDirty |= ImGui::DragFloat("Fresnel Power", &node->vectorValue2.z, 0.05f, 0.0f, 16.0f);
                m_compileDirty |= ImGui::ColorEdit4("Fresnel Color", &node->vectorValue6.x);
                m_compileDirty |= ImGui::DragFloat("Rim Power",     &node->vectorValue4.x, 0.05f, 0.0f, 16.0f);
                m_compileDirty |= ImGui::ColorEdit4("Rim Color",    &node->vectorValue7.x);
            }

            // Emission: additive glow multiplied by the emission texture alpha.
            if (ImGui::CollapsingHeader("Emission", ImGuiTreeNodeFlags_None)) {
                m_compileDirty |= ImGui::DragFloat("Emission Intensity", &node->vectorValue4.y, 0.05f, 0.0f, 16.0f);
                m_compileDirty |= ImGui::ColorEdit4("Emission Color",    &node->vectorValue8.x);
            }

            // B3: Preset Templates
            if (ImGui::CollapsingHeader("Effect Presets", ImGuiTreeNodeFlags_None)) {
                struct PresetEntry { const char* label; uint32_t flags; };
                static const PresetEntry kPresets[] = {
                    { "Slash Basic",        0x00004003u },
                    { "Slash Glow",         0x00006003u },
                    { "Slash Flow",         0x00014021u },
                    { "Slash Full",         0x00036023u },
                    { "Magic Circle",       0x00014011u },
                    { "Magic Summon",       0x0000080Bu },
                    { "Magic Aura",         0x00024021u },
                    { "Magic Explosion",    0x00004105u },
                    { "Universal Glow",     0x00084021u },
                    { "Universal Flow",     0x00018001u },
                };
                for (const auto& preset : kPresets) {
                    if (ImGui::Button(preset.label, ImVec2(130.0f, 0.0f))) {
                        node->intValue2 = static_cast<int>(preset.flags);
                        m_compileDirty = true;
                    }
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::Text("Current Key: 0x%08X", static_cast<uint32_t>(node->intValue2));
            }

            // B1: Shader Feature Flags
            if (ImGui::CollapsingHeader("Shader Features", ImGuiTreeNodeFlags_DefaultOpen)) {
                uint32_t flags = static_cast<uint32_t>(node->intValue2);
                bool dirty = false;
                auto FlagCheck = [&](const char* label, uint32_t bit) {
                    bool v = (flags & bit) != 0;
                    if (ImGui::Checkbox(label, &v)) {
                        flags = v ? (flags | bit) : (flags & ~bit);
                        dirty = true;
                    }
                };

                ImGui::TextDisabled("-- Base --");
                FlagCheck("Texture",          1u << 0);
                FlagCheck("AlphaFade",         1u << 14);

                ImGui::TextDisabled("-- Surface --");
                FlagCheck("NormalMap",         1u << 11);
                FlagCheck("Distort",           1u << 2);
                FlagCheck("Mask",              1u << 4);

                ImGui::TextDisabled("-- Motion --");
                FlagCheck("FlowMap",           1u << 12);
                FlagCheck("Scroll",            1u << 20);
                FlagCheck("Flipbook",          1u << 6);

                ImGui::TextDisabled("-- Dissolve --");
                FlagCheck("Dissolve",          1u << 1);
                FlagCheck("DissolveGlow",      1u << 9);

                ImGui::TextDisabled("-- Lighting --");
                FlagCheck("Lighting",          1u << 3);
                FlagCheck("Fresnel",           1u << 5);
                FlagCheck("RimLight",          1u << 17);
                FlagCheck("MatCap",            1u << 10);
                FlagCheck("Toon",              1u << 16);

                ImGui::TextDisabled("-- Color --");
                FlagCheck("GradientMap",       1u << 7);
                FlagCheck("ChromaticAberration", 1u << 8);
                FlagCheck("VertexColorBlend",  1u << 18);
                FlagCheck("SubTexture",        1u << 15);
                FlagCheck("Emission",          1u << 19);
                FlagCheck("SideFade",          1u << 13);

                if (dirty) {
                    node->intValue2 = static_cast<int>(flags);
                    m_compileDirty = true;
                }
            }

            // B2: Effect Parameters (shown only when relevant flags are set)
            {
                const uint32_t flags = static_cast<uint32_t>(node->intValue2);

                if ((flags & (1u << 1)) && ImGui::CollapsingHeader("Dissolve", ImGuiTreeNodeFlags_DefaultOpen)) {
                    m_compileDirty |= ImGui::DragFloat("Amount##D",    &node->vectorValue2.x, 0.01f, 0.0f, 1.0f, "%.3f");
                    m_compileDirty |= ImGui::DragFloat("Edge Width##D",&node->vectorValue2.y, 0.001f, 0.0f, 0.5f, "%.4f");
                    if (flags & (1u << 9)) {
                        m_compileDirty |= ImGui::ColorEdit4("Glow Color##D", &node->vectorValue5.x);
                    }
                    DrawAssetSlotControl("Mask Tex##D", node->stringValue2, AssetPickerKind::Texture, node->id, false);
                }

                if ((flags & (1u << 12)) && ImGui::CollapsingHeader("Flow Map", ImGuiTreeNodeFlags_DefaultOpen)) {
                    m_compileDirty |= ImGui::DragFloat("Strength##F",  &node->vectorValue2.w, 0.01f, 0.0f, 2.0f, "%.3f");
                    m_compileDirty |= ImGui::DragFloat2("Speed##F",    &node->vectorValue3.x, 0.005f, -2.0f, 2.0f, "%.3f");
                    DrawAssetSlotControl("Flow Map Tex##F", node->stringValue4, AssetPickerKind::Texture, node->id, false);
                }

                if ((flags & (1u << 20)) && ImGui::CollapsingHeader("Scroll", ImGuiTreeNodeFlags_DefaultOpen)) {
                    m_compileDirty |= ImGui::DragFloat2("Scroll Speed##S", &node->vectorValue3.z, 0.005f, -2.0f, 2.0f, "%.3f");
                }

                if ((flags & (1u << 5)) && ImGui::CollapsingHeader("Fresnel", ImGuiTreeNodeFlags_DefaultOpen)) {
                    m_compileDirty |= ImGui::DragFloat("Power##Fr",    &node->vectorValue2.z, 0.1f, 0.1f, 10.0f, "%.2f");
                    m_compileDirty |= ImGui::ColorEdit4("Color##Fr",   &node->vectorValue6.x);
                }

                if ((flags & (1u << 17)) && ImGui::CollapsingHeader("Rim Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                    m_compileDirty |= ImGui::DragFloat("Rim Power##RL",  &node->vectorValue4.x, 0.1f, 0.1f, 10.0f, "%.2f");
                    m_compileDirty |= ImGui::ColorEdit4("Rim Color##RL", &node->vectorValue7.x);
                }

                if ((flags & (1u << 19)) && ImGui::CollapsingHeader("Emission", ImGuiTreeNodeFlags_DefaultOpen)) {
                    m_compileDirty |= ImGui::DragFloat("Intensity##Em",  &node->vectorValue4.y, 0.1f, 0.0f, 20.0f, "%.2f");
                    m_compileDirty |= ImGui::ColorEdit4("Color##Em",     &node->vectorValue8.x);
                    DrawAssetSlotControl("Emission Tex##Em", node->stringValue6, AssetPickerKind::Texture, node->id, false);
                }

                if ((flags & (1u << 2)) && ImGui::CollapsingHeader("Distortion", ImGuiTreeNodeFlags_DefaultOpen)) {
                    m_compileDirty |= ImGui::DragFloat("Strength##Dist", &node->vectorValue4.z, 0.005f, 0.0f, 1.0f, "%.4f");
                }

                if ((flags & (1u << 11)) && ImGui::CollapsingHeader("Normal Map", ImGuiTreeNodeFlags_DefaultOpen)) {
                    DrawAssetSlotControl("Normal Tex##NM", node->stringValue3, AssetPickerKind::Texture, node->id, false);
                }

                if ((flags & (1u << 15)) && ImGui::CollapsingHeader("Sub Texture", ImGuiTreeNodeFlags_DefaultOpen)) {
                    DrawAssetSlotControl("Sub Tex##Sub", node->stringValue5, AssetPickerKind::Texture, node->id, false);
                }
            }
            break;
        }
        case EffectGraphNodeType::SpriteRenderer: {
            if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                int drawMode = node->intValue;
                int sortMode = node->intValue2;
                const char* drawModes[] = { "Billboard", "Mesh", "Ribbon" };
                const char* sortModes[] = { "None", "Back To Front", "Front To Back" };
                if (ImGui::Combo("Draw Mode", &drawMode, drawModes, IM_ARRAYSIZE(drawModes))) {
                    node->intValue = drawMode;
                    m_compileDirty = true;
                }
                if (ImGui::Combo("Sort Mode", &sortMode, sortModes, IM_ARRAYSIZE(sortModes))) {
                    node->intValue2 = sortMode;
                    m_compileDirty = true;
                }
                m_compileDirty |= ImGui::ColorEdit4("Tint", &node->vectorValue.x);
                m_compileDirty |= ImGui::DragFloat("Ribbon Width", &node->vectorValue2.x, 0.005f, 0.01f, 10.0f, "%.3f");
                m_compileDirty |= ImGui::DragFloat("Velocity Stretch", &node->vectorValue2.y, 0.01f, 0.05f, 20.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Alpha Scale", &node->vectorValue2.z, 0.01f, 0.01f, 4.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Flipbook FPS", &node->vectorValue2.w, 0.1f, 0.0f, 120.0f, "%.1f");
                m_compileDirty |= ImGui::DragFloat("Size Curve Bias", &node->vectorValue3.x, 0.01f, 0.05f, 8.0f, "%.2f");
                m_compileDirty |= ImGui::DragFloat("Alpha Curve Bias", &node->vectorValue3.y, 0.01f, 0.05f, 8.0f, "%.2f");
                if (ImGui::Checkbox("Soft Particle", &node->boolValue)) {
                    m_compileDirty = true;
                }
                if (node->boolValue) {
                    m_compileDirty |= ImGui::DragFloat("Soft Fade Scale", &node->scalar, 1.0f, 1.0f, 512.0f, "%.1f");
                }
                int subUvColumns = (std::max)(1, static_cast<int>(std::round(node->vectorValue3.z > 0.0f ? node->vectorValue3.z : 1.0f)));
                int subUvRows = (std::max)(1, static_cast<int>(std::round(node->vectorValue3.w > 0.0f ? node->vectorValue3.w : 1.0f)));
                if (ImGui::DragInt("Flipbook Columns", &subUvColumns, 1.0f, 1, 32)) {
                    node->vectorValue3.z = static_cast<float>(subUvColumns);
                    m_compileDirty = true;
                }
                if (ImGui::DragInt("Flipbook Rows", &subUvRows, 1.0f, 1, 32)) {
                    node->vectorValue3.w = static_cast<float>(subUvRows);
                    m_compileDirty = true;
                }
                DrawAssetSlotControl("Texture", node->stringValue, AssetPickerKind::Texture, node->id, false);
            }
            break;
        }
        case EffectGraphNodeType::Float:
            m_compileDirty |= ImGui::DragFloat("Value", &node->scalar, 0.01f);
            break;
        case EffectGraphNodeType::Vec3:
            m_compileDirty |= ImGui::DragFloat3("Value", &node->vectorValue.x, 0.01f);
            break;
        default:
            ImGui::TextDisabled("This module has no extra properties.");
            break;
        }
    };

    const EffectGraphLink* selectedLink = nullptr;
    if (m_selectedLinkId != 0) {
        auto linkIt = std::find_if(
            m_asset.links.begin(),
            m_asset.links.end(),
            [&](const EffectGraphLink& link) { return link.id == m_selectedLinkId; });
        if (linkIt != m_asset.links.end()) {
            selectedLink = &(*linkIt);
        }
    }

    if (node) {
        ImGui::TextColored(NiagaraSectionColor(node->type), "%s", node->title.c_str());
        ImGui::TextDisabled("Selected Module");
    } else if (selectedLink) {
        ImGui::TextDisabled("Selected Link");
        const EffectGraphPin* startPin = m_asset.FindPin(selectedLink->startPinId);
        const EffectGraphPin* endPin = m_asset.FindPin(selectedLink->endPinId);
        if (startPin && endPin) {
            ImGui::Text("Link #%u", selectedLink->id);
            ImGui::TextDisabled("%s -> %s", startPin->name.c_str(), endPin->name.c_str());
        }
    } else {
        ImGui::TextDisabled("No module selected");
        ImGui::TextWrapped("Select a row in Stack Mode or a node in Node Mode, then edit numeric values here.");
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Selected Item", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (node) {
            drawSelectedNodeInspector();
        } else if (selectedLink) {
            const EffectGraphPin* startPin = m_asset.FindPin(selectedLink->startPinId);
            const EffectGraphPin* endPin = m_asset.FindPin(selectedLink->endPinId);
            if (startPin && endPin) {
                ImGui::Text("From: %s", startPin->name.c_str());
                ImGui::Text("To: %s", endPin->name.c_str());
                ImGui::Text("Type: %s", EffectValueTypeLabel(startPin->valueType));
            }
            if (ImGui::Button("Delete Link")) {
                m_asset.links.erase(
                    std::remove_if(
                        m_asset.links.begin(),
                        m_asset.links.end(),
                        [&](const EffectGraphLink& link) { return link.id == selectedLink->id; }),
                    m_asset.links.end());
                m_selectedLinkId = 0;
                m_compileDirty = true;
            }
        } else {
            ImGui::TextDisabled("Select a module from the center view.");
        }
    }

    ImGui::End();
}

void EffectEditorPanel::DrawCompiledPanel()
{
    if (!m_compiled) {
        ImGui::TextDisabled("Compile the graph to see runtime output.");
        return;
    }

    ImGui::Text("Status: %s", m_compiled->valid ? "Valid" : "Invalid");
    ImGui::Text("Duration: %.2f", m_compiled->duration);
    ImGui::Text("Mesh Descriptor: %s", m_compiled->meshRenderer.enabled ? "Enabled" : "Disabled");
    if (m_compiled->meshRenderer.enabled) {
        ImGui::TextWrapped("Mesh: %s", m_compiled->meshRenderer.meshAssetPath.c_str());
        ImGui::TextWrapped("Material: %s", m_compiled->meshRenderer.materialAssetPath.c_str());
        ImGui::Text("Blend: %s", BlendStateLabel(m_compiled->meshRenderer.blendState));
        ImGui::Text("Shader Variant Key: %u", m_compiled->meshRenderer.shaderVariantKey);
    }

    ImGui::Text("Particle Descriptor: %s", m_compiled->particleRenderer.enabled ? "Enabled" : "Disabled");
    if (m_compiled->particleRenderer.enabled) {
        const uint32_t alignedCapacity = AlignEffectParticleCount(m_compiled->particleRenderer.maxParticles);
        const uint32_t estimatedPages = (alignedCapacity + 8191u) / 8192u;
        ImGui::Text("Max Particles: %u", m_compiled->particleRenderer.maxParticles);
        ImGui::Text("GPU Capacity: %u", alignedCapacity);
        ImGui::Text("Arena Pages: %u", estimatedPages);
        ImGui::Text("Burst: %u", m_compiled->particleRenderer.burstCount);
        ImGui::Text("Motion: Curl %.2f / Vortex %.2f",
            m_compiled->particleRenderer.curlNoiseStrength,
            m_compiled->particleRenderer.vortexStrength);
        ImGui::Text("SubUV: %ux%u @ %.1f fps",
            m_compiled->particleRenderer.subUvColumns,
            m_compiled->particleRenderer.subUvRows,
            m_compiled->particleRenderer.subUvFrameRate);
        ImGui::Text("Soft Particle: %s (%.1f)",
            m_compiled->particleRenderer.softParticleEnabled ? "On" : "Off",
            m_compiled->particleRenderer.softParticleScale);
        ImGui::TextWrapped("Texture: %s", m_compiled->particleRenderer.texturePath.c_str());
    }

    if (!m_compiled->errors.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Errors");
        for (const auto& error : m_compiled->errors) {
            ImGui::BulletText("%s", error.c_str());
        }
    }

    if (!m_compiled->warnings.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "Warnings");
        for (const auto& warning : m_compiled->warnings) {
            ImGui::BulletText("%s", warning.c_str());
        }
    }

    ImGui::Separator();
    ImGui::Text("Execution Plan");
    ImGui::BulletText("Spawn Nodes: %zu", m_compiled->spawnNodeList.size());
    ImGui::BulletText("Update Nodes: %zu", m_compiled->updateNodeList.size());
    ImGui::BulletText("Render Nodes: %zu", m_compiled->renderNodeList.size());
}

void EffectEditorPanel::DrawAssetPanel()
{
    if (!ImGui::Begin(kAssetWindowTitle)) {
        ImGui::End();
        return;
    }

    static char parameterSearch[128] = {};
    const auto addColorParameter = [&](const char* name, const DirectX::XMFLOAT4& defaultValue) {
        EffectExposedParameter parameter;
        parameter.name = name;
        parameter.valueType = EffectValueType::Color;
        parameter.defaultValue = defaultValue;
        m_asset.exposedParameters.push_back(std::move(parameter));
        m_compileDirty = true;
    };
    const auto addFloatParameter = [&](const char* name, float defaultValue) {
        EffectExposedParameter parameter;
        parameter.name = name;
        parameter.valueType = EffectValueType::Float;
        parameter.defaultValue = { defaultValue, 0.0f, 0.0f, 0.0f };
        m_asset.exposedParameters.push_back(std::move(parameter));
        m_compileDirty = true;
    };

    ImGui::TextDisabled("Blackboard");
    ImGui::TextWrapped("Parameter ledger only. Main authoring stays in the center stack.");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##BlackboardSearch", "Search parameters", parameterSearch, sizeof(parameterSearch));
    if (ImGui::Button("+ Float")) {
        addFloatParameter("Scalar", 1.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Color")) {
        addColorParameter("Tint", { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    ImGui::Separator();
    if (m_asset.exposedParameters.empty()) {
        ImGui::TextDisabled("No parameters yet.");
    }

    for (size_t index = 0; index < m_asset.exposedParameters.size(); ++index) {
        auto& parameter = m_asset.exposedParameters[index];
        if (!ContainsInsensitive(parameter.name, parameterSearch)) {
            continue;
        }

        ImGui::PushID(static_cast<int>(index));
        ImGui::Separator();
        ImGui::TextColored(GetValueTypeColor(parameter.valueType), "%s", EffectValueTypeLabel(parameter.valueType));
        ImGui::SameLine();
        m_compileDirty |= InputTextString("##BlackboardParamName", parameter.name);
        if (parameter.valueType == EffectValueType::Color) {
            m_compileDirty |= ImGui::ColorEdit4("Default", &parameter.defaultValue.x);
        } else {
            m_compileDirty |= ImGui::DragFloat("Default", &parameter.defaultValue.x, 0.01f);
        }

        if (const char* binding = EffectParameterBindings::DescribeBinding(parameter.valueType, parameter.name)) {
            ImGui::TextDisabled("Binding: %s", binding);
        } else {
            ImGui::TextDisabled("Binding: custom / inactive");
        }

        if (ImGui::Button("Remove")) {
            m_asset.exposedParameters.erase(m_asset.exposedParameters.begin() + static_cast<long long>(index));
            m_compileDirty = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    ImGui::End();
}

void EffectEditorPanel::DrawAssetPickerPopup()
{
    if (m_assetPickerOpenRequested) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("EffectEditorAssetPicker");
        m_assetPickerOpenRequested = false;
    }

    if (!ImGui::BeginPopupModal("EffectEditorAssetPicker", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    const bool wantModels = m_assetPickerKind == AssetPickerKind::Mesh;
    const std::string* targetPath = GetAssetPickerTargetPath();
    const char* popupTitle = wantModels ? "Select Mesh" : "Select Texture";
    ImGui::TextUnformatted(popupTitle);
    ImGui::Separator();
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputTextWithHint("##PickerSearch", "Search assets", m_assetPickerSearch, sizeof(m_assetPickerSearch));

    std::vector<std::filesystem::path> assets;
    if (wantModels) {
        assets = CollectAssets("Data/Model", true);
    } else {
        assets = CollectAssets("Data/Texture", false);
        auto effectTextures = CollectAssets("Data/Effect", false);
        assets.insert(assets.end(), effectTextures.begin(), effectTextures.end());
    }
    std::sort(assets.begin(), assets.end());
    assets.erase(std::unique(assets.begin(), assets.end()), assets.end());

    if (targetPath && !targetPath->empty()) {
        ImGui::TextDisabled("Current: %s", targetPath->c_str());
    } else {
        ImGui::TextDisabled("Current: none");
    }

    std::vector<std::string> allAssetPaths;
    allAssetPaths.reserve(assets.size());
    for (const auto& path : assets) {
        allAssetPaths.push_back(path.generic_string());
    }

    const std::vector<std::string>& favoriteAssets = wantModels ? m_favoriteMeshAssets : m_favoriteTextureAssets;
    const std::vector<std::string>& recentAssets = wantModels ? m_recentMeshAssets : m_recentTextureAssets;

    const auto drawAssetGrid = [&](const std::vector<std::string>& assetPaths, const char* emptyLabel) {
        ImGui::BeginChild("##PickerGrid", ImVec2(720.0f, 420.0f), true);
        const float cellSize = 124.0f;
        const float width = ImGui::GetContentRegionAvail().x;
        const int columns = (std::max)(1, static_cast<int>(width / cellSize));
        ImGui::Columns(columns, "##PickerColumns", false);

        int visibleCount = 0;
        for (const auto& pathString : assetPaths) {
            const std::filesystem::path path(pathString);
            const std::string filename = path.filename().string();
            if (!ContainsInsensitive(filename, m_assetPickerSearch) && !ContainsInsensitive(pathString, m_assetPickerSearch)) {
                continue;
            }

            ++visibleCount;
            void* thumbId = nullptr;
            if (wantModels) {
                ThumbnailGenerator::Instance().Request(pathString);
                if (auto thumb = ThumbnailGenerator::Instance().Get(pathString)) {
                    thumbId = ImGuiRenderer::GetTextureID(thumb.get());
                }
            } else {
                if (auto texture = ResourceManager::Instance().GetTexture(pathString)) {
                    thumbId = ImGuiRenderer::GetTextureID(texture.get());
                }
            }

            ImGui::PushID(pathString.c_str());
            const bool isAssigned = targetPath && *targetPath == pathString;
            const bool isFavorite = IsFavoriteAsset(pathString, m_assetPickerKind);
            if (isAssigned) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.40f, 0.70f, 0.90f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.48f, 0.82f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.34f, 0.62f, 1.0f));
            }

            bool picked = false;
            if (thumbId) {
                picked = ImGui::ImageButton("##Thumb", thumbId, ImVec2(72.0f, 72.0f));
            } else {
                picked = ImGui::Button(wantModels ? ICON_FA_CUBE : ICON_FA_IMAGE, ImVec2(72.0f, 72.0f));
            }
            if (isAssigned) {
                ImGui::PopStyleColor(3);
            }
            if (picked) {
                AssignPickedAsset(pathString);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(isFavorite ? ICON_FA_STAR : ICON_FA_STAR_HALF_STROKE)) {
                ToggleFavoriteAsset(pathString, m_assetPickerKind);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(isFavorite ? "Remove from favorites" : "Add to favorites");
            }

            if (isAssigned) {
                ImGui::TextColored(ImVec4(0.45f, 0.76f, 1.0f, 1.0f), "%s", filename.c_str());
                ImGui::TextDisabled("Assigned");
            } else {
                ImGui::TextWrapped("%s", filename.c_str());
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", pathString.c_str());
            }
            ImGui::NextColumn();
            ImGui::PopID();
        }

        ImGui::Columns(1);
        if (visibleCount == 0) {
            ImGui::TextDisabled("%s", emptyLabel);
        }
        ImGui::EndChild();
    };

    if (ImGui::BeginTabBar("##EffectAssetPickerTabs")) {
        if (ImGui::BeginTabItem("All")) {
            drawAssetGrid(allAssetPaths, "No matching assets.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Favorites")) {
            drawAssetGrid(favoriteAssets, "No favorite assets yet.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Recent")) {
            drawAssetGrid(recentAssets, "No recent assets yet.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void EffectEditorPanel::NewDocument()
{
    StopPreview();
    m_asset = CreateDefaultEffectGraphAsset();
    m_compiled.reset();
    m_selectedNodeId = 0;
    m_selectedLinkId = 0;
    m_compileDirty = true;
    m_syncNodePositions = true;
}

bool EffectEditorPanel::SaveDocument()
{
    m_documentPath = m_documentPathBuffer;
    if (!std::filesystem::path(m_documentPath).has_parent_path()) {
        m_documentPath = "Data/EffectGraph/" + m_documentPath;
    }
    strcpy_s(m_documentPathBuffer, m_documentPath.c_str());
    return EffectGraphSerializer::Save(m_documentPath, m_asset);
}

bool EffectEditorPanel::LoadDocument()
{
    m_documentPath = m_documentPathBuffer;
    EffectGraphAsset loadedAsset;
    if (!EffectGraphSerializer::Load(m_documentPath, loadedAsset)) {
        return false;
    }
    SanitizeGraphAsset(loadedAsset);
    StopPreview();
    m_asset = std::move(loadedAsset);
    strcpy_s(m_documentPathBuffer, m_documentPath.c_str());
    m_selectedNodeId = 0;
    m_selectedLinkId = 0;
    m_compiled.reset();
    m_compileDirty = true;
    m_syncNodePositions = true;
    return true;
}

std::string EffectEditorPanel::BuildTransientAssetKey() const
{
    return "editor://effect/" + m_asset.graphId;
}

std::string EffectEditorPanel::GetActiveAssetKey() const
{
    // Always use the transient key: CompileDocument() registers the freshly
    // compiled asset only under the transient key. Returning the document
    // path here would cause the registry's disk-backed cache to be used,
    // which ignores live template/graph edits.
    return BuildTransientAssetKey();
}

bool EffectEditorPanel::CompileDocument()
{
    EnsureGuiAuthoringLinks(m_asset);
    EffectGraphAsset graphToCompile = m_asset;
    EnsureGuiAuthoringLinks(graphToCompile);
    SanitizeGraphAsset(graphToCompile);
    SanitizeGraphAsset(m_asset);
    if (graphToCompile.previewDefaults.previewMeshPath.empty() && !m_selectedMeshPath.empty()) {
        graphToCompile.previewDefaults.previewMeshPath = m_selectedMeshPath;
    }

    LogGraphStructure(graphToCompile, "EffectCompile:graphToCompile");

    m_compiled = EffectCompiler::Compile(graphToCompile, m_documentPath);
    EffectRuntimeRegistry::Instance().RegisterTransientAsset(BuildTransientAssetKey(), m_compiled);
    m_compileDirty = false;
    return m_compiled && m_compiled->valid;
}

void EffectEditorPanel::QueuePreviewSpawn()
{
    if (!m_registry || !m_compiled || !m_compiled->valid) {
        return;
    }

    if (Entity::IsNull(m_previewEntity) || !m_registry->IsAlive(m_previewEntity)) {
        m_previewEntity = m_registry->CreateEntity();
        m_registry->AddComponent(m_previewEntity, NameComponent{ "Effect Preview" });
        m_registry->AddComponent(m_previewEntity, HierarchyComponent{});
        m_registry->AddComponent(m_previewEntity, TransformComponent{});
        m_registry->AddComponent(m_previewEntity, EffectPreviewTagComponent{});
    }

    auto* transform = m_registry->GetComponent<TransformComponent>(m_previewEntity);
    if (transform) {
        transform->localPosition = m_previewAnchor;
        transform->localScale = { 1.0f, 1.0f, 1.0f };
        transform->isDirty = true;
    }

    EffectAssetComponent assetComponent;
    assetComponent.assetPath = GetActiveAssetKey();
    assetComponent.autoPlay = true;
    assetComponent.loop = true;
    assetComponent.useSelectedMeshFallback = true;

    EffectPlaybackComponent playbackComponent;
    playbackComponent.seed = m_asset.previewDefaults.seed;
    playbackComponent.loop = true;

    EffectSpawnRequestComponent requestComponent;
    requestComponent.pending = true;
    requestComponent.restartIfActive = true;

    m_registry->AddComponent(m_previewEntity, assetComponent);
    m_registry->AddComponent(m_previewEntity, playbackComponent);
    m_registry->AddComponent(m_previewEntity, requestComponent);
    EnsureRuntimeOverrideComponent(m_previewEntity);
}

void EffectEditorPanel::StopPreview()
{
    if (!m_registry || Entity::IsNull(m_previewEntity) || !m_registry->IsAlive(m_previewEntity)) {
        m_previewEntity = Entity::NULL_ID;
        return;
    }
    // Destroy the runtime instance BEFORE destroying the entity, otherwise the
    // runtime is leaked in EffectRuntimeRegistry::m_instances (DestroyEntity
    // does not run FinalizeStoppedPlayback on POD components). The runtime's
    // GPU particle allocation stays in runtimeAllocations for up to 240 frames
    // regardless, but at least we drop the CPU-side state immediately.
    if (auto* playback = m_registry->GetComponent<EffectPlaybackComponent>(m_previewEntity)) {
        if (playback->runtimeInstanceId != 0) {
            EffectRuntimeRegistry::Instance().Destroy(playback->runtimeInstanceId);
            playback->runtimeInstanceId = 0;
        }
    }
    m_registry->DestroyEntity(m_previewEntity);
    m_previewEntity = Entity::NULL_ID;
}

