// 入力名。Component の ECS コンポーネント定義をまとめます。
#pragma once
#include <string>

struct NameComponent {
    std::string name;

    NameComponent() : name("New Entity") {}
    NameComponent(const std::string& inName) : name(inName) {}
};
