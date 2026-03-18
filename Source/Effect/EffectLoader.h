#pragma once
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

class EffectNode;

class EffectLoader
{
public:
    //// ファイルからエフェクトを読み込む
    //static std::shared_ptr<EffectNode> LoadEffect(const std::string& filePath);

    //// エフェクトをファイルへ保存する
    //static bool SaveEffect(const std::string& filePath, const std::shared_ptr<EffectNode>& rootNode);

    static std::shared_ptr<EffectNode> LoadEffect(
        const std::string& filePath,
        float* outLife = nullptr,
        float* outFadeIn = nullptr,
        float* outFadeOut = nullptr,
        bool* outLoop = nullptr
    );

    // ★修正: 設定値を保存するための引数を追加
    static bool SaveEffect(
        const std::string& filePath,
        const std::shared_ptr<EffectNode>& rootNode,
        float life,
        float fadeIn,
        float fadeOut,
        bool loop
    );

private:
    // 内部処理用
    static std::shared_ptr<EffectNode> ParseNode(const nlohmann::json& j);
    static nlohmann::json SerializeNode(const std::shared_ptr<EffectNode>& node);
};