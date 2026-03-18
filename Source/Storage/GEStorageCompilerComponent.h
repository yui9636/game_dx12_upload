#pragma once
#include "Component/Component.h"
#include "Storage/GameplayAsset.h"
#include <string>
#include <vector>

class GEStorageCompilerComponent : public Component
{
public:
    const char* GetName() const override { return "GEStorageCompiler"; }

    // ========================================================================
    // ★追加: ターゲットファイルパス管理
    // ========================================================================
    const std::string& GetTargetFilePath() const { return targetFilePath; }

    // ========================================================================
    // 保存 API
    // ========================================================================
    void SaveGameplayData(const std::string& modelName,
        const std::vector<std::vector<GESequencerItem>>& timelines,
        const std::vector<GECurveSettings>& curves);

    void SaveGameplayDataToPath(const std::string& fullPath,
        const std::vector<std::vector<GESequencerItem>>& timelines,
        const std::vector<GECurveSettings>& curves);

    // ========================================================================
    // 読み込み API
    // ========================================================================
    GameplayAsset LoadGameplayData(const std::string& modelName);
    GameplayAsset LoadGameplayDataFromPath(const std::string& fullPath);

    // ========================================================================
    // オーバーライド
    // ========================================================================
    void OnGUI() override;                         // ★追加: エディターでファイル選択
    void Serialize(json& outJson) const override;  // ★追加: パスの保存
    void Deserialize(const json& inJson) override; // ★追加: パスの復元

private:
    std::string GetFilePath(const std::string& modelName) const;

    // ★追加: Inspectorで設定した読み込み対象のファイルパス
    std::string targetFilePath;
};