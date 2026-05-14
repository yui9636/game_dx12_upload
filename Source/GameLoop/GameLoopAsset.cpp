#include "GameLoopAsset.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <queue>
#include <set>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include "System/PathResolver.h"

namespace
{
    const char* NodeTypeToString(GameLoopNodeType t)
    {
        switch (t) {
        case GameLoopNodeType::Scene: return "Scene";
        case GameLoopNodeType::State: return "State";
        case GameLoopNodeType::Event: return "Event";
        case GameLoopNodeType::Action:return "Action";
        case GameLoopNodeType::Battle:return "Battle";
        }
        return "Scene";
    }

    GameLoopNodeType NodeTypeFromString(const std::string& s)
    {
        if (s == "State") return GameLoopNodeType::State;
        if (s == "Event") return GameLoopNodeType::Event;
        if (s == "Action") return GameLoopNodeType::Action;
        if (s == "Battle") return GameLoopNodeType::Battle;
        if (s == "Flow") return GameLoopNodeType::State;
        return GameLoopNodeType::Scene;
    }

    const char* ConditionModeToString(GameFlowConditionMode mode)
    {
        switch (mode) {
        case GameFlowConditionMode::All: return "All";
        case GameFlowConditionMode::Any: return "Any";
        }
        return "All";
    }

    GameFlowConditionMode ConditionModeFromString(const std::string& s)
    {
        if (s == "Any") return GameFlowConditionMode::Any;
        return GameFlowConditionMode::All;
    }

    const char* ConditionTypeToString(GameFlowConditionType type)
    {
        switch (type) {
        case GameFlowConditionType::Event:        return "Event";
        case GameFlowConditionType::InputAction:  return "InputAction";
        case GameFlowConditionType::UIButtonClick:return "UIButtonClick";
        case GameFlowConditionType::TimerElapsed: return "TimerElapsed";
        case GameFlowConditionType::FlagEquals:   return "FlagEquals";
        case GameFlowConditionType::BattleResult: return "BattleResult";
        case GameFlowConditionType::SceneLoaded:  return "SceneLoaded";
        }
        return "Event";
    }

    GameFlowConditionType ConditionTypeFromString(const std::string& s)
    {
        if (s == "InputAction") return GameFlowConditionType::InputAction;
        if (s == "UIButtonClick") return GameFlowConditionType::UIButtonClick;
        if (s == "TimerElapsed") return GameFlowConditionType::TimerElapsed;
        if (s == "FlagEquals") return GameFlowConditionType::FlagEquals;
        if (s == "BattleResult") return GameFlowConditionType::BattleResult;
        if (s == "SceneLoaded") return GameFlowConditionType::SceneLoaded;
        return GameFlowConditionType::Event;
    }

    const char* ActionTypeToString(GameFlowActionType type)
    {
        switch (type) {
        case GameFlowActionType::LoadScene:       return "LoadScene";
        case GameFlowActionType::SetCurrentNode:  return "SetCurrentNode";
        case GameFlowActionType::EmitEvent:       return "EmitEvent";
        case GameFlowActionType::SetFlag:         return "SetFlag";
        case GameFlowActionType::ClearFlag:       return "ClearFlag";
        case GameFlowActionType::StartBattleFlow: return "StartBattleFlow";
        case GameFlowActionType::ResetBattleFlow: return "ResetBattleFlow";
        case GameFlowActionType::Fade:            return "Fade";
        case GameFlowActionType::Wait:            return "Wait";
        case GameFlowActionType::ShowLoadingOverlay:return "ShowLoadingOverlay";
        case GameFlowActionType::HideLoadingOverlay:return "HideLoadingOverlay";
        }
        return "LoadScene";
    }

