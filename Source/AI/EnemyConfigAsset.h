#pragma once

// 敵キャラクター設定アセットのデータ構造を定義するファイル。
#include <filesystem>
#include <string>

// 敵 1 種類分のアセット参照と基本ステータスをまとめた設定アセット。
struct EnemyConfigAsset
{
    int          version = 1;
    std::string  name;

    // 敵が使用するビヘイビアツリーのパス。
    std::string  behaviorTreePath;
    // 敵が使用する StateMachine のパス。
    std::string  stateMachinePath;
    // 敵が使用する Timeline のパス。
    std::string  timelinePath;
    // 敵モデルアセットのパス。
    std::string  modelPath;
    // 敵 Animator アセットのパス。
    std::string  animatorPath;

    // 最大 HP。
    float        maxHealth     = 100.0f;
    // 歩き速度。
    float        walkSpeed     = 2.0f;
    // 走り速度。
    float        runSpeed      = 4.5f;
    // 旋回速度。
    float        turnSpeed     = 540.0f;

    // 視覚検知距離。
    float        sightRadius   = 10.0f;
    // 視野角。
    float        sightFOV      = 1.5708f;
    // 聴覚検知距離。
    float        hearingRadius = 0.0f;

    // 基礎攻撃力。
    float        baseAttack    = 10.0f;

    // アセットファイルから設定を読み込む。
    bool LoadFromFile(const std::filesystem::path& path);
    // 現在の設定をアセットファイルへ保存する。
    bool SaveToFile(const std::filesystem::path& path) const;

    // 攻撃型騎士の標準設定を生成する。
    static EnemyConfigAsset CreateAggressiveKnight();
};
