#pragma once
#include "Component/Component.h"

// ステージの半径などの情報を持つコンポーネント
class StageInfoComponent : public Component
{
public:
    StageInfoComponent() = default;
    ~StageInfoComponent() override = default;

    // ステージ半径 (初期値 50m)
    float radius = 50.0f;

    // --- Component 基底クラスのオーバーライド ---

    // コンポーネント名 (LevelLoaderでの識別に使用)
    const char* GetName() const override { return "StageInfo"; }

    // インスペクター描画
    void OnGUI() override;

    // セーブ・ロード
    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;
};