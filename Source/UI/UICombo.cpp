#include "UICombo.h"
#include "Font/FontManager.h"
#include "Graphics.h"
#include "Sprite/Sprite.h" 
#include <cmath>
#include <cstdlib>
#include "RHI/ICommandList.h"

using namespace DirectX;

UICombo::UICombo()
{
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
            displayTimer = MAX_DISPLAY_TIME;

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

    float alpha = 1.0f;
    if (displayTimer < 0.5f) alpha = displayTimer / 0.5f;
    XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, alpha };

    float shakeX = 0.0f;
    float shakeY = 0.0f;
    if (shakeTimer > 0.0f)
    {
        float power = 8.0f * (shakeTimer / SHAKE_DURATION);
        shakeX = ((float)rand() / RAND_MAX - 0.5f) * power;
        shakeY = ((float)rand() / RAND_MAX - 0.5f) * power;
    }

    float drawX = position.x + shakeX;
    float drawY = position.y + shakeY;

    // ---------------------------------------------------
    // ---------------------------------------------------
    float numberScale = 1.5f;

    FontManager::Instance().DrawFormat(
        rc.commandList,
        "ComboFont",
        drawX, drawY,
        color,
        numberScale,
        FontAlign::Right,
        L"%d", currentCombo
    );

    // ---------------------------------------------------
    // ---------------------------------------------------
    FontManager::Instance().DrawFormat(
        rc.commandList,
        "ComboFont",
        drawX + 5.0f, drawY + 60.0f,
        { 1.0f, 1.0f, 1.0f, alpha * 0.8f },
        0.4f,
        FontAlign::Left,
        L"Combo"
    );

    // ---------------------------------------------------
    // ---------------------------------------------------
    if (gaugeSprite)
    {
        float gaugeWidth = 150.0f * (displayTimer / MAX_DISPLAY_TIME);
        float gaugeHeight = 5.0f;

        float barX = drawX - gaugeWidth;
        float barY = drawY + 100.0f;

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
