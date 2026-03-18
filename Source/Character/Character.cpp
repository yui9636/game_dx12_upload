#include "Character.h"
#include "Stage/Stage.h"
#include "System/Mathf.h"
#include "Collision/ColliderComponent.h"

void Character::Start()
{
 
    this->position = Actor::GetLocalPosition();
 
    this->scale = Actor::GetLocalScale();


    DirectX::XMFLOAT4 q = Actor::GetLocalRotation();
    DirectX::XMVECTOR Q = DirectX::XMLoadFloat4(&q);

    // 「モデルにとっての正面(Z+)」が、クォータニオンによって
    // 「世界でどの方角(WorldForward)」に向いたかを計算します。
    DirectX::XMVECTOR DefaultForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    DirectX::XMVECTOR CurrentForward = DirectX::XMVector3Rotate(DefaultForward, Q);

    DirectX::XMFLOAT3 dir;
    DirectX::XMStoreFloat3(&dir, CurrentForward);

    // その方角ベクトル(x, z)から、Y軸の回転角度(Yaw)を逆算します。
    // atan2 は -180度?+180度 の全方位を正しく返します。
    this->angle.y = std::atan2(dir.x, dir.z);

    // キャラクターは通常直立しているので、X(Pitch)とZ(Roll)は0で初期化します。
    // (寝ているキャラなどを配置したい場合は別途処理が必要ですが、通常はこれで完璧です)
    this->angle.x = 0.0f;
    this->angle.z = 0.0f;






    Actor::Start();
}


void Character::UpdateTransform()
{
    // 1) angle(オイラー) → quat に最小変換して Actor に回転を渡す
    DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(angle.x, angle.y, angle.z);
    DirectX::XMFLOAT4 qf; DirectX::XMStoreFloat4(&qf, q);

    // 2) 位置・スケールは既に SetPosition/SetScale で Actor 側に同期済みだが、
    Actor::SetPosition(position);
    Actor::SetScale(scale);
    Actor::SetRotation(qf);

    // 3) ワールド行列の最終合成は Actor 側に一元化
    Actor::UpdateTransform();

    // 4) 互換のため、公開メンバ transform を Actor の行列で上書き
    transform = Actor::GetTransform();
}

void Character::Render(ModelRenderer* renderer)
{
    Actor::Render(renderer);
}


//────────────────────────────────────────────────────
// ダメージを与える
//────────────────────────────────────────────────────
bool Character::ApplyDamage(int damage, float invincibleTime)
{
    // if (damage == 0) return false; // 元コメントは保持
    if (health <= 0) return false;
    if (this->invincibleTimer > 0.0f) return false;

    this->invincibleTimer = invincibleTime;

    lastDamage = damage;

    health -= damage;

    if (health <= 0) { OnDead(); }
    else { OnDamaged(); }

    return true;
}

//────────────────────────────────────────────────────
// 衝撃を与える
//────────────────────────────────────────────────────
void Character::AddImpulse(const DirectX::XMFLOAT3& impulse)
{
    velocity.x += impulse.x;
    velocity.y += impulse.y;
    velocity.z += impulse.z;
}

//────────────────────────────────────────────────────
// 移動処理
//────────────────────────────────────────────────────
void Character::Move(float vx, float vz, float speed)
{
    moveVecX = vx;
    moveVecZ = vz;
    maxMoveSpeed = speed;
}

//────────────────────────────────────────────────────
// 旋回処理
//────────────────────────────────────────────────────
void Character::Turn(float dt, float vx, float vz, float speed)
{
    speed *= dt;

    float length = sqrtf(vx * vx + vz * vz);
    if (length < 0.001f) return;

    vx /= length;
    vz /= length;

    float frontX = sinf(angle.y);
    float frontZ = cosf(angle.y);

    float dot = (frontX * vx) + (frontZ * vz);
    float rot = 1.0f - dot;
    if (rot > speed) rot = speed;

    float cross = (frontZ * vx) - (frontX * vz);
    if (cross < 0.0f) { angle.y -= rot; }
    else { angle.y += rot; }
}

