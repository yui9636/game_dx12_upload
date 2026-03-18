#pragma once

class Registry;

class CameraFinalizeSystem {
public:
    // Transform と Lens のデータから最終的な行列 (Matrices) を生成する
    static void Update(Registry& registry);
};