#include "GameLoopAsset.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <unordered_set>
#include <queue>

#include <nlohmann/json.hpp>

#include "Input/ResolvedInputStateComponent.h"

namespace
{
    // Converts condition enum to save-file string.
    const char* ConditionTypeToString(GameLoopConditionType t)
    {
        switch (t) {
        case GameLoopConditionType::None:               return "None";
        case GameLoopConditionType::InputPressed:       return "InputPressed";
        case GameLoopConditionType::UIButtonClicked:    return "UIButtonClicked";
        case GameLoopConditionType::TimerElapsed:       return "TimerElapsed";
        case GameLoopConditionType::ActorDead:          return "ActorDead";
        case GameLoopConditionType::AllActorsDead:      return "AllActorsDead";
        case GameLoopConditionType::ActorMovedDistance: return "ActorMovedDistance";
        case GameLoopConditionType::RuntimeFlag:        return "RuntimeFlag";
        case GameLoopConditionType::StateMachineState:  return "StateMachineState";
        case GameLoopConditionType::TimelineEvent:      return "TimelineEvent";
        case GameLoopConditionType::CustomEvent:        return "CustomEvent";
        }
        return "None";
    }

    // Converts save-file string to condition enum.
    GameLoopConditionType ConditionTypeFromString(const std::string& s)
    {
        if (s == "None")               return GameLoopConditionType::None;
        if (s == "InputPressed")       return GameLoopConditionType::InputPressed;
        if (s == "UIButtonClicked")    return GameLoopConditionType::UIButtonClicked;
        if (s == "TimerElapsed")       return GameLoopConditionType::TimerElapsed;
        if (s == "ActorDead")          return GameLoopConditionType::ActorDead;
        if (s == "AllActorsDead")      return GameLoopConditionType::AllActorsDead;
        if (s == "ActorMovedDistance") return GameLoopConditionType::ActorMovedDistance;
        if (s == "RuntimeFlag")        return GameLoopConditionType::RuntimeFlag;
        if (s == "StateMachineState")  return GameLoopConditionType::StateMachineState;
        if (s == "TimelineEvent")      return GameLoopConditionType::TimelineEvent;
        if (s == "CustomEvent")        return GameLoopConditionType::CustomEvent;
        return GameLoopConditionType::None;
    }

    // Converts actor enum to save-file string.
    const char* ActorTypeToString(ActorType t)
    {
        switch (t) {
        case ActorType::None:    return "None";
        case ActorType::Player:  return "Player";
        case ActorType::Enemy:   return "Enemy";
        case ActorType::NPC:     return "NPC";
        case ActorType::Neutral: return "Neutral";
        }
        return "None";
    }

    // Converts save-file string to actor enum.
    ActorType ActorTypeFromString(const std::string& s)
    {
        if (s == "Player")  return ActorType::Player;
        if (s == "Enemy")   return ActorType::Enemy;
        if (s == "NPC")     return ActorType::NPC;
        if (s == "Neutral") return ActorType::Neutral;
        return ActorType::None;
    }

    // Converts node enum to save-file string.
    const char* NodeTypeToString(GameLoopNodeType t)
    {
        switch (t) {
        case GameLoopNodeType::Scene: return "Scene";
        }
        return "Scene";
    }

    // Converts save-file string to node enum.
    GameLoopNodeType NodeTypeFromString(const std::string& s)
    {
        (void)s;
        return GameLoopNodeType::Scene;
    }

    // Serializes one transition condition.
    nlohmann::json ConditionToJson(const GameLoopCondition& c)
    {
        nlohmann::json j;
        j["type"] = ConditionTypeToString(c.type);
        if (c.actorType != ActorType::None) j["actorType"] = ActorTypeToString(c.actorType);
        if (!c.targetName.empty())          j["targetName"] = c.targetName;
        if (!c.parameterName.empty())       j["parameterName"] = c.parameterName;
        if (!c.eventName.empty())           j["eventName"] = c.eventName;
        if (c.actionIndex >= 0)             j["actionIndex"] = c.actionIndex;
        if (c.threshold != 0.0f)            j["threshold"] = c.threshold;
        if (c.seconds != 0.0f)              j["seconds"] = c.seconds;
        return j;
    }

