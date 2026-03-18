#pragma once
#include "FrameGraphTypes.h"

class FrameGraph;
class ITexture;

// ============================================================
// FrameGraphResources: Execute フェーズでハンドル→物理テクスチャを解決
// ============================================================
class FrameGraphResources {
public:
    FrameGraphResources(FrameGraph& graph) : m_graph(graph) {}

    ITexture* GetTexture(ResourceHandle handle);

private:
    FrameGraph& m_graph;
};
