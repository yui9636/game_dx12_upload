#include "EnemyAIController.h"
#include "BT/BTBuilder.h"
#include "Actor/Actor.h"
#include "Character/Enemy/EnemyBoss.h"
#include "EnemyLocomotionComponent.h"
#include <imgui.h>
#include "BT/BTEditorComponent.h"

void EnemyAIController::Start()
{
    m_Context.owner = GetActor();

    // デフォルトのAIアセットがあればロード
    LoadAIAsset("Data/AI/Boss_Phase1.json");
}

void EnemyAIController::LoadAIAsset(const std::string& path)
{
    m_CurrentAssetPath = path;
    m_Brain = BTBuilder::BuildFromFile(path);

    if (m_Brain) {
        OutputDebugStringA("[EnemyAIController] Behavior Tree Loaded successfully.\n");
    }
}

void EnemyAIController::Update(float dt)
{
    if (!m_Brain) return;

    // 1. ブラックボード（Context）の更新
    m_Context.deltaTime = dt;
    UpdateBlackboard();

    // 2. BTの実行
    // 内部で BTAction_MoveTo や BTAction_PlayAnim が走り、ボスを動かす
    m_Brain->Tick(m_Context);

    auto liveStatus = m_Brain->GetLiveStatusMap();
    BTEditorComponent::PushLiveDebugData(liveStatus);
}

void EnemyAIController::UpdateBlackboard()
{
    // weak_ptr なので expired() で有効性をチェック
    if (m_Context.target.expired())
    {
        auto& actors = ActorManager::Instance().GetActors();
        for (const auto& actor : actors)
        {
            if (actor->GetName().find("Player") != std::string::npos)
            {
                m_Context.target = actor; // shared_ptr から weak_ptr への代入は可能
                break;
            }
        }
    }

    if (auto boss = std::dynamic_pointer_cast<EnemyBoss>(GetActor())) {
        m_Context.stageRadius = boss->GetStageLimitRadius();
    }

}

void EnemyAIController::OnGUI()
{
    if (ImGui::CollapsingHeader("Enemy AI (BehaviorTree)"))
    {
        ImGui::Text("Asset: %s", m_CurrentAssetPath.c_str());

        // 変数名を targetPtr にして名前衝突を回避
        auto targetPtr = m_Context.target.lock();
        ImGui::Text("Current Target: %s", targetPtr ? targetPtr->GetName().c_str() : "NONE");

        if (ImGui::Button("Manual Reload")) {
            LoadAIAsset(m_CurrentAssetPath);
        }
    }
}

