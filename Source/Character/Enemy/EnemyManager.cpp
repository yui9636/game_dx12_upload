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
    if (enemy)
    {
        enemies.push_back(enemy);
    }
}


std::shared_ptr<Enemy> EnemyManager::CreateEnemyTest(ID3D11Device* device, const DirectX::XMFLOAT3& position)
{
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
    float minLengthSq = range * range;

    DirectX::XMVECTOR VTarget = DirectX::XMLoadFloat3(&targetPos);

    for (const auto& enemy : enemies)
    {
        if (enemy->GetHealth() <= 0) continue;

        DirectX::XMVECTOR VEnemy = DirectX::XMLoadFloat3(&enemy->GetPosition());
        DirectX::XMVECTOR VDiff = DirectX::XMVectorSubtract(VEnemy, VTarget);

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

            if (ImGui::TreeNode(enemy->GetName().c_str()))
            {

                if (auto loco = enemy->GetComponent<EnemyLocomotionComponent>())
                {
                    loco->OnGUI();
                }

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
