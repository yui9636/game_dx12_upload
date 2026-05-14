#pragma once
#include "RHI/PipelineStateDesc.h"
// 作成済みパイプラインの設定内容を RHI 共通の形で参照する。
class IPipelineState {
public:
    virtual ~IPipelineState() = default;

    virtual const PipelineStateDesc& GetDesc() const = 0;
};
