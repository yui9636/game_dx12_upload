#pragma once

#include <string>
#include <DirectXMath.h>
#include "Entity/Entity.h"

struct EffectAttachmentComponent
{
    EntityID parentEntity = Entity::NULL_ID;
    std::string socketName;
    DirectX::XMFLOAT3 offsetLocal = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetRotDeg = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 offsetScale = { 1.0f, 1.0f, 1.0f };

    // CharacterTrailEffects 仕様に基づく速度連動 modulation。
    // 親 socket の world velocity に応じて spawnRate、size、alpha を変化させる。
    // 加算形式で packet.spawnRate に velocitySpawnRateAdd * modulator を足す。
    // base が 0 でも速度駆動の afterimage 発生を可能にする。
    bool   velocityModulateEnabled = false;
    float  velocitySpeedRef        = 5.8f;   // engine unit/sec。LocomotionStateComponent::runMaxSpeed と合わせる。
    float  velocitySpawnRateAdd    = 0.0f;   // |v| が基準速度のときに秒間 +N spawn する。
    float  velocityWidthAdd        = 0.0f;   // |v| が基準速度のときに開始サイズへ Δ を足す。
    float  velocityAlphaAdd        = 0.0f;   // |v| が基準速度のときに alpha へ [0..1] の Δ を足す。
    float  velocityModulatorMax    = 3.0f;   // teleport や初期化直後の spike を抑えるため |v|/ref を clamp する。

    // EffectAttachmentSystem が書き込む実行時 cache。
    bool              velocityInitialized = false;
    DirectX::XMFLOAT3 prevWorldPos        = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 worldVelocity       = { 0.0f, 0.0f, 0.0f };
    float             worldSpeed          = 0.0f;
};
