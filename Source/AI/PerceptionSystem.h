#pragma once

// AI 知覚システムの公開インターフェース定義。

class Registry;

// 敵の知覚情報を更新し、ターゲットをブラックボードへ書き込むシステム。
class PerceptionSystem
{
public:
    // 全対象 Entity のビヘイビアツリーを更新する。
    static void Update(Registry& registry, float dt);
};
