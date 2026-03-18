#pragma once
#include "RHI/PipelineStateDesc.h"

// ==========================================
// コンパイル済みのパイプラインステート（Immutable）
// ==========================================
class IPipelineState {
public:
    virtual ~IPipelineState() = default;

    // 生成に使用された定義を取得（内部でのバインド展開やハッシュ計算に使用）
    virtual const PipelineStateDesc& GetDesc() const = 0;
};