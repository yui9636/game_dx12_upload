#pragma once

#include "Sprite/Sprite.h" 

class	HeadUpDisplay
{
public:
	HeadUpDisplay();
	~HeadUpDisplay();

	void Update(float dt);

	void Render(ID3D11DeviceContext* dc);

private:
	void OnLockOn(void* data);

	void OnLockOff(void* data);

private:
	Sprite*				lockonCursol	= nullptr;
	float				lockonTimer		= -1;
	float				lockonDirection	= 0;
	float				lockonTimerMax	= 8;
	DirectX::XMFLOAT2	lockonPosition	= { 0, 0 };

	uint64_t			CAMERACHANGEFREEMODEKEY;
	uint64_t			CAMERACHANGELOCKONMODEKEY;
	uint64_t			CAMERACHANGEMOTIONMODEKEY;
};
