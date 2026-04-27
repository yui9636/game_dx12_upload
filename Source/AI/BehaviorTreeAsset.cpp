#include "BehaviorTreeAsset.h"

#include <algorithm>
#include <fstream>
#include <queue>
#include <set>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace
{
    const char* BBValueTypeToString(BlackboardValueType t)
    {
        switch (t) {
        case BlackboardValueType::None:    return "None";
        case BlackboardValueType::Bool:    return "Bool";
        case BlackboardValueType::Int:     return "Int";
        case BlackboardValueType::Float:   return "Float";
        case BlackboardValueType::Vector3: return "Vector3";
        case BlackboardValueType::Entity:  return "Entity";
        case BlackboardValueType::String:  return "String";
        }
        return "None";
    }

    BlackboardValueType BBValueTypeFromString(const std::string& s)
    {
        if (s == "Bool")    return BlackboardValueType::Bool;
        if (s == "Int")     return BlackboardValueType::Int;
        if (s == "Float")   return BlackboardValueType::Float;
        if (s == "Vector3") return BlackboardValueType::Vector3;
        if (s == "Entity")  return BlackboardValueType::Entity;
        if (s == "String")  return BlackboardValueType::String;
        return BlackboardValueType::None;
    }

    nlohmann::json NodeToJson(const BTNode& n)
    {
        nlohmann::json j;
        j["id"]   = n.id;
        j["type"] = BTNodeTypeToString(n.type);
        if (!n.name.empty()) j["name"] = n.name;
        if (!n.childrenIds.empty()) j["children"] = n.childrenIds;
        if (n.fParam0 != 0.0f) j["fParam0"] = n.fParam0;
        if (n.fParam1 != 0.0f) j["fParam1"] = n.fParam1;
        if (n.fParam2 != 0.0f) j["fParam2"] = n.fParam2;
        if (n.iParam0 != 0)    j["iParam0"] = n.iParam0;
        if (!n.sParam0.empty()) j["sParam0"] = n.sParam0;
        if (!n.sParam1.empty()) j["sParam1"] = n.sParam1;
        if (n.bbType != BlackboardValueType::None) j["bbType"] = BBValueTypeToString(n.bbType);
        if (n.graphPos.x != 0.0f || n.graphPos.y != 0.0f) {
            j["graphPos"] = { n.graphPos.x, n.graphPos.y };
        }
        return j;
    }

    BTNode NodeFromJson(const nlohmann::json& j)
    {
        BTNode n;
        n.id    = j.value("id", 0u);
        n.type  = BTNodeTypeFromString(j.value("type", std::string{ "Root" }));
        n.name  = j.value("name", std::string{});
        if (j.contains("children") && j["children"].is_array()) {
            for (const auto& cj : j["children"]) {
                n.childrenIds.push_back(cj.get<uint32_t>());
            }
        }
        n.fParam0 = j.value("fParam0", 0.0f);
        n.fParam1 = j.value("fParam1", 0.0f);
        n.fParam2 = j.value("fParam2", 0.0f);
        n.iParam0 = j.value("iParam0", 0);
        n.sParam0 = j.value("sParam0", std::string{});
        n.sParam1 = j.value("sParam1", std::string{});
        n.bbType  = BBValueTypeFromString(j.value("bbType", std::string{ "None" }));
        if (j.contains("graphPos") && j["graphPos"].is_array() && j["graphPos"].size() == 2) {
            n.graphPos.x = j["graphPos"][0].get<float>();
            n.graphPos.y = j["graphPos"][1].get<float>();
        }
        return n;
    }
}

