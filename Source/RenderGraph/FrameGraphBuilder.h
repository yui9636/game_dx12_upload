#pragma once
#include <string>
#include "FrameGraphTypes.h"

// ============================================================
// ============================================================
class FrameGraphBuilder {
public:
    virtual ~FrameGraphBuilder() = default;

    virtual ResourceHandle CreateTexture(const std::string& name, const TextureDesc& desc) = 0;

    virtual ResourceHandle Read(ResourceHandle input) = 0;

    virtual ResourceHandle Write(ResourceHandle input) = 0;

    virtual void RegisterHandle(const std::string& name, ResourceHandle handle) = 0;

    virtual ResourceHandle GetHandle(const std::string& name) const = 0;
};
