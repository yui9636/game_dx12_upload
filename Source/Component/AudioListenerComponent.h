// AudioListenerComponent の ECS コンポーネント定義をまとめます。
#pragma once

struct AudioListenerComponent
{
    bool isPrimary = true;
    float volumeScale = 1.0f;
};
