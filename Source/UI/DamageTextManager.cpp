#include "DamageTextManager.h"

using namespace DirectX;

void DamageTextManager::Initialize()
{
    pool.clear();
    pool.reserve(POOL_SIZE);

    for (int i = 0; i < POOL_SIZE; ++i)
    {
        auto popup = std::make_shared<UIDamagePopup>();
        pool.push_back(popup);
    }
}

void DamageTextManager::Update(float dt)
{
    for (auto& popup : pool)
    {
        if (popup && popup->IsActive()) popup->Update(dt);
    }
}

void DamageTextManager::Render(const RenderContext& rc)
{
    for (auto& popup : pool)
    {
        if (popup && popup->IsActive()) popup->Render(rc);
    }
}

void DamageTextManager::Spawn(const DirectX::XMFLOAT3& position, int damage)
{
    for (auto& popup : pool)
    {
        if (!popup->IsActive())
        {
            // SetupŒÄ‚Ño‚µ‚àˆø”‚ðŒ¸‚ç‚·
            popup->Setup(position, damage);
            return;
        }
    }
}