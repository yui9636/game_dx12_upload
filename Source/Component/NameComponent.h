#pragma once
#include <string>

// エンティティの識別名を管理するコンポーネント
struct NameComponent {
    std::string name;

    NameComponent() : name("New Entity") {}
    NameComponent(const std::string& inName) : name(inName) {}
};