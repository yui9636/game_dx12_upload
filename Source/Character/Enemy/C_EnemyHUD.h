#pragma once
#include "Component/Component.h"
#include <memory>
#include <string>

// 前方宣言
class UIProgressBar2D;
class Sprite;

class C_EnemyHUD : public Component
{
public:
    C_EnemyHUD();
    ~C_EnemyHUD() override;

    // ★必須: Componentの純粋仮想関数を実装
    const char* GetName() const override { return "EnemyHUD"; }

    void Start() override;
    void Update(float dt) override;

    // ★修正: Component.h に合わせて引数なしに変更
    void Render() override;

    // ボス名設定
    void SetBossName(const std::string& name) { bossName = name; }


    void Finalize();
private:
    std::shared_ptr<UIProgressBar2D> redBar;
    std::shared_ptr<UIProgressBar2D> whiteBar;
    std::shared_ptr<UIProgressBar2D> grayBar;
    std::shared_ptr<Sprite> barSprite;

    std::string bossName = "UNKNOWN CONSTRUCT";
    int totalStocks = 1;
    float hpPerStock = 0.0f;

    float delayedHP = 0.0f;
    float delayTimer = 0.0f;
    const float DELAY_TIME = 0.5f;
    const float LERP_SPEED = 5.0f;

    void UpdateBars(float currentHP, float maxHP);
};