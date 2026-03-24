#pragma once
#include <string>

struct NameComponent {
    std::string name;

    NameComponent() : name("New Entity") {}
    NameComponent(const std::string& inName) : name(inName) {}
};
