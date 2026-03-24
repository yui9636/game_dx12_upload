#include "BTEditorComponent.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <fstream>
#include "System/Dialog.h"
#include "System/PathResolver.h"
#include"JSONManager.h"
#include "BTCommands.h"

namespace ed = ax::NodeEditor;

const float SIDE_PANEL_WIDTH = 350.0f;
const float BOTTOM_PANEL_HEIGHT = 160.0f;

std::map<unsigned int, BTNodeReport> BTEditorComponent::s_LiveDebugData;

std::map<void*, BTAgentInfo> BTEditorComponent::s_AgentRegistry;
void* BTEditorComponent::s_SelectedAgentPtr = nullptr;

void BTEditorComponent::PushLiveDebugData(const std::map<unsigned int, BTNodeReport>& data)
{
    s_LiveDebugData = data;
}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void BTEditorComponent::ApplyNiagaraTheme() {
    auto& style = ed::GetStyle();
    style.NodeRounding = 10.0f;
    style.NodeBorderWidth = 2.0f;
    style.HoveredNodeBorderWidth = 4.0f;
    style.SelectedNodeBorderWidth = 4.0f;
    style.LinkStrength = 100.0f;

    style.Colors[ed::StyleColor_NodeBg] = ImColor(30, 30, 32, 245);
    style.Colors[ed::StyleColor_NodeBorder] = ImColor(60, 60, 65, 255);
    style.Colors[ed::StyleColor_HovNodeBorder] = ImColor(100, 100, 110, 255);
    style.Colors[ed::StyleColor_SelNodeBorder] = ImColor(255, 200, 50, 255);
}

void BTEditorComponent::Start() {
    ed::Config config;
    config.SettingsFile = "BTEditorLayout.json";
    m_Context = ed::CreateEditor(&config);

    ed::SetCurrentEditor(m_Context);
    ApplyNiagaraTheme();
    ed::SetCurrentEditor(nullptr);


    if (m_Graph.rootNodes.empty()) {
        unsigned int id = m_Graph.GetNextId();
        BTNodeEditorData root;
        root.id = id; root.name = "Root_Default"; root.type = BTNodeType::Root;
        root.pos = ImVec2(100, 100);
        root.outputs.push_back(BTNodePin(m_Graph.GetNextId(), id, BTNodePinType::Output));
        m_Graph.nodes.push_back(root);
        m_Graph.rootNodes["Default"] = id;
    }
}

void BTEditorComponent::Update(float dt) {
 
    for (auto& node : m_Graph.nodes)
    {
        auto it = s_LiveDebugData.find(node.id);
        if (it != s_LiveDebugData.end())
        {
            const BTNodeReport& report = it->second;

            switch (report.status)
            {
            case BTStatus::Running: node.lastStatus = BTExecuteStatus::Running; break;
            case BTStatus::Success: node.lastStatus = BTExecuteStatus::Success; break;
            case BTStatus::Failure: node.lastStatus = BTExecuteStatus::Failure; break;
            default:                node.lastStatus = BTExecuteStatus::Idle;    break;
            }

            if (node.lastStatus != BTExecuteStatus::Idle)
            {
                m_Graph.AddTrace(node.id, node.lastStatus, 0.0f);
            }
        }
        else
        {
            node.lastStatus = BTExecuteStatus::Idle;
        }
    }

    for (auto& link : m_Graph.links)
    {
        auto startNode = m_Graph.FindNodeByPin(link.startPinId);
        auto endNode = m_Graph.FindNodeByPin(link.endPinId);

        if (startNode && endNode)
        {
            link.isActive = (startNode->lastStatus == BTExecuteStatus::Running &&
                (endNode->lastStatus == BTExecuteStatus::Running || endNode->lastStatus == BTExecuteStatus::Success));
        }
    }


}

