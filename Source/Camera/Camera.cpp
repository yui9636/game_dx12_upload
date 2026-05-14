#include "Camera.h"
// 注視点を指定してビュー行列と方向ベクトルを更新します。
void Camera::SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up)
{
	// 入力された位置・注視点・上方向を SIMD 用のベクトルへ変換します。
	DirectX::XMVECTOR Eye = DirectX::XMLoadFloat3(&eye);
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&focus);
	DirectX::XMVECTOR Up = DirectX::XMLoadFloat3(&up);

	// 左手座標系の LookAt 行列を作成し、ビュー行列として保存します。
	DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(Eye, Focus, Up);
	DirectX::XMStoreFloat4x4(&view, View);

	// ビュー行列の逆行列から、カメラのワールド方向ベクトルを取り出します。
	DirectX::XMMATRIX World = DirectX::XMMatrixInverse(nullptr, View);
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, World);

	// ワールド行列の X 軸を右方向として保持します。
	this->right.x = world._11;
	this->right.y = world._12;
	this->right.z = world._13;

	// ワールド行列の Y 軸を上方向として保持します。
	this->up.x = world._21;
	this->up.y = world._22;
	this->up.z = world._23;

	// ワールド行列の Z 軸を前方向として保持します。
	this->front.x = world._31;
	this->front.y = world._32;
	this->front.z = world._33;

	// 入力されたカメラ位置と注視点を保持します。
	this->eye = eye;
	this->focus = focus;
}
// 透視投影のパラメータを保存し、射影行列を更新します。
void Camera::SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
{
	// 後から参照できるように、射影パラメータをメンバへ保存します。
	this->fovY = fovY;
    this->aspect = aspect;
    this->nearZ = nearZ;
    this->farZ = farZ;

	// 左手座標系の透視投影行列を作成して保存します。
	DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
	DirectX::XMStoreFloat4x4(&projection, Projection);
}
