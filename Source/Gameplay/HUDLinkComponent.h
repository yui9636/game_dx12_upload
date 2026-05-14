#pragma once

// HealthComponent と UI 表示をつなぐ。HUDBindingSystem が毎フレーム読む。
// Inspector から簡単に編集できるよう、経路指定は単純な bool にしている。
struct HUDLinkComponent {
    bool  asPlayerHUD   = false; // 画面下のプレイヤー HP バー
    bool  asBossHUD     = false; // 画面上のボス HP バー
    bool  asWorldFloat  = true;  // 頭上に浮かぶ 3D HP ゲージ
    float worldOffsetY  = 2.0f;
};
