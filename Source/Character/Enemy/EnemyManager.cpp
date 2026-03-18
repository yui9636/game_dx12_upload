#include "EnemyManager.h"
#include "Enemy.h"
#include "EnemyBoss.h"
#include <algorithm> // std::find
#include <imgui.h>
#include "EnemyLocomotionComponent.h"
#include "EnemyAIController.h"

// EnemyManager.cpp

void EnemyManager::Update(float dt)
{
    

    // 削除処理
    if (!removeQueue.empty())
    {
        for (auto& target : removeQueue)
        {
            auto it = std::find(enemies.begin(), enemies.end(), target);
            if (it != enemies.end())
            {
                enemies.erase(it);
            }
        }
        removeQueue.clear();
    }

  
}



void EnemyManager::Render(ModelRenderer* renderer)
{
   
}

void EnemyManager::RegisterEnemy(std::shared_ptr<Enemy> enemy)
{
    // 追加しようとしている敵が有効ならリストに入れる
    if (enemy)
    {
        enemies.push_back(enemy);
    }
}


std::shared_ptr<Enemy> EnemyManager::CreateEnemyTest(ID3D11Device* device, const DirectX::XMFLOAT3& position)
{
    // インスタンス生成
    auto enemy = std::make_shared<EnemyBoss>();


    enemy->Initialize(device);
    //enemy->Start();
    enemy->SetPosition(position);
    enemy->UpdateTransform();

    enemies.push_back(enemy);

    ActorManager::Instance().AddActor(enemy);

    return enemy;
}

void EnemyManager::Remove(std::shared_ptr<Enemy> enemy)
{
    if (enemy)
    {
        removeQueue.push_back(enemy);
    }
}

void EnemyManager::Clear()
{
    enemies.clear();
    removeQueue.clear();
}

std::shared_ptr<Enemy> EnemyManager::GetNearestEnemy(const DirectX::XMFLOAT3& targetPos, float range)
{
    std::shared_ptr<Enemy> nearest = nullptr;
    float minLengthSq = range * range; // 距離の2乗で比較

    DirectX::XMVECTOR VTarget = DirectX::XMLoadFloat3(&targetPos);

    for (const auto& enemy : enemies)
    {
        // 死亡している敵は除外
        if (enemy->GetHealth() <= 0) continue;

        DirectX::XMVECTOR VEnemy = DirectX::XMLoadFloat3(&enemy->GetPosition());
        DirectX::XMVECTOR VDiff = DirectX::XMVectorSubtract(VEnemy, VTarget);

        // 距離の2乗を取得
        float lengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(VDiff));

        if (lengthSq < minLengthSq)
        {
            minLengthSq = lengthSq;
            nearest = enemy;
        }
    }

    return nearest;
}

void EnemyManager::OnGUI()
{
    if (ImGui::Begin("Enemy Manager"))
    {
        for (auto& enemy : enemies)
        {
            if (!enemy) continue;

            ImGui::PushID(enemy.get());

            // 敵の名前でツリー表示
            if (ImGui::TreeNode(enemy->GetName().c_str()))
            {
                // ★修正: enemy->OnGUI() は全コンポーネントが出てしまうので廃止！
                // 代わりに、調整したいコンポーネントだけをピンポイントで取得して表示します。

                // 1. 移動・行動範囲 (Arena Radius)
                if (auto loco = enemy->GetComponent<EnemyLocomotionComponent>())
                {
                    loco->OnGUI();
                }

                // 2. AI・索敵範囲 (Ranges)
                if (auto ai = enemy->GetComponent<EnemyAIController>())
                {
                    ai->OnGUI();
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }
    ImGui::End();
}