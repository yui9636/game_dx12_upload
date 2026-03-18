#pragma once
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <memory>
#include <deque>
#include <imgui.h>

// --- 超究極ポイント1: 動的プロパティ ---
using BTParamVariant = std::variant<float, int, std::string, bool>;

// サブツリー対応のためのノードタイプ拡張
enum class BTNodeType {
    Root,       // ツリーの開始点
    Composite,  // Selector, Sequence (制御)
    Decorator,  // 条件分岐
    Action,     // 実行
    SubTree     // 外部ファイル参照 (超究極案2)
};

enum class BTNodePinType { Input, Output };

// ノードの実行状態
enum class BTExecuteStatus { Idle, Running, Success, Failure };

// --- 超究極ポイント13: バリデーションエラー構造体 ---
struct BTValidationError {
    unsigned int nodeId;
    std::string message;
    bool isCritical; // 実行不可レベルの警告か
};

// --- 超究極ポイント4: 実行トレースログ (過去の思考履歴) ---
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
    unsigned int startPinId; // 出力側
    unsigned int endPinId;   // 入力側
    bool isActive = false;   // 現在実行中か (Live Pulse: 超究極案12連携)

    // --- 超究極ポイント7: 重み付けランダム ---
    float weight = 1.0f;
};

struct BTNodeEditorData {
    unsigned int id;
    std::string name;
    BTNodeType type;
    ImVec2 pos;
    std::vector<BTNodePin> inputs;
    std::vector<BTNodePin> outputs;

    // パラメータ領域
    std::map<std::string, BTParamVariant> properties;

    // 実行状態
    BTExecuteStatus lastStatus = BTExecuteStatus::Idle;
    float lastActiveTime = 0.0f; // Live Pulseの減衰演出用
};

class BTGraph {
public:
    BTGraph() : nextId(1), version(0) {}

    // 基本データ
    std::vector<BTNodeEditorData> nodes;
    std::vector<BTNodeLink> links;

    // --- 超究極ポイント3: フェーズ管理 (マルチルート) ---
    // 名前付きルート (例: "Phase1", "Phase2", "Angry") -> NodeID
    std::map<std::string, unsigned int> rootNodes;
    std::string activePhase = "Default";

    // --- 超究極ポイント13: バリデーション履歴 ---
    std::vector<BTValidationError> validationErrors;

    // --- 超究極ポイント4: トレース履歴 (思考の可視化用) ---
    // 直近100ステップ程度の実行順序を記録
    std::deque<BTTraceLog> executionTrace;
    const size_t MaxTraceCount = 100;

    // --- 超究極ポイント12: ホットリロード用バージョン管理 ---
    unsigned int version;

    // ID発行管理
    unsigned int GetNextId() { return nextId++; }
    void SetNextId(unsigned int id) { nextId = id; }

    // 検索ヘルパー
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

    // 履歴への追加
    void AddTrace(unsigned int nodeId, BTExecuteStatus status, float time) {
        executionTrace.push_back({ time, nodeId, status });
        if (executionTrace.size() > MaxTraceCount) executionTrace.pop_front();
    }

private:
    unsigned int nextId;
};