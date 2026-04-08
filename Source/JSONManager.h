#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <DirectXMath.h>
#include <iostream>

using json = nlohmann::json;

namespace nlohmann {
    template<> struct adl_serializer<DirectX::XMFLOAT2> {
        static void to_json(json& j, const DirectX::XMFLOAT2& v) { j = { {"x", v.x}, {"y", v.y} }; }
        static void from_json(const json& j, DirectX::XMFLOAT2& v) { j.at("x").get_to(v.x); j.at("y").get_to(v.y); }
    };
    template<> struct adl_serializer<DirectX::XMFLOAT3> {
        static void to_json(json& j, const DirectX::XMFLOAT3& v) { j = { {"x", v.x}, {"y", v.y}, {"z", v.z} }; }
        static void from_json(const json& j, DirectX::XMFLOAT3& v) { j.at("x").get_to(v.x); j.at("y").get_to(v.y); j.at("z").get_to(v.z); }
    };
    template<> struct adl_serializer<DirectX::XMFLOAT4> {
        static void to_json(json& j, const DirectX::XMFLOAT4& v) { j = { {"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w} }; }
        static void from_json(const json& j, DirectX::XMFLOAT4& v) { j.at("x").get_to(v.x); j.at("y").get_to(v.y); j.at("z").get_to(v.z); j.at("w").get_to(v.w); }
    };
    template<> struct adl_serializer<DirectX::XMFLOAT4X4> {
        static void to_json(json& j, const DirectX::XMFLOAT4X4& m) {
            j = json::array();
            for (int i = 0; i < 16; ++i) j.push_back((&m.m[0][0])[i]);
        }
        static void from_json(const json& j, DirectX::XMFLOAT4X4& m) {
            for (int i = 0; i < 16; ++i) (&m.m[0][0])[i] = j.at(i).get<float>();
        }
    };
}

class JSONManager {
public:
    explicit JSONManager(const std::string& filePath);

    void Load();
    void Save() const;

    static std::string ToRelativePath(const std::string& fullPath);

    template<typename T>
    void Set(const std::string& key, const T& value);

    template<typename T>
    T Get(const std::string& key, const T& defaultValue = T{}) const;

private:
    std::string filePath;
    json jsonData;
};

template<typename T>
void JSONManager::Set(const std::string& key, const T& value) {
    try { jsonData[key] = value; }
    catch (const std::exception& e) { std::cerr << "Set error (" << key << "): " << e.what() << std::endl; }
}

template<typename T>
T JSONManager::Get(const std::string& key, const T& defaultValue) const {
    try {
        if (jsonData.contains(key)) return jsonData.at(key).get<T>();
    }
    catch (const std::exception& e) {
        std::cerr << "Get error (" << key << "): " << e.what() << std::endl;
    }
    return defaultValue;
}