//────────────────────────────────────────────────────
// ジャンプ処理
//────────────────────────────────────────────────────
void Character::Jump(float speed)
{
    velocity.y = speed;
}

//────────────────────────────────────────────────────
// 速力処理更新
//────────────────────────────────────────────────────
void Character::UpdateVelocity(float dt)
{
    float elapsedFrame = 60.0f * dt;

    UpdateVerticalVelocity(elapsedFrame);
    UpdateHorizontalVelocity(elapsedFrame);

    UpdateVerticalMove(dt);
    UpdateHorizontalMove(dt);
}

//────────────────────────────────────────────────────
// 無敵時間更新
//────────────────────────────────────────────────────
void Character::UpdateInvincibleTimer(float dt)
{
    if (invincibleTimer > 0.0f)
    {
        invincibleTimer -= dt;
    }
}

//────────────────────────────────────────────────────
// 垂直速力更新処理
//────────────────────────────────────────────────────
void Character::UpdateVerticalVelocity(float elapsedFrame)
{
    velocity.y += gravity * elapsedFrame;
}

//────────────────────────────────────────────────────
// 垂直移動更新処理（元の仕様を保持）
//────────────────────────────────────────────────────
void Character::UpdateVerticalMove(float /*dt*/)
{
    // 必要時に元実装を戻す
}

//────────────────────────────────────────────────────
// 水平速力更新処理
//────────────────────────────────────────────────────
void Character::UpdateHorizontalVelocity(float elapsedFrame)
{
    Vector2 velocityXZ(velocity.x, velocity.z);
    float speed = velocityXZ.Length();

    if (speed > 0.0f)
    {
        float frictionForce = friction * elapsedFrame;
        if (!isGround) frictionForce *= airControl;

        if (speed > frictionForce)
        {
            Vector2 unit = velocityXZ;
            unit.Normalize();
            unit *= frictionForce;
            velocityXZ -= unit;
        }
        else
        {
            velocityXZ = Vector2::Zero;
        }
    }

    if (speed <= maxMoveSpeed)
    {
        Vector2 moveVec(moveVecX, moveVecZ);
        float moveLen = moveVec.Length();

        if (moveLen > 0.0f)
        {
            float accel = acceleration * elapsedFrame;
            if (!isGround) accel *= airControl;

            Vector2 unit = moveVec;
            unit.Normalize();
            unit *= accel;
            velocityXZ += unit;

            if (velocityXZ.Length() > maxMoveSpeed)
            {
                unit = velocityXZ;
                unit.Normalize();
                velocityXZ = unit * maxMoveSpeed;
            }

            if (isGround && slopeRate > 0.0f)
                velocity.y -= velocityXZ.Length() * slopeRate * elapsedFrame;
        }
    }

    velocity.x = velocityXZ.x;
    velocity.z = velocityXZ.y;

    moveVecX = 0.0f;
    moveVecZ = 0.0f;
}

