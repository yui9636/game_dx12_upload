#pragma once
#include <string>
#include <memory>
#include "BehaviorTree.h"

// エディタのJSONからランタイム用のBTBrainを構築する工場クラス
class BTBuilder {
public:
    // JSONファイルを読み込み、構築されたBrainを返す
    static std::shared_ptr<BTBrain> BuildFromFile(const std::string& path);
};