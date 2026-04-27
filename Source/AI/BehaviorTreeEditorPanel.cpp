#include "BehaviorTreeEditorPanel.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include <imgui.h>

#include "Archetype/Archetype.h"
#include "Component/ComponentSignature.h"
#include "Console/Logger.h"
#include "Gameplay/EnemyTagComponent.h"
#include "Registry/Registry.h"
#include "Type/TypeInfo.h"

#include "BehaviorTreeSystem.h"
#include "BlackboardComponent.h"

namespace
{
    const char* kNodeLabels[] = {
        "Root",
        "Sequence", "Selector", "Parallel",
        "Inverter", "Repeat", "Cooldown", "ConditionGuard",
        "HasTarget", "TargetInRange", "TargetVisible",
        "HealthBelow", "StaminaAbove", "BlackboardEqual",
        "Wait", "FaceTarget", "MoveToTarget", "StrafeAroundTarget", "Retreat",
        "Attack", "DodgeAction",
        "SetSMParam", "PlayState", "SetBlackboard",
    };
    const BTNodeType kNodeValues[] = {
        BTNodeType::Root,
        BTNodeType::Sequence, BTNodeType::Selector, BTNodeType::Parallel,
        BTNodeType::Inverter, BTNodeType::Repeat, BTNodeType::Cooldown, BTNodeType::ConditionGuard,
        BTNodeType::HasTarget, BTNodeType::TargetInRange, BTNodeType::TargetVisible,
        BTNodeType::HealthBelow, BTNodeType::StaminaAbove, BTNodeType::BlackboardEqual,
        BTNodeType::Wait, BTNodeType::FaceTarget, BTNodeType::MoveToTarget, BTNodeType::StrafeAroundTarget, BTNodeType::Retreat,
        BTNodeType::Attack, BTNodeType::DodgeAction,
        BTNodeType::SetSMParam, BTNodeType::PlayState, BTNodeType::SetBlackboard,
    };
    constexpr int kNodeTypeCount = sizeof(kNodeValues) / sizeof(kNodeValues[0]);

    int FindNodeTypeIndex(BTNodeType t)
    {
        for (int i = 0; i < kNodeTypeCount; ++i) if (kNodeValues[i] == t) return i;
        return 0;
    }

    void DrawTreeRowRecursive(BehaviorTreeAsset& a, uint32_t nodeId, int depth,
                              int& selectedIndex, std::vector<uint32_t>& visitStack)
    {
        // Cycle guard.
        for (uint32_t v : visitStack) if (v == nodeId) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "(cycle: %u)", nodeId);
            return;
        }
        visitStack.push_back(nodeId);

        BTNode* n = a.FindNode(nodeId);
        if (!n) { ImGui::TextDisabled("(missing %u)", nodeId); visitStack.pop_back(); return; }

        // index in nodes[]
        int index = -1;
        for (size_t i = 0; i < a.nodes.size(); ++i) if (a.nodes[i].id == nodeId) { index = (int)i; break; }

        ImGui::PushID((int)nodeId);
        ImGui::Indent(depth * 14.0f);

        char label[192];
        std::snprintf(label, sizeof(label), "[%s] %s (id=%u)",
            BTNodeTypeToString(n->type),
            n->name.empty() ? "(unnamed)" : n->name.c_str(),
            n->id);

        if (ImGui::Selectable(label, selectedIndex == index)) {
            selectedIndex = index;
        }

        ImGui::Unindent(depth * 14.0f);
        ImGui::PopID();

        for (uint32_t cid : n->childrenIds) {
            DrawTreeRowRecursive(a, cid, depth + 1, selectedIndex, visitStack);
        }
        visitStack.pop_back();
    }
}

BehaviorTreeEditorPanel::BehaviorTreeEditorPanel()
{
    m_btAsset     = BehaviorTreeAsset::CreateAggressiveTemplate();
    m_enemyConfig = EnemyConfigAsset::CreateAggressiveKnight();
    std::strncpy(m_btLoadPathBuf,    m_btPath.string().c_str(),    sizeof(m_btLoadPathBuf)    - 1);
    std::strncpy(m_btSaveAsPathBuf,  m_btPath.string().c_str(),    sizeof(m_btSaveAsPathBuf)  - 1);
    std::strncpy(m_enemyLoadPathBuf, m_enemyPath.string().c_str(), sizeof(m_enemyLoadPathBuf) - 1);
}

