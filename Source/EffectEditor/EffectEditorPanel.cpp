#include "EffectEditorPanel.h"
#include "EffectEditorPanelInternal.h"
#include "EffectEditorTemplates.h"

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
    // Window title constants moved to EffectEditorPanelInternal.h for sharing.
    constexpr float kPreviewMinSize = 96.0f;
    constexpr float kPreviewToolbarHeight = 28.0f;
    // kSystemVisualNodeId / kEmitterVisualNodeId moved to EffectEditorPanelInternal.h.

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

    // GetValueTypeColor moved to EffectEditorPanelInternal.h.

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

    // Path/asset utilities moved to EffectEditorPanelInternal.h (inline) so
    // that EffectEditorAssetPicker.cpp / EffectEditorTemplates.cpp can share them.

    // NiagaraSectionColor moved to EffectEditorPanelInternal.h.

} // close anonymous namespace

// External-linkage authoring helpers shared between EffectEditorPanel.cpp and
// its extracted sibling modules (EffectEditorTemplates.cpp etc.).
namespace EffectEditorInternal
{
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
} // namespace EffectEditorInternal

using namespace EffectEditorInternal;

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
            EffectEditorTemplates::DrawMenuContents(*this);
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

                // MeshParticle Phase 2: only visible when drawMode == Mesh.
                //   slots: stringValue2=meshPath, vectorValue5=scale+scaleRand,
                //          vectorValue6=axis+speed, vectorValue7=orientRand+speedRand
                if (drawMode == static_cast<int>(EffectParticleDrawMode::Mesh)) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Mesh Particle");
                    DrawAssetSlotControl("Mesh Asset##SpriteMesh", spriteNode->stringValue2, AssetPickerKind::Mesh, spriteNode->id, false);
                    m_compileDirty |= ImGui::DragFloat3("Initial Scale", &spriteNode->vectorValue5.x, 0.01f, 0.001f, 100.0f, "%.3f");
                    m_compileDirty |= ImGui::SliderFloat("Scale Random", &spriteNode->vectorValue5.w, 0.0f, 1.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
                    m_compileDirty |= ImGui::DragFloat3("Angular Axis", &spriteNode->vectorValue6.x, 0.01f, -1.0f, 1.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat("Angular Speed (rad/s)", &spriteNode->vectorValue6.w, 0.05f, -20.0f, 20.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat3("Orient Rand YPR (rad)", &spriteNode->vectorValue7.x, 0.01f, 0.0f, 6.2832f, "%.2f");
                    m_compileDirty |= ImGui::SliderFloat("Speed Random##Mesh", &spriteNode->vectorValue7.w, 0.0f, 1.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
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

                // MeshParticle Phase 2: show mesh-only fields when drawMode == Mesh.
                if (drawMode == static_cast<int>(EffectParticleDrawMode::Mesh)) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Mesh Particle");
                    DrawAssetSlotControl("Mesh Asset##NodeMesh", node->stringValue2, AssetPickerKind::Mesh, node->id, false);
                    m_compileDirty |= ImGui::DragFloat3("Initial Scale", &node->vectorValue5.x, 0.01f, 0.001f, 100.0f, "%.3f");
                    m_compileDirty |= ImGui::SliderFloat("Scale Random", &node->vectorValue5.w, 0.0f, 1.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
                    m_compileDirty |= ImGui::DragFloat3("Angular Axis", &node->vectorValue6.x, 0.01f, -1.0f, 1.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat("Angular Speed (rad/s)", &node->vectorValue6.w, 0.05f, -20.0f, 20.0f, "%.2f");
                    m_compileDirty |= ImGui::DragFloat3("Orient Rand YPR (rad)", &node->vectorValue7.x, 0.01f, 0.0f, 6.2832f, "%.2f");
                    m_compileDirty |= ImGui::SliderFloat("Speed Random##Mesh", &node->vectorValue7.w, 0.0f, 1.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
                }
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


