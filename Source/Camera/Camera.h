#pragma once

#include <DirectXMath.h>

class Camera
{
private:
	//Camera() {}
	//~Camera() {}
public:
	static Camera& Instance()
	{
		static Camera camera;
		return camera;
	}

	void SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up);

	void SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ);

	const DirectX::XMFLOAT4X4& GetView() const { return view; }

	const DirectX::XMFLOAT4X4& GetProjection() const { return projection; }

	const DirectX::XMFLOAT3& GetEye() const { return eye; }

	const DirectX::XMFLOAT3& GetFocus() const { return focus; }

	const DirectX::XMFLOAT3& GetUp() const { return up; }

	const DirectX::XMFLOAT3& GetFront() const { return front; }

	const DirectX::XMFLOAT3& GetRight() const { return right; }

	const	float& GetFovY() { return	fovY; }

	const	float& GetAspect() { return	aspect; }

	const	float& GetNearZ() { return	nearZ; }

	const	float& GetFarZ() { return	farZ; }

private:
	DirectX::XMFLOAT4X4		view;
	DirectX::XMFLOAT4X4		projection;

	DirectX::XMFLOAT3		eye;
	DirectX::XMFLOAT3		focus;

	DirectX::XMFLOAT3		up{ 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3		front{ 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3		right{ 1.0f, 0.0f, 0.0f };


	float fovY = DirectX::XM_PIDIV4;
	float aspect = 1.777f;
	float nearZ = 0.1f;
	float farZ = 100000.0f;

};