    GameFlowActionType ActionTypeFromString(const std::string& s)
    {
        if (s == "SetCurrentNode") return GameFlowActionType::SetCurrentNode;
        if (s == "EmitEvent") return GameFlowActionType::EmitEvent;
        if (s == "SetFlag") return GameFlowActionType::SetFlag;
        if (s == "ClearFlag") return GameFlowActionType::ClearFlag;
        if (s == "StartBattleFlow") return GameFlowActionType::StartBattleFlow;
        if (s == "ResetBattleFlow") return GameFlowActionType::ResetBattleFlow;
        if (s == "Fade") return GameFlowActionType::Fade;
        if (s == "Wait") return GameFlowActionType::Wait;
        if (s == "ShowLoadingOverlay") return GameFlowActionType::ShowLoadingOverlay;
        if (s == "HideLoadingOverlay") return GameFlowActionType::HideLoadingOverlay;
        return GameFlowActionType::LoadScene;
    }

    nlohmann::json ConditionToJson(const GameFlowCondition& condition)
    {
        nlohmann::json j;
        j["type"] = ConditionTypeToString(condition.type);
        if (!condition.name.empty()) j["name"] = condition.name;
        if (!condition.value.empty()) j["value"] = condition.value;
        if (condition.keyboardScancode != 0) j["keyboardScancode"] = condition.keyboardScancode;
        if (condition.gamepadButton != kGameFlowUnboundGamepadButton) {
            j["gamepadButton"] = static_cast<int>(condition.gamepadButton);
        }
        if (condition.seconds != 0.0f) j["seconds"] = condition.seconds;
        j["expectedFlagValue"] = condition.expectedFlagValue;
        return j;
    }

    GameFlowCondition ConditionFromJson(const nlohmann::json& j)
    {
        GameFlowCondition condition;
        condition.type = ConditionTypeFromString(j.value("type", std::string{ "Event" }));
        condition.name = j.value("name", std::string{});
        condition.value = j.value("value", std::string{});
        condition.keyboardScancode = j.value("keyboardScancode", 0u);
        const int gamepadButton = j.value("gamepadButton", -1);
        if (gamepadButton < 0) condition.gamepadButton = kGameFlowUnboundGamepadButton;
        else if (gamepadButton > 254) condition.gamepadButton = 254;
        else condition.gamepadButton = static_cast<uint8_t>(gamepadButton);
        condition.seconds = j.value("seconds", 0.0f);
        condition.expectedFlagValue = j.value("expectedFlagValue", true);
        return condition;
    }

    nlohmann::json ActionToJson(const GameFlowAction& action)
    {
        nlohmann::json j;
        j["type"] = ActionTypeToString(action.type);
        if (!action.target.empty()) j["target"] = action.target;
        if (!action.value.empty()) j["value"] = action.value;
        j["boolValue"] = action.boolValue;
        if (action.seconds != 0.0f) j["seconds"] = action.seconds;
        if (!action.message.empty()) j["message"] = action.message;
        return j;
    }

    GameFlowAction ActionFromJson(const nlohmann::json& j)
    {
        GameFlowAction action;
        action.type = ActionTypeFromString(j.value("type", std::string{ "LoadScene" }));
        action.target = j.value("target", std::string{});
        action.value = j.value("value", std::string{});
        action.boolValue = j.value("boolValue", true);
        action.seconds = j.value("seconds", 0.0f);
        action.message = j.value("message", std::string{});
        return action;
    }

    bool IsAbsolutePathString(const std::string& path)
    {
        if (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
            (path[2] == '/' || path[2] == '\\')) {
            return true;
        }
        return path.rfind("\\\\", 0) == 0 || path.rfind("//", 0) == 0;
    }

    bool ContainsParentTraversal(const std::string& path)
    {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        return normalized == ".." ||
            normalized.rfind("../", 0) == 0 ||
            normalized.find("/../") != std::string::npos ||
            normalized.size() >= 3 && normalized.compare(normalized.size() - 3, 3, "/..") == 0;
    }

    size_t FindDataSegment(const std::string& path)
    {
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const size_t dataPos = lower.find("data/");
        if (dataPos == std::string::npos) {
            return std::string::npos;
        }
        return dataPos;
    }

