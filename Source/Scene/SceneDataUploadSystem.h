#pragma once
#include "RenderContext/RenderContext.h"
#include "Render/GlobalRootSignature.h"
// SceneDataUploadSystem は対象コンポーネントを走査し、対応する実行時更新を担当する。

class SceneDataUploadSystem {
public:
    void Upload(const RenderContext& rc, GlobalRootSignature& rootSig);
};
