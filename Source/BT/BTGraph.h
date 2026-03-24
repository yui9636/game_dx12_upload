#pragma once
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <memory>
#include <deque>
#include <imgui.h>

using BTParamVariant = std::variant<float, int, std::string, bool>;

enum class BTNodeType {
    Root,
    Composite,
    Decorator,
    Action,
    SubTree
};

enum class BTNodePinType { Input, Output };

enum class BTExecuteStatus { Idle, Running, Success, Failure };

struct BTValidationError {
    unsigned int nodeId;
    std::string message;
    bool isCritical;
};

struct BTTraceLog {
    float timestamp;
    unsigned int nodeId;
    BTExecuteStatus status;
};

struct BTNodePin {
    unsigned int id;
    unsigned int nodeId;
    BTNodePinType type;
    BTNodePin(unsigned int inId, unsigned int inNodeId, BTNodePinType inType)
        : id(inId), nodeId(inNodeId), type(inType) {
    }
};

struct BTNodeLink {
    unsigned int id;
    unsigned int startPinId;
    unsigned int endPinId;
    bool isActive = false;

    float weight = 1.0f;
};

struct BTNodeEditorData {
    unsigned int id;
    std::string name;
    BTNodeType type;
    ImVec2 pos;
    std::vector<BTNodePin> inputs;
    std::vector<BTNodePin> outputs;

    std::map<std::string, BTParamVariant> properties;

    BTExecuteStatus lastStatus = BTExecuteStatus::Idle;
    float lastActiveTime = 0.0f;
};

class BTGraph {
public:
    BTGraph() : nextId(1), version(0) {}

    std::vector<BTNodeEditorData> nodes;
    std::vector<BTNodeLink> links;

    std::map<std::string, unsigned int> rootNodes;
    std::string activePhase = "Default";

    std::vector<BTValidationError> validationErrors;

    std::deque<BTTraceLog> executionTrace;
    const size_t MaxTraceCount = 100;

    unsigned int version;

    unsigned int GetNextId() { return nextId++; }
    void SetNextId(unsigned int id) { nextId = id; }

    BTNodePin* FindPin(unsigned int pinId) {
        for (auto& n : nodes) {
            for (auto& p : n.inputs) if (p.id == pinId) return &p;
            for (auto& p : n.outputs) if (p.id == pinId) return &p;
        }
        return nullptr;
    }

    BTNodeEditorData* FindNodeByPin(unsigned int pinId) {
        for (auto& n : nodes) {
            for (auto& p : n.inputs) if (p.id == pinId) return &n;
            for (auto& p : n.outputs) if (p.id == pinId) return &n;
        }
        return nullptr;
    }

    BTNodeEditorData* FindNode(unsigned int nodeId) {
        for (auto& n : nodes) if (n.id == nodeId) return &n;
        return nullptr;
    }

    void AddTrace(unsigned int nodeId, BTExecuteStatus status, float time) {
        executionTrace.push_back({ time, nodeId, status });
        if (executionTrace.size() > MaxTraceCount) executionTrace.pop_front();
    }

private:
    unsigned int nextId;
};
