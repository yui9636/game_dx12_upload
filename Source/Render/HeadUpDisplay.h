#pragma once

#include "Sprite/Sprite.h"
#include "UI/UIElement.h"
#include <memory>

struct RenderContext;

class	HeadUpDisplay : public UIElement
{
public:
	HeadUpDisplay();
	~HeadUpDisplay() override;

	// lock-on cursor の時間変化などを更新する。
	void Update(float dt) override;

	// HUD sprite を描画する。
	void Render(const RenderContext& rc) override;

private:
	// lock-on 開始イベントを受け取り、cursor 表示を開始する。
	void OnLockOn(void* data);

	// lock-on 終了イベントを受け取り、cursor 表示を止める。
	void OnLockOff(void* data);

private:
	std::shared_ptr<Sprite> lockonCursol; // lock-on cursor sprite。
	float				lockonTimer		= -1; // 表示中の残り時間。負値なら非表示。
	float				lockonDirection	= 0;  // cursor の開閉アニメーション方向。
	float				lockonTimerMax	= 8;  // lock-on cursor 表示時間。
	DirectX::XMFLOAT2	lockonPosition	= { 0.5f, 0.5f }; // screen 正規化座標。

	uint64_t			CAMERACHANGEFREEMODEKEY;   // free camera 切替イベント key。
	uint64_t			CAMERACHANGELOCKONMODEKEY; // lock-on camera 切替イベント key。
	uint64_t			CAMERACHANGEMOTIONMODEKEY; // motion camera 切替イベント key。
};