const char* BTNodeTypeToString(BTNodeType t)
{
    switch (t) {
    case BTNodeType::Root:               return "Root";
    case BTNodeType::Sequence:           return "Sequence";
    case BTNodeType::Selector:           return "Selector";
    case BTNodeType::Parallel:           return "Parallel";
    case BTNodeType::Inverter:           return "Inverter";
    case BTNodeType::Repeat:             return "Repeat";
    case BTNodeType::Cooldown:           return "Cooldown";
    case BTNodeType::ConditionGuard:     return "ConditionGuard";
    case BTNodeType::HasTarget:          return "HasTarget";
    case BTNodeType::TargetInRange:      return "TargetInRange";
    case BTNodeType::TargetVisible:      return "TargetVisible";
    case BTNodeType::HealthBelow:        return "HealthBelow";
    case BTNodeType::StaminaAbove:       return "StaminaAbove";
    case BTNodeType::BlackboardEqual:    return "BlackboardEqual";
    case BTNodeType::Wait:               return "Wait";
    case BTNodeType::FaceTarget:         return "FaceTarget";
    case BTNodeType::MoveToTarget:       return "MoveToTarget";
    case BTNodeType::StrafeAroundTarget: return "StrafeAroundTarget";
    case BTNodeType::Retreat:            return "Retreat";
    case BTNodeType::Attack:             return "Attack";
    case BTNodeType::DodgeAction:        return "DodgeAction";
    case BTNodeType::SetSMParam:         return "SetSMParam";
    case BTNodeType::PlayState:          return "PlayState";
    case BTNodeType::SetBlackboard:      return "SetBlackboard";
    }
    return "Root";
}

BTNodeType BTNodeTypeFromString(const std::string& s)
{
    if (s == "Root")               return BTNodeType::Root;
    if (s == "Sequence")           return BTNodeType::Sequence;
    if (s == "Selector")           return BTNodeType::Selector;
    if (s == "Parallel")           return BTNodeType::Parallel;
    if (s == "Inverter")           return BTNodeType::Inverter;
    if (s == "Repeat")             return BTNodeType::Repeat;
    if (s == "Cooldown")           return BTNodeType::Cooldown;
    if (s == "ConditionGuard")     return BTNodeType::ConditionGuard;
    if (s == "HasTarget")          return BTNodeType::HasTarget;
    if (s == "TargetInRange")      return BTNodeType::TargetInRange;
    if (s == "TargetVisible")      return BTNodeType::TargetVisible;
    if (s == "HealthBelow")        return BTNodeType::HealthBelow;
    if (s == "StaminaAbove")       return BTNodeType::StaminaAbove;
    if (s == "BlackboardEqual")    return BTNodeType::BlackboardEqual;
    if (s == "Wait")               return BTNodeType::Wait;
    if (s == "FaceTarget")         return BTNodeType::FaceTarget;
    if (s == "MoveToTarget")       return BTNodeType::MoveToTarget;
    if (s == "StrafeAroundTarget") return BTNodeType::StrafeAroundTarget;
    if (s == "Retreat")            return BTNodeType::Retreat;
    if (s == "Attack")             return BTNodeType::Attack;
    if (s == "DodgeAction")        return BTNodeType::DodgeAction;
    if (s == "SetSMParam")         return BTNodeType::SetSMParam;
    if (s == "PlayState")          return BTNodeType::PlayState;
    if (s == "SetBlackboard")      return BTNodeType::SetBlackboard;
    return BTNodeType::Root;
}

BTNodeCategory CategoryOfBTNodeType(BTNodeType t)
{
    const uint16_t v = static_cast<uint16_t>(t);
    if (v == 0)            return BTNodeCategory::Root;
    if (v >= 100 && v < 200) return BTNodeCategory::Composite;
    if (v >= 200 && v < 300) return BTNodeCategory::Decorator;
    if (v >= 300 && v < 400) return BTNodeCategory::Condition;
    if (v >= 400)            return BTNodeCategory::Action;
    return BTNodeCategory::Action;
}

