#pragma once
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderQueue.h"
#include <Engine\EngineTime.h>

// エンジンの実行レイヤー基底クラス
class Layer
{
public:
    virtual ~Layer() = default;

    virtual void Initialize() {}
    virtual void Finalize() {}

    // ロジックの更新
    virtual void Update(const EngineTime& time) {}

    // 描画伝票（RenderPacket）の提出など
    virtual void Render(RenderContext& rc, RenderQueue& queue) {}

    // ImGuiによるUI描画
    virtual void RenderUI() {}
};