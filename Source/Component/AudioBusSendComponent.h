// AudioBusSendComponent の ECS コンポーネント定義をまとめます。
#pragma once

#include "Component/AudioEmitterComponent.h"

struct AudioBusSendComponent
{
    AudioBusType bus = AudioBusType::SFX;
    float sendVolume = 1.0f;
};
