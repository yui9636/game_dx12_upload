#pragma once

#include <DirectXMath.h>
#include <vector>
#include <random>

#include "Message/MessageData.h"
#include "Message/Messenger.h"

class Actor;
class Camera;

class CameraController
{
public:
	static CameraController* Instance();

	enum class Mode
	{
		FreeCamera,
		FollowCamera,
		LockonCamera,
		MotionCamera,
		Cinematic,
	};

public:
	CameraController();
	~CameraController();

	void Update(float dt);

	void SetPlayer(const Actor* player);
	void ResetCameraState(const DirectX::XMFLOAT3& startEye, const DirectX::XMFLOAT3& startFocus);

	void DebugGUI();

	void DebugShakeGUI();

	void AddShake(float amplitude, float duration, float frequency, float decay);

	void SetTimelineShakeOffset(const DirectX::XMFLOAT3& offset);


	void ApplyShakeAndSetCamera(Camera* targetCam, const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up);




	void SetMode(Mode newMode) { mode = newMode; }
	Mode GetMode() const { return mode; }

private:
	void FreeCamera(float dt);

	void FollowCamera(float dt);

	void LockonCamera(float dt);

	void MotionCamera(float dt);


private:
	void OnFreeMode(void* data);

	void OnFollowMode(void* data);

	void OnLockonMode(void* data);

	void OnMotionMode(void* data);

	void OnShake(void* data);

	float CalcSide(DirectX::XMFLOAT3 p1, DirectX::XMFLOAT3 p2);

private:
	static CameraController* instance;

	Mode				mode = Mode::FollowCamera;

	DirectX::XMFLOAT3 currentEye = { 0, 0, 0 };
	DirectX::XMFLOAT3 currentFocus = { 0, 0, 0 };

	const Actor* playerActor = nullptr;
	const Actor* targetActor = nullptr;

	DirectX::XMFLOAT3 camLocalOffset = { 0.0f, 24.53f, -60.9f };
	DirectX::XMFLOAT3 camLookOffset = { 0.0f, 13.0f,  -6.95f };
	float             camFollowLerp = 12.0f;


	DirectX::XMFLOAT3	position = { 0, 0, 0 };
	DirectX::XMFLOAT3	target = { 0, 0, 0 };
	DirectX::XMFLOAT3	angle = { 0, 0, 0 };
	float				rollSpeed = DirectX::XMConvertToRadians(90);
	float				range = 10.0f;
	float				maxAngleX = DirectX::XMConvertToRadians(+45);
	float				minAngleX = DirectX::XMConvertToRadians(-45);
	DirectX::XMFLOAT3	newPosition = { 0, 0, 0 };
	DirectX::XMFLOAT3	newTarget = { 0, 0, 0 };
	DirectX::XMFLOAT3	targetWork[2] = { { 0, 0, 0 }, { 0, 0, 0 } };
	float				lengthLimit[2] = { 5, 7 };
	float				sideValue = 1;
	float				motionTimer = 0;
	std::vector<CameraMotionData>	motionData;

	struct ShakeState {
		float amplitude = 0.0f;
		float duration = 0.0f;
		float frequency = 0.0f;
		float decay = 0.0f;
		float seed = 0.0f;
	} activeShake;

	DirectX::XMFLOAT3 shakeOffset = { 0, 0, 0 };
	DirectX::XMFLOAT3 timelineShakeOffset = { 0, 0, 0 };

	uint64_t			CAMERACHANGEFREEMODEKEY;
	uint64_t			CAMERACHANGEFOLLOWMODEKEY;
	uint64_t			CAMERACHANGELOCKONMODEKEY;
	uint64_t			CAMERACHANGEMOTIONMODEKEY;
	uint64_t			CAMERASHAKEKEY;
};
