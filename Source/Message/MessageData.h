#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>

struct CameraMotionData
{
	float time;
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 target;
};

namespace	MessageData
{
	static	constexpr	char* CAMERACHANGEFREEMODE = "CAMERA CHANGE FREEMODE";
	struct	CAMERACHANGEFREEMODEDATA
	{
		DirectX::XMFLOAT3	target;
	};

	static	constexpr	char* CAMERACHANGELOCKONMODE = "CAMERA CHANGE LOCKONMODE";
	struct	CAMERACHANGELOCKONMODEDATA
	{
		DirectX::XMFLOAT3	start;
		DirectX::XMFLOAT3	target;
	};

	static constexpr char* CAMERACHANGEFOLLOWMODE = "CAMERA CHANGE FOLLOWMODE";
	struct CAMERACHANGEFOLLOWMODEDATA 
	{
		DirectX::XMFLOAT3	target;

	};


	static	constexpr	char* CAMERACHANGEARIELMODE = "CAMERA CHANGE ARIELMODE";
	struct	CAMERACHANGEARIELMODEDATA
	{
		DirectX::XMFLOAT3	start;
		DirectX::XMFLOAT3	target;
		DirectX::XMFLOAT3	lockonTarget;
	};

	static	constexpr	char* CAMERACHANGEMOTIONMODE = "CAMERA CHANGE MOTIONMODE";
	struct	CAMERACHANGEMOTIONMODEDATA
	{
		std::vector<CameraMotionData>	data;
	};

	static	constexpr	char* CAMERASHAKE = "CAMERA SHAKE";
	struct	CAMERASHAKEDATA
	{
		float	shakeTimer;
		float	shakePower;
	};


	static constexpr char* CINEMATIC_EVENT_TRIGGER = "CINEMATIC_EVENT_TRIGGER";

	struct CINEMATIC_EVENT_TRIGGER_DATA
	{
		std::string eventName;
		std::string eventCategory;
		std::string payloadType;
		std::string payloadJson;
	};

};
