#pragma once

#include "Sprite/Sprite.h"
#include "UI/UIElement.h"
#include <memory>
// RenderContext はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

struct RenderContext;

class	HeadUpDisplay : public UIElement
{
public:
	HeadUpDisplay();
	~HeadUpDisplay() override;

	void Update(float dt) override;

	void Render(const RenderContext& rc) override;

private:
	void OnLockOn(void* data);

	void OnLockOff(void* data);

private:
	std::shared_ptr<Sprite> lockonCursol;
	float				lockonTimer		= -1;
	float				lockonDirection	= 0;
	float				lockonTimerMax	= 8;
	DirectX::XMFLOAT2	lockonPosition	= { 0.5f, 0.5f };

	uint64_t			CAMERACHANGEFREEMODEKEY;
	uint64_t			CAMERACHANGELOCKONMODEKEY;
	uint64_t			CAMERACHANGEMOTIONMODEKEY;
};
