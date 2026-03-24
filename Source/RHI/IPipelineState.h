#pragma once
#include "RHI/PipelineStateDesc.h"

// ==========================================
// ==========================================
class IPipelineState {
public:
    virtual ~IPipelineState() = default;

    virtual const PipelineStateDesc& GetDesc() const = 0;
};
