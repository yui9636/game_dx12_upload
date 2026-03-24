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
    // ========================================================================
    const std::string& GetTargetFilePath() const { return targetFilePath; }

    // ========================================================================
    // ========================================================================
    void SaveGameplayData(const std::string& modelName,
        const std::vector<std::vector<GESequencerItem>>& timelines,
        const std::vector<GECurveSettings>& curves);

    void SaveGameplayDataToPath(const std::string& fullPath,
        const std::vector<std::vector<GESequencerItem>>& timelines,
        const std::vector<GECurveSettings>& curves);

    // ========================================================================
    // ========================================================================
    GameplayAsset LoadGameplayData(const std::string& modelName);
    GameplayAsset LoadGameplayDataFromPath(const std::string& fullPath);

    // ========================================================================
    // ========================================================================
    void OnGUI() override;
    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;

private:
    std::string GetFilePath(const std::string& modelName) const;

    std::string targetFilePath;
};
