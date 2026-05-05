#pragma once

// AI が現在狙っている対象とヘイト状態を保持するコンポーネント定義。

#include "Entity/Entity.h"

// AI の現在ターゲットとヘイト状態を保持するコンポーネント。
struct AggroComponent
{
    // 現在狙っているターゲット Entity。
    EntityID currentTarget   = Entity::NULL_ID;
    // 現在ターゲットへのヘイト量。
    float    threat          = 0.0f;
    // 最後に視認してからの経過時間。
    float    timeSinceSighted = 0.0f;
    // この秒数以上見失ったらターゲットを解除する。
    float    loseTargetAfter  = 5.0f;
};
