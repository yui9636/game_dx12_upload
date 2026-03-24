#pragma once
#include <string>
#include <memory>
#include "BehaviorTree.h"

class BTBuilder {
public:
    static std::shared_ptr<BTBrain> BuildFromFile(const std::string& path);
};
