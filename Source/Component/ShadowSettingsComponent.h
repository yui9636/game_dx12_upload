#pragma once
#include <DirectXMath.h>

struct ShadowSettingsComponent {
    bool enableShadow = true;
    DirectX::XMFLOAT3 shadowColor = { 0.1f, 0.1f, 0.1f };
    // «—ˆ“I‚É•ªŠ„‹——£‚â‰ğ‘œ“x‚ğ‚±‚±‚É“ü‚ê‚é‚±‚Æ‚à‰Â”\
};