//────────────────────────────────────────────────────
// 水平移動更新処理（元の仕様を保持）
//────────────────────────────────────────────────────
void Character::UpdateHorizontalMove(float dt)
{
    // 移動処理
    position.x += velocity.x * dt;
    position.z += velocity.z * dt;

    //// 水平速力計算
    //float velocityLengthXZ = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&velocity)));
    //if (velocityLengthXZ > 0.0f)
    //{
    //    // 水平移動値
    //    float mx = velocity.x * dt;
    //    float mz = velocity.z * dt;

    //    // レイの開始位置と終点位置
    //    DirectX::XMFLOAT3 start = { position.x, position.y + stepOffset, position.z };
    //    DirectX::XMFLOAT3 end = { position.x + mx, position.y + stepOffset, position.z + mz };

    //    //// レイキャストによる壁判定
    //    //HitResult hit;
    //    //if (Stage::Instance().RayCast(start, end, hit))
    //    //{


    //    //    // 壁までのベクトル
    //    //    DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
    //    //    DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);
    //    //    DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(End, Start);

    //    //    // 壁の法線ベクトル
    //    //    DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(&hit.normal);

    //    //    // 入射ベクトルを法線に射影
    //    //    //DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(Vec, Normal);
    //    //    DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(DirectX::XMVectorNegate(Vec), Normal);

    //    //    // 補正値の計算 
    //    //    // 法線ベクトル方向に Dot 分スケーリングする
    //    //    //DirectX::XMVECTOR CollisionPosition = DirectX::XMVectorMultiply(Normal, Dot);
    //    //    DirectX::XMVECTOR CollisionPosition = DirectX::XMVectorMultiplyAdd(Normal, Dot, End);

    //    //    // CollisionPositionにEndの位置を足した位置が最終的な位置
    //    //    DirectX::XMFLOAT3 collisionPosition;
    //    //    //DirectX::XMStoreFloat3(&collisionPosition, DirectX::XMVectorAdd(End, CollisionPosition));
    //    //    DirectX::XMStoreFloat3(&collisionPosition, CollisionPosition);


    //        //// hit.position を開始とし、collisionPosition を終点位置としてさらにレイキャストによる壁判定を行う
    //        //HitResult hit2;
    //        //if (!Stage::Instance().RayCast(hit.position, collisionPosition, hit2))
    //        //{
    //        //    // 当たっていなかったら
    //        //    // x と z の成分のみ反映
    //        //    position.x = collisionPosition.x;
    //        //    position.z = collisionPosition.z;
    //        //}

    //        //else
    //        //{
    //        //    // 当たってたら hit2.position を最終的な位置として反映
    //        //  /*  position.x = hit2.position.x;
    //        //    position.z = hit2.position.z;*/
    //        //}
    //    }
    //    else
    //    {

    //        position.x += mx;
    //        position.z += mz;
    //    }
    //}
}

void Character::ApplyStageConstraint(float stageRadius)
{
    // 1. 自分の半径を決定
    // コライダーを持っていれば、その形状から最大半径を取得
    float myRadius = this->radius; // デフォルト値
    auto collider = GetComponent<ColliderComponent>();
    if (collider)
    {
        myRadius = collider->GetMaxRadiusXZ();
    }

    // 2. 許容距離の計算
    // ステージ半径から自分の厚みを引いた距離が「限界ライン」
    float limitDist = stageRadius - myRadius;
    if (limitDist < 0.0f) limitDist = 0.0f;

    // 3. 現在位置のチェック (XZ平面)
    // Y軸(高さ)は無視して円形ステージとして判定
    float currentX = position.x;
    float currentZ = position.z;
    float distSq = currentX * currentX + currentZ * currentZ;

    // 限界を超えているか？ (平方根計算を避けるため2乗で比較)
    if (distSq > limitDist * limitDist)
    {
        float dist = std::sqrt(distSq);
        if (dist <= 0.0001f) return; // 0除算防止

        // --- A. 位置補正 (Depenetration) ---
        // 中心方向へのベクトル (ToCenter)
        // 外向き法線 (Normal) = (currentX, currentZ) / dist
        float normX = currentX / dist;
        float normZ = currentZ / dist;

        // 限界距離の位置まで押し戻す
        position.x = normX * limitDist;
        position.z = normZ * limitDist;

        // --- B. 速度補正 (Wall Slide) ---
        // 壁に向かって進もうとしている成分(外向き成分)だけを消去する

        // 速度と外向き法線の内積
        float dot = velocity.x * normX + velocity.z * normZ;

        // 外側に向かって進んでいる(dot > 0)場合のみ補正
        if (dot > 0.0f)
        {
            // 速度から「壁に向かう成分」を引き算する
            // V_new = V - (V . N) * N
            velocity.x -= normX * dot;
            velocity.z -= normZ * dot;
        }
    }
}


