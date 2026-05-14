#pragma once

// 2D rendering で使う active Camera2D を示す tag。
// 3D 側の CameraMainTagComponent と対になる。
// 複数存在する場合は最初に見つかったものを使い、warning を出す。
struct Camera2DMainTagComponent {};
