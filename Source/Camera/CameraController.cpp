#include "CameraController.h"
#include "Camera/Camera.h"
#include "Input/Input.h"
#include "Actor/Actor.h"

#include <string.h>
#include "Stage/Stage.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

CameraController* CameraController::instance = nullptr;

CameraController* CameraController::Instance()
{
	return instance;
}

CameraController::CameraController()
{
	instance = this; 

	std::memset(&activeShake, 0, sizeof(activeShake));

	timelineShakeOffset = { 0.0f, 0.0f, 0.0f };
	shakeOffset = { 0.0f, 0.0f, 0.0f };

	position = Camera::Instance().GetEye();
	newPosition = Camera::Instance().GetEye();
	CAMERACHANGEFREEMODEKEY = Messenger::Instance().AddReceiver(MessageData::CAMERACHANGEFREEMODE, [&](void* data) { OnFreeMode(data); });
	CAMERACHANGEFOLLOWMODEKEY = Messenger::Instance().AddReceiver(MessageData::CAMERACHANGEFOLLOWMODE, [&](void* data) { OnFollowMode(data); });
	CAMERACHANGELOCKONMODEKEY = Messenger::Instance().AddReceiver(MessageData::CAMERACHANGELOCKONMODE, [&](void* data) { OnLockonMode(data); });
	CAMERACHANGEMOTIONMODEKEY = Messenger::Instance().AddReceiver(MessageData::CAMERACHANGEMOTIONMODE, [&](void* data) { OnMotionMode(data); });
	CAMERASHAKEKEY = Messenger::Instance().AddReceiver(MessageData::CAMERASHAKE, [&](void* data) { OnShake(data); });
}

CameraController::~CameraController()
{
	if (instance == this) instance = nullptr; 

	Messenger::Instance().RemoveReceiver(CAMERACHANGEFREEMODEKEY);
	Messenger::Instance().RemoveReceiver(CAMERACHANGEFOLLOWMODEKEY);
	Messenger::Instance().RemoveReceiver(CAMERACHANGELOCKONMODEKEY);
	Messenger::Instance().RemoveReceiver(CAMERACHANGEMOTIONMODEKEY);
	Messenger::Instance().RemoveReceiver(CAMERASHAKEKEY);
}

void CameraController::SetPlayer(const Actor* player)
{
	playerActor = player;
}

void CameraController::ResetCameraState(const DirectX::XMFLOAT3& startEye, const DirectX::XMFLOAT3& startFocus)
{
	currentEye = startEye;
	currentFocus = startFocus;
	position = startEye;
	target = startFocus;

	Camera::Instance().SetLookAt(currentEye, currentFocus, DirectX::XMFLOAT3(0, 1, 0));
}

void CameraController::AddShake(float amplitude, float duration, float frequency, float decay)
{
	if (amplitude >= activeShake.amplitude * 0.5f)
	{
		activeShake.amplitude = amplitude;
		activeShake.duration = duration;
		activeShake.frequency = frequency;
		activeShake.decay = decay;
		activeShake.seed = (float)(rand() % 1000);
	}
}

void CameraController::SetTimelineShakeOffset(const DirectX::XMFLOAT3& offset)
{
	timelineShakeOffset = offset;
}

void CameraController::ApplyShakeAndSetCamera(Camera* targetCam, const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up)
{
	if (!targetCam) return;

	shakeOffset = { 0, 0, 0 };

	if (activeShake.duration > 0.0f)
	{
		float dt = 1.0f / 60.0f;
		activeShake.duration -= dt;
		activeShake.amplitude *= activeShake.decay;

		if (activeShake.amplitude > 0.001f)
		{
			float time = (10.0f - activeShake.duration) * activeShake.frequency;
			float nX = sinf(time + activeShake.seed) + sinf(time * 0.3f);
			float nY = cosf(time * 0.9f + activeShake.seed);
			float nZ = sinf(time * 1.2f + 2.0f);

			shakeOffset.x = nX * activeShake.amplitude;
			shakeOffset.y = nY * activeShake.amplitude;
			shakeOffset.z = nZ * activeShake.amplitude;
		}
		else
		{
			activeShake.duration = 0.0f;
		}
	}

	float totalShakeX = shakeOffset.x + timelineShakeOffset.x;
	float totalShakeY = shakeOffset.y + timelineShakeOffset.y;
	float totalShakeZ = shakeOffset.z + timelineShakeOffset.z;

	DirectX::XMFLOAT3 finalEye = { eye.x + totalShakeX, eye.y + totalShakeY, eye.z + totalShakeZ };
	DirectX::XMFLOAT3 finalFocus = { focus.x + totalShakeX, focus.y + totalShakeY, focus.z + totalShakeZ };

	targetCam->SetLookAt(finalEye, finalFocus, up);

	timelineShakeOffset = { 0, 0, 0 };
}


