#pragma once

#include <DirectXMath.h>
#include <string>

#include "Entity/Entity.h"
// HPGaugeTargetMode はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

enum class HPGaugeTargetMode
{
    Explicit = 0,
    FirstPlayer = 1,
    FirstEnemy = 2,
    FirstBoss = 3
};

enum class HPGaugeFillDirection
{
    LeftToRight = 0,
    RightToLeft = 1,
    BottomToTop = 2,
    TopToBottom = 3
};

enum class HPGaugeColorMode
{
    Fixed = 0,
    Threshold = 1,
    Gradient = 2
};

enum class HPGaugeTextFormat
{
    CurrentMax = 0,
    CurrentOnly = 1,
    Percent = 2,
    LabelCurrentMax = 3
};

struct HPGaugeBindingComponent
{
    HPGaugeTargetMode targetMode = HPGaugeTargetMode::FirstPlayer;
    EntityID explicitTarget = Entity::NULL_ID;

    bool visibleWhenNoTarget = false;
    bool hideWhenDead = true;
    bool hideWhenFull = false;

    float smoothingSpeed = 12.0f;
    float damagePreviewDelay = 0.35f;
    float damagePreviewSpeed = 6.0f;

    EntityID resolvedTarget = Entity::NULL_ID;
    bool targetValid = false;
    int currentHP = 0;
    int maxHP = 0;
    float targetRatio = 1.0f;
    float displayedRatio = 1.0f;
    float delayedRatio = 1.0f;
    float damagePreviewTimer = 0.0f;
};

struct HPGaugeFillComponent
{
    EntityID bindingRoot = Entity::NULL_ID;
    HPGaugeFillDirection fillDirection = HPGaugeFillDirection::LeftToRight;
    HPGaugeColorMode colorMode = HPGaugeColorMode::Threshold;

    bool useDisplayedRatio = true;
    bool useDelayedRatio = false;
    bool hideWhenNoTarget = false;
    bool hideWhenFull = false;

    float minVisibleRatio = 0.0f;
    float runtimeRatio = 1.0f;

    DirectX::XMFLOAT4 fixedColor = { 0.18f, 0.86f, 0.36f, 0.94f };
    DirectX::XMFLOAT4 highColor = { 0.18f, 0.86f, 0.36f, 0.94f };
    DirectX::XMFLOAT4 midColor = { 0.95f, 0.75f, 0.18f, 0.94f };
    DirectX::XMFLOAT4 lowColor = { 0.95f, 0.16f, 0.12f, 0.94f };
    float midThreshold = 0.50f;
    float lowThreshold = 0.25f;
};

struct HPGaugeTextComponent
{
    EntityID bindingRoot = Entity::NULL_ID;
    HPGaugeTextFormat format = HPGaugeTextFormat::CurrentMax;
    std::string label = "HP";

    bool hideWhenNoTarget = true;
    bool hideWhenDead = false;
    bool hideWhenFull = false;
};
