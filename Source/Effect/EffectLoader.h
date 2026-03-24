#pragma once
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

class EffectNode;

class EffectLoader
{
public:
    //static std::shared_ptr<EffectNode> LoadEffect(const std::string& filePath);

    //static bool SaveEffect(const std::string& filePath, const std::shared_ptr<EffectNode>& rootNode);

    static std::shared_ptr<EffectNode> LoadEffect(
        const std::string& filePath,
        float* outLife = nullptr,
        float* outFadeIn = nullptr,
        float* outFadeOut = nullptr,
        bool* outLoop = nullptr
    );

    static bool SaveEffect(
        const std::string& filePath,
        const std::shared_ptr<EffectNode>& rootNode,
        float life,
        float fadeIn,
        float fadeOut,
        bool loop
    );

private:
    static std::shared_ptr<EffectNode> ParseNode(const nlohmann::json& j);
    static nlohmann::json SerializeNode(const std::shared_ptr<EffectNode>& node);
};
