#include "GameLoopEditorPanel.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <imgui.h>

#include "Console/Logger.h"
#include "Engine/EngineKernel.h"
#include "GameLoopAsset.h"
#include "GameLoopRuntime.h"

namespace
{
    GameLoopAsset& Asset() { return EngineKernel::Instance().GetGameLoopAsset(); }
    const GameLoopRuntime& Runtime() { return EngineKernel::Instance().GetGameLoopRuntime(); }

    const char* kConditionTypeLabels[] = {
        "None", "InputPressed", "UIButtonClicked", "TimerElapsed",
        "ActorDead", "AllActorsDead", "ActorMovedDistance", "RuntimeFlag",
        "StateMachineState", "TimelineEvent", "CustomEvent"
    };
    const GameLoopConditionType kConditionTypeValues[] = {
        GameLoopConditionType::None,
        GameLoopConditionType::InputPressed,
        GameLoopConditionType::UIButtonClicked,
        GameLoopConditionType::TimerElapsed,
        GameLoopConditionType::ActorDead,
        GameLoopConditionType::AllActorsDead,
        GameLoopConditionType::ActorMovedDistance,
        GameLoopConditionType::RuntimeFlag,
        GameLoopConditionType::StateMachineState,
        GameLoopConditionType::TimelineEvent,
        GameLoopConditionType::CustomEvent,
    };
    constexpr int kConditionTypeCount = sizeof(kConditionTypeValues) / sizeof(kConditionTypeValues[0]);

    int FindConditionTypeIndex(GameLoopConditionType t)
    {
        for (int i = 0; i < kConditionTypeCount; ++i) {
            if (kConditionTypeValues[i] == t) return i;
        }
        return 0;
    }

    const char* kActorTypeLabels[] = { "None", "Player", "Enemy", "NPC", "Neutral" };
    const ActorType kActorTypeValues[] = {
        ActorType::None, ActorType::Player, ActorType::Enemy, ActorType::NPC, ActorType::Neutral
    };
    constexpr int kActorTypeCount = sizeof(kActorTypeValues) / sizeof(kActorTypeValues[0]);

    int FindActorTypeIndex(ActorType a)
    {
        for (int i = 0; i < kActorTypeCount; ++i) {
            if (kActorTypeValues[i] == a) return i;
        }
        return 0;
    }
}

GameLoopEditorPanel::GameLoopEditorPanel()
{
    std::strncpy(m_loadPathBuf,   m_currentPath.string().c_str(), sizeof(m_loadPathBuf)   - 1);
    std::strncpy(m_saveAsPathBuf, m_currentPath.string().c_str(), sizeof(m_saveAsPathBuf) - 1);
}

void GameLoopEditorPanel::Draw(bool* p_open, bool* outFocused)
{
    if (!ImGui::Begin("GameLoop Editor", p_open)) {
        if (outFocused) *outFocused = false;
        ImGui::End();
        return;
    }
    if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    DrawToolbar();
    ImGui::Separator();

    ImGui::Columns(2, "GameLoopColumns");

    DrawNodeList();
    ImGui::Spacing();
    DrawTransitionList();

    ImGui::NextColumn();

    DrawNodeInspector();
    ImGui::Spacing();
    DrawTransitionInspector();

    ImGui::Columns(1);
    ImGui::Separator();

    DrawRuntimeStatus();
    ImGui::Separator();

    DrawValidateResult();

    ImGui::End();
}

