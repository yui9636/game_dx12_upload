#pragma once

#include "Sprite/Sprite.h" 

//	画面表示情報
class	HeadUpDisplay
{
public:
	HeadUpDisplay();
	~HeadUpDisplay();

	// 更新処理
	void Update(float dt);

	// 描画処理
	void Render(ID3D11DeviceContext* dc);

private:
	// ロックオンカーソル描画処理判定
	void OnLockOn(void* data);

	// ロックオンカーソル描画処理判定
	void OnLockOff(void* data);

private:
	Sprite*				lockonCursol	= nullptr;
	float				lockonTimer		= -1;
	float				lockonDirection	= 0;
	float				lockonTimerMax	= 8;
	DirectX::XMFLOAT2	lockonPosition	= { 0, 0 };

	// メッセージキー
	uint64_t			CAMERACHANGEFREEMODEKEY;
	uint64_t			CAMERACHANGELOCKONMODEKEY;
	uint64_t			CAMERACHANGEMOTIONMODEKEY;
};
