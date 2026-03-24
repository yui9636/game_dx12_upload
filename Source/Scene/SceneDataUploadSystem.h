#pragma once
#include "RenderContext/RenderContext.h"
#include "Render/GlobalRootSignature.h"

class SceneDataUploadSystem {
public:
    void Upload(const RenderContext& rc, GlobalRootSignature& rootSig);
};
