#pragma once

#include <DirectXMath.h>

class Registry;
class UIButtonClickEventQueue;
class InputEventQueue;

// Detect 2D UI Button clicks and push them into the event queue.
// view/projection are passed in (built from the main camera by EngineKernel).
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
