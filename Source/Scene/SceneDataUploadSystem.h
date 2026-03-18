#pragma once
#include "RenderContext/RenderContext.h"
#include "Render/GlobalRootSignature.h"

class SceneDataUploadSystem {
public:
    // 毎フレーム1回だけ呼ばれ、CPUの計算とGPUへの転送を一手に引き受ける
    void Upload(const RenderContext& rc, GlobalRootSignature& rootSig);
};