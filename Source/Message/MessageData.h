#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>

// カメラモーション用の1キー分の情報をまとめる構造体。
// 指定時間におけるカメラ位置と注視点を保持する。
struct CameraMotionData
{
	// このキーに到達する時間。
	float time;

	// カメラ本体の位置。
	DirectX::XMFLOAT3 position;

	// カメラが見る対象位置。
	DirectX::XMFLOAT3 target;
};

// メッセンジャーでやり取りするメッセージ名と、各メッセージに渡すデータ型をまとめる名前空間。
namespace	MessageData
{
	// カメラを自由操作モードへ切り替える通知名。
	static	constexpr	char* CAMERACHANGEFREEMODE = "CAMERA CHANGE FREEMODE";

	// 自由操作モードへ切り替えるときに渡すデータ。
	struct	CAMERACHANGEFREEMODEDATA
	{
		// 切り替え後に見る基準位置。
		DirectX::XMFLOAT3	target;
	};

	// カメラをロックオンモードへ切り替える通知名。
	static	constexpr	char* CAMERACHANGELOCKONMODE = "CAMERA CHANGE LOCKONMODE";

	// ロックオンモードへ切り替えるときに渡すデータ。
	struct	CAMERACHANGELOCKONMODEDATA
	{
		// カメラ遷移開始時の位置。
		DirectX::XMFLOAT3	start;

		// ロックオン対象、または注視したい位置。
		DirectX::XMFLOAT3	target;
	};

	// カメラを追従モードへ切り替える通知名。
	static constexpr char* CAMERACHANGEFOLLOWMODE = "CAMERA CHANGE FOLLOWMODE";

	// 追従モードへ切り替えるときに渡すデータ。
	struct CAMERACHANGEFOLLOWMODEDATA 
	{
		// 追従する対象位置。
		DirectX::XMFLOAT3	target;

	};


	// カメラを空中用モードへ切り替える通知名。
	static	constexpr	char* CAMERACHANGEARIELMODE = "CAMERA CHANGE ARIELMODE";

	// 空中用モードへ切り替えるときに渡すデータ。
	struct	CAMERACHANGEARIELMODEDATA
	{
		// カメラ遷移開始時の位置。
		DirectX::XMFLOAT3	start;

		// カメラが見る対象位置。
		DirectX::XMFLOAT3	target;

		// ロックオン中の対象位置。
		DirectX::XMFLOAT3	lockonTarget;
	};

	// カメラをモーション再生モードへ切り替える通知名。
	static	constexpr	char* CAMERACHANGEMOTIONMODE = "CAMERA CHANGE MOTIONMODE";

	// カメラモーション再生に必要なキー列を渡すデータ。
	struct	CAMERACHANGEMOTIONMODEDATA
	{
		// 時間ごとのカメラ位置と注視点の配列。
		std::vector<CameraMotionData>	data;
	};

	// カメラ揺れを発生させる通知名。
	static	constexpr	char* CAMERASHAKE = "CAMERA SHAKE";

	// カメラ揺れに必要な時間と強さを渡すデータ。
	struct	CAMERASHAKEDATA
	{
		// 揺れを継続する時間。
		float	shakeTimer;

		// 揺れの強さ。
		float	shakePower;
	};


	// シネマティック用イベントを発火する通知名。
	static constexpr char* CINEMATIC_EVENT_TRIGGER = "CINEMATIC_EVENT_TRIGGER";

	// シネマティックイベントへ渡す汎用データ。
	struct CINEMATIC_EVENT_TRIGGER_DATA
	{
		// 発火するイベント名。
		std::string eventName;

		// イベントの分類名。
		std::string eventCategory;

		// payloadJson の中身を解釈するための種類名。
		std::string payloadType;

		// イベント固有の追加情報を JSON 文字列として保持する。
		std::string payloadJson;
	};

};
