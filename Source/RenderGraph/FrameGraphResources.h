#pragma once
#include "FrameGraphTypes.h"

class FrameGraph;
class ITexture;
// 実行時の ResourceHandle から実体テクスチャへ解決するビュー。
class FrameGraphResources {
public:
    FrameGraphResources(FrameGraph& graph) : m_graph(graph) {}

    ITexture* GetTexture(ResourceHandle handle);

private:
    FrameGraph& m_graph;
};
