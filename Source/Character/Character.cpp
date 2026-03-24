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

    DirectX::XMVECTOR DefaultForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    DirectX::XMVECTOR CurrentForward = DirectX::XMVector3Rotate(DefaultForward, Q);

    DirectX::XMFLOAT3 dir;
    DirectX::XMStoreFloat3(&dir, CurrentForward);

    this->angle.y = std::atan2(dir.x, dir.z);

    this->angle.x = 0.0f;
    this->angle.z = 0.0f;






    Actor::Start();
}


void Character::UpdateTransform()
{
    DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(angle.x, angle.y, angle.z);
    DirectX::XMFLOAT4 qf; DirectX::XMStoreFloat4(&qf, q);

    Actor::SetPosition(position);
    Actor::SetScale(scale);
    Actor::SetRotation(qf);

    Actor::UpdateTransform();

    transform = Actor::GetTransform();
}

void Character::Render(ModelRenderer* renderer)
{
    Actor::Render(renderer);
}


bool Character::ApplyDamage(int damage, float invincibleTime)
{
    if (health <= 0) return false;
    if (this->invincibleTimer > 0.0f) return false;

    this->invincibleTimer = invincibleTime;

    lastDamage = damage;

    health -= damage;

    if (health <= 0) { OnDead(); }
    else { OnDamaged(); }

    return true;
}

void Character::AddImpulse(const DirectX::XMFLOAT3& impulse)
{
    velocity.x += impulse.x;
    velocity.y += impulse.y;
    velocity.z += impulse.z;
}

void Character::Move(float vx, float vz, float speed)
{
    moveVecX = vx;
    moveVecZ = vz;
    maxMoveSpeed = speed;
}

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

void Character::Jump(float speed)
{
    velocity.y = speed;
}

void Character::UpdateVelocity(float dt)
{
    float elapsedFrame = 60.0f * dt;

    UpdateVerticalVelocity(elapsedFrame);
    UpdateHorizontalVelocity(elapsedFrame);

    UpdateVerticalMove(dt);
    UpdateHorizontalMove(dt);
}

void Character::UpdateInvincibleTimer(float dt)
{
    if (invincibleTimer > 0.0f)
    {
        invincibleTimer -= dt;
    }
}

void Character::UpdateVerticalVelocity(float elapsedFrame)
{
    velocity.y += gravity * elapsedFrame;
}

void Character::UpdateVerticalMove(float /*dt*/)
{
}

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

void Character::UpdateHorizontalMove(float dt)
{
    position.x += velocity.x * dt;
    position.z += velocity.z * dt;

    //float velocityLengthXZ = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&velocity)));
    //if (velocityLengthXZ > 0.0f)
    //{
    //    float mx = velocity.x * dt;
    //    float mz = velocity.z * dt;

    //    DirectX::XMFLOAT3 start = { position.x, position.y + stepOffset, position.z };
    //    DirectX::XMFLOAT3 end = { position.x + mx, position.y + stepOffset, position.z + mz };

    //    //HitResult hit;
    //    //if (Stage::Instance().RayCast(start, end, hit))
    //    //{


    //    //    DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&start);
    //    //    DirectX::XMVECTOR End = DirectX::XMLoadFloat3(&end);
    //    //    DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(End, Start);

    //    //    DirectX::XMVECTOR Normal = DirectX::XMLoadFloat3(&hit.normal);

    //    //    //DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(Vec, Normal);
    //    //    DirectX::XMVECTOR Dot = DirectX::XMVector3Dot(DirectX::XMVectorNegate(Vec), Normal);

    //    //    //DirectX::XMVECTOR CollisionPosition = DirectX::XMVectorMultiply(Normal, Dot);
    //    //    DirectX::XMVECTOR CollisionPosition = DirectX::XMVectorMultiplyAdd(Normal, Dot, End);

    //    //    DirectX::XMFLOAT3 collisionPosition;
    //    //    //DirectX::XMStoreFloat3(&collisionPosition, DirectX::XMVectorAdd(End, CollisionPosition));
    //    //    DirectX::XMStoreFloat3(&collisionPosition, CollisionPosition);


    //        //HitResult hit2;
    //        //if (!Stage::Instance().RayCast(hit.position, collisionPosition, hit2))
    //        //{
    //        //    position.x = collisionPosition.x;
    //        //    position.z = collisionPosition.z;
    //        //}

    //        //else
    //        //{
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
    float myRadius = this->radius;
    auto collider = GetComponent<ColliderComponent>();
    if (collider)
    {
        myRadius = collider->GetMaxRadiusXZ();
    }

    float limitDist = stageRadius - myRadius;
    if (limitDist < 0.0f) limitDist = 0.0f;

    float currentX = position.x;
    float currentZ = position.z;
    float distSq = currentX * currentX + currentZ * currentZ;

    if (distSq > limitDist * limitDist)
    {
        float dist = std::sqrt(distSq);
        if (dist <= 0.0001f) return;

        float normX = currentX / dist;
        float normZ = currentZ / dist;

        position.x = normX * limitDist;
        position.z = normZ * limitDist;


        float dot = velocity.x * normX + velocity.z * normZ;

        if (dot > 0.0f)
        {
            // V_new = V - (V . N) * N
            velocity.x -= normX * dot;
            velocity.z -= normZ * dot;
        }
    }
}


