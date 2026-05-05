#pragma once

// ビヘイビアツリーを実行するための Entity ごとの一時状態を保持するコンポーネント定義。
#include <cstdint>

// ビヘイビアツリーの実行途中状態やデバッグ情報を保持するコンポーネント。
struct BehaviorTreeRuntimeComponent
{
    static constexpr int MAX_ACTIVE_STACK = 16;
    static constexpr int MAX_NODE_STATE   = 32;
    static constexpr int MAX_DEBUG_TRACE  = 64;

    // 前フレームに実行中だったノード経路。
    uint32_t activeNodeStack[MAX_ACTIVE_STACK] = {};
    // activeNodeStack の有効要素数。
    uint8_t  activeNodeStackDepth              = 0;

    // 一時状態を持つノード ID 一覧。
    uint32_t nodeStateIds[MAX_NODE_STATE]    = {};
    // nodeStateIds に対応する一時状態値。
    float    nodeStateValues[MAX_NODE_STATE] = {};
    // nodeStateIds / nodeStateValues の有効要素数。
    uint8_t  nodeStateCount                  = 0;

    // 直近 Tick で評価されたノード ID 一覧。
    uint32_t debugTraceIds[MAX_DEBUG_TRACE]    = {};
    // debugTraceIds に対応する評価結果。
    uint8_t  debugTraceStatus[MAX_DEBUG_TRACE] = {};
    // debugTraceIds / debugTraceStatus の有効要素数。
    uint8_t  debugTraceCount                   = 0;

    // 前回 Tick 時に観測した StateMachine の状態 ID。
    uint32_t lastTickedStateId = 0;

    // 指定ノード ID に紐づく一時状態値を取得する。
    float GetNodeState(uint32_t nodeId, float defaultValue = 0.0f) const
    {
        for (int i = 0; i < nodeStateCount; ++i) {
            if (nodeStateIds[i] == nodeId) return nodeStateValues[i];
        }
        return defaultValue;
    }

    // 指定ノード ID に紐づく一時状態値を書き込む。
    void SetNodeState(uint32_t nodeId, float value)
    {
        for (int i = 0; i < nodeStateCount; ++i) {
            if (nodeStateIds[i] == nodeId) { nodeStateValues[i] = value; return; }
        }
        if (nodeStateCount < MAX_NODE_STATE) {
            nodeStateIds[nodeStateCount]    = nodeId;
            nodeStateValues[nodeStateCount] = value;
            ++nodeStateCount;
        }
    }

    // 指定ノード ID に紐づく一時状態値を削除する。
    void ClearNodeState(uint32_t nodeId)
    {
        for (int i = 0; i < nodeStateCount; ++i) {
            if (nodeStateIds[i] == nodeId) {
                nodeStateIds[i]    = nodeStateIds[nodeStateCount - 1];
                nodeStateValues[i] = nodeStateValues[nodeStateCount - 1];
                --nodeStateCount;
                return;
            }
        }
    }

    // ビヘイビアツリーの実行状態とデバッグ履歴を初期化する。
    void ResetAll()
    {
        activeNodeStackDepth = 0;
        nodeStateCount       = 0;
        debugTraceCount      = 0;
    }

    // ノード評価結果をデバッグ表示用の履歴に追加する。
    void PushDebugTrace(uint32_t nodeId, uint8_t status)
    {
        if (debugTraceCount < MAX_DEBUG_TRACE) {
            debugTraceIds[debugTraceCount]    = nodeId;
            debugTraceStatus[debugTraceCount] = status;
            ++debugTraceCount;
        }
    }

    // 指定ノードの直近デバッグ状態を取得する。
    uint8_t GetLastDebugStatus(uint32_t nodeId) const
    {
        for (int i = 0; i < debugTraceCount; ++i) {
            if (debugTraceIds[i] == nodeId) return debugTraceStatus[i];
        }
        return 0;
    }
};