void CameraController::Update(float dt)
{

	if (mode == Mode::Cinematic) return;

	switch (mode)
	{
	case	Mode::FreeCamera:	FreeCamera(dt);	break;
	case	Mode::FollowCamera: FollowCamera(dt);  return;
	case	Mode::LockonCamera:	LockonCamera(dt);	break;
	case	Mode::MotionCamera:	MotionCamera(dt);	break;
	}

	static	constexpr	float	Speed = 1.0f / 8.0f;
	position.x += (newPosition.x - position.x) * Speed;
	position.y += (newPosition.y - position.y) * Speed;
	position.z += (newPosition.z - position.z) * Speed;
	target.x += (newTarget.x - target.x) * Speed;
	target.y += (newTarget.y - target.y) * Speed;
	target.z += (newTarget.z - target.z) * Speed;

	ApplyShakeAndSetCamera(&Camera::Instance(), position, target, DirectX::XMFLOAT3(0, 1, 0));

}

void CameraController::FreeCamera(float dt)
{
	GamePad& gamePad = Input::Instance().GetGamePad();
	float ax = gamePad.GetAxisRX();
	float ay = gamePad.GetAxisRY();
	float speed = rollSpeed * dt;

	angle.x += ay * speed;
	angle.y += ax * speed;

	if (angle.x < minAngleX) angle.x = minAngleX;
	if (angle.x > maxAngleX) angle.x = maxAngleX;

	if (angle.y < -DirectX::XM_PI) angle.y += DirectX::XM_2PI;
	if (angle.y > DirectX::XM_PI) angle.y -= DirectX::XM_2PI;

	DirectX::XMMATRIX Transform = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);

	DirectX::XMVECTOR Front = Transform.r[2];
	DirectX::XMFLOAT3 front;
	DirectX::XMStoreFloat3(&front, Front);

	newPosition.x = target.x - front.x * range;
	newPosition.y = target.y - front.y * range;
	newPosition.z = target.z - front.z * range;
}

void CameraController::FollowCamera(float dt)
{
	if (!playerActor) return;

	DirectX::XMFLOAT3 p = playerActor->GetPosition();

	const DirectX::XMFLOAT3 targetEye = {
		p.x + camLocalOffset.x,
		p.y + camLocalOffset.y,
		p.z + camLocalOffset.z
	};
	const DirectX::XMFLOAT3 targetFocus = {
		p.x + camLookOffset.x,
		p.y + camLookOffset.y,
		p.z + camLookOffset.z
	};

	float t = camFollowLerp * dt;
	if (t > 1.0f) t = 1.0f;

	currentEye.x += (targetEye.x - currentEye.x) * t;
	currentEye.y += (targetEye.y - currentEye.y) * t;
	currentEye.z += (targetEye.z - currentEye.z) * t;

	currentFocus.x += (targetFocus.x - currentFocus.x) * t;
	currentFocus.y += (targetFocus.y - currentFocus.y) * t;
	currentFocus.z += (targetFocus.z - currentFocus.z) * t;

	ApplyShakeAndSetCamera(&Camera::Instance(), currentEye, currentFocus, DirectX::XMFLOAT3(0, 1, 0));
}

void CameraController::LockonCamera(float dt)
{
	DirectX::XMVECTOR	t0 = DirectX::XMVectorSet(targetWork[0].x, 0.5f, targetWork[0].z, 0);
	DirectX::XMVECTOR	t1 = DirectX::XMVectorSet(targetWork[1].x, 0.5f, targetWork[1].z, 0);
	DirectX::XMVECTOR	crv = DirectX::XMLoadFloat3(&Camera::Instance().GetRight());
	DirectX::XMVECTOR	cuv = DirectX::XMVectorSet(0, 1, 0, 0);
	DirectX::XMVECTOR	v = DirectX::XMVectorSubtract(t1, t0);
	DirectX::XMVECTOR	l = DirectX::XMVector3Length(v);

	t0 = DirectX::XMLoadFloat3(&targetWork[0]);
	t1 = DirectX::XMLoadFloat3(&targetWork[1]);

	DirectX::XMStoreFloat3(&newTarget, DirectX::XMVectorMultiplyAdd(v, DirectX::XMVectorReplicate(0.5f), t0));

	l = DirectX::XMVectorClamp(l
		, DirectX::XMVectorReplicate(lengthLimit[0])
		, DirectX::XMVectorReplicate(lengthLimit[1]));
	t0 = DirectX::XMVectorMultiplyAdd(l, DirectX::XMVector3Normalize(DirectX::XMVectorNegate(v)), t0);
	t0 = DirectX::XMVectorMultiplyAdd(crv, DirectX::XMVectorReplicate(sideValue * 3.0f), t0);
	t0 = DirectX::XMVectorMultiplyAdd(cuv, DirectX::XMVectorReplicate(3.0f), t0);
	DirectX::XMStoreFloat3(&newPosition, t0);
}