const BTNode* BehaviorTreeAsset::FindNode(uint32_t id) const
{
    for (const auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

BTNode* BehaviorTreeAsset::FindNode(uint32_t id)
{
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

uint32_t BehaviorTreeAsset::AllocateNodeId() const
{
    uint32_t maxId = 0;
    for (const auto& n : nodes) if (n.id > maxId) maxId = n.id;
    return maxId + 1;
}

BehaviorTreeAsset BehaviorTreeAsset::CreateAggressiveTemplate()
{
    BehaviorTreeAsset a;
    a.version = 1;
    a.rootId  = 1;

    BTNode root;     root.id = 1; root.type = BTNodeType::Root;     root.name = "Root";       root.childrenIds = { 2 };
    BTNode topSel;   topSel.id = 2; topSel.type = BTNodeType::Selector; topSel.name = "TopSelect"; topSel.childrenIds = { 3, 6, 9 };
    BTNode atkSeq;   atkSeq.id = 3; atkSeq.type = BTNodeType::Sequence; atkSeq.name = "AttackIfClose"; atkSeq.childrenIds = { 4, 5 };
    BTNode inRange;  inRange.id = 4; inRange.type = BTNodeType::TargetInRange; inRange.name = "InRange?"; inRange.fParam0 = 2.0f;
    BTNode atkAct;   atkAct.id = 5; atkAct.type = BTNodeType::Attack;          atkAct.name = "Attack";
    BTNode chaseSeq; chaseSeq.id = 6; chaseSeq.type = BTNodeType::Sequence;    chaseSeq.name = "ChaseIfVisible"; chaseSeq.childrenIds = { 7, 8 };
    BTNode hasTgt;   hasTgt.id = 7; hasTgt.type = BTNodeType::HasTarget;       hasTgt.name = "HasTarget?";
    BTNode mv;       mv.id = 8; mv.type = BTNodeType::MoveToTarget;            mv.name = "Chase"; mv.fParam0 = 1.5f;
    BTNode wait;     wait.id = 9; wait.type = BTNodeType::Wait;                wait.name = "Wait"; wait.fParam0 = 1.0f;

    a.nodes = { root, topSel, atkSeq, inRange, atkAct, chaseSeq, hasTgt, mv, wait };
    return a;
}

BehaviorTreeAsset BehaviorTreeAsset::CreateDefensiveTemplate()
{
    BehaviorTreeAsset a;
    a.version = 1;
    a.rootId  = 1;

    BTNode root;       root.id = 1; root.type = BTNodeType::Root; root.name = "Root"; root.childrenIds = { 2 };
    BTNode sel;        sel.id = 2; sel.type = BTNodeType::Selector; sel.name = "TopSelect"; sel.childrenIds = { 3, 6, 9, 12 };

    BTNode retreatSeq; retreatSeq.id = 3; retreatSeq.type = BTNodeType::Sequence; retreatSeq.name = "Retreat"; retreatSeq.childrenIds = { 4, 5 };
    BTNode hpLow;      hpLow.id = 4; hpLow.type = BTNodeType::HealthBelow; hpLow.name = "HPBelow30"; hpLow.fParam0 = 0.3f;
    BTNode retreatAct; retreatAct.id = 5; retreatAct.type = BTNodeType::Retreat; retreatAct.name = "MoveAway"; retreatAct.fParam0 = 4.0f;

    BTNode atkSeq;     atkSeq.id = 6; atkSeq.type = BTNodeType::Sequence; atkSeq.name = "AttackInRange"; atkSeq.childrenIds = { 7, 8 };
    BTNode inRange;    inRange.id = 7; inRange.type = BTNodeType::TargetInRange; inRange.name = "InRange?"; inRange.fParam0 = 3.0f;
    BTNode atkAct;     atkAct.id = 8; atkAct.type = BTNodeType::Attack; atkAct.name = "Attack";

    BTNode strafeSeq;  strafeSeq.id = 9; strafeSeq.type = BTNodeType::Sequence; strafeSeq.name = "Strafe"; strafeSeq.childrenIds = { 10, 11 };
    BTNode hasTgt;     hasTgt.id = 10; hasTgt.type = BTNodeType::HasTarget; hasTgt.name = "HasTarget?";
    BTNode strafeAct;  strafeAct.id = 11; strafeAct.type = BTNodeType::StrafeAroundTarget; strafeAct.name = "Strafe"; strafeAct.fParam0 = 2.0f;

    BTNode wait;       wait.id = 12; wait.type = BTNodeType::Wait; wait.name = "Wait"; wait.fParam0 = 1.0f;

    a.nodes = { root, sel, retreatSeq, hpLow, retreatAct, atkSeq, inRange, atkAct, strafeSeq, hasTgt, strafeAct, wait };
    return a;
}

BehaviorTreeAsset BehaviorTreeAsset::CreatePatrolTemplate()
{
    BehaviorTreeAsset a;
    a.version = 1;
    a.rootId  = 1;
    BTNode root; root.id = 1; root.type = BTNodeType::Root; root.name = "Root"; root.childrenIds = { 2 };
    BTNode seq;  seq.id = 2; seq.type = BTNodeType::Sequence; seq.name = "PatrolLoop"; seq.childrenIds = { 3, 4 };
    BTNode wait; wait.id = 3; wait.type = BTNodeType::Wait; wait.name = "Idle"; wait.fParam0 = 1.0f;
    BTNode mv;   mv.id = 4; mv.type = BTNodeType::MoveToTarget; mv.name = "GoToWaypoint"; mv.fParam0 = 0.5f;
    a.nodes = { root, seq, wait, mv };
    return a;
}

bool BehaviorTreeAsset::LoadFromFile(const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    if (!ifs) return false;
    nlohmann::json j;
    try { ifs >> j; } catch (...) { return false; }

    BehaviorTreeAsset out;
    out.version = j.value("version", 1);
    out.rootId  = j.value("rootId", 0u);
    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (const auto& nj : j["nodes"]) {
            out.nodes.push_back(NodeFromJson(nj));
        }
    }
    *this = std::move(out);
    return true;
}

bool BehaviorTreeAsset::SaveToFile(const std::filesystem::path& path) const
{
    nlohmann::json j;
    j["version"] = version;
    j["rootId"]  = rootId;
    nlohmann::json nodesJson = nlohmann::json::array();
    for (const auto& n : nodes) nodesJson.push_back(NodeToJson(n));
    j["nodes"] = nodesJson;

    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << j.dump(2);
    return true;
}

bool BTValidateResult::HasError() const
{
    for (const auto& m : messages) if (m.severity == BTValidateSeverity::Error) return true;
    return false;
}
int BTValidateResult::ErrorCount() const
{
    int n = 0;
    for (const auto& m : messages) if (m.severity == BTValidateSeverity::Error) ++n;
    return n;
}
int BTValidateResult::WarningCount() const
{
    int n = 0;
    for (const auto& m : messages) if (m.severity == BTValidateSeverity::Warning) ++n;
    return n;
}

namespace
{
    void CheckChildArity(const BTNode& n, BTValidateResult& r)
    {
        const std::string label = "node[" + std::to_string(n.id) + "][" + n.name + "]";
        switch (CategoryOfBTNodeType(n.type)) {
        case BTNodeCategory::Root:
            if (n.childrenIds.size() != 1)
                r.messages.push_back({ BTValidateSeverity::Error, label + " Root must have exactly 1 child" });
            break;
        case BTNodeCategory::Composite:
            if (n.childrenIds.empty())
                r.messages.push_back({ BTValidateSeverity::Error, label + " Composite must have at least 1 child" });
            break;
        case BTNodeCategory::Decorator:
            if (n.childrenIds.size() != 1)
                r.messages.push_back({ BTValidateSeverity::Error, label + " Decorator must have exactly 1 child" });
            break;
        case BTNodeCategory::Action:
        case BTNodeCategory::Condition:
            if (!n.childrenIds.empty())
                r.messages.push_back({ BTValidateSeverity::Error, label + " Leaf must have 0 children" });
            break;
        }
    }

    void CheckParamRanges(const BTNode& n, BTValidateResult& r)
    {
        const std::string label = "node[" + std::to_string(n.id) + "][" + n.name + "]";
        switch (n.type) {
        case BTNodeType::Cooldown:
            if (n.fParam0 <= 0.0f) r.messages.push_back({ BTValidateSeverity::Error, label + " Cooldown.seconds must be > 0" });
            break;
        case BTNodeType::Repeat:
            if (n.iParam0 <= 0) r.messages.push_back({ BTValidateSeverity::Error, label + " Repeat.count must be > 0" });
            break;
        case BTNodeType::Wait:
            if (n.fParam0 <= 0.0f) r.messages.push_back({ BTValidateSeverity::Error, label + " Wait.seconds must be > 0" });
            break;
        case BTNodeType::TargetInRange:
            if (n.fParam0 <= 0.0f) r.messages.push_back({ BTValidateSeverity::Error, label + " TargetInRange.range must be > 0" });
            break;
        case BTNodeType::HealthBelow:
            if (n.fParam0 <= 0.0f || n.fParam0 > 1.0f)
                r.messages.push_back({ BTValidateSeverity::Warning, label + " HealthBelow.threshold should be in (0,1]" });
            break;
        case BTNodeType::Parallel:
            if (n.iParam0 < 1 || n.iParam0 > static_cast<int>(n.childrenIds.size()))
                r.messages.push_back({ BTValidateSeverity::Error, label + " Parallel.successThreshold must be in [1, child count]" });
            break;
        default:
            break;
        }
    }
}

BTValidateResult ValidateBehaviorTree(const BehaviorTreeAsset& asset)
{
    BTValidateResult r;

    // Root existence + uniqueness
    int rootCount = 0;
    for (const auto& n : asset.nodes) if (n.type == BTNodeType::Root) ++rootCount;
    if (rootCount == 0) r.messages.push_back({ BTValidateSeverity::Error, "no Root node" });
    if (rootCount > 1)  r.messages.push_back({ BTValidateSeverity::Error, "multiple Root nodes (must be exactly 1)" });

    // rootId points to a Root
    if (const BTNode* rootNode = asset.FindNode(asset.rootId)) {
        if (rootNode->type != BTNodeType::Root)
            r.messages.push_back({ BTValidateSeverity::Error, "rootId does not point to a Root-type node" });
    } else {
        r.messages.push_back({ BTValidateSeverity::Error, "rootId not present in nodes" });
    }

    // Unique ids
    {
        std::set<uint32_t> seen;
        for (const auto& n : asset.nodes) {
            if (!seen.insert(n.id).second)
                r.messages.push_back({ BTValidateSeverity::Error, "duplicate node id: " + std::to_string(n.id) });
        }
    }

    // Per-node arity / params + children-existence
    for (const auto& n : asset.nodes) {
        CheckChildArity(n, r);
        CheckParamRanges(n, r);
        for (uint32_t cid : n.childrenIds) {
            if (asset.FindNode(cid) == nullptr)
                r.messages.push_back({ BTValidateSeverity::Error,
                    "node[" + std::to_string(n.id) + "] references missing child id " + std::to_string(cid) });
        }
    }

    // Reachability from rootId, and DAG / no-cycle.
    if (asset.FindNode(asset.rootId) != nullptr) {
        std::unordered_set<uint32_t> reachable;
        std::queue<uint32_t> q;
        reachable.insert(asset.rootId);
        q.push(asset.rootId);
        while (!q.empty()) {
            const uint32_t cur = q.front(); q.pop();
            const BTNode* node = asset.FindNode(cur);
            if (!node) continue;
            for (uint32_t cid : node->childrenIds) {
                if (reachable.insert(cid).second) q.push(cid);
            }
        }
        for (const auto& n : asset.nodes) {
            if (reachable.find(n.id) == reachable.end()) {
                r.messages.push_back({ BTValidateSeverity::Warning,
                    "node[" + std::to_string(n.id) + "][" + n.name + "] not reachable from root" });
            }
        }
    }

    return r;
}
