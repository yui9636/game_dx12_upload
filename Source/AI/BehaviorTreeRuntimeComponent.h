#pragma once
#include <cstdint>

// Per-entity runtime state for BehaviorTree execution.
// activeNodeStack stores the path (root -> ... -> running leaf)
// from the previous frame so Sequence/Selector can resume.
//
// nodeStateValues[] is a small key-value scratchpad used by stateful nodes:
//   - Wait: total elapsed time
//   - Cooldown: timestamp of last Success
//   - Repeat: completed count
//   - Attack / DodgeAction: phase (0=Idle, 1=Requested, 2=InProgress)
//
// debugTrace* records per-node Status from the latest Tick for editor overlay.
struct BehaviorTreeRuntimeComponent
{
    static constexpr int MAX_ACTIVE_STACK = 16;
    static constexpr int MAX_NODE_STATE   = 32;
    static constexpr int MAX_DEBUG_TRACE  = 64;

    uint32_t activeNodeStack[MAX_ACTIVE_STACK] = {};
    uint8_t  activeNodeStackDepth              = 0;

    uint32_t nodeStateIds[MAX_NODE_STATE]    = {};
    float    nodeStateValues[MAX_NODE_STATE] = {};
    uint8_t  nodeStateCount                  = 0;

    // None=0, Running=1, Success=2, Failure=3
    uint32_t debugTraceIds[MAX_DEBUG_TRACE]    = {};
    uint8_t  debugTraceStatus[MAX_DEBUG_TRACE] = {};
    uint8_t  debugTraceCount                   = 0;

    // ---- v2.0 (state-bound) ----
    // StateMachine state id observed at the previous tick.
    // BehaviorTreeSystem calls ResetAll() whenever this differs from the
    // current state, so each state always starts the BT from scratch.
    // 0 = "no state observed yet". Any change triggers a reset.
    uint32_t lastTickedStateId = 0;

    // ---- Helpers (header-only inlines) ----
    float GetNodeState(uint32_t nodeId, float defaultValue = 0.0f) const
    {
        for (int i = 0; i < nodeStateCount; ++i) {
            if (nodeStateIds[i] == nodeId) return nodeStateValues[i];
        }
        return defaultValue;
    }

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

    void ClearNodeState(uint32_t nodeId)
    {
        for (int i = 0; i < nodeStateCount; ++i) {
            if (nodeStateIds[i] == nodeId) {
                // swap with last
                nodeStateIds[i]    = nodeStateIds[nodeStateCount - 1];
                nodeStateValues[i] = nodeStateValues[nodeStateCount - 1];
                --nodeStateCount;
                return;
            }
        }
    }

    void ResetAll()
    {
        activeNodeStackDepth = 0;
        nodeStateCount       = 0;
        debugTraceCount      = 0;
        // lastTickedStateId is intentionally left untouched here; the system
        // is responsible for synchronising it with the current SM state.
    }

    void PushDebugTrace(uint32_t nodeId, uint8_t status)
    {
        if (debugTraceCount < MAX_DEBUG_TRACE) {
            debugTraceIds[debugTraceCount]    = nodeId;
            debugTraceStatus[debugTraceCount] = status;
            ++debugTraceCount;
        }
    }

    uint8_t GetLastDebugStatus(uint32_t nodeId) const
    {
        for (int i = 0; i < debugTraceCount; ++i) {
            if (debugTraceIds[i] == nodeId) return debugTraceStatus[i];
        }
        return 0;
    }
};