void CameraController::MotionCamera(float dt)
{
	if (motionData.empty())
		return;

	motionTimer += dt * 60;
	if (motionData.size() == 1)
	{
		if (motionTimer >= motionData[0].time)
		{
			newPosition = motionData[0].position;
			newTarget = motionData[0].target;
			position = newPosition;
			target = newTarget;
		}
	}
	else
	{
		bool set = false;
		for (int i = 0; i < motionData.size() - 1; ++i)
		{
			if (motionData[i].time <= motionTimer && motionTimer < motionData[i + 1].time)
			{
				set = true;
				float	value = motionData[i + 1].time - motionData[i].time;
				value = (motionTimer - motionData[i].time) / value;
				newPosition = motionData[i].position;
				newPosition.x += (motionData[i + 1].position.x - motionData[i].position.x) * value;
				newPosition.y += (motionData[i + 1].position.y - motionData[i].position.y) * value;
				newPosition.z += (motionData[i + 1].position.z - motionData[i].position.z) * value;
				newTarget = motionData[i].target;
				newTarget.x += (motionData[i + 1].target.x - motionData[i].target.x) * value;
				newTarget.y += (motionData[i + 1].target.y - motionData[i].target.y) * value;
				newTarget.z += (motionData[i + 1].target.z - motionData[i].target.z) * value;
				position = newPosition;
				target = newTarget;
				break;
			}
		}
		if (!set)
		{
			if (motionTimer >= motionData[motionData.size() - 1].time)
			{
				newPosition = motionData[motionData.size() - 1].position;
				newTarget = motionData[motionData.size() - 1].target;
				position = newPosition;
				target = newTarget;
			}
		}
	}
}

void CameraController::OnFreeMode(void* data)
{
	MessageData::CAMERACHANGEFREEMODEDATA* p = static_cast<MessageData::CAMERACHANGEFREEMODEDATA*>(data);
	if (this->mode != Mode::FreeCamera)
	{
		DirectX::XMFLOAT3	v;
		v.x = newPosition.x - newTarget.x;
		v.y = newPosition.y - newTarget.y;
		v.z = newPosition.z - newTarget.z;
		angle.y = atan2f(v.x, v.z) + DirectX::XM_PI;
		angle.x = atan2f(v.y, v.z);
		angle.y = atan2f(sinf(angle.y), cosf(angle.y));
		angle.x = atan2f(sinf(angle.x), cosf(angle.x));
	}
	this->mode = Mode::FreeCamera;
	this->newTarget = p->target;
	this->newTarget.y += 0.01f;
}

void CameraController::OnFollowMode(void* data)
{
	MessageData::CAMERACHANGEFOLLOWMODEDATA* p = static_cast<MessageData::CAMERACHANGEFOLLOWMODEDATA*>(data);

	this->mode = Mode::FollowCamera;

}

void CameraController::OnLockonMode(void* data)
{
	MessageData::CAMERACHANGELOCKONMODEDATA* p = static_cast<MessageData::CAMERACHANGELOCKONMODEDATA*>(data);
	if (this->mode != Mode::LockonCamera)
		sideValue = CalcSide(p->start, p->target);

	this->mode = Mode::LockonCamera;
	targetWork[0] = p->start;
	targetWork[1] = p->target;
	targetWork[0].y += 0.01f;
	targetWork[1].y += 0.01f;
}

void CameraController::OnMotionMode(void* data)
{
	MessageData::CAMERACHANGEMOTIONMODEDATA* p = static_cast<MessageData::CAMERACHANGEMOTIONMODEDATA*>(data);
	if (this->mode != Mode::MotionCamera)
		motionTimer = 0;
	this->mode = Mode::MotionCamera;
	motionData.clear();
	motionData = p->data;
}

void CameraController::OnShake(void* data)
{
	MessageData::CAMERASHAKEDATA* p = static_cast<MessageData::CAMERASHAKEDATA*>(data);
	AddShake(p->shakePower * 0.1f, p->shakeTimer / 60.0f, 20.0f, 0.9f);
}

