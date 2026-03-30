#pragma once

#include <string>

#include <DirectXMath.h>

#include "Component/AudioEmitterComponent.h"

struct AudioOneShotRequestComponent
{
    std::string clipAssetPath;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool is3D = false;
    DirectX::XMFLOAT3 worldPosition = { 0.0f, 0.0f, 0.0f };
    AudioBusType bus = AudioBusType::SFX;
    int lifetimeFrames = 1;
    bool loop = false;
    bool streaming = false;
    float minDistance = 1.0f;
    float maxDistance = 50.0f;
};
