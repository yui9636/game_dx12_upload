#pragma once
#include <DirectXMath.h>
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

#include "Actor/Actor.h"

class Character : public Actor
{
public:
    Character() = default;
    virtual ~Character() = default;

    virtual void Start() override;

    void UpdateTransform();

    void Render(ModelRenderer* renderer) override;
 
    // 位置取得：Actor 側の値をそのまま返す（自前の position は参照しない）
    const DirectX::XMFLOAT3& GetPosition() const { return Actor::GetPosition(); }

    // 位置設定：自前メンバも更新しつつ、最終的な正は Actor に委譲
    void SetPosition(const DirectX::XMFLOAT3& position) {
        this->position = position;          // 互換のため内部も保持（既存コードが直接参照してもズレない）
        Actor::SetPosition(position);       // 実体は Actor 側
    }

    // スケール取得：Actor 側の値をそのまま返す
    const DirectX::XMFLOAT3& GetScale() const { return Actor::GetScale(); }

    // スケール設定：自前メンバも更新しつつ、Actor に委譲
    void SetScale(const DirectX::XMFLOAT3& scale) {
        this->scale = scale;                // 互換保持
        Actor::SetScale(scale);             // 実体は Actor 側
    }

    // 回転取得（オイラー）
    const DirectX::XMFLOAT3& GetAngle() const { return angle; }
    // 回転設定（オイラー）
    void SetAngle(const DirectX::XMFLOAT3& angle) { this->angle = angle; }

    // 半径取得
    float GetRadius() const { return radius; }

    // 地面に接地しているかどうか
    bool IsGround() const { return isGround; }

    // 高さ取得
    float GetHeight() const { return height; }

    // ダメージを与える
    bool ApplyDamage(int damage, float invincibleTime);

    // 衝撃を与える
    void AddImpulse(const DirectX::XMFLOAT3& impulse);

    // 健康状態を取得
    int GetHealth() const { return health; }

    // 最大健康状態を取得
    int GetMaxHealth() const { return maxHealth; }

    // 移動処理
    void Move(float vx, float vz, float speed);

    // 旋回処理
    void Turn(float dt, float vx, float vz, float speed);

    void ApplyStageConstraint(float stageRadius);
protected:


    // ジャンプ処理
    void Jump(float speed);

    // 速力処理更新
    void UpdateVelocity(float dt);

    // 無敵時間更新
    void UpdateInvincibleTimer(float dt);

    // 着地した時に呼ばれる
    virtual void OnLanding() {}

    // ダメージを受けたときに呼ばれる
    virtual void OnDamaged() {}

    // 死亡した際に呼ばれる
    virtual void OnDead() {}

    // 水平移動更新処理
    void UpdateHorizontalMove(float dt);
private:
    // 垂直速力更新処理
    void UpdateVerticalVelocity(float elapsedFrame);

    // 垂直移動更新処理
    void UpdateVerticalMove(float dt);

    // 水平速力更新処理
    void UpdateHorizontalVelocity(float elapsedFrame);

public:
    // ── 既存公開変数（名前変更禁止） ──
    DirectX::XMFLOAT3   position = { 0,0,0 };
    DirectX::XMFLOAT3   angle = { 0,0,0 };
    DirectX::XMFLOAT3   scale = { 1,1,1 };
    DirectX::XMFLOAT4X4 transform = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

        // 半径
    float radius = 0.5f;

    // 重力
    float gravity = -1.0f;

    // 速度
    DirectX::XMFLOAT3 velocity = { 0,0,0 };

    // 地面にいるか
    bool isGround = false;

    // 高さ
    float height = 2.0f;

    // 健康状態
    int health = 5;

    // MAXHP
    int maxHealth = 5;

    // 無敵時間
    float invincibleTimer = 1.0f;

    // 摩擦力
    float friction = 10.8f;

    // 加速度
    float acceleration = 50.0f;

    // 最大移動速度
    float maxMoveSpeed = 100.0f;

    // 移動方向
    float moveVecX = 0.0f;
    float moveVecZ = 0.0f;

    // 空中制御
    float airControl = 0.3f;

    // レイの始点
    float stepOffset = 1.0f;

    // 傾斜率
    float slopeRate = 1.0f;

    // 無敵フラグ
    bool isInvincible = false;

    int lastDamage = 0;
};
