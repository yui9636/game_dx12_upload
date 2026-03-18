#pragma once
#include <cstdint>
#include <DirectXMath.h>

// ---------------- 基本結果 ----------------
struct HitResult
{
    DirectX::XMFLOAT3 hitPosition{};       // 代表/最近接点
    float             penetrationDepth{};  // 貫通量 >= 0
    DirectX::XMFLOAT3 selfOutPosition{};   // 自身の押し出し後基準点
    DirectX::XMFLOAT3 otherOutPosition{};  // 相手の押し出し後基準点
};

// ---------------- 形状 ----------------
enum class ColliderShape : uint8_t { Sphere = 0, Capsule = 1, Box = 2}; // Capsule: Y-up

// ★追加: 当たり判定の属性 (体か、攻撃か)
enum class ColliderAttribute : uint8_t
{
    Body = 0,   // 物理的な体（押し合い対象）
    Attack = 1  // 攻撃判定（ダメージ発生源・押し合いしない）
};

struct SphereDesc {
    DirectX::XMFLOAT3 center{ 0,0,0 };
    float             radius{ 0.5f };
};

// Capsule は「base から +Y に height（円柱長）」、全長=height+2*radius
struct CapsuleDesc {
    DirectX::XMFLOAT3 base{ 0,0,0 };
    float             radius{ 0.5f };
    float             height{ 1.0f };
};

struct BoxDesc {
    DirectX::XMFLOAT3 center{ 0,0,0 };
    DirectX::XMFLOAT3 size{ 1.0f, 1.0f, 1.0f }; // 幅・高さ・奥行き
};



// ---------------- コライダー（軽量プロキシ） ----------------
struct Collider
{
    uint32_t      id{ 0 };                 // Manager 内ユニークID
    ColliderShape shape{ ColliderShape::Sphere };

    ColliderAttribute attribute{ ColliderAttribute::Body };

    bool          enabled{ true };
    void* userPtr{ nullptr };      // Actor* 等の任意ハンドル

    SphereDesc    sphere{};
    CapsuleDesc   capsule{};
    BoxDesc       box{};
};

// レイ（始点と方向）
struct Ray
{
    DirectX::XMFLOAT3 origin{ 0,0,0 };    // 発射地点
    DirectX::XMFLOAT3 direction{ 0,0,1 }; // 方向（※正規化必須）
};

// レイキャストの結果
struct RaycastHit
{
    float             distance = FLT_MAX; // 衝突点までの距離
    DirectX::XMFLOAT3 point{ 0,0,0 };     // 衝突座標
    DirectX::XMFLOAT3 normal{ 0,1,0 };    // 衝突面の法線
    void* userPtr = nullptr;  // 当たったコライダーのユーザーデータ（Actor等）

    // ヘルパー: 何かに当たったか？
    bool IsHit() const { return userPtr != nullptr; }
};

