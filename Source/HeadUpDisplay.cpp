#include	"HeadUpDisplay.h"

#include	<string.h>
#include	"Message/MessageData.h"
#include	"Message/Messenger.h"

#include	"Camera/Camera.h"
#include	"Graphics.h"
#include	"RenderContext/RenderContext.h"
#include	"Sprite/SpriteRenderer.h"

HeadUpDisplay::HeadUpDisplay()
{
	lockonCursol = std::make_shared<Sprite>("Data/Sprite/LockonCursol.png");

	CAMERACHANGEFREEMODEKEY		= Messenger::Instance().AddReceiver(MessageData::CAMERACHANGEFREEMODE, [&](void* data){ OnLockOff(data); });
	CAMERACHANGELOCKONMODEKEY	= Messenger::Instance().AddReceiver(MessageData::CAMERACHANGELOCKONMODE, [&](void* data){ OnLockOn(data); });
	CAMERACHANGEMOTIONMODEKEY	= Messenger::Instance().AddReceiver(MessageData::CAMERACHANGEMOTIONMODE, [&](void* data){ OnLockOff(data); });
}

HeadUpDisplay::~HeadUpDisplay()
{
	Messenger::Instance().RemoveReceiver(CAMERACHANGEFREEMODEKEY);
	Messenger::Instance().RemoveReceiver(CAMERACHANGELOCKONMODEKEY);
	Messenger::Instance().RemoveReceiver(CAMERACHANGEMOTIONMODEKEY);
}

void HeadUpDisplay::Update(float dt)
{
	{
		lockonTimer	+= lockonDirection * dt * 60;
		if( lockonTimer <= 0 )
			lockonTimer = 0;
		if( lockonTimer >= lockonTimerMax )
			lockonTimer = lockonTimerMax;
	}
}

void HeadUpDisplay::Render(const RenderContext& rc)
{
	(void)rc;

	if( lockonTimer > 0 && lockonCursol )
	{
		float	cursolWidth		= static_cast<float>(  lockonCursol->GetTextureWidth() ) * 0.25f;
		float	cursolHeight	= static_cast<float>( lockonCursol->GetTextureHeight() ) * 0.25f;
		float	halfHeight		= static_cast<float>( lockonCursol->GetTextureHeight() ) * 0.25f;
		float	alphaValue		= lockonTimer / lockonTimerMax;
		float	sideValue		= 32 + 128 * ( 1 - alphaValue );
		SpriteRenderer::Instance().Draw(*lockonCursol,
			lockonPosition.x - sideValue, lockonPosition.y - halfHeight, cursolWidth, cursolHeight,
			0, 0, static_cast<float>(lockonCursol->GetTextureWidth()), static_cast<float>(lockonCursol->GetTextureHeight()),
			0,
			{ 1, 1, 1, alphaValue });

		SpriteRenderer::Instance().Draw(*lockonCursol,
			lockonPosition.x + sideValue, lockonPosition.y - halfHeight, cursolWidth, cursolHeight,
			0, 0, static_cast<float>(lockonCursol->GetTextureWidth()), static_cast<float>(lockonCursol->GetTextureHeight()),
			DirectX::XM_PI,
			{ 1, 1, 1, alphaValue });


	}
}

void HeadUpDisplay::OnLockOn(void* data)
{
	MessageData::CAMERACHANGELOCKONMODEDATA*	p	= static_cast<MessageData::CAMERACHANGELOCKONMODEDATA*>(data);

	DirectX::XMMATRIX	vm	= DirectX::XMLoadFloat4x4( &Camera::Instance().GetView() );
	DirectX::XMMATRIX	pm	= DirectX::XMLoadFloat4x4( &Camera::Instance().GetProjection() );
	DirectX::XMVECTOR	wp	= DirectX::XMLoadFloat3( &p->target );
	DirectX::XMFLOAT3	sp;
	DirectX::XMStoreFloat3( &sp, DirectX::XMVector3TransformCoord( wp, vm * pm ) );

	lockonPosition.x	= Graphics::Instance().GetScreenWidth() * ( +sp.x * 0.5f + 0.5f );
	lockonPosition.y	= Graphics::Instance().GetScreenHeight() * ( -sp.y * 0.5f + 0.5f );
	lockonDirection	= +1;
}

void HeadUpDisplay::OnLockOff(void* data)
{
	lockonDirection	= -1;
}
