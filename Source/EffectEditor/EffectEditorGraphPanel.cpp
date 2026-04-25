// Graph + NodeMode panels for EffectEditorPanel — extracted from EffectEditorPanel.cpp.
// All entry points remain member functions of EffectEditorPanel.

#include "EffectEditorPanel.h"
#include "EffectEditorPanelInternal.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_node_editor.h>

#include "Console/Logger.h"
#include "EffectRuntime/EffectGraphAsset.h"
#include "Icon/IconsFontAwesome7.h"

namespace ed = ax::NodeEditor;

using namespace EffectEditorInternal;

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
