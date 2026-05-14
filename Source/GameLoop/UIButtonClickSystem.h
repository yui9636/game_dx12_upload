#pragma once

#include <DirectXMath.h>

class Registry;
class UIButtonClickEventQueue;
class InputEventQueue;

// 2D UI Button のクリックを検出し、event queue へ積む。
// view / projection は EngineKernel が main camera から作成して渡す。
class UIButtonClickSystem
{
public:
    static void ResetCapture();

    static void Update(
        Registry&                  gameRegistry,
        UIButtonClickEventQueue&   outQueue,
        const InputEventQueue&     inputQueue,
        const DirectX::XMFLOAT4&   gameViewRect,
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection);
};