void BTEditorComponent::OnGUI()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("BEHAVIOR EDITOR", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar);

    ed::SetCurrentEditor(m_Context);

    static bool showInspector = true;
    static float inspectorWidth = 300.0f;

    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    if (ImGui::BeginTable("MainLayoutTable", showInspector ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableNextColumn();

        ed::Begin("BT_Canvas");
        DrawNodes();
        DrawLinks();
        HandleInteraction();

        ed::Suspend();
        ShowSearchablePalette();

        ImGui::SetCursorPos(ImVec2(15, 15));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.09f, 0.85f));
        if (ImGui::BeginChild("OverlayHUD", ImVec2(300, 110), true))
        {
            ImGui::TextColored(ImVec4(0, 1, 1, 1), ":: AI DEBUG COMMANDER ::");
            ImGui::Separator();

            const char* currentName = "NO TARGET";
            if (s_SelectedAgentPtr != nullptr && s_AgentRegistry.count(s_SelectedAgentPtr)) {
                currentName = s_AgentRegistry[s_SelectedAgentPtr].name.c_str();
            }

            ImGui::SetNextItemWidth(160);
            if (ImGui::BeginCombo("Agent", currentName)) {
                for (auto& pair : s_AgentRegistry) {
                    void* pAgent = pair.first;
                    const std::string& aName = pair.second.name;
                    if (ImGui::Selectable(aName.c_str(), s_SelectedAgentPtr == pAgent)) {
                        s_SelectedAgentPtr = pAgent;
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("SAVE")) {
                char pBuf[MAX_PATH] = "";
                if (Dialog::SaveFileName(pBuf, MAX_PATH, "AI(*.json)\0*.json\0", "Save AI") == DialogResult::OK) {
                    SaveGraph(PathResolver::Resolve(pBuf));
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("LOAD")) {
                char pBuf[MAX_PATH] = "";
                if (Dialog::OpenFileName(pBuf, MAX_PATH, "AI(*.json)\0*.json\0", "Load AI") == DialogResult::OK) {
                    LoadGraph(PathResolver::Resolve(pBuf));
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(showInspector ? "HIDE UI >>" : "<< SHOW UI")) showInspector = !showInspector;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ed::Resume();
        ed::End();

        if (showInspector)
        {
            ImGui::TableNextColumn();
            ImGui::BeginChild("RightPanel", ImVec2(0, 0), false);
            DrawDynamicInspector();
            ImGui::Separator();
            DrawBlackboardWatcher();
            ImGui::Separator();
            DrawTraceTimeline();
            ImGui::EndChild();
        }
        ImGui::EndTable();
    }

    ed::SetCurrentEditor(nullptr);
    ImGui::End();
    ImGui::PopStyleVar();
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void BTEditorComponent::HandleInteraction() {
    if (ed::BeginCreate(ImColor(255, 255, 255), 3.0f)) {
        ed::PinId s, e;
        if (ed::QueryNewLink(&s, &e)) {
            if (ed::AcceptNewItem()) {
                BTNodeLink nl;
                nl.id = m_Graph.GetNextId();
                nl.startPinId = (unsigned int)s.Get();
                nl.endPinId = (unsigned int)e.Get();
                UndoSystem::Instance().Execute(std::make_shared<CmdBTAddLink>(&m_Graph, nl));
            }
        }
    }
    ed::EndCreate();

    if (ed::BeginDelete()) {
        ed::LinkId deletedLinkId;
        while (ed::QueryDeletedLink(&deletedLinkId)) {
            if (ed::AcceptDeletedItem()) {
                unsigned int id = (unsigned int)deletedLinkId.Get();
                for (const auto& link : m_Graph.links) {
                    if (link.id == id) {
                        UndoSystem::Instance().Execute(std::make_shared<CmdBTDeleteLink>(&m_Graph, link));
                        break;
                    }
                }
            }
        }

        ed::NodeId deletedNodeId;
        while (ed::QueryDeletedNode(&deletedNodeId)) {
            if (ed::AcceptDeletedItem()) {
                unsigned int nid = (unsigned int)deletedNodeId.Get();
                BTNodeEditorData* nodeData = m_Graph.FindNode(nid);
                if (nodeData) {
                    std::vector<BTNodeLink> attachedLinks;
                    for (const auto& l : m_Graph.links) {
                        if (m_Graph.FindNodeByPin(l.startPinId)->id == nid ||
                            m_Graph.FindNodeByPin(l.endPinId)->id == nid) {
                            attachedLinks.push_back(l);
                        }
                    }
                    nodeData->pos = ed::GetNodePosition(nid);
                    UndoSystem::Instance().Execute(std::make_shared<CmdBTDeleteNode>(&m_Graph, *nodeData, attachedLinks));
                }
            }
        }
    }
    ed::EndDelete();

    static ImVec2 startDragPos;
    static unsigned int draggingId = 0;
    ed::NodeId activeId;
    if (ed::GetSelectedNodes(&activeId, 1) > 0) {
        if (ImGui::IsMouseClicked(0)) {
            startDragPos = ed::GetNodePosition(activeId);
            draggingId = (unsigned int)activeId.Get();
        }
        if (draggingId == (unsigned int)activeId.Get() && ImGui::IsMouseReleased(0)) {
            ImVec2 endPos = ed::GetNodePosition(activeId);
            if (startDragPos.x != endPos.x || startDragPos.y != endPos.y) {
                UndoSystem::Instance().Record(std::make_shared<CmdBTMoveNode>(draggingId, startDragPos, endPos));
            }
            draggingId = 0;
        }
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void BTEditorComponent::DrawDynamicInspector() {
    ImGui::TextColored(ImVec4(0, 1, 1, 1), ">> PROPERTY INSPECTOR");
    ImGui::Separator();

    ed::NodeId selected;
    if (ed::GetSelectedNodes(&selected, 1) > 0) {
        unsigned int id = (unsigned int)selected.Get();
        BTNodeEditorData* node = m_Graph.FindNode(id);
        if (node) {
            ImGui::LabelText("ID", "%u", node->id);
            char b[64]; strcpy_s(b, node->name.c_str());
            if (ImGui::InputText("Node Name", b, 64)) node->name = b;

            ImGui::Separator();
            ImGui::Text("Custom Properties");

            for (auto& [key, val] : node->properties) {
                if (auto* pF = std::get_if<float>(&val)) ImGui::DragFloat(key.c_str(), pF, 0.1f);
                else if (auto* pI = std::get_if<int>(&val)) ImGui::DragInt(key.c_str(), pI);
                else if (auto* pB = std::get_if<bool>(&val)) ImGui::Checkbox(key.c_str(), pB);
                else if (auto* pS = std::get_if<std::string>(&val)) {
                    char sBuf[128]; strcpy_s(sBuf, pS->c_str());
                    if (ImGui::InputText(key.c_str(), sBuf, 128)) *pS = sBuf;
                }
            }

            if (ImGui::Button("+ Add Parameter")) ImGui::OpenPopup("AddParam");
            if (ImGui::BeginPopup("AddParam")) {
                if (ImGui::MenuItem("New Float")) node->properties["NewFloat"] = 0.0f;
                if (ImGui::MenuItem("New Int"))   node->properties["NewInt"] = 0;
                if (ImGui::MenuItem("New Bool"))  node->properties["NewBool"] = false;
                ImGui::EndPopup();
            }
        }
    }
    else {
        ImGui::TextDisabled("Select a node to edit properties.");
    }
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void BTEditorComponent::DrawNodes() {
    for (auto& node : m_Graph.nodes) {
        ed::BeginNode(node.id);

        ImColor headCol = ImColor(60, 60, 65);
        if (node.type == BTNodeType::Root) headCol = ImColor(180, 50, 50);
        else if (node.type == BTNodeType::Composite) headCol = ImColor(200, 150, 20);
        else if (node.type == BTNodeType::Action) headCol = ImColor(40, 100, 180);

        bool hasError = false;
        for (auto& err : m_Graph.validationErrors) {
            if (err.nodeId == node.id) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "[!] %s", node.name.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", err.message.c_str());
                hasError = true; break;
            }
        }
        if (!hasError) ImGui::TextColored(headCol, "E½E½ %s", node.name.c_str());

        ImGui::Separator();

        ImGui::BeginGroup();
        for (auto& p : node.inputs) { ed::BeginPin(p.id, ed::PinKind::Input); ImGui::Text(" >"); ed::EndPin(); }
        ImGui::EndGroup();
        ImGui::SameLine(); ImGui::Dummy(ImVec2(40, 0)); ImGui::SameLine();
        ImGui::BeginGroup();
        for (auto& p : node.outputs) { ed::BeginPin(p.id, ed::PinKind::Output); ImGui::Text("> "); ed::EndPin(); }
        ImGui::EndGroup();

        ed::EndNode();
    }
}

void BTEditorComponent::DrawLinks() {
    for (auto& link : m_Graph.links) {
        ImColor col = link.isActive ? ImColor(0, 255, 120) : ImColor(200, 200, 200, 180);
        ed::Link(link.id, link.startPinId, link.endPinId, col, 2.0f);

        BTNodeEditorData* startNode = m_Graph.FindNodeByPin(link.startPinId);
        BTNodeEditorData* endNode = m_Graph.FindNodeByPin(link.endPinId);

        if (startNode && endNode) {
            ImVec2 pStart = ed::GetNodePosition(startNode->id);
            ImVec2 pEnd = ed::GetNodePosition(endNode->id);

            ImVec2 midPos = ImVec2((pStart.x + pEnd.x) * 0.5f + 50.0f, (pStart.y + pEnd.y) * 0.5f);

            ed::Suspend();
            ImGui::SetCursorScreenPos(ed::CanvasToScreen(midPos));
            ImGui::PushID(link.id);
            ImGui::SetNextItemWidth(45);

            float currentW = link.weight;
            if (ImGui::DragFloat("##W", &currentW, 0.05f, 0.0f, 10.0f, "%.1f")) {
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (link.weight != currentW) {
                    UndoSystem::Instance().Record(std::make_shared<CmdBTChangeWeight>(&m_Graph, link.id, link.weight, currentW));
                    link.weight = currentW;
                }
            }
            ImGui::PopID();
            ed::Resume();
        }
    }
}

void BTEditorComponent::DrawTraceTimeline() {
    ImGui::TextColored(ImVec4(0, 1, 1, 1), ">> LOGIC TRACE HISTORY (PAST 100 STEPS)");
    ImGui::BeginChild("ScrollTimeline", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    float stepW = 30.0f;
    for (auto& log : m_Graph.executionTrace) {
        ImColor c = (log.status == BTExecuteStatus::Success) ? ImColor(50, 220, 100) :
            (log.status == BTExecuteStatus::Failure) ? ImColor(220, 50, 50) : ImColor(220, 200, 50);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + stepW - 2, p.y + 40), c, 4.0f);
        ImGui::Dummy(ImVec2(stepW, 40));
        if (ImGui::IsItemHovered()) {
            BTNodeEditorData* n = m_Graph.FindNode(log.nodeId);
            ImGui::SetTooltip("Node: %s\nTime: %.2f", n ? n->name.c_str() : "???", log.timestamp);
        }
        ImGui::SameLine();
    }
    ImGui::EndChild();
}

void BTEditorComponent::DrawBlackboardWatcher() {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), ">> BLACKBOARD WATCHER");
    static float dist = 10.0f; ImGui::DragFloat("DistToPlayer", &dist, 0.1f);
    static int hp = 100; ImGui::SliderInt("Boss HP", &hp, 0, 100);
}

void BTEditorComponent::ShowSearchablePalette() {
    if (ed::ShowBackgroundContextMenu()) ImGui::OpenPopup("SPalette");
    if (ImGui::BeginPopup("SPalette")) {
        static char filter[64] = ""; ImGui::InputText("Search Nodes", filter, 64);
        ImGui::Separator();
        auto Add = [&](const char* label, BTNodeType type) {
            if (filter[0] != '\0' && !strstr(label, filter)) return;
            if (ImGui::MenuItem(label)) {
                BTNodeEditorData n; n.id = m_Graph.GetNextId(); n.name = label; n.type = type;
                n.pos = ed::ScreenToCanvas(ImGui::GetMousePos());
                if (type != BTNodeType::Root) n.inputs.push_back(BTNodePin(m_Graph.GetNextId(), n.id, BTNodePinType::Input));
                if (type != BTNodeType::Action) n.outputs.push_back(BTNodePin(m_Graph.GetNextId(), n.id, BTNodePinType::Output));
                m_Graph.nodes.push_back(n);
                ed::SetNodePosition(ed::NodeId(n.id), n.pos);
            }
            };
        if (ImGui::BeginMenu("Composite")) { Add("Selector", BTNodeType::Composite); Add("Sequence", BTNodeType::Composite); ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Action")) { Add("PlayAction", BTNodeType::Action); Add("Chase", BTNodeType::Action); ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Advanced")) { Add("SubTree", BTNodeType::SubTree); ImGui::EndMenu(); }
        ImGui::EndPopup();
    }
}

void BTEditorComponent::SaveGraph(const std::string& path) {
    json root;
    root["nextId"] = m_Graph.GetNextId();
    root["activePhase"] = m_Graph.activePhase;
    json phasesJ; for (auto& [name, id] : m_Graph.rootNodes) phasesJ[name] = id;
    root["phases"] = phasesJ;

    for (auto& n : m_Graph.nodes) {
        json nJ; nJ["id"] = n.id; nJ["name"] = n.name; nJ["type"] = (int)n.type;
        ImVec2 p = ed::GetNodePosition(ed::NodeId(n.id)); nJ["pos"] = { p.x, p.y };
        json pJ; for (auto& [k, v] : n.properties) std::visit([&](auto&& arg) { pJ[k] = arg; }, v);
        nJ["props"] = pJ;
        json pinsIn = json::array(), pinsOut = json::array();
        for (auto& pin : n.inputs) pinsIn.push_back({ {"id", pin.id} });
        for (auto& pin : n.outputs) pinsOut.push_back({ {"id", pin.id} });
        nJ["inputs"] = pinsIn; nJ["outputs"] = pinsOut;
        root["nodes"].push_back(nJ);
    }
    for (auto& l : m_Graph.links) root["links"].push_back({ {"id", l.id}, {"s", l.startPinId}, {"e", l.endPinId} });
    std::ofstream ofs(path); if (ofs.is_open()) { ofs << root.dump(4); ofs.close(); }
}

void BTEditorComponent::LoadGraph(const std::string& path) {
    std::ifstream ifs(path); if (!ifs.is_open()) return;
    json root; ifs >> root;
    m_Graph.nodes.clear(); m_Graph.links.clear(); m_Graph.rootNodes.clear();
    for (auto& [name, id] : root["phases"].items()) m_Graph.rootNodes[name] = id;
    m_Graph.activePhase = root.value("activePhase", "Default");
    for (auto& nJ : root["nodes"]) {
        BTNodeEditorData n; n.id = nJ["id"]; n.name = nJ["name"]; n.type = (BTNodeType)nJ["type"];
        n.pos = ImVec2(nJ["pos"][0], nJ["pos"][1]);
        for (auto& pJ : nJ["inputs"]) n.inputs.push_back(BTNodePin(pJ["id"], n.id, BTNodePinType::Input));
        for (auto& pJ : nJ["outputs"]) n.outputs.push_back(BTNodePin(pJ["id"], n.id, BTNodePinType::Output));
        if (nJ.contains("props")) {
            for (auto& [k, v] : nJ["props"].items()) {
                if (v.is_number_float()) n.properties[k] = v.get<float>();
                else if (v.is_number_integer()) n.properties[k] = v.get<int>();
                else if (v.is_boolean()) n.properties[k] = v.get<bool>();
                else if (v.is_string()) n.properties[k] = v.get<std::string>();
            }
        }
        m_Graph.nodes.push_back(n);
        ed::SetNodePosition(ed::NodeId(n.id), n.pos);
    }
    for (auto& lJ : root["links"]) m_Graph.links.push_back({ lJ["id"], lJ["s"], lJ["e"] });
    m_Graph.SetNextId(root["nextId"]);
}
