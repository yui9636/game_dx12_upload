#pragma once
#include <cstdint>
#include <DirectXMath.h>

struct HitResult
{
    DirectX::XMFLOAT3 hitPosition{};
    float             penetrationDepth{};
    DirectX::XMFLOAT3 selfOutPosition{};
    DirectX::XMFLOAT3 otherOutPosition{};
};

enum class ColliderShape : uint8_t { Sphere = 0, Capsule = 1, Box = 2}; // Capsule: Y-up

enum class ColliderAttribute : uint8_t
{
    Body = 0,
    Attack = 1
};

struct SphereDesc {
    DirectX::XMFLOAT3 center{ 0,0,0 };
    float             radius{ 0.5f };
};

struct CapsuleDesc {
    DirectX::XMFLOAT3 base{ 0,0,0 };
    float             radius{ 0.5f };
    float             height{ 1.0f };
};

struct BoxDesc {
    DirectX::XMFLOAT3 center{ 0,0,0 };
    DirectX::XMFLOAT3 size{ 1.0f, 1.0f, 1.0f };
};



struct Collider
{
    uint32_t      id{ 0 };
    ColliderShape shape{ ColliderShape::Sphere };

    ColliderAttribute attribute{ ColliderAttribute::Body };

    bool          enabled{ true };
    void* userPtr{ nullptr };

    SphereDesc    sphere{};
    CapsuleDesc   capsule{};
    BoxDesc       box{};
};

struct Ray
{
    DirectX::XMFLOAT3 origin{ 0,0,0 };
    DirectX::XMFLOAT3 direction{ 0,0,1 };
};

struct RaycastHit
{
    float             distance = FLT_MAX;
    DirectX::XMFLOAT3 point{ 0,0,0 };
    DirectX::XMFLOAT3 normal{ 0,1,0 };
    void* userPtr = nullptr;

    bool IsHit() const { return userPtr != nullptr; }
};

