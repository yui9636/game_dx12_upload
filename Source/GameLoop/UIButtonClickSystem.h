#pragma once

#include <DirectXMath.h>

class Registry;
class UIButtonClickEventQueue;
class InputEventQueue;

// 2D UI Button の click を検出し、UIButtonClickEventQueue に Push する。
// view/projection は呼び出し側 (EngineKernel) が main camera から構築する。
class UIButtonClickSystem
{
public:
    static void Update(
        Registry&                  gameRegistry,
        UIButtonClickEventQueue&   outQueue,
        const InputEventQueue&     inputQueue,
        const DirectX::XMFLOAT4&   gameViewRect,
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection);
};
