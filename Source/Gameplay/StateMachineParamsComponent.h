#pragma once
#include <cstdint>
#include <cstring>

// ============================================================================
// Runtime parameters for StateMachineSystem evaluation
// ============================================================================

struct StateMachineParamsComponent
{
    static constexpr int MAX_PARAMS = 16;

    struct Param {
        char  name[32] = {};
        float value     = 0.0f;
    };

    Param    params[MAX_PARAMS] = {};
    uint8_t  paramCount         = 0;

    // Current runtime state
    uint32_t currentStateId     = 0;
    float    stateTimer         = 0.0f;
    bool     animFinished       = false;

    // Asset path
    char     assetPath[256]     = {};

    // Helpers
    float GetParam(const char* name) const
    {
        for (int i = 0; i < paramCount; ++i)
            if (strcmp(params[i].name, name) == 0) return params[i].value;
        return 0.0f;
    }

    void SetParam(const char* name, float val)
    {
        for (int i = 0; i < paramCount; ++i) {
            if (strcmp(params[i].name, name) == 0) { params[i].value = val; return; }
        }
        if (paramCount < MAX_PARAMS) {
            strncpy_s(params[paramCount].name, name, _TRUNCATE);
            params[paramCount].value = val;
            paramCount++;
        }
    }
};
