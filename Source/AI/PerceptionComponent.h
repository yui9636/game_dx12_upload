#pragma once

// AI の視覚・聴覚検知パラメータを保持するコンポーネント定義。
#include <cstdint>

// AI がターゲットを検知するための視覚・聴覚設定。
struct PerceptionComponent
{
    // 視覚検知を有効にするか。
    bool  sightEnabled        = true;
    // 視覚検知できる半径。
    float sightRadius         = 10.0f;
    // 視覚検知の視野角。
    float sightFOV            = 1.5708f;
    // 視線判定に使う目線の高さ。
    float sightHeight         = 1.6f;
    // 遮蔽物チェックを要求するか。
    bool  requireLineOfSight  = false;

    // 聴覚検知を有効にするか。
    bool  hearingEnabled      = false;
    // 聴覚検知できる半径。
    float hearingRadius       = 6.0f;

    // 検知対象にする陣営マスク。
    uint16_t targetFactionMask = 0;
};
