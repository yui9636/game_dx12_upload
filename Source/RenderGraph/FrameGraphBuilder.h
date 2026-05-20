#pragma once
#include <string>
#include "FrameGraphTypes.h"
// レンダーパスの Setup 中に、読み書きするリソースを FrameGraph へ登録するための抽象インターフェース。
class FrameGraphBuilder {
public:
    virtual ~FrameGraphBuilder() = default;

    // 新しい仮想 texture を作成し、その初期世代の handle を返す。
    virtual ResourceHandle CreateTexture(const std::string& name, const TextureDesc& desc) = 0;

    // pass が input として読む resource を登録する。
    virtual ResourceHandle Read(ResourceHandle input) = 0;

    // pass が output として書く resource を登録し、version を進めた handle を返す。
    virtual ResourceHandle Write(ResourceHandle input) = 0;

    // 他の pass から名前で取得できるよう、Blackboard に handle を公開する。
    virtual void RegisterHandle(const std::string& name, ResourceHandle handle) = 0;

    // 先行 pass が RegisterHandle した resource を名前で取得する。
    virtual ResourceHandle GetHandle(const std::string& name) const = 0;
};
