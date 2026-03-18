#pragma once

class Registry;

class FreeCameraSystem {
public:
    // ImGuiからの入力を用いてカメラのTransformを更新する
    static void Update(Registry& registry, float dt);
};