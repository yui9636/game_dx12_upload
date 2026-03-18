#pragma once
#include "Component/Component.h"
#include "BT/BehaviorTree.h" // ランタイム基盤
#include <memory>
#include <string>

class EnemyAIController : public Component
{
public:
    const char* GetName() const override { return "EnemyAIController"; }

    void Start() override;
    void Update(float dt) override;
    void OnGUI() override;

    void SyncToEditor(std::shared_ptr<BTBrain> brain);

    // 特定のAIアセットをロードする
    void LoadAIAsset(const std::string& path);

private:
    void UpdateBlackboard(); // ターゲット検索などの情報更新

private:
    // --- Behavior Tree 実装 ---
    std::shared_ptr<BTBrain> m_Brain;
    BTContext m_Context;
    std::string m_CurrentAssetPath;

    // デバッグ用
    std::string m_CurrentPhaseName = "Default";
};