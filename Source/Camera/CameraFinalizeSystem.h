#pragma once

// Registry はここでは参照だけでよいため、前方宣言にしています。
class Registry;

// =========================================================
// CameraFinalizeSystem
// ---------------------------------------------------------
// TransformComponent と CameraLensComponent から、
// 実際に描画で使う CameraMatricesComponent を最終更新するシステムです。
// =========================================================
class CameraFinalizeSystem {
public:
    // =========================================================
    // 全カメラのビュー行列・射影行列・カメラ方向を更新します。
    // =========================================================
    static void Update(Registry& registry);
};
