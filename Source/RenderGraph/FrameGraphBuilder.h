#pragma once
#include <string>
#include "FrameGraphTypes.h"

// ============================================================
// FrameGraphBuilder: パスが Setup() 中にリソースを宣言するインターフェース
// 実装は FrameGraph.cpp 内の BuilderImpl
// ============================================================
class FrameGraphBuilder {
public:
    virtual ~FrameGraphBuilder() = default;

    // 新しいトランジェントリソースを作成 (version=0)
    virtual ResourceHandle CreateTexture(const std::string& name, const TextureDesc& desc) = 0;

    // 読み取り宣言: producer パスからこのパスへの依存エッジを生成
    virtual ResourceHandle Read(ResourceHandle input) = 0;

    // 書き込み宣言: version をインクリメントし、新ハンドルを返す
    virtual ResourceHandle Write(ResourceHandle input) = 0;

    // Blackboard にハンドルを名前で登録
    virtual void RegisterHandle(const std::string& name, ResourceHandle handle) = 0;

    // Blackboard から名前でハンドルを検索 (見つからなければ無効ハンドル)
    virtual ResourceHandle GetHandle(const std::string& name) const = 0;
};
