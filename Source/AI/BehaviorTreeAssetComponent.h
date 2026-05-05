#pragma once

// Entity が参照するビヘイビアツリーアセットのパスを保持するコンポーネント定義。
#include <string>

// ビヘイビアツリー全体を保持するアセットデータ。
struct BehaviorTreeAssetComponent
{
    // 読み込む .bt ファイルのパス。
    std::string assetPath;
};
