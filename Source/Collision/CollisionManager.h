// CollisionManager.h
#pragma once
#include <vector>
#include <cstdint>
#include "Collision.h"

// 1組の衝突結果を表す構造体。
// どのコライダー同士が当たったかと、詳細な HitResult をまとめて持つ。
struct CollisionContact
{
    // 衝突した片方のコライダー ID。
    uint32_t idA{ 0 };

    // 衝突したもう片方のコライダー ID。
    uint32_t idB{ 0 };

    // 接触位置、めり込み深さ、押し戻し位置などの詳細結果。
    HitResult hit{};
};

// シーン内のコライダーを一括管理するマネージャ。
// コライダーの追加・更新・削除、全接触判定、レイキャストを担当する。
class CollisionManager
{
public:
    // singleton インスタンスを返す。
    static CollisionManager& Instance();

    // 球コライダーを登録する。
    // userPtr は所有者や紐付け先を保持するための任意ポインタ。
    // attr はコライダー属性。
    // 戻り値は登録されたコライダーの一意 ID。
    uint32_t AddSphere(const SphereDesc& desc, void* userPtr = nullptr, ColliderAttribute attr = ColliderAttribute::Body);

    // カプセルコライダーを登録する。
    // 戻り値は登録されたコライダーの一意 ID。
    uint32_t AddCapsule(const CapsuleDesc& desc, void* userPtr = nullptr, ColliderAttribute attr = ColliderAttribute::Body);

    // Box コライダーを登録する。
    // 戻り値は登録されたコライダーの一意 ID。
    uint32_t AddBox(const BoxDesc& desc, void* userPtr = nullptr, ColliderAttribute attr = ColliderAttribute::Body);

    // 指定 ID の Box 形状を更新する。
    bool UpdateBox(uint32_t id, const BoxDesc& desc);

    // 指定 ID の Sphere 形状を更新する。
    bool UpdateSphere(uint32_t id, const SphereDesc& desc);

    // 指定 ID の Capsule 形状を更新する。
    bool UpdateCapsule(uint32_t id, const CapsuleDesc& desc);

    // 指定 ID の enabled 状態を変更する。
    bool SetEnabled(uint32_t id, bool enabled);

    // 指定 ID の enabled 状態を取得する。
    bool GetEnabled(uint32_t id) const;

    // 指定 ID の userPtr を設定する。
    bool SetUserPtr(uint32_t id, void* p);

    // 指定 ID の userPtr を取得する。
    void* GetUserPtr(uint32_t id) const;

    // 指定 ID のコライダー本体を取得する。
    // 見つからなければ nullptr を返す。
    const Collider* Get(uint32_t id) const;

    // 指定 ID のコライダーを削除する。
    bool Remove(uint32_t id);

    // すべてのコライダーを削除する。
    void Clear();

    // 登録中の全コライダー組み合わせについて接触判定を行い、
    // 当たっている組を outContacts へ書き出す。
    void ComputeAllContacts(std::vector<CollisionContact>& outContacts) const;

    // 現在登録されている全コライダー配列を参照で返す。
    const std::vector<Collider>& GetAll() const { return colliders_; }

    // レイキャストを行い、最も近いヒット 1 件を outHit に返す。
    // maxDistance 以内にヒットが無ければ false を返す。
    bool Raycast(const ::Ray& ray, RaycastHit& outHit, float maxDistance = FLT_MAX);

private:
    // singleton 用なのでコンストラクタは private。
    CollisionManager() = default;

    // 特別な破棄処理は不要。
    ~CollisionManager() = default;

    // コピー禁止。
    CollisionManager(const CollisionManager&) = delete;

    // 代入禁止。
    void operator=(const CollisionManager&) = delete;

    // 次に発行するコライダー ID。
    uint32_t issueId_ = 1;

    // 管理中の全コライダー一覧。
    std::vector<Collider> colliders_;
};