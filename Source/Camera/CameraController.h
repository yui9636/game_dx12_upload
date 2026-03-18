#pragma once

#include <DirectXMath.h>
#include <vector>
#include <random> // ノイズ用

#include "Message/MessageData.h"
#include "Message/Messenger.h"

class Actor;
class Camera;

// カメラコントローラー
class CameraController
{
public:
	// ★追加: シングルトンアクセサ
	static CameraController* Instance();

	// モード
	enum class Mode
	{
		FreeCamera,		// フリーカメラ
		FollowCamera,	// フォローカメラ	
		LockonCamera,	// ロックオンカメラ
		MotionCamera,	// モーションカメラ
		Cinematic,
	};

public:
	CameraController();
	~CameraController();

	// 更新処理
	void Update(float dt);

	void SetPlayer(const Actor* player);
	void ResetCameraState(const DirectX::XMFLOAT3& startEye, const DirectX::XMFLOAT3& startFocus);

	// ★既存: 既存のデバッグGUI (維持)
	void DebugGUI();

	// ★追加: シェイク調整用GUI (新規)
	void DebugShakeGUI();

	// ★追加: 外部からシェイクをリクエストする関数
	void AddShake(float amplitude, float duration, float frequency, float decay);

	void SetTimelineShakeOffset(const DirectX::XMFLOAT3& offset);


	void ApplyShakeAndSetCamera(Camera* targetCam, const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up);




	void SetMode(Mode newMode) { mode = newMode; }
	Mode GetMode() const { return mode; }

private:
	// フリーカメラ更新処理
	void FreeCamera(float dt);

	// フォローカメラ更新処理
	void FollowCamera(float dt);

	// ロックオンカメラ更新処理
	void LockonCamera(float dt);

	// モーションカメラ更新処理
	void MotionCamera(float dt);

	// ★追加: シェイクを合成してカメラに適用するヘルパー

private:
	// フリーカメラ
	void OnFreeMode(void* data);

	// フォローカメラ
	void OnFollowMode(void* data);

	// ロックオンカメラ
	void OnLockonMode(void* data);

	// モーションカメラ
	void OnMotionMode(void* data);

	// カメラ揺れ
	void OnShake(void* data);

	// 横軸のズレ方向算出
	float CalcSide(DirectX::XMFLOAT3 p1, DirectX::XMFLOAT3 p2);

private:
	static CameraController* instance; // シングルトン実体

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
	// ロックオン処理
	DirectX::XMFLOAT3	newPosition = { 0, 0, 0 };
	DirectX::XMFLOAT3	newTarget = { 0, 0, 0 };
	DirectX::XMFLOAT3	targetWork[2] = { { 0, 0, 0 }, { 0, 0, 0 } };	// 0 : 座標, 1 : 注視点
	float				lengthLimit[2] = { 5, 7 };
	float				sideValue = 1;
	// モーションカメラ
	float				motionTimer = 0;
	std::vector<CameraMotionData>	motionData;

	// ★追加: リッチなシェイク用パラメータ
	struct ShakeState {
		float amplitude = 0.0f;
		float duration = 0.0f;
		float frequency = 0.0f;
		float decay = 0.0f;
		float seed = 0.0f;
	} activeShake;

	DirectX::XMFLOAT3 shakeOffset = { 0, 0, 0 };
	DirectX::XMFLOAT3 timelineShakeOffset = { 0, 0, 0 };

	// メッセージキー
	uint64_t			CAMERACHANGEFREEMODEKEY;
	uint64_t			CAMERACHANGEFOLLOWMODEKEY;
	uint64_t			CAMERACHANGELOCKONMODEKEY;
	uint64_t			CAMERACHANGEMOTIONMODEKEY;
	uint64_t			CAMERASHAKEKEY;
};