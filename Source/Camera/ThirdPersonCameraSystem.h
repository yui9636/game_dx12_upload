#pragma once

// Registry はここでは参照だけでよいため、前方宣言にしています。
class Registry;
// ThirdPersonCameraSystem は追従カメラの位置と向きを更新する。
// プレイヤーなどの target を追従する三人称カメラを更新するシステムです。
class ThirdPersonCameraSystem {
public:
// 三人称カメラの追従位置・回転を更新します。
static void Update(Registry& registry, float dt);
};
