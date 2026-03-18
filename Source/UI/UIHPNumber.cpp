#include "UIHPNumber.h"
#include "Font/FontManager.h"
#include "Camera/Camera.h"
#include "RHI/ICommandList.h"

using namespace DirectX;

UIHPNumber::UIHPNumber()
{
}

// ★エラーの原因: この関数の実装が消えていました
void UIHPNumber::SetHP(int current, int max)
{
    currentHP = current;
    maxHP = max;
}

void UIHPNumber::Render(const RenderContext& rc)
{
    if (!visible) return;

    Camera& cam = Camera::Instance();
    XMMATRIX view = XMLoadFloat4x4(&cam.GetView());
    XMMATRIX projection = XMLoadFloat4x4(&cam.GetProjection());

    // 2. 座標とパラメータの設定 (ご提示のコード準拠)
    XMFLOAT3 drawPos = position;


    XMFLOAT4 textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float scale = 0.055f;

    // 3. FontManagerへ描画指示
    // 修正した DrawFormat3D (dc, view, proj を直接受け取る版) を使用
    FontManager::Instance().DrawFormat3D(
        rc.commandList->GetNativeContext(),  // DC
        view,              // View行列
        projection,        // Projection行列
        "ComboFont",       // フォントキー
        drawPos,           // 座標
        rotation,          // 回転 (このクラスが持つ回転を適用)
        scale,             // サイズ
        textColor,         // 色
        FontAlign::Center, // 中央揃え
        L"%d / %d",        // 書式
        currentHP, maxHP   // 値
    );

}