void GameLoopEditorPanel::DrawToolbar()
{
    if (ImGui::Button("New Default Loop")) {
        DoNewDefault();
    }
    ImGui::SameLine();
    if (ImGui::Button("Validate")) {
        DoValidate();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        DoSave();
    }

    ImGui::Spacing();
    ImGui::Text("Path: %s", m_currentPath.string().c_str());

    ImGui::PushItemWidth(280.0f);
    ImGui::InputText("##loadPath", m_loadPathBuf, sizeof(m_loadPathBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        DoLoad(std::filesystem::path(m_loadPathBuf));
    }

    ImGui::PushItemWidth(280.0f);
    ImGui::InputText("##saveAsPath", m_saveAsPathBuf, sizeof(m_saveAsPathBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        DoSaveAs(std::filesystem::path(m_saveAsPathBuf));
    }
}

void GameLoopEditorPanel::DrawNodeList()
{
    GameLoopAsset& a = Asset();
    ImGui::TextUnformatted("Nodes");

    if (ImGui::Button("+##AddNode")) {
        GameLoopNode n;
        n.id   = a.AllocateNodeId();
        n.name = "NewNode";
        a.nodes.push_back(n);
        m_selectedNodeIndex = static_cast<int>(a.nodes.size()) - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("-##RemoveNode") && m_selectedNodeIndex >= 0 && m_selectedNodeIndex < (int)a.nodes.size()) {
        const uint32_t removedId = a.nodes[m_selectedNodeIndex].id;
        a.nodes.erase(a.nodes.begin() + m_selectedNodeIndex);
        // 関連 transition も除去する。
        a.transitions.erase(
            std::remove_if(a.transitions.begin(), a.transitions.end(),
                [removedId](const GameLoopTransition& t) {
                    return t.fromNodeId == removedId || t.toNodeId == removedId;
                }),
            a.transitions.end());
        if (a.startNodeId == removedId) a.startNodeId = a.nodes.empty() ? 0 : a.nodes.front().id;
        m_selectedNodeIndex = -1;
    }

    ImGui::BeginChild("NodeListChild", ImVec2(0.0f, 180.0f), true);
    for (int i = 0; i < (int)a.nodes.size(); ++i) {
        const auto& n = a.nodes[i];
        const bool isStart   = (n.id == a.startNodeId);
        const bool isCurrent = (Runtime().isActive && n.id == Runtime().currentNodeId);
        char label[192];
        std::snprintf(label, sizeof(label), "%s%s%s [id=%u]",
            isCurrent ? "* " : "  ",
            n.name.c_str(),
            isStart   ? " (start)" : "");
        if (ImGui::Selectable(label, m_selectedNodeIndex == i)) {
            m_selectedNodeIndex = i;
        }
    }
    ImGui::EndChild();
}

void GameLoopEditorPanel::DrawNodeInspector()
{
    GameLoopAsset& a = Asset();
    ImGui::TextUnformatted("Node Inspector");

    if (m_selectedNodeIndex < 0 || m_selectedNodeIndex >= (int)a.nodes.size()) {
        ImGui::TextDisabled("(node 未選択)");
        return;
    }

    GameLoopNode& n = a.nodes[m_selectedNodeIndex];

    char nameBuf[128] = {};
    std::strncpy(nameBuf, n.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("name", nameBuf, sizeof(nameBuf))) {
        n.name = nameBuf;
    }

    char pathBuf[256] = {};
    std::strncpy(pathBuf, n.scenePath.c_str(), sizeof(pathBuf) - 1);
    if (ImGui::InputText("scenePath", pathBuf, sizeof(pathBuf))) {
        n.scenePath = pathBuf;
    }

    ImGui::Text("id: %u", n.id);

    if (n.id != a.startNodeId) {
        if (ImGui::Button("Set as Start")) a.startNodeId = n.id;
    }
    else {
        ImGui::TextDisabled("[ Start Node ]");
    }
}

void GameLoopEditorPanel::DrawTransitionList()
{
    GameLoopAsset& a = Asset();
    ImGui::TextUnformatted("Transitions (top = highest priority)");

    if (ImGui::Button("+##AddTr")) {
        GameLoopTransition t;
        if (!a.nodes.empty()) {
            t.fromNodeId = a.nodes.front().id;
            t.toNodeId   = a.nodes.front().id;
        }
        t.name = "NewTransition";
        t.conditions.push_back(GameLoopCondition{}); // None で開始
        a.transitions.push_back(t);
        m_selectedTransitionIndex = (int)a.transitions.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("-##RemoveTr") && m_selectedTransitionIndex >= 0 && m_selectedTransitionIndex < (int)a.transitions.size()) {
        a.transitions.erase(a.transitions.begin() + m_selectedTransitionIndex);
        m_selectedTransitionIndex = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Up##MoveTrUp") && m_selectedTransitionIndex > 0) {
        std::swap(a.transitions[m_selectedTransitionIndex], a.transitions[m_selectedTransitionIndex - 1]);
        --m_selectedTransitionIndex;
    }
    ImGui::SameLine();
    if (ImGui::Button("Down##MoveTrDown")
        && m_selectedTransitionIndex >= 0
        && m_selectedTransitionIndex + 1 < (int)a.transitions.size())
    {
        std::swap(a.transitions[m_selectedTransitionIndex], a.transitions[m_selectedTransitionIndex + 1]);
        ++m_selectedTransitionIndex;
    }

    ImGui::BeginChild("TrListChild", ImVec2(0.0f, 180.0f), true);
    for (int i = 0; i < (int)a.transitions.size(); ++i) {
        const auto& t = a.transitions[i];
        const auto* fn = a.FindNode(t.fromNodeId);
        const auto* tn = a.FindNode(t.toNodeId);
        char label[256];
        std::snprintf(label, sizeof(label), "[%d] %s -> %s : %s",
            i,
            fn ? fn->name.c_str() : "(?)",
            tn ? tn->name.c_str() : "(?)",
            t.name.c_str());
        if (ImGui::Selectable(label, m_selectedTransitionIndex == i)) {
            m_selectedTransitionIndex = i;
        }
    }
    ImGui::EndChild();
}

void GameLoopEditorPanel::DrawTransitionInspector()
{
    GameLoopAsset& a = Asset();
    ImGui::TextUnformatted("Transition Inspector");

    if (m_selectedTransitionIndex < 0 || m_selectedTransitionIndex >= (int)a.transitions.size()) {
        ImGui::TextDisabled("(transition 未選択)");
        return;
    }

    GameLoopTransition& t = a.transitions[m_selectedTransitionIndex];

    char nameBuf[128] = {};
    std::strncpy(nameBuf, t.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("name", nameBuf, sizeof(nameBuf))) {
        t.name = nameBuf;
    }

    auto NodeCombo = [&a](const char* label, uint32_t& value) {
        const auto* selected = a.FindNode(value);
        const char* preview = selected ? selected->name.c_str() : "(none)";
        if (ImGui::BeginCombo(label, preview)) {
            for (const auto& n : a.nodes) {
                const bool isSel = (n.id == value);
                if (ImGui::Selectable(n.name.c_str(), isSel)) value = n.id;
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    };
    NodeCombo("from", t.fromNodeId);
    NodeCombo("to",   t.toNodeId);

    ImGui::Checkbox("requireAllConditions (AND)", &t.requireAllConditions);

    ImGui::Spacing();
    ImGui::TextUnformatted("Conditions:");
    if (ImGui::Button("+##AddCond")) {
        t.conditions.push_back(GameLoopCondition{});
    }

    int removeAt = -1;
    for (int ci = 0; ci < (int)t.conditions.size(); ++ci) {
        ImGui::PushID(ci);
        DrawConditionEditor(t.conditions[ci], ci);
        ImGui::SameLine();
        if (ImGui::Button("X##RemCond")) removeAt = ci;
        ImGui::PopID();
    }
    if (removeAt >= 0) {
        t.conditions.erase(t.conditions.begin() + removeAt);
    }
}

void GameLoopEditorPanel::DrawConditionEditor(GameLoopCondition& c, int /*conditionIndex*/)
{
    int typeIdx = FindConditionTypeIndex(c.type);
    if (ImGui::Combo("type", &typeIdx, kConditionTypeLabels, kConditionTypeCount)) {
        c.type = kConditionTypeValues[typeIdx];
    }

    switch (c.type) {
    case GameLoopConditionType::None:
        ImGui::TextDisabled("(常に true)");
        break;

    case GameLoopConditionType::InputPressed:
    {
        ImGui::InputInt("actionIndex", &c.actionIndex);
        ImGui::TextDisabled("hint: 0=Confirm, 1=Cancel, 2=Retry");
        break;
    }

    case GameLoopConditionType::UIButtonClicked:
    {
        char buf[128] = {};
        std::strncpy(buf, c.targetName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("targetName", buf, sizeof(buf))) c.targetName = buf;
        break;
    }

    case GameLoopConditionType::TimerElapsed:
        ImGui::InputFloat("seconds", &c.seconds);
        break;

    case GameLoopConditionType::ActorDead:
    case GameLoopConditionType::AllActorsDead:
    {
        int aIdx = FindActorTypeIndex(c.actorType);
        if (ImGui::Combo("actorType", &aIdx, kActorTypeLabels, kActorTypeCount)) {
            c.actorType = kActorTypeValues[aIdx];
        }
        break;
    }

    case GameLoopConditionType::ActorMovedDistance:
    {
        int aIdx = FindActorTypeIndex(c.actorType);
        if (ImGui::Combo("actorType", &aIdx, kActorTypeLabels, kActorTypeCount)) {
            c.actorType = kActorTypeValues[aIdx];
        }
        ImGui::InputFloat("threshold", &c.threshold);
        break;
    }

    case GameLoopConditionType::RuntimeFlag:
    case GameLoopConditionType::StateMachineState:
    {
        char buf[128] = {};
        std::strncpy(buf, c.parameterName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("parameterName", buf, sizeof(buf))) c.parameterName = buf;
        break;
    }

    case GameLoopConditionType::TimelineEvent:
    case GameLoopConditionType::CustomEvent:
    {
        char buf[128] = {};
        std::strncpy(buf, c.eventName.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("eventName", buf, sizeof(buf))) c.eventName = buf;
        break;
    }

    default: break;
    }
}

void GameLoopEditorPanel::DrawValidateResult()
{
    if (!m_validatedOnce) {
        ImGui::TextDisabled("Validate ボタンを押すと結果がここに出ます。");
        return;
    }
    ImGui::Text("Validate: %d errors, %d warnings", m_lastValidate.ErrorCount(), m_lastValidate.WarningCount());
    ImGui::BeginChild("ValidateChild", ImVec2(0.0f, 120.0f), true);
    for (const auto& m : m_lastValidate.messages) {
        ImVec4 col = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        const char* tag = "[I]";
        if (m.severity == GameLoopValidateSeverity::Warning) { col = ImVec4(1.0f, 0.85f, 0.4f, 1.0f); tag = "[W]"; }
        if (m.severity == GameLoopValidateSeverity::Error)   { col = ImVec4(1.0f, 0.4f,  0.4f, 1.0f); tag = "[E]"; }
        ImGui::TextColored(col, "%s %s", tag, m.message.c_str());
    }
    ImGui::EndChild();
}

void GameLoopEditorPanel::DrawRuntimeStatus()
{
    const GameLoopRuntime& rt = Runtime();
    const GameLoopAsset& a = Asset();
    const auto* curNode = a.FindNode(rt.currentNodeId);
    const auto* pendNode = a.FindNode(rt.pendingNodeId);
    ImGui::Text("Runtime: active=%s | currentNode=%s | nodeTimer=%.2f | pending=%s%s",
        rt.isActive ? "true" : "false",
        curNode ? curNode->name.c_str() : "(none)",
        rt.nodeTimer,
        pendNode ? pendNode->name.c_str() : "(none)",
        rt.sceneTransitionRequested ? " [requested]" : "");
}

void GameLoopEditorPanel::DoValidate()
{
    m_lastValidate = ValidateGameLoopAsset(Asset());
    m_validatedOnce = true;
}

void GameLoopEditorPanel::DoSave()
{
    DoValidate();
    if (m_lastValidate.HasError()) {
        LOG_WARN("[GameLoop] Validate に Error があります。Save 中止。");
        return;
    }
    if (Asset().SaveToFile(m_currentPath)) {
        LOG_INFO("[GameLoop] saved to %s", m_currentPath.string().c_str());
    } else {
        LOG_ERROR("[GameLoop] save failed: %s", m_currentPath.string().c_str());
    }
}

void GameLoopEditorPanel::DoSaveAs(const std::filesystem::path& newPath)
{
    if (newPath.empty()) return;
    m_currentPath = newPath;
    DoSave();
}

void GameLoopEditorPanel::DoLoad(const std::filesystem::path& fromPath)
{
    if (fromPath.empty()) return;
    if (Asset().LoadFromFile(fromPath)) {
        m_currentPath = fromPath;
        m_selectedNodeIndex = -1;
        m_selectedTransitionIndex = -1;
        m_validatedOnce = false;
        LOG_INFO("[GameLoop] loaded %s", fromPath.string().c_str());
    } else {
        LOG_ERROR("[GameLoop] load failed: %s", fromPath.string().c_str());
    }
}

void GameLoopEditorPanel::DoNewDefault()
{
    Asset() = GameLoopAsset::CreateDefault();
    m_selectedNodeIndex = -1;
    m_selectedTransitionIndex = -1;
    m_validatedOnce = false;
}
