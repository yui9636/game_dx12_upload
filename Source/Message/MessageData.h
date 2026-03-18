#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>

class Actor;

// カメラモーション情報
struct CameraMotionData
{
	float time;
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 target;
};

namespace	MessageData
{
	// フリーカメラ
	static	constexpr	char* CAMERACHANGEFREEMODE = "CAMERA CHANGE FREEMODE";
	struct	CAMERACHANGEFREEMODEDATA
	{
		DirectX::XMFLOAT3	target;
	};

	// ロックオンカメラ
	static	constexpr	char* CAMERACHANGELOCKONMODE = "CAMERA CHANGE LOCKONMODE";
	struct	CAMERACHANGELOCKONMODEDATA
	{
		DirectX::XMFLOAT3	start;
		DirectX::XMFLOAT3	target;
			
		const Actor* targetActor = nullptr;
	};

	static constexpr char* CAMERACHANGEFOLLOWMODE = "CAMERA CHANGE FOLLOWMODE";
	struct CAMERACHANGEFOLLOWMODEDATA 
	{
		DirectX::XMFLOAT3	target;

	};


	// エリアルカメラ
	static	constexpr	char* CAMERACHANGEARIELMODE = "CAMERA CHANGE ARIELMODE";
	struct	CAMERACHANGEARIELMODEDATA
	{
		DirectX::XMFLOAT3	start;
		DirectX::XMFLOAT3	target;
		DirectX::XMFLOAT3	lockonTarget;
	};

	// モーションカメラ
	static	constexpr	char* CAMERACHANGEMOTIONMODE = "CAMERA CHANGE MOTIONMODE";
	struct	CAMERACHANGEMOTIONMODEDATA
	{
		std::vector<CameraMotionData>	data;
	};

	// カメラ揺れエフェクト
	static	constexpr	char* CAMERASHAKE = "CAMERA SHAKE";
	struct	CAMERASHAKEDATA
	{
		float	shakeTimer;
		float	shakePower;
	};


	static constexpr char* CINEMATIC_EVENT_TRIGGER = "CINEMATIC_EVENT_TRIGGER";

	// イベントデータ (荷物)
	struct CINEMATIC_EVENT_TRIGGER_DATA
	{
		std::string eventName; // 発火したイベントの名前 ("StartGame", "BossRoar" など)
	};

};
