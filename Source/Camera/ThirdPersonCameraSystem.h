#pragma once

// Registry はここでは参照だけでよいため、前方宣言にしています。
class Registry;
// ThirdPersonCameraSystem は追従カメラの位置と向きを更新する。
// CameraTPVControlComponent を持つ Entity を Main Camera が追従する。
class ThirdPersonCameraSystem {
public:
// 三人称カメラの追従位置・回転を更新します。
static void Update(Registry& registry, float dt);
};
