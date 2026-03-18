#include "UIDamagePopup.h"
#include "Font/FontManager.h"
#include <cmath>
#include <cstdlib>
#include "RHI/ICommandList.h"

using namespace DirectX;

UIDamagePopup::UIDamagePopup()
{
    isActive = false;
    // 親クラスの sprite は nullptr のままでOK
}

void UIDamagePopup::Setup(const DirectX::XMFLOAT3& pos, int damage)
{
    isActive = true;
    position = pos; // 親クラスのメンバにセット
    damageValue = damage;
    lifeTime = 0.0f;
    maxLifeTime = 0.8f;

    // 散らし処理
    float randX = ((float)rand() / RAND_MAX - 0.5f) * 2.5f;
    float randZ = ((float)rand() / RAND_MAX - 0.5f) * 2.5f;
    float randY = ((float)rand() / RAND_MAX) * 2.0f + 4.5f;

    velocity = { randX, randY, randZ };

    // 初期ズレ
    position.x += randX * 0.5f;
    position.y += ((float)rand() / RAND_MAX);
    position.z += randZ * 0.5f;
}

void UIDamagePopup::Update(float dt)
{
    if (!isActive) return;

    lifeTime += dt;
    if (lifeTime >= maxLifeTime)
    {
        isActive = false;
        return;
    }
}

void UIDamagePopup::Render(const RenderContext& rc)
{
    if (!isActive) return;

    // ★親クラスの WorldToScreen を使って座標変換
    XMFLOAT3 screenPos;
    if (!WorldToScreen(rc, screenPos))
    {
        return; // 画面外
    }

    // フェードアウト
    float alpha = 1.0f;
    if (lifeTime > maxLifeTime * 0.5f)
    {
        float t = (lifeTime - maxLifeTime * 0.5f) / (maxLifeTime * 0.5f);
        alpha = 1.0f - t;
    }

    XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, alpha };
    float drawScale = 0.5f;

    // フォント描画
    FontManager::Instance().DrawFormat(
        rc.commandList->GetNativeContext(),
        "ComboFont",
        screenPos.x, screenPos.y,
        color,
        drawScale,
        FontAlign::Center,
        L"%d",
        damageValue
    );
}