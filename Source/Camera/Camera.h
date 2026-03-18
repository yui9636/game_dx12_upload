#pragma once

#include <DirectXMath.h>

// カメラ
class Camera
{
private:
	//Camera() {}
	//~Camera() {}
public:
	//唯一のインスタンス取得
	static Camera& Instance()
	{
		static Camera camera;
		return camera;
	}

	// 指定方向を向く
	void SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up);

	// パースペクティブ設定
	void SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ);

	// ビュー行列取得
	const DirectX::XMFLOAT4X4& GetView() const { return view; }

	// プロジェクション行列取得
	const DirectX::XMFLOAT4X4& GetProjection() const { return projection; }

	// 視点取得
	const DirectX::XMFLOAT3& GetEye() const { return eye; }

	// 注視点取得
	const DirectX::XMFLOAT3& GetFocus() const { return focus; }

	// 上方向取得
	const DirectX::XMFLOAT3& GetUp() const { return up; }

	// 前方向取得
	const DirectX::XMFLOAT3& GetFront() const { return front; }

	// 右方向取得
	const DirectX::XMFLOAT3& GetRight() const { return right; }

	// 視野角取得
	const	float& GetFovY() { return	fovY; }

	// アスペクト比取得
	const	float& GetAspect() { return	aspect; }

	// 近クリップ面までの距離を取得
	const	float& GetNearZ() { return	nearZ; }

	// 遠クリップ面までの距離をっ取得
	const	float& GetFarZ() { return	farZ; }

private:
	DirectX::XMFLOAT4X4		view;
	DirectX::XMFLOAT4X4		projection;

	DirectX::XMFLOAT3		eye;
	DirectX::XMFLOAT3		focus;

	DirectX::XMFLOAT3		up{ 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3		front{ 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3		right{ 1.0f, 0.0f, 0.0f };


	float fovY = DirectX::XM_PIDIV4; // 初期値を入れておくと安全
	float aspect = 1.777f;
	float nearZ = 0.1f;
	float farZ = 100000.0f;

};
