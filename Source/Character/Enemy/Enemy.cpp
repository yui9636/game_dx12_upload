#include "Enemy.h"
#include "UI/DamageTextManager.h"
#include <cstdlib>

Enemy::Enemy()
{
   /* name = "Enemy";
    health = 100;
    isDead = false;*/
}

// ★追加: 基底クラスのInitialize
void Enemy::Initialize(ID3D11Device* device)
{
    Character::Initialize(device);

    // 必要ならここで共通の初期化（エフェクト読み込みなど）
    // Start() をここで呼んでも良いが、ActorManagerのライフサイクルに任せるなら呼ばない
    // 今回はモデルロード用として使う
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

    // 2. ランダム拡散 (Scatter)
    // 毎回同じ場所だと数字が重なって読めないので、少しずらします

    // 横幅(半径)の範囲で散らす
    float r = this->GetRadius() * this->GetScale().x;
    float randX = ((float)rand() / RAND_MAX - 0.5f) * r * 5.0f; // 半径の1.5倍くらいの範囲
    float randZ = ((float)rand() / RAND_MAX - 0.5f) * r * 5.0f;

    // 高さも少し散らす (中心から上下 0.5m くらいの範囲)
    float randY = ((float)rand() / RAND_MAX - 0.5f) * 3.0f;

    // 3. 座標の確定
    pos.x += randX;
    pos.y += centerY + randY; // 足元 + 中心高さ + ズレ
    pos.z += randZ;

    // マネージャーに依頼
    DamageTextManager::Instance().Spawn(pos, lastDamage);

}

void Enemy::OnDead()
{
    isDead = true;
}