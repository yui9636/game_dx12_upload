#pragma once
#include "UIWorld.h"

// ============================================================================
// UIProgressBar3D
// 
// 概要: 3D空間(World Space)配置のプログレスバー
// 用途: プレイヤーの斜めHPバー、敵頭上のHPバー
// 挙動: シェーダーによるUVカットを行い、3Dパースペクティブ環境下で正しく表示します
// ============================================================================
class UIProgressBar3D : public UIWorld
{
public:
    UIProgressBar3D();
    virtual ~UIProgressBar3D() = default;

    // 描画オーバーライド
    void Render(const RenderContext& rc) override;

    // 進行度設定 (0.0f ～ 1.0f)
    void SetProgress(float v);
    float GetProgress() const { return progress; }

    // 背景スプライト設定 (HPバーの下地)
    void SetBackgroundSprite(std::shared_ptr<Sprite3D> sprite);

    // 背景色の設定 (デフォルトは半透明の黒など)
    void SetBackgroundColor(float r, float g, float b, float a) { backgroundColor = { r, g, b, a }; }

private:
    std::shared_ptr<Sprite3D> backgroundSprite;
    DirectX::XMFLOAT4 backgroundColor;
};