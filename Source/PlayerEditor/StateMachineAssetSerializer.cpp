#include "StateMachineAssetSerializer.h"
#include "StateMachineAsset.h"
#include "JSONManager.h"
#include <fstream>

// ============================================================================
// JSON helpers
// ============================================================================

static nlohmann::json ConditionToJson(const TransitionCondition& c)
{
    return {
        {"type",    static_cast<int>(c.type)},
        {"param",   std::string(c.param)},
        {"compare", static_cast<int>(c.compare)},
        {"value",   c.value}
    };
}

static TransitionCondition ConditionFromJson(const nlohmann::json& j)
{
    TransitionCondition c;
    c.type    = static_cast<ConditionType>(j.value("type", 0));
    std::string p = j.value("param", "");
    strncpy_s(c.param, p.c_str(), _TRUNCATE);
    c.compare = static_cast<CompareOp>(j.value("compare", 0));
    c.value   = j.value("value", 0.0f);
    return c;
}

static nlohmann::json TransitionToJson(const StateTransition& t)
{
    nlohmann::json j;
    j["id"]                  = t.id;
    j["fromState"]           = t.fromState;
    j["toState"]             = t.toState;
    j["priority"]            = t.priority;
    j["exitTimeNormalized"]  = t.exitTimeNormalized;
    j["blendDuration"]       = t.blendDuration;
    j["hasExitTime"]         = t.hasExitTime;

    j["conditions"] = nlohmann::json::array();
    for (auto& c : t.conditions)
        j["conditions"].push_back(ConditionToJson(c));

    return j;
}

static StateTransition TransitionFromJson(const nlohmann::json& j)
{
    StateTransition t;
    t.id                 = j.value("id", 0u);
    t.fromState          = j.value("fromState", 0u);
    t.toState            = j.value("toState", 0u);
    t.priority           = j.value("priority", 0);
    t.exitTimeNormalized = j.value("exitTimeNormalized", 0.0f);
    t.blendDuration      = j.value("blendDuration", 0.2f);
    t.hasExitTime        = j.value("hasExitTime", false);

    if (j.contains("conditions") && j["conditions"].is_array())
        for (auto& jc : j["conditions"])
            t.conditions.push_back(ConditionFromJson(jc));

    return t;
}

static nlohmann::json StateToJson(const StateNode& s)
{
    nlohmann::json j;
    j["id"]                = s.id;
    j["name"]              = s.name;
    j["type"]              = static_cast<int>(s.type);
    j["animationIndex"]    = s.animationIndex;
    j["timelineId"]        = s.timelineId;
    j["loopAnimation"]     = s.loopAnimation;
    j["animSpeed"]         = s.animSpeed;
    j["canInterrupt"]      = s.canInterrupt;
    j["posX"]              = s.position.x;
    j["posY"]              = s.position.y;

    // v2.0: state-bound BT path + designer note (omit when empty for cleaner JSON).
    if (!s.behaviorTreePath.empty()) j["behaviorTreePath"] = s.behaviorTreePath;
    if (!s.aiNote.empty())           j["aiNote"]           = s.aiNote;

    if (!s.properties.empty()) {
        nlohmann::json props;
        for (auto& [k, v] : s.properties) props[k] = v;
        j["properties"] = props;
    }
    return j;
}

static StateNode StateFromJson(const nlohmann::json& j)
{
    StateNode s;
    s.id                = j.value("id", 0u);
    s.name              = j.value("name", "");
    s.type              = static_cast<StateNodeType>(j.value("type", 0));
    s.animationIndex    = j.value("animationIndex", -1);
    s.timelineId        = j.value("timelineId", 0u);
    s.loopAnimation     = j.value("loopAnimation", false);
    s.animSpeed         = j.value("animSpeed", 1.0f);
    s.canInterrupt      = j.value("canInterrupt", true);
    s.position.x        = j.value("posX", 0.0f);
    s.position.y        = j.value("posY", 0.0f);

    // v2.0: state-bound BT path + designer note (default: empty).
    s.behaviorTreePath  = j.value("behaviorTreePath", std::string{});
    s.aiNote            = j.value("aiNote", std::string{});

    if (j.contains("properties") && j["properties"].is_object())
        for (auto& [k, v] : j["properties"].items())
            s.properties[k] = v.get<float>();

    return s;
}

static nlohmann::json ParamToJson(const ParameterDef& p)
{
    return {
        {"name",         p.name},
        {"type",         static_cast<int>(p.type)},
        {"defaultValue", p.defaultValue}
    };
}

static ParameterDef ParamFromJson(const nlohmann::json& j)
{
    ParameterDef p;
    p.name         = j.value("name", "");
    p.type         = static_cast<ParameterType>(j.value("type", 0));
    p.defaultValue = j.value("defaultValue", 0.0f);
    return p;
}

// ============================================================================
// Save / Load
// ============================================================================

nlohmann::json StateMachineAssetSerializer::ToJson(const StateMachineAsset& asset)
{
    nlohmann::json root;
    root["name"]           = asset.name;
    root["defaultStateId"] = asset.defaultStateId;
    root["nextId"]         = asset.nextId;

    root["states"] = nlohmann::json::array();
    for (auto& s : asset.states)
        root["states"].push_back(StateToJson(s));

    root["transitions"] = nlohmann::json::array();
    for (auto& t : asset.transitions)
        root["transitions"].push_back(TransitionToJson(t));

    root["parameters"] = nlohmann::json::array();
    for (auto& p : asset.parameters)
        root["parameters"].push_back(ParamToJson(p));

    return root;
}

bool StateMachineAssetSerializer::FromJson(const nlohmann::json& root, StateMachineAsset& outAsset)
{
    outAsset = {};
    outAsset.name           = root.value("name", "");
    outAsset.defaultStateId = root.value("defaultStateId", 0u);
    outAsset.nextId         = root.value("nextId", 1u);

    if (root.contains("states") && root["states"].is_array())
        for (auto& js : root["states"])
            outAsset.states.push_back(StateFromJson(js));

    if (root.contains("transitions") && root["transitions"].is_array())
        for (auto& jt : root["transitions"])
            outAsset.transitions.push_back(TransitionFromJson(jt));

    if (root.contains("parameters") && root["parameters"].is_array())
        for (auto& jp : root["parameters"])
            outAsset.parameters.push_back(ParamFromJson(jp));

    return true;
}

bool StateMachineAssetSerializer::Save(const std::string& path, const StateMachineAsset& asset)
{
    nlohmann::json root = ToJson(asset);
    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;
    ofs << root.dump(2);
    return true;
}

bool StateMachineAssetSerializer::Load(const std::string& path, StateMachineAsset& outAsset)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    nlohmann::json root;
    try { ifs >> root; }
    catch (...) { return false; }

    return FromJson(root, outAsset);
}
