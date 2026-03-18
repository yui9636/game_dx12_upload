#pragma once

class Registry;

class ThirdPersonCameraSystem {
public:
    // アクターのTransformを参照し、カメラのTransformを追従させる
    static void Update(Registry& registry, float dt);
};