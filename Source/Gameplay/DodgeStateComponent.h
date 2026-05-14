#pragma once
#include <DirectXMath.h>

struct DodgeStateComponent {
    float dodgeMoveSpeed = 30.0f;       // 回避中の移動速度（単位/秒。以前は 6.0 * 5.0）
    float dodgeDuration = 0.4f;         // 回避の総時間（秒）
    float dodgeExitNormalized = 0.9f;   // 回避時間に対する終了位置
    float dodgeTimer = 0.0f;            // 現在の経過時間
    float dodgeAngleY = 0.0f;           // 回避中の向き（ラジアン）
    bool dodgeTriggered = false;        // PlayerInputSystem が立てる入力フラグ
};
