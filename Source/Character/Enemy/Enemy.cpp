#include "Enemy.h"
#include "UI/DamageTextManager.h"
#include <cstdlib>

Enemy::Enemy()
{
   /* name = "Enemy";
    health = 100;
    isDead = false;*/
}

void Enemy::Initialize(ID3D11Device* device)
{
    Character::Initialize(device);

}

void Enemy::Update(float dt)
{
 

    UpdateInvincibleTimer(dt);
    Actor::Update(dt);
}

void Enemy::Render(ModelRenderer* renderer)
{
    //if (isDead) return;
    Actor::Render(renderer);

}

void Enemy::OnDamaged()
{
 
    DirectX::XMFLOAT3 pos = this->GetPosition();
    float centerY = this->GetHeight() * this->GetScale().y * 3.0f;


    float r = this->GetRadius() * this->GetScale().x;
    float randX = ((float)rand() / RAND_MAX - 0.5f) * r * 5.0f;
    float randZ = ((float)rand() / RAND_MAX - 0.5f) * r * 5.0f;

    float randY = ((float)rand() / RAND_MAX - 0.5f) * 3.0f;

    pos.x += randX;
    pos.y += centerY + randY;
    pos.z += randZ;

    DamageTextManager::Instance().Spawn(pos, lastDamage);

}

void Enemy::OnDead()
{
    isDead = true;
}
