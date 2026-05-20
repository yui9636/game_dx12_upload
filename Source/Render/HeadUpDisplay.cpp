#include	"Render/HeadUpDisplay.h"

#include	<string.h>
#include	"Message/MessageData.h"
#include	"Message/Messenger.h"

#include	"Camera/Camera.h"
#include	"Render/Graphics.h"
#include	"RenderContext/RenderContext.h"
#include	"Sprite/SpriteRenderer.h"

#include <algorithm>

HeadUpDisplay::HeadUpDisplay()
{
	// lock-on cursor sprite と camera mode change message を登録する。
	lockonCursol = std::make_shared<Sprite>("Data/Texture/UI/lockoncursor.png");

	CAMERACHANGEFREEMODEKEY		= Messenger::Instance().AddReceiver(MessageData::CAMERACHANGEFREEMODE, [&](void* data){ OnLockOff(data); });
	CAMERACHANGELOCKONMODEKEY	= Messenger::Instance().AddReceiver(MessageData::CAMERACHANGELOCKONMODE, [&](void* data){ OnLockOn(data); });
	CAMERACHANGEMOTIONMODEKEY	= Messenger::Instance().AddReceiver(MessageData::CAMERACHANGEMOTIONMODE, [&](void* data){ OnLockOff(data); });
}

HeadUpDisplay::~HeadUpDisplay()
{
	// Messenger に保持された callback が破棄済み HUD を呼ばないよう解除する。
	Messenger::Instance().RemoveReceiver(CAMERACHANGEFREEMODEKEY);
	Messenger::Instance().RemoveReceiver(CAMERACHANGELOCKONMODEKEY);
	Messenger::Instance().RemoveReceiver(CAMERACHANGEMOTIONMODEKEY);
}

void HeadUpDisplay::Update(float dt)
{
	{
		// lockonDirection に応じて表示 alpha 用 timer を増減させる。
		lockonTimer	+= lockonDirection * dt * 60;
		if( lockonTimer <= 0 )
			lockonTimer = 0;
		if( lockonTimer >= lockonTimerMax )
			lockonTimer = lockonTimerMax;
	}
}

void HeadUpDisplay::Render(const RenderContext& rc)
{
	if( lockonTimer > 0 && lockonCursol )
	{
		// editor/game viewport のどちらでも使えるよう、RenderContext の表示サイズを優先する。
		float viewportW = static_cast<float>(rc.displayWidth);
		float viewportH = static_cast<float>(rc.displayHeight);
		if (viewportW <= 0.0f) viewportW = rc.mainViewport.width;
		if (viewportH <= 0.0f) viewportH = rc.mainViewport.height;
		if (viewportW <= 0.0f) viewportW = Graphics::Instance().GetScreenWidth();
		if (viewportH <= 0.0f) viewportH = Graphics::Instance().GetScreenHeight();
		viewportW = (std::max)(1.0f, viewportW);
		viewportH = (std::max)(1.0f, viewportH);

		const float centerX = viewportW * lockonPosition.x;
		const float centerY = viewportH * lockonPosition.y;
		// 左右 2 枚の sprite を向かい合わせに描き、lock-on 収束演出にする。
		float	cursolWidth		= static_cast<float>(  lockonCursol->GetTextureWidth() ) * 0.25f;
		float	cursolHeight	= static_cast<float>( lockonCursol->GetTextureHeight() ) * 0.25f;
		float	halfHeight		= static_cast<float>( lockonCursol->GetTextureHeight() ) * 0.25f;
		float	alphaValue		= lockonTimer / lockonTimerMax;
		float	sideValue		= 32 + 128 * ( 1 - alphaValue );
		SpriteRenderer::Instance().Draw(*lockonCursol,
			centerX - sideValue, centerY - halfHeight, cursolWidth, cursolHeight,
			0, 0, static_cast<float>(lockonCursol->GetTextureWidth()), static_cast<float>(lockonCursol->GetTextureHeight()),
			0,
			{ 1, 1, 1, alphaValue });

		SpriteRenderer::Instance().Draw(*lockonCursol,
			centerX + sideValue, centerY - halfHeight, cursolWidth, cursolHeight,
			0, 0, static_cast<float>(lockonCursol->GetTextureWidth()), static_cast<float>(lockonCursol->GetTextureHeight()),
			DirectX::XM_PI,
			{ 1, 1, 1, alphaValue });


	}
}

void HeadUpDisplay::OnLockOn(void* data)
{
	// target world position を screen 正規化座標へ変換し、cursor 表示を開始する。
	MessageData::CAMERACHANGELOCKONMODEDATA*	p	= static_cast<MessageData::CAMERACHANGELOCKONMODEDATA*>(data);

	DirectX::XMMATRIX	vm	= DirectX::XMLoadFloat4x4( &Camera::Instance().GetView() );
	DirectX::XMMATRIX	pm	= DirectX::XMLoadFloat4x4( &Camera::Instance().GetProjection() );
	DirectX::XMVECTOR	wp	= DirectX::XMLoadFloat3( &p->target );
	DirectX::XMFLOAT3	sp;
	DirectX::XMStoreFloat3( &sp, DirectX::XMVector3TransformCoord( wp, vm * pm ) );

	lockonPosition.x	= +sp.x * 0.5f + 0.5f;
	lockonPosition.y	= -sp.y * 0.5f + 0.5f;
	lockonDirection	= +1;
}

void HeadUpDisplay::OnLockOff(void* data)
{
	// timer を減少方向へ倒し、fade out させる。
	lockonDirection	= -1;
}