    // Deserializes one transition condition.
    GameLoopCondition ConditionFromJson(const nlohmann::json& j)
    {
        GameLoopCondition c;
        c.type = ConditionTypeFromString(j.value("type", std::string{ "None" }));
        c.actorType = ActorTypeFromString(j.value("actorType", std::string{ "None" }));
        c.targetName = j.value("targetName", std::string{});
        c.parameterName = j.value("parameterName", std::string{});
        c.eventName = j.value("eventName", std::string{});
        c.actionIndex = j.value("actionIndex", -1);
        c.threshold = j.value("threshold", 0.0f);
        c.seconds = j.value("seconds", 0.0f);
        return c;
    }

    // Creates an InputPressed condition for the specified action index.
    GameLoopCondition MakeInputPressedCondition(int actionIndex)
    {
        GameLoopCondition condition;
        condition.type = GameLoopConditionType::InputPressed;
        condition.actionIndex = actionIndex;
        return condition;
    }
}

const GameLoopNode* GameLoopAsset::FindNode(uint32_t id) const
{
    for (const auto& n : nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

GameLoopNode* GameLoopAsset::FindNode(uint32_t id)
{
    for (auto& n : nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

uint32_t GameLoopAsset::AllocateNodeId() const
{
    uint32_t maxId = 0;
    for (const auto& n : nodes) {
        if (n.id > maxId) maxId = n.id;
    }
    return maxId + 1;
}

GameLoopAsset GameLoopAsset::CreateDefault()
{
    GameLoopAsset a;
    a.version = 1;
    a.startNodeId = 1;

    a.nodes.push_back({ 1, "Title",  "Data/Scenes/Title.scene",  GameLoopNodeType::Scene, { 80.0f, 120.0f } });
    a.nodes.push_back({ 2, "Battle", "Data/Scenes/Battle.scene", GameLoopNodeType::Scene, { 420.0f, 120.0f } });
    a.nodes.push_back({ 3, "Result", "Data/Scenes/Result.scene", GameLoopNodeType::Scene, { 760.0f, 120.0f } });

    {
        GameLoopTransition t;
        t.fromNodeId = 1; t.toNodeId = 2;
        t.name = "Start";
        t.requireAllConditions = false;
        GameLoopCondition c1; c1.type = GameLoopConditionType::UIButtonClicked; c1.targetName = "StartButton";
        GameLoopCondition c2; c2.type = GameLoopConditionType::InputPressed;    c2.actionIndex = 0;
        t.conditions = { c1, c2 };
        a.transitions.push_back(t);
    }
    {
        GameLoopTransition t;
        t.fromNodeId = 2; t.toNodeId = 3;
        t.name = "Clear";
        t.requireAllConditions = false;
        GameLoopCondition c1; c1.type = GameLoopConditionType::AllActorsDead;      c1.actorType = ActorType::Enemy;
        GameLoopCondition c2; c2.type = GameLoopConditionType::ActorMovedDistance; c2.actorType = ActorType::Player; c2.threshold = 1.0f;
        t.conditions = { c1, c2 };
        a.transitions.push_back(t);
    }
    {
        GameLoopTransition t;
        t.fromNodeId = 3; t.toNodeId = 2;
        t.name = "Retry";
        t.requireAllConditions = false;
        GameLoopCondition c1; c1.type = GameLoopConditionType::UIButtonClicked; c1.targetName = "RetryButton";
        GameLoopCondition c2; c2.type = GameLoopConditionType::InputPressed;    c2.actionIndex = 2;
        t.conditions = { c1, c2 };
        a.transitions.push_back(t);
    }
    {
        GameLoopTransition t;
        t.fromNodeId = 3; t.toNodeId = 1;
        t.name = "BackToTitle";
        t.requireAllConditions = false;
        GameLoopCondition c1; c1.type = GameLoopConditionType::UIButtonClicked; c1.targetName = "TitleButton";
        GameLoopCondition c2; c2.type = GameLoopConditionType::InputPressed;    c2.actionIndex = 1;
        t.conditions = { c1, c2 };
        a.transitions.push_back(t);
    }
    return a;
}

GameLoopAsset GameLoopAsset::CreateZTestLoop()
{
    GameLoopAsset a;
    a.version = 1;
    a.startNodeId = 1;

    a.nodes.push_back({ 1, "Title",  "Data/Scenes/Title.scene",  GameLoopNodeType::Scene, { 80.0f, 120.0f } });
    a.nodes.push_back({ 2, "Battle", "Data/Scenes/Battle.scene", GameLoopNodeType::Scene, { 420.0f, 120.0f } });
    a.nodes.push_back({ 3, "Result", "Data/Scenes/Result.scene", GameLoopNodeType::Scene, { 760.0f, 120.0f } });

    {
        GameLoopTransition t;
        t.fromNodeId = 1;
        t.toNodeId = 2;
        t.name = "TitleToBattle";
        t.requireAllConditions = true;
        t.conditions.push_back(MakeInputPressedCondition(0));
        a.transitions.push_back(t);
    }
    {
        GameLoopTransition t;
        t.fromNodeId = 2;
        t.toNodeId = 3;
        t.name = "BattleToResult";
        t.requireAllConditions = true;
        t.conditions.push_back(MakeInputPressedCondition(0));
        a.transitions.push_back(t);
    }
    {
        GameLoopTransition t;
        t.fromNodeId = 3;
        t.toNodeId = 2;
        t.name = "ResultToBattle";
        t.requireAllConditions = true;
        t.conditions.push_back(MakeInputPressedCondition(0));
        a.transitions.push_back(t);
    }

    return a;
}

bool GameLoopAsset::LoadFromFile(const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    if (!ifs) return false;

    nlohmann::json j;
    try { ifs >> j; }
    catch (...) { return false; }

    GameLoopAsset out;
    out.version = j.value("version", 1);
    out.startNodeId = j.value("startNodeId", 0u);

    if (j.contains("nodes") && j["nodes"].is_array()) {
        int nodeIndex = 0;
        for (const auto& nj : j["nodes"]) {
            GameLoopNode n;
            n.id = nj.value("id", 0u);
            n.name = nj.value("name", std::string{});
            n.scenePath = nj.value("scenePath", std::string{});
            n.type = NodeTypeFromString(nj.value("type", std::string{ "Scene" }));
            n.graphPos.x = nj.value("posX", 80.0f + 340.0f * static_cast<float>(nodeIndex));
            n.graphPos.y = nj.value("posY", 120.0f);
            out.nodes.push_back(n);
            ++nodeIndex;
        }
    }

    if (j.contains("transitions") && j["transitions"].is_array()) {
        for (const auto& tj : j["transitions"]) {
            GameLoopTransition t;
            t.fromNodeId = tj.value("fromNodeId", 0u);
            t.toNodeId = tj.value("toNodeId", 0u);
            t.name = tj.value("name", std::string{});
            t.requireAllConditions = tj.value("requireAllConditions", true);
            if (tj.contains("conditions") && tj["conditions"].is_array()) {
                for (const auto& cj : tj["conditions"]) {
                    t.conditions.push_back(ConditionFromJson(cj));
                }
            }
            out.transitions.push_back(t);
        }
    }

    *this = std::move(out);
    return true;
}

bool GameLoopAsset::SaveToFile(const std::filesystem::path& path) const
{
    nlohmann::json j;
    j["version"] = version;
    j["startNodeId"] = startNodeId;

    nlohmann::json nodesJson = nlohmann::json::array();
    for (const auto& n : nodes) {
        nlohmann::json nj;
        nj["id"] = n.id;
        nj["name"] = n.name;
        nj["scenePath"] = n.scenePath;
        nj["type"] = NodeTypeToString(n.type);
        nj["posX"] = n.graphPos.x;
        nj["posY"] = n.graphPos.y;
        nodesJson.push_back(nj);
    }
    j["nodes"] = nodesJson;

    nlohmann::json transitionsJson = nlohmann::json::array();
    for (const auto& t : transitions) {
        nlohmann::json tj;
        tj["fromNodeId"] = t.fromNodeId;
        tj["toNodeId"] = t.toNodeId;
        tj["name"] = t.name;
        tj["requireAllConditions"] = t.requireAllConditions;
        nlohmann::json condsJson = nlohmann::json::array();
        for (const auto& c : t.conditions) {
            condsJson.push_back(ConditionToJson(c));
        }
        tj["conditions"] = condsJson;
        transitionsJson.push_back(tj);
    }
    j["transitions"] = transitionsJson;

    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << j.dump(2);
    return true;
}

bool GameLoopValidateResult::HasError() const
{
    for (const auto& m : messages) {
        if (m.severity == GameLoopValidateSeverity::Error) return true;
    }
    return false;
}

int GameLoopValidateResult::ErrorCount() const
{
    int n = 0;
    for (const auto& m : messages) {
        if (m.severity == GameLoopValidateSeverity::Error) ++n;
    }
    return n;
}

int GameLoopValidateResult::WarningCount() const
{
    int n = 0;
    for (const auto& m : messages) {
        if (m.severity == GameLoopValidateSeverity::Warning) ++n;
    }
    return n;
}

GameLoopValidateResult ValidateGameLoopAsset(const GameLoopAsset& asset)
{
    GameLoopValidateResult r;

    // start node existence.
    if (asset.FindNode(asset.startNodeId) == nullptr) {
        r.messages.push_back({ GameLoopValidateSeverity::Error, "startNodeId is not in nodes" });
    }

    // duplicate node ids.
    {
        std::set<uint32_t> seen;
        for (const auto& n : asset.nodes) {
            if (!seen.insert(n.id).second) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, "duplicate node id: " + std::to_string(n.id) });
            }
        }
    }

    // per-node checks.
    for (const auto& n : asset.nodes) {
        if (n.name.empty()) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, "node name empty (id=" + std::to_string(n.id) + ")" });
        }
        if (n.scenePath.empty()) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, "node scenePath empty (" + n.name + ")" });
        }
    }

    // transition checks.
    for (size_t i = 0; i < asset.transitions.size(); ++i) {
        const auto& t = asset.transitions[i];
        const std::string label = "transition[" + std::to_string(i) + "]";
        if (asset.FindNode(t.fromNodeId) == nullptr) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " fromNodeId not found" });
        }
        if (asset.FindNode(t.toNodeId) == nullptr) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " toNodeId not found" });
        }
        if (t.conditions.empty()) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " conditions is empty" });
        }
        for (size_t ci = 0; ci < t.conditions.size(); ++ci) {
            const auto& c = t.conditions[ci];
            const std::string clabel = label + ".cond[" + std::to_string(ci) + "]";
            switch (c.type) {
            case GameLoopConditionType::InputPressed:
                if (c.actionIndex < 0 || c.actionIndex >= ResolvedInputStateComponent::MAX_ACTIONS) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, clabel + " InputPressed actionIndex out of range" });
                }
                break;
            case GameLoopConditionType::UIButtonClicked:
                if (c.targetName.empty()) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, clabel + " UIButtonClicked targetName is empty" });
                }
                break;
            case GameLoopConditionType::TimerElapsed:
                if (c.seconds <= 0.0f) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, clabel + " TimerElapsed seconds must be > 0" });
                }
                break;
            case GameLoopConditionType::ActorMovedDistance:
                if (c.threshold <= 0.0f) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, clabel + " ActorMovedDistance threshold must be > 0" });
                }
                if (c.actorType == ActorType::None) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, clabel + " ActorMovedDistance actorType cannot be None" });
                }
                break;
            case GameLoopConditionType::ActorDead:
            case GameLoopConditionType::AllActorsDead:
                if (c.actorType == ActorType::None) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, clabel + " actor condition actorType cannot be None" });
                }
                break;
            case GameLoopConditionType::RuntimeFlag:
                if (c.parameterName.empty()) {
                    r.messages.push_back({ GameLoopValidateSeverity::Warning, clabel + " RuntimeFlag parameterName is empty" });
                }
                break;
            case GameLoopConditionType::StateMachineState:
                if (c.parameterName.empty()) {
                    r.messages.push_back({ GameLoopValidateSeverity::Warning, clabel + " StateMachineState parameterName is empty" });
                }
                break;
            case GameLoopConditionType::TimelineEvent:
            case GameLoopConditionType::CustomEvent:
                if (c.eventName.empty()) {
                    r.messages.push_back({ GameLoopValidateSeverity::Warning, clabel + " event-type eventName is empty" });
                }
                break;
            default:
                break;
            }
        }
    }

    // dead-end / unreachable nodes.
    {
        std::unordered_set<uint32_t> hasOutgoing;
        for (const auto& t : asset.transitions) hasOutgoing.insert(t.fromNodeId);
        for (const auto& n : asset.nodes) {
            if (hasOutgoing.find(n.id) == hasOutgoing.end()) {
                r.messages.push_back({ GameLoopValidateSeverity::Warning, "node '" + n.name + "' has no outgoing transition" });
            }
        }

        std::unordered_set<uint32_t> reachable;
        std::queue<uint32_t> q;
        if (asset.FindNode(asset.startNodeId)) {
            reachable.insert(asset.startNodeId);
            q.push(asset.startNodeId);
        }
        while (!q.empty()) {
            const uint32_t cur = q.front(); q.pop();
            for (const auto& t : asset.transitions) {
                if (t.fromNodeId == cur && reachable.insert(t.toNodeId).second) {
                    q.push(t.toNodeId);
                }
            }
        }
        for (const auto& n : asset.nodes) {
            if (reachable.find(n.id) == reachable.end()) {
                r.messages.push_back({ GameLoopValidateSeverity::Warning, "node '" + n.name + "' is not reachable from start" });
            }
        }
    }

    return r;
}
