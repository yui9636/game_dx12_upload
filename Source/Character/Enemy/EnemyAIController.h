#pragma once
#include "Component/Component.h"
#include "BT/BehaviorTree.h"
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

    void LoadAIAsset(const std::string& path);

private:
    void UpdateBlackboard();

private:
    std::shared_ptr<BTBrain> m_Brain;
    BTContext m_Context;
    std::string m_CurrentAssetPath;

    std::string m_CurrentPhaseName = "Default";
};
