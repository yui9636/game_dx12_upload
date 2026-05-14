#pragma once

// Registry はここでは参照だけでよいため、前方宣言にしています。
class Registry;
// FreeCameraSystem はエディター向け自由カメラの入力と移動を処理する。
// エディタやデバッグ用の自由移動カメラを更新するシステムです。
// マウス右ボタン + WASD / EQ / ホイールで操作します。
class FreeCameraSystem {
public:
// 自由移動カメラの入力処理と Transform 更新を行います。
static void Update(Registry& registry, float dt);
};
