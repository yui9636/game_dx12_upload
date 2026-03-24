#pragma once
#include "FrameGraphTypes.h"

class FrameGraph;
class ITexture;

// ============================================================
// ============================================================
class FrameGraphResources {
public:
    FrameGraphResources(FrameGraph& graph) : m_graph(graph) {}

    ITexture* GetTexture(ResourceHandle handle);

private:
    FrameGraph& m_graph;
};
