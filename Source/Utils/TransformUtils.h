#pragma once

#include <DirectXMath.h>
// TransformUtils は複数の処理から使う変換・補助関数をまとめて提供する。

class TransformUtils
{
public:
	static bool MatrixToRollPitchYaw(const DirectX::XMFLOAT4X4& m, float& pitch, float& yaw, float& roll);

	static bool QuaternionToRollPitchYaw(const DirectX::XMFLOAT4& q, float& pitch, float& yaw, float& roll);
};