void BehaviorTreeEditorPanel::Draw(Registry* registry, bool* p_open, bool* outFocused)
{
    if (!ImGui::Begin("Behavior Tree Editor", p_open)) {
        if (outFocused) *outFocused = false;
        ImGui::End();
        return;
    }
    if (outFocused) *outFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    DrawToolbar();
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "BTColumns");
        DrawTreeView();
        ImGui::NextColumn();
        DrawNodeInspector();
        ImGui::Columns(1);
    }

    ImGui::Separator();
    DrawValidateResult();

    ImGui::Separator();
    DrawEnemyConfigSection();

    if (registry) {
        ImGui::Separator();
        DrawBlackboardLivePeek(registry);
    }

    ImGui::End();
}

void BehaviorTreeEditorPanel::DrawToolbar()
{
    if (ImGui::Button("New Aggressive Template"))   { DoNewTemplate(0); }
    ImGui::SameLine();
    if (ImGui::Button("New Defensive Template"))    { DoNewTemplate(1); }
    ImGui::SameLine();
    if (ImGui::Button("New Patrol Template"))       { DoNewTemplate(2); }

    if (ImGui::Button("Validate")) DoValidate();
    ImGui::SameLine();
    if (ImGui::Button("Save"))     DoSave();

    ImGui::Text("BT Path: %s", m_btPath.string().c_str());
    ImGui::PushItemWidth(280.0f);
    ImGui::InputText("##btLoadPath", m_btLoadPathBuf, sizeof(m_btLoadPathBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Load##bt")) DoLoad(std::filesystem::path(m_btLoadPathBuf));

    ImGui::PushItemWidth(280.0f);
    ImGui::InputText("##btSaveAsPath", m_btSaveAsPathBuf, sizeof(m_btSaveAsPathBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Save As##bt")) DoSaveAs(std::filesystem::path(m_btSaveAsPathBuf));
}

void BehaviorTreeEditorPanel::DrawTreeView()
{
    ImGui::TextUnformatted("Tree");
    ImGui::BeginChild("BTTreeView", ImVec2(0.0f, 260.0f), true);
    if (m_btAsset.nodes.empty()) {
        ImGui::TextDisabled("(empty tree, press 'New ... Template')");
    } else if (m_btAsset.FindNode(m_btAsset.rootId) == nullptr) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "rootId %u not found", m_btAsset.rootId);
    } else {
        std::vector<uint32_t> visitStack;
        DrawTreeRowRecursive(m_btAsset, m_btAsset.rootId, 0, m_selectedNodeIndex, visitStack);
    }
    ImGui::EndChild();

    if (ImGui::Button("Add Child")) {
        if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < (int)m_btAsset.nodes.size()) {
            BTNode child;
            child.id = m_btAsset.AllocateNodeId();
            child.type = BTNodeType::Wait;
            child.fParam0 = 1.0f;
            child.name = "NewNode";
            m_btAsset.nodes[m_selectedNodeIndex].childrenIds.push_back(child.id);
            m_btAsset.nodes.push_back(child);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        if (m_selectedNodeIndex >= 0 && m_selectedNodeIndex < (int)m_btAsset.nodes.size()) {
            const uint32_t removedId = m_btAsset.nodes[m_selectedNodeIndex].id;
            if (removedId != m_btAsset.rootId) {
                // Remove the node and detach references.
                m_btAsset.nodes.erase(m_btAsset.nodes.begin() + m_selectedNodeIndex);
                for (auto& n : m_btAsset.nodes) {
                    n.childrenIds.erase(
                        std::remove(n.childrenIds.begin(), n.childrenIds.end(), removedId),
                        n.childrenIds.end());
                }
                m_selectedNodeIndex = -1;
            }
        }
    }
}

void BehaviorTreeEditorPanel::DrawNodeInspector()
{
    ImGui::TextUnformatted("Inspector");
    if (m_selectedNodeIndex < 0 || m_selectedNodeIndex >= (int)m_btAsset.nodes.size()) {
        ImGui::TextDisabled("(no node selected)");
        return;
    }
    BTNode& n = m_btAsset.nodes[m_selectedNodeIndex];

    char nameBuf[128] = {};
    std::strncpy(nameBuf, n.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("name", nameBuf, sizeof(nameBuf))) n.name = nameBuf;

    int typeIdx = FindNodeTypeIndex(n.type);
    if (ImGui::Combo("type", &typeIdx, kNodeLabels, kNodeTypeCount)) {
        n.type = kNodeValues[typeIdx];
    }
    ImGui::Text("id: %u", n.id);

    switch (n.type) {
    case BTNodeType::Wait:
    case BTNodeType::Cooldown:
    case BTNodeType::TargetInRange:
    case BTNodeType::Retreat:
    case BTNodeType::StrafeAroundTarget:
        ImGui::InputFloat("fParam0", &n.fParam0);
        break;
    case BTNodeType::MoveToTarget:
        ImGui::InputFloat("stopRange", &n.fParam0);
        break;
    case BTNodeType::HealthBelow:
        ImGui::SliderFloat("threshold (0-1)", &n.fParam0, 0.0f, 1.0f);
        break;
    case BTNodeType::Repeat:
        ImGui::InputInt("count", &n.iParam0);
        break;
    case BTNodeType::Parallel:
        ImGui::InputInt("successThreshold", &n.iParam0);
        break;
    case BTNodeType::ConditionGuard: {
        char buf[64] = {};
        std::strncpy(buf, n.sParam0.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("conditionNodeId", buf, sizeof(buf))) n.sParam0 = buf;
        break;
    }
    case BTNodeType::SetSMParam: {
        char buf[64] = {};
        std::strncpy(buf, n.sParam0.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("paramName", buf, sizeof(buf))) n.sParam0 = buf;
        ImGui::InputFloat("value", &n.fParam0);
        break;
    }
    case BTNodeType::PlayState: {
        char buf[64] = {};
        std::strncpy(buf, n.sParam0.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("stateName", buf, sizeof(buf))) n.sParam0 = buf;
        break;
    }
    case BTNodeType::BlackboardEqual: {
        char keyBuf[64] = {};
        std::strncpy(keyBuf, n.sParam0.c_str(), sizeof(keyBuf) - 1);
        if (ImGui::InputText("key", keyBuf, sizeof(keyBuf))) n.sParam0 = keyBuf;
        const char* bbTypeLabels[] = { "None", "Bool", "Int", "Float", "Vector3", "Entity", "String" };
        int bbIdx = static_cast<int>(n.bbType);
        if (ImGui::Combo("bbType", &bbIdx, bbTypeLabels, IM_ARRAYSIZE(bbTypeLabels))) {
            n.bbType = static_cast<BlackboardValueType>(bbIdx);
        }
        if (n.bbType == BlackboardValueType::Float)   ImGui::InputFloat("value", &n.fParam0);
        if (n.bbType == BlackboardValueType::Int ||
            n.bbType == BlackboardValueType::Bool)    ImGui::InputInt("value", &n.iParam0);
        if (n.bbType == BlackboardValueType::String) {
            char vBuf[128] = {};
            std::strncpy(vBuf, n.sParam1.c_str(), sizeof(vBuf) - 1);
            if (ImGui::InputText("value", vBuf, sizeof(vBuf))) n.sParam1 = vBuf;
        }
        break;
    }
    case BTNodeType::SetBlackboard: {
        char keyBuf[64] = {};
        std::strncpy(keyBuf, n.sParam1.c_str(), sizeof(keyBuf) - 1);
        if (ImGui::InputText("key", keyBuf, sizeof(keyBuf))) n.sParam1 = keyBuf;
        const char* bbTypeLabels[] = { "None", "Bool", "Int", "Float", "Vector3", "Entity", "String" };
        int bbIdx = static_cast<int>(n.bbType);
        if (ImGui::Combo("bbType", &bbIdx, bbTypeLabels, IM_ARRAYSIZE(bbTypeLabels))) {
            n.bbType = static_cast<BlackboardValueType>(bbIdx);
        }
        if (n.bbType == BlackboardValueType::Float)  ImGui::InputFloat("value", &n.fParam0);
        if (n.bbType == BlackboardValueType::Int ||
            n.bbType == BlackboardValueType::Bool)   ImGui::InputInt("value (int form)", &n.iParam0);
        if (n.bbType == BlackboardValueType::String) {
            char vBuf[128] = {};
            std::strncpy(vBuf, n.sParam0.c_str(), sizeof(vBuf) - 1);
            if (ImGui::InputText("value", vBuf, sizeof(vBuf))) n.sParam0 = vBuf;
        }
        break;
    }
    default:
        ImGui::TextDisabled("(no editable params)");
        break;
    }
}

void BehaviorTreeEditorPanel::DrawValidateResult()
{
    if (!m_validatedOnce) {
        ImGui::TextDisabled("Press [Validate] to populate this section.");
        return;
    }
    ImGui::Text("Validate: %d errors, %d warnings", m_lastValidate.ErrorCount(), m_lastValidate.WarningCount());
    ImGui::BeginChild("BTValidateChild", ImVec2(0.0f, 100.0f), true);
    for (const auto& m : m_lastValidate.messages) {
        ImVec4 col = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        const char* tag = "[I]";
        if (m.severity == BTValidateSeverity::Warning) { col = ImVec4(1.0f, 0.85f, 0.4f, 1.0f); tag = "[W]"; }
        if (m.severity == BTValidateSeverity::Error)   { col = ImVec4(1.0f, 0.4f,  0.4f, 1.0f); tag = "[E]"; }
        ImGui::TextColored(col, "%s %s", tag, m.message.c_str());
    }
    ImGui::EndChild();
}

void BehaviorTreeEditorPanel::DrawEnemyConfigSection()
{
    if (!ImGui::CollapsingHeader("Enemy Config", ImGuiTreeNodeFlags_DefaultOpen)) return;

    char nameBuf[128] = {};
    std::strncpy(nameBuf, m_enemyConfig.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("name", nameBuf, sizeof(nameBuf))) m_enemyConfig.name = nameBuf;

    char btBuf[256] = {};
    std::strncpy(btBuf, m_enemyConfig.behaviorTreePath.c_str(), sizeof(btBuf) - 1);
    if (ImGui::InputText("behaviorTreePath", btBuf, sizeof(btBuf))) m_enemyConfig.behaviorTreePath = btBuf;

    ImGui::InputFloat("maxHealth",   &m_enemyConfig.maxHealth);
    ImGui::InputFloat("walkSpeed",   &m_enemyConfig.walkSpeed);
    ImGui::InputFloat("runSpeed",    &m_enemyConfig.runSpeed);
    ImGui::InputFloat("turnSpeed",   &m_enemyConfig.turnSpeed);
    ImGui::InputFloat("sightRadius", &m_enemyConfig.sightRadius);
    ImGui::SliderFloat("sightFOV (rad)", &m_enemyConfig.sightFOV, 0.1f, 3.14f);
    ImGui::InputFloat("hearingRadius", &m_enemyConfig.hearingRadius);
    ImGui::InputFloat("baseAttack",  &m_enemyConfig.baseAttack);

    ImGui::Text("Enemy Path: %s", m_enemyPath.string().c_str());
    ImGui::PushItemWidth(280.0f);
    ImGui::InputText("##enemyPath", m_enemyLoadPathBuf, sizeof(m_enemyLoadPathBuf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Load##enemy")) DoEnemyConfigLoad(std::filesystem::path(m_enemyLoadPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Save##enemy")) DoEnemyConfigSave();
}

void BehaviorTreeEditorPanel::DrawBlackboardLivePeek(Registry* registry)
{
    if (!ImGui::CollapsingHeader("Live Blackboard Peek (first enemy)")) return;
    if (!registry) return;

    Signature sig = CreateSignature<EnemyTagComponent, BlackboardComponent>();
    for (auto* arch : registry->GetAllArchetypes()) {
        if (!SignatureMatches(arch->GetSignature(), sig)) continue;
        auto* bbCol = arch->GetColumn(TypeManager::GetComponentTypeID<BlackboardComponent>());
        if (!bbCol) continue;
        if (arch->GetEntityCount() == 0) continue;

        const auto& bb = *static_cast<const BlackboardComponent*>(bbCol->Get(0));
        for (const auto& kv : bb.entries) {
            const auto& v = kv.second;
            switch (v.type) {
            case BlackboardValueType::Float:   ImGui::Text("%-16s float = %.3f", kv.first.c_str(), v.f); break;
            case BlackboardValueType::Int:     ImGui::Text("%-16s int   = %d",   kv.first.c_str(), v.i); break;
            case BlackboardValueType::Bool:    ImGui::Text("%-16s bool  = %s",   kv.first.c_str(), v.i ? "true" : "false"); break;
            case BlackboardValueType::Entity:  ImGui::Text("%-16s entity= %u",   kv.first.c_str(), (unsigned)v.entity); break;
            case BlackboardValueType::Vector3: ImGui::Text("%-16s v3    = (%.2f, %.2f, %.2f)", kv.first.c_str(), v.v3.x, v.v3.y, v.v3.z); break;
            case BlackboardValueType::String:  ImGui::Text("%-16s str   = %s",   kv.first.c_str(), v.s.c_str()); break;
            default:                            ImGui::Text("%-16s (none)",      kv.first.c_str()); break;
            }
        }
        return;
    }
    ImGui::TextDisabled("(no enemy with BlackboardComponent in current scene)");
}

void BehaviorTreeEditorPanel::DoNewTemplate(int which)
{
    switch (which) {
    case 0: m_btAsset = BehaviorTreeAsset::CreateAggressiveTemplate(); break;
    case 1: m_btAsset = BehaviorTreeAsset::CreateDefensiveTemplate(); break;
    case 2: m_btAsset = BehaviorTreeAsset::CreatePatrolTemplate(); break;
    default: return;
    }
    m_selectedNodeIndex = -1;
    m_validatedOnce = false;
}

void BehaviorTreeEditorPanel::DoLoad(const std::filesystem::path& p)
{
    if (p.empty()) return;
    if (m_btAsset.LoadFromFile(p)) {
        m_btPath = p;
        m_selectedNodeIndex = -1;
        m_validatedOnce = false;
        BehaviorTreeSystem::InvalidateAssetCache(p.string().c_str());
        LOG_INFO("[BT] loaded %s", p.string().c_str());
    } else {
        LOG_ERROR("[BT] load failed: %s", p.string().c_str());
    }
}

void BehaviorTreeEditorPanel::DoSave()
{
    DoValidate();
    if (m_lastValidate.HasError()) {
        LOG_WARN("[BT] Validate has errors. Save aborted.");
        return;
    }
    if (m_btAsset.SaveToFile(m_btPath)) {
        BehaviorTreeSystem::InvalidateAssetCache(m_btPath.string().c_str());
        LOG_INFO("[BT] saved to %s", m_btPath.string().c_str());
    } else {
        LOG_ERROR("[BT] save failed: %s", m_btPath.string().c_str());
    }
}

void BehaviorTreeEditorPanel::DoSaveAs(const std::filesystem::path& p)
{
    if (p.empty()) return;
    m_btPath = p;
    DoSave();
}

void BehaviorTreeEditorPanel::DoValidate()
{
    m_lastValidate = ValidateBehaviorTree(m_btAsset);
    m_validatedOnce = true;
}

void BehaviorTreeEditorPanel::DoEnemyConfigLoad(const std::filesystem::path& p)
{
    if (p.empty()) return;
    if (m_enemyConfig.LoadFromFile(p)) {
        m_enemyPath = p;
        LOG_INFO("[Enemy] loaded %s", p.string().c_str());
    } else {
        LOG_ERROR("[Enemy] load failed: %s", p.string().c_str());
    }
}

void BehaviorTreeEditorPanel::DoEnemyConfigSave()
{
    if (m_enemyConfig.SaveToFile(m_enemyPath)) {
        LOG_INFO("[Enemy] saved to %s", m_enemyPath.string().c_str());
    } else {
        LOG_ERROR("[Enemy] save failed: %s", m_enemyPath.string().c_str());
    }
}
