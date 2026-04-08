#pragma once

#include <DirectXMath.h>

struct AnimatorComponent
{
    struct LayerState
    {
        int currentAnimIndex = -1;
        float currentTime = 0.0f;
        float currentSpeed = 1.0f;
        bool isLoop = true;
        float weight = 0.0f;
        bool isFullBody = false;

        int prevAnimIndex = -1;
        float prevAnimTime = 0.0f;
        float blendDuration = 0.0f;
        float blendTimer = 0.0f;
        bool isBlending = false;
    };

    LayerState baseLayer{};
    LayerState actionLayer{};

    bool enableRootMotion = true;
    bool bakeRootMotionY = false;
    float rootMotionScale = 0.1f;
    DirectX::XMFLOAT3 rootMotionDelta = { 0.0f, 0.0f, 0.0f };

    bool driverConnected = false;
    bool driverAllowInternalUpdate = false;
    int driverOverrideAnimIndex = -1;
    float driverTime = 0.0f;
    bool driverLoop = false;
};
