#pragma once
#include <DirectXMath.h>

class Registry;

class TimelineShakeSystem {
public:
    static void Update(Registry& registry, float dt);

    // このフレームで蓄積した揺れ offset（カメラが読む）
    static DirectX::XMFLOAT3 GetShakeOffset();
    static void ResetShakeOffset();

private:
    static DirectX::XMFLOAT3 s_shakeOffset;
};
