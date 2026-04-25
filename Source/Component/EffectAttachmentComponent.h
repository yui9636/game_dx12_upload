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

    // ---- Velocity Modulation (CharacterTrailEffects_Spec_2026-04-25) ----
    // Drives spawnRate / size / alpha based on the parent socket's world velocity.
    // Additive form: packet.spawnRate += velocitySpawnRateAdd * modulator,
    // so a base of 0 still allows velocity-driven emission (Afterimage).
    bool   velocityModulateEnabled = false;
    float  velocitySpeedRef        = 5.8f;   // engine unit/sec; align with LocomotionStateComponent::runMaxSpeed.
    float  velocitySpawnRateAdd    = 0.0f;   // +N spawn/sec at |v| == ref
    float  velocityWidthAdd        = 0.0f;   // +Δstart size at |v| == ref
    float  velocityAlphaAdd        = 0.0f;   // +Δalpha [0..1] at |v| == ref
    float  velocityModulatorMax    = 3.0f;   // clamp on |v|/ref to suppress teleport/init spikes.

    // Runtime cache (written by EffectAttachmentSystem).
    bool              velocityInitialized = false;
    DirectX::XMFLOAT3 prevWorldPos        = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 worldVelocity       = { 0.0f, 0.0f, 0.0f };
    float             worldSpeed          = 0.0f;
};
