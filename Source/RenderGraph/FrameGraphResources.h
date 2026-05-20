#pragma once
#include "FrameGraphTypes.h"

class FrameGraph;
class ITexture;
// pass 実行中に ResourceHandle から実体 ITexture へ解決するビュー。
// pass 側は FrameGraph 本体を知らず、このクラス経由で必要な texture だけ取得する。
class FrameGraphResources {
public:
    FrameGraphResources(FrameGraph& graph) : m_graph(graph) {}

    // handle が指す imported / pooled texture の実体を返す。無効 handle なら nullptr。
    ITexture* GetTexture(ResourceHandle handle);

private:
    // 実体解決は FrameGraph の resource table に委譲する。
    FrameGraph& m_graph;
};
