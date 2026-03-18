#include "UICombo.h"
#include "Font/FontManager.h"
#include "Graphics.h"
#include "Sprite/Sprite.h" 
#include <cmath>
#include <cstdlib> // rand用
#include "RHI/ICommandList.h"

using namespace DirectX;

UICombo::UICombo()
{
    // 配置位置調整
    //// Y座標を 300 -> 250 に上げて、少し上に移動
    //position = { 420.0f, 290.0f };

    //ID3D11Device* device = Graphics::Instance().GetDevice();
    //gaugeSprite = std::make_shared<Sprite>(device, "Data/Texture/UI/white0.png");


}

void UICombo::SetCombo(int count)
{
    if (currentCombo != count)
    {
        if (count > currentCombo)
        {
            // タイマーリセット
            displayTimer = MAX_DISPLAY_TIME;

            // 揺れ開始 (拡大アニメーションは削除)
            shakeTimer = SHAKE_DURATION;
        }
        currentCombo = count;
    }
}

void UICombo::Update(float dt)
{
    if (displayTimer > 0.0f) displayTimer -= dt;
    if (shakeTimer > 0.0f) shakeTimer -= dt;
}

void UICombo::Render(const RenderContext& rc)
{
    if (currentCombo <= 0 || displayTimer <= 0.0f) return;

    // フェードアウト
    float alpha = 1.0f;
    if (displayTimer < 0.5f) alpha = displayTimer / 0.5f;
    XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, alpha };

    // 揺れ計算 (数字が更新された時だけガガッと揺らす)
    float shakeX = 0.0f;
    float shakeY = 0.0f;
    if (shakeTimer > 0.0f)
    {
        float power = 8.0f * (shakeTimer / SHAKE_DURATION);
        shakeX = ((float)rand() / RAND_MAX - 0.5f) * power;
        shakeY = ((float)rand() / RAND_MAX - 0.5f) * power;
    }

    // 揺れを加味した描画原点
    float drawX = position.x + shakeX;
    float drawY = position.y + shakeY;

    // ---------------------------------------------------
    // 1. 数字 (メイン)
    // ---------------------------------------------------
    // サイズを 2.0 -> 1.7 に縮小
    // アニメーション(scaleAnim)は掛けず、固定サイズで描画
    float numberScale = 1.5f;

    FontManager::Instance().DrawFormat(
        rc.commandList->GetNativeContext(),
        "ComboFont",
        drawX, drawY,
        color,
        numberScale,
        FontAlign::Right,     // 右揃え
        L"%d", currentCombo
    );

    // ---------------------------------------------------
    // 2. "Combo" テキスト
    // ---------------------------------------------------
    // 数字の真ん中ではなく「下の方」に配置してスッキリさせる
    // Yオフセットを増やして(22 -> 40)、数字の底辺に合わせるイメージ
    FontManager::Instance().DrawFormat(
        rc.commandList->GetNativeContext(),
        "ComboFont",
        drawX + 5.0f, drawY + 60.0f, // 位置を下へ調整
        { 1.0f, 1.0f, 1.0f, alpha * 0.8f },
        0.4f,                // 小さめ維持
        FontAlign::Left,
        L"Combo"
    );

    // ---------------------------------------------------
    // 3. コンボ維持ゲージ
    // ---------------------------------------------------
    if (gaugeSprite)
    {
        float gaugeWidth = 150.0f * (displayTimer / MAX_DISPLAY_TIME);
        float gaugeHeight = 5.0f;

        // 数字の下、"Combo"文字の下あたりに配置
        float barX = drawX - gaugeWidth;
        float barY = drawY + 100.0f; // さらに下へ

        gaugeSprite->Render(
            rc.commandList->GetNativeContext(),
            barX, barY,
            0.0f,
            gaugeWidth, gaugeHeight,
            0.0f,
            1.0f, 1.0f, 1.0f, alpha
        );
    }
}