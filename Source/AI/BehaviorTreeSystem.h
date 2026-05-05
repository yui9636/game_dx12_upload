#pragma once

// ビヘイビアツリー実行システムの公開インターフェース定義。

class Registry;

// 敵 AI のビヘイビアツリーを更新するシステム。
class BehaviorTreeSystem
{
public:
    // 全対象 Entity のビヘイビアツリーを更新する。
    static void Update(Registry& registry, float dt);
    // 読み込み済みアセットキャッシュを無効化する。
    static void InvalidateAssetCache(const char* path = nullptr);
};