float CameraController::CalcSide(DirectX::XMFLOAT3 p1, DirectX::XMFLOAT3 p2)
{
	DirectX::XMFLOAT2	v;
	v.x = position.x - target.x;
	v.y = position.z - target.z;
	float	l = sqrtf(v.x * v.x + v.y * v.y);
	v.y /= l;
	v.y /= l;
	DirectX::XMFLOAT2	n;
	n.x = p1.x - p2.x;
	n.y = p1.z - p2.z;
	l = sqrtf(n.x * n.x + n.y * n.y);
	n.x /= l;
	n.y /= l;
	return	((v.x * n.y) - (v.y * n.x) < 0) ? +1.0f : -1.0f;
}

void CameraController::DebugGUI()
{
	if (ImGui::CollapsingHeader("Camera Controller Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// ---------------------------------------------------------
		// ---------------------------------------------------------
		const char* modeName = "Unknown";
		switch (mode)
		{
		case Mode::FreeCamera:   modeName = "Free (Debug)";     break;
		case Mode::FollowCamera: modeName = "Follow (Player)";  break;
		case Mode::LockonCamera: modeName = "Lockon (Combat)";  break;
		case Mode::MotionCamera: modeName = "Motion (Event)";   break;
		}
		ImGui::TextColored(ImVec4(0.1f, 1.0f, 1.0f, 1.0f), "Current Mode: %s", modeName);

		ImGui::Separator();

		// ---------------------------------------------------------
		// ---------------------------------------------------------
		ImGui::Text("Force Mode Switch:");

		if (ImGui::Button("Free"))
		{
			MessageData::CAMERACHANGEFREEMODEDATA data;
			data.target = Camera::Instance().GetFocus();
			Messenger::Instance().SendData(MessageData::CAMERACHANGEFREEMODE, &data);
		}

		ImGui::SameLine();

		if (ImGui::Button("Follow"))
		{
			Messenger::Instance().SendData(MessageData::CAMERACHANGEFOLLOWMODE, nullptr);
		}

		ImGui::Separator();

		// ---------------------------------------------------------
		// ---------------------------------------------------------
		if (mode == Mode::FollowCamera || mode == Mode::LockonCamera)
		{
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[ Follow / Lockon Params ]");

			bool change = false;
			change |= ImGui::DragFloat3("Local Offset", &camLocalOffset.x, 0.1f);
			change |= ImGui::DragFloat3("Look Offset", &camLookOffset.x, 0.1f);
			change |= ImGui::SliderFloat("Lerp Speed", &camFollowLerp, 1.0f, 30.0f);

			if (ImGui::Button("Reset to Default"))
			{
				camLocalOffset = { 0.0f, 24.53f, -60.9f };
				camLookOffset = { 0.0f, 13.0f,  -6.95f };
				camFollowLerp = 12.0f;
			}
		}
		else if (mode == Mode::FreeCamera)
		{
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[ Free Params ]");
			ImGui::DragFloat("Distance Range", &range, 0.1f, 1.0f, 100.0f);

			float deg = DirectX::XMConvertToDegrees(rollSpeed);
			if (ImGui::SliderFloat("Roll Speed (deg)", &deg, 10.0f, 180.0f)) {
				rollSpeed = DirectX::XMConvertToRadians(deg);
			}
		}

		ImGui::Separator();

		// ---------------------------------------------------------
		// ---------------------------------------------------------
		const auto& eye = Camera::Instance().GetEye();
		const auto& focus = Camera::Instance().GetFocus();

		ImGui::Text("Eye   : (%.2f, %.2f, %.2f)", eye.x, eye.y, eye.z);
		ImGui::Text("Focus : (%.2f, %.2f, %.2f)", focus.x, focus.y, focus.z);

		if (ImGui::CollapsingHeader("Shake Debug"))
		{
			ImGui::Text("Active Amplitude: %.3f", activeShake.amplitude);
			ImGui::Text("Active Duration:  %.3f", activeShake.duration);

			if (ImGui::Button("Small Shake")) AddShake(0.2f, 0.2f, 30.0f, 0.8f);
			if (ImGui::Button("Big Shake"))   AddShake(1.0f, 0.5f, 15.0f, 0.95f);
		}



	}
}

void CameraController::DebugShakeGUI()
{
	if (ImGui::CollapsingHeader("Shake Debug"))
	{
		ImGui::Text("Active Amplitude: %.3f", activeShake.amplitude);
		ImGui::Text("Active Duration:  %.3f", activeShake.duration);

		if (ImGui::Button("Small Shake")) AddShake(0.2f, 0.2f, 30.0f, 0.8f);
		if (ImGui::Button("Big Shake"))   AddShake(1.0f, 0.5f, 15.0f, 0.95f);
	}
}
