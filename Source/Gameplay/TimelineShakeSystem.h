#pragma once
#include <DirectXMath.h>

class Registry;

class TimelineShakeSystem {
public:
    static void Update(Registry& registry, float dt);

    // Accumulated shake offset this frame (read by camera)
    static DirectX::XMFLOAT3 GetShakeOffset();
    static void ResetShakeOffset();

private:
    static DirectX::XMFLOAT3 s_shakeOffset;
};
