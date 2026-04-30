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
    const char* LoadingModeToString(GameLoopLoadingMode mode)
    {
        switch (mode) {
        case GameLoopLoadingMode::Immediate:      return "Immediate";
        case GameLoopLoadingMode::FadeOnly:       return "FadeOnly";
        case GameLoopLoadingMode::LoadingOverlay: return "LoadingOverlay";
        }
        return "Immediate";
    }

    GameLoopLoadingMode LoadingModeFromString(const std::string& s)
    {
        if (s == "FadeOnly") return GameLoopLoadingMode::FadeOnly;
        if (s == "LoadingOverlay") return GameLoopLoadingMode::LoadingOverlay;
        return GameLoopLoadingMode::Immediate;
    }

    const char* NodeTypeToString(GameLoopNodeType t)
    {
        switch (t) {
        case GameLoopNodeType::Scene: return "Scene";
        }
        return "Scene";
    }

    GameLoopNodeType NodeTypeFromString(const std::string& s)
    {
        (void)s;
        return GameLoopNodeType::Scene;
    }

    nlohmann::json InputToJson(const GameLoopTransitionInput& input)
    {
        nlohmann::json j;
        j["keyboardScancode"] = input.keyboardScancode;
        j["gamepadButton"] = input.gamepadButton == 0xFF ? -1 : static_cast<int>(input.gamepadButton);
        return j;
    }

    GameLoopTransitionInput InputFromJson(const nlohmann::json& j)
    {
        GameLoopTransitionInput input;
        input.keyboardScancode = j.value("keyboardScancode", 0u);
        int gamepadButton = j.value("gamepadButton", -1);
        if (gamepadButton < 0) input.gamepadButton = 0xFF;
        else if (gamepadButton > 254) input.gamepadButton = 254;
        else input.gamepadButton = static_cast<uint8_t>(gamepadButton);
        return input;
    }

    nlohmann::json LoadingPolicyToJson(const GameLoopLoadingPolicy& p)
    {
        nlohmann::json j;
        j["mode"] = LoadingModeToString(p.mode);
        j["fadeOutSeconds"] = p.fadeOutSeconds;
        j["fadeInSeconds"] = p.fadeInSeconds;
        j["minimumLoadingSeconds"] = p.minimumLoadingSeconds;
        if (!p.loadingMessage.empty()) {
            j["loadingMessage"] = p.loadingMessage;
        }
        j["blockInput"] = p.blockInput;
        return j;
    }

    GameLoopLoadingPolicy LoadingPolicyFromJson(const nlohmann::json& j)
    {
        GameLoopLoadingPolicy p;
        p.mode = LoadingModeFromString(j.value("mode", std::string{ "Immediate" }));
        p.fadeOutSeconds = j.value("fadeOutSeconds", 0.15f);
        p.fadeInSeconds = j.value("fadeInSeconds", 0.15f);
        p.minimumLoadingSeconds = j.value("minimumLoadingSeconds", 0.0f);
        p.loadingMessage = j.value("loadingMessage", std::string{});
        p.blockInput = j.value("blockInput", true);
        return p;
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

    bool HasTransitionInputBinding(const GameLoopTransitionInput& input)
    {
        return input.keyboardScancode != 0 || input.gamepadButton != 0xFF;
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
    asset.version = 4;
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
    if (fileVersion != 4) {
        return false;
    }
    if (!j.contains("nodes") || !j["nodes"].is_array()) {
        return false;
    }
    if (!j.contains("transitions") || !j["transitions"].is_array()) {
        return false;
    }

    GameLoopAsset out;
    out.version = 4;
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
        if (tj.contains("input") && tj["input"].is_object()) {
            t.input = InputFromJson(tj["input"]);
        }
        if (tj.contains("loading") && tj["loading"].is_object()) {
            t.loadingPolicy = LoadingPolicyFromJson(tj["loading"]);
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
    j["version"] = 4;
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
        tj["input"] = InputToJson(t.input);
        tj["loading"] = LoadingPolicyToJson(t.loadingPolicy);
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

    if (asset.version != 4) {
        r.messages.push_back({ GameLoopValidateSeverity::Error, "GameLoopAsset version must be 4" });
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
        if (asset.FindNode(t.fromNodeId) == nullptr) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " fromNodeId not found" });
        }
        if (asset.FindNode(t.toNodeId) == nullptr) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " toNodeId not found" });
        }
        if (!HasTransitionInputBinding(t.input)) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " input is unbound" });
        }
        if (t.loadingPolicy.fadeOutSeconds < 0.0f) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " loading fadeOutSeconds must be >= 0" });
        }
        if (t.loadingPolicy.fadeInSeconds < 0.0f) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " loading fadeInSeconds must be >= 0" });
        }
        if (t.loadingPolicy.minimumLoadingSeconds < 0.0f) {
            r.messages.push_back({ GameLoopValidateSeverity::Error, label + " loading minimumLoadingSeconds must be >= 0" });
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