    bool IsTransitionConditionBound(const GameFlowCondition& condition)
    {
        switch (condition.type) {
        case GameFlowConditionType::Event:
            return !condition.name.empty();
        case GameFlowConditionType::InputAction:
            return condition.keyboardScancode != 0 ||
                condition.gamepadButton != kGameFlowUnboundGamepadButton ||
                !condition.value.empty();
        case GameFlowConditionType::UIButtonClick:
        case GameFlowConditionType::BattleResult:
        case GameFlowConditionType::SceneLoaded:
            return !condition.value.empty();
        case GameFlowConditionType::TimerElapsed:
            return condition.seconds >= 0.0f;
        case GameFlowConditionType::FlagEquals:
            return !condition.name.empty();
        }
        return false;
    }
}

std::string NormalizeGameLoopScenePath(const std::string& inputPath)
{
    if (inputPath.empty()) {
        return {};
    }

    std::string path = inputPath;
    std::replace(path.begin(), path.end(), '\\', '/');

    const size_t dataPos = FindDataSegment(path);
    if (dataPos == std::string::npos) {
        return {};
    }

    std::string relative = path.substr(dataPos);
    if (ContainsParentTraversal(relative)) {
        return {};
    }

    if (relative.size() < 5 || relative.substr(0, 5) != "Data/") {
        relative = "Data/" + relative.substr(5);
    }
    return relative;
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

uint32_t GameLoopAsset::AllocateNodeId()
{
    uint32_t maxId = nextNodeId;
    for (const auto& n : nodes) {
        if (n.id >= maxId) maxId = n.id + 1;
    }
    nextNodeId = maxId + 1;
    return maxId;
}

uint32_t GameLoopAsset::AllocateTransitionId()
{
    uint32_t id = nextTransitionId;
    if (id == 0) {
        id = 1;
    }
    for (;;) {
        bool used = false;
        for (const auto& transition : transitions) {
            if (transition.id == id) {
                used = true;
                break;
            }
        }
        if (!used) break;
        ++id;
    }
    nextTransitionId = id + 1;
    return id;
}

GameLoopAsset GameLoopAsset::CreateDefault()
{
    GameLoopAsset asset;
    asset.version = kGameFlowAssetVersion;
    return asset;
}

GameLoopAsset GameLoopAsset::CreateZTestLoop()
{
    return CreateDefault();
}

bool GameLoopAsset::LoadFromFile(const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    if (!ifs) return false;

    nlohmann::json j;
    try {
        ifs >> j;
    }
    catch (...) {
        return false;
    }

    const int fileVersion = j.value("version", 0);
    if (fileVersion != kGameFlowAssetVersion) {
        return false;
    }
    if (!j.contains("nodes") || !j["nodes"].is_array()) {
        return false;
    }
    if (!j.contains("transitions") || !j["transitions"].is_array()) {
        return false;
    }

    GameLoopAsset out;
    out.version = kGameFlowAssetVersion;
    out.startNodeId = j.value("startNodeId", 0u);
    out.nextNodeId = j.value("nextNodeId", 1u);
    out.nextTransitionId = j.value("nextTransitionId", 1u);

    int nodeIndex = 0;
    for (const auto& nj : j["nodes"]) {
        GameLoopNode n;
        n.id = nj.value("id", 0u);
        n.name = nj.value("name", std::string{});
        const std::string rawScenePath = nj.value("scenePath", std::string{});
        const std::string normalizedScenePath = NormalizeGameLoopScenePath(rawScenePath);
        n.scenePath = normalizedScenePath.empty() ? rawScenePath : normalizedScenePath;
        n.type = NodeTypeFromString(nj.value("type", std::string{ "Scene" }));
        n.graphPos.x = nj.value("posX", 400.0f * static_cast<float>(nodeIndex));
        n.graphPos.y = nj.value("posY", 0.0f);
        out.nodes.push_back(n);
        ++nodeIndex;
    }

    for (const auto& tj : j["transitions"]) {
        GameLoopTransition t;
        t.id = tj.value("id", 0u);
        if (t.id == 0) {
            t.id = out.AllocateTransitionId();
        }
        t.fromNodeId = tj.value("fromNodeId", 0u);
        t.toNodeId = tj.value("toNodeId", 0u);
        t.name = tj.value("name", std::string{});
        t.priority = tj.value("priority", 0);
        t.conditionMode = ConditionModeFromString(tj.value("conditionMode", std::string{ "All" }));
        const std::string rawLoadingScenePath = tj.value("loadingScenePath", std::string{});
        const std::string normalizedLoadingScenePath = NormalizeGameLoopScenePath(rawLoadingScenePath);
        t.loadingScenePath = normalizedLoadingScenePath.empty() ? rawLoadingScenePath : normalizedLoadingScenePath;
        t.loadingMinimumSeconds = tj.value("loadingMinimumSeconds", 0.0f);
        if (tj.contains("conditions") && tj["conditions"].is_array()) {
            for (const auto& cj : tj["conditions"]) {
                if (cj.is_object()) {
                    t.conditions.push_back(ConditionFromJson(cj));
                }
            }
        }
        if (tj.contains("actions") && tj["actions"].is_array()) {
            for (const auto& aj : tj["actions"]) {
                if (aj.is_object()) {
                    t.actions.push_back(ActionFromJson(aj));
                }
            }
        }
        out.transitions.push_back(t);
    }

    for (const auto& n : out.nodes) {
        if (n.id >= out.nextNodeId) {
            out.nextNodeId = n.id + 1;
        }
    }
    for (const auto& t : out.transitions) {
        if (t.id >= out.nextTransitionId) {
            out.nextTransitionId = t.id + 1;
        }
    }

    *this = std::move(out);
    return true;
}

bool GameLoopAsset::SaveToFile(const std::filesystem::path& path) const
{
    nlohmann::json j;
    j["version"] = kGameFlowAssetVersion;
    j["startNodeId"] = startNodeId;
    j["nextNodeId"] = nextNodeId;
    j["nextTransitionId"] = nextTransitionId;

    nlohmann::json nodesJson = nlohmann::json::array();
    for (const auto& n : nodes) {
        nlohmann::json nj;
        nj["id"] = n.id;
        nj["name"] = n.name;
        const std::string normalizedScenePath = NormalizeGameLoopScenePath(n.scenePath);
        nj["scenePath"] = normalizedScenePath.empty() ? n.scenePath : normalizedScenePath;
        nj["type"] = NodeTypeToString(n.type);
        nj["posX"] = n.graphPos.x;
        nj["posY"] = n.graphPos.y;
        nodesJson.push_back(nj);
    }
    j["nodes"] = nodesJson;

    nlohmann::json transitionsJson = nlohmann::json::array();
    for (const auto& t : transitions) {
        nlohmann::json tj;
        tj["id"] = t.id;
        tj["fromNodeId"] = t.fromNodeId;
        tj["toNodeId"] = t.toNodeId;
        tj["name"] = t.name;
        tj["priority"] = t.priority;
        tj["conditionMode"] = ConditionModeToString(t.conditionMode);
        if (!t.loadingScenePath.empty()) {
            const std::string normalizedLoadingScenePath = NormalizeGameLoopScenePath(t.loadingScenePath);
            tj["loadingScenePath"] = normalizedLoadingScenePath.empty() ? t.loadingScenePath : normalizedLoadingScenePath;
        }
        if (t.loadingMinimumSeconds > 0.0f) {
            tj["loadingMinimumSeconds"] = t.loadingMinimumSeconds;
        }

        nlohmann::json conditionsJson = nlohmann::json::array();
        for (const auto& condition : t.conditions) {
            conditionsJson.push_back(ConditionToJson(condition));
        }
        tj["conditions"] = conditionsJson;

        nlohmann::json actionsJson = nlohmann::json::array();
        for (const auto& action : t.actions) {
            actionsJson.push_back(ActionToJson(action));
        }
        tj["actions"] = actionsJson;
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

    if (asset.version != kGameFlowAssetVersion) {
        r.messages.push_back({ GameLoopValidateSeverity::Error, "GameFlowAsset version must be 1" });
    }

    if (asset.nodes.empty()) {
        r.messages.push_back({ GameLoopValidateSeverity::Error, "nodes is empty" });
    }

    if (asset.FindNode(asset.startNodeId) == nullptr) {
        r.messages.push_back({ GameLoopValidateSeverity::Error, "startNodeId is not in nodes" });
    }

    {
        std::set<uint32_t> seen;
        for (const auto& n : asset.nodes) {
            if (!seen.insert(n.id).second) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, "duplicate node id: " + std::to_string(n.id) });
            }
        }
    }

    {
        std::set<uint32_t> seen;
        for (const auto& t : asset.transitions) {
            if (t.id == 0) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, "transition id is 0" });
                continue;
            }
            if (!seen.insert(t.id).second) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, "duplicate transition id: " + std::to_string(t.id) });
            }
        }
    }

    for (const auto& n : asset.nodes) {
        const std::string nodeLabel = n.name.empty() ? ("id=" + std::to_string(n.id)) : n.name;
        if (n.type == GameLoopNodeType::Scene) {
            if (n.scenePath.empty()) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, "node scenePath empty (" + nodeLabel + ")" });
            }
            else {
                if (IsAbsolutePathString(n.scenePath)) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, "node scenePath must not be absolute (" + nodeLabel + ")" });
                }
                if (ContainsParentTraversal(n.scenePath)) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, "node scenePath must not contain parent traversal (" + nodeLabel + ")" });
                }

                const std::string normalized = NormalizeGameLoopScenePath(n.scenePath);
                if (normalized.empty()) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, "node scenePath must be under Data/ (" + nodeLabel + ")" });
                }
                else {
                    std::error_code ec;
                    if (!std::filesystem::exists(PathResolver::Resolve(normalized), ec)) {
                        r.messages.push_back({ GameLoopValidateSeverity::Warning, "scene file not found: " + normalized });
                    }
                }
            }
        }
        else if (!n.scenePath.empty()) {
            r.messages.push_back({ GameLoopValidateSeverity::Warning, "non-scene node has scenePath; it will be ignored (" + nodeLabel + ")" });
        }
        if (!std::isfinite(n.graphPos.x) || !std::isfinite(n.graphPos.y)) {
            r.messages.push_back({ GameLoopValidateSeverity::Warning, "node graphPos is not finite (" + nodeLabel + ")" });
        }
        else if (std::fabs(n.graphPos.x) > 100000.0f || std::fabs(n.graphPos.y) > 100000.0f) {
            r.messages.push_back({ GameLoopValidateSeverity::Warning, "node graphPos is very far from origin (" + nodeLabel + ")" });
        }
    }

    for (size_t i = 0; i < asset.transitions.size(); ++i) {
        const auto& t = asset.transitions[i];
        const std::string label = "transition[" + std::to_string(i) + "]";
        const GameLoopNode* toNode = asset.FindNode(t.toNodeId);
        const bool targetsBattleNode = toNode && toNode->type == GameLoopNodeType::Battle;
        bool hasStartBattleFlowAction = false;
        if (asset.FindNode(t.fromNodeId) == nullptr) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " fromNodeId not found" });
        }
        if (toNode == nullptr) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " toNodeId not found" });
        }
        if (!t.loadingScenePath.empty()) {
            const std::string normalizedLoadingScenePath = NormalizeGameLoopScenePath(t.loadingScenePath);
            if (normalizedLoadingScenePath.empty()) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, label + " loadingScenePath must be a scene under Data/" });
            }
            else {
                std::error_code ec;
                if (!std::filesystem::exists(PathResolver::Resolve(normalizedLoadingScenePath), ec)) {
                    r.messages.push_back({ GameLoopValidateSeverity::Warning, label + " loading scene file not found: " + normalizedLoadingScenePath });
                }
            }
        }
        if (t.loadingMinimumSeconds < 0.0f) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " loadingMinimumSeconds must be >= 0" });
        }

        if (t.conditions.empty()) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " has no conditions" });
        }
        for (size_t ci = 0; ci < t.conditions.size(); ++ci) {
            const GameFlowCondition& condition = t.conditions[ci];
            const std::string conditionLabel = label + ".conditions[" + std::to_string(ci) + "]";
            if (!IsTransitionConditionBound(condition)) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, conditionLabel + " is unbound" });
            }
            if (condition.type == GameFlowConditionType::TimerElapsed && condition.seconds < 0.0f) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, conditionLabel + " seconds must be >= 0" });
            }
        }

        if (t.actions.empty() && (!toNode || toNode->type != GameLoopNodeType::Scene || toNode->scenePath.empty())) {
            r.messages.push_back({ GameLoopValidateSeverity::Warning, label + " has no actions; GameFlow runtime will use the to-node fallback" });
        }
        for (size_t ai = 0; ai < t.actions.size(); ++ai) {
            const GameFlowAction& action = t.actions[ai];
            const std::string actionLabel = label + ".actions[" + std::to_string(ai) + "]";
            if (action.type == GameFlowActionType::StartBattleFlow) {
                hasStartBattleFlowAction = true;
            }
            if ((action.type == GameFlowActionType::Fade || action.type == GameFlowActionType::Wait) && action.seconds < 0.0f) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, actionLabel + " seconds must be >= 0" });
            }
            if ((action.type == GameFlowActionType::SetFlag || action.type == GameFlowActionType::ClearFlag ||
                 action.type == GameFlowActionType::EmitEvent) && action.target.empty()) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, actionLabel + " target is empty" });
            }
            if (action.type == GameFlowActionType::StartBattleFlow && action.target.empty()) {
                r.messages.push_back({ GameLoopValidateSeverity::Error, actionLabel + " battle id is empty" });
            }
            if (action.type == GameFlowActionType::LoadScene) {
                const std::string targetScene = action.target.empty() &&
                    toNode &&
                    toNode->type == GameLoopNodeType::Scene
                    ? toNode->scenePath
                    : action.target;
                if (targetScene.empty()) {
                    r.messages.push_back({ GameLoopValidateSeverity::Error, actionLabel + " target scene is empty" });
                }
                else {
                    const std::string normalized = NormalizeGameLoopScenePath(targetScene);
                    if (normalized.empty()) {
                        r.messages.push_back({ GameLoopValidateSeverity::Error, actionLabel + " target scene must be under Data/" });
                    }
                }
            }
        }
        if (targetsBattleNode && !hasStartBattleFlowAction) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " targets a Battle node but has no StartBattleFlow action" });
        }
    }

    {
        std::unordered_set<uint32_t> hasOutgoing;
        for (const auto& t : asset.transitions) hasOutgoing.insert(t.fromNodeId);
        for (const auto& n : asset.nodes) {
            if (hasOutgoing.find(n.id) == hasOutgoing.end()) {
                const std::string label = n.name.empty() ? std::to_string(n.id) : n.name;
                r.messages.push_back({ GameLoopValidateSeverity::Warning, "node '" + label + "' has no outgoing transition" });
            }
        }

        std::unordered_set<uint32_t> reachable;
        std::queue<uint32_t> q;
        if (asset.FindNode(asset.startNodeId)) {
            reachable.insert(asset.startNodeId);
            q.push(asset.startNodeId);
        }
        while (!q.empty()) {
            const uint32_t cur = q.front();
            q.pop();
            for (const auto& t : asset.transitions) {
                if (t.fromNodeId == cur && reachable.insert(t.toNodeId).second) {
                    q.push(t.toNodeId);
                }
            }
        }
        for (const auto& n : asset.nodes) {
            if (reachable.find(n.id) == reachable.end()) {
                const std::string label = n.name.empty() ? std::to_string(n.id) : n.name;
                r.messages.push_back({ GameLoopValidateSeverity::Warning, "node '" + label + "' is not reachable from start" });
            }
        }
    }

    return r;
}
