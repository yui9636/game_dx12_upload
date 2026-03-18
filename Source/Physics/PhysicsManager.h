#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <memory>
#include <Entity\Entity.h>
#include <d3d11.h>
#include <DirectXMath.h>


// Joltのレイヤー定義（最低限、動的オブジェクトと静的オブジェクトを分ける）
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

struct PhysicsRaycastResult {
    bool hasHit = false;
    uint32_t entityID = 0; // 当たったエンティティ
    float distance = 0.0f;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal; // 進化ポイント：面の向きを取得
};

class PhysicsManager {
public:
    static PhysicsManager& Instance();


    void Initialize();
    void Update(float deltaTime);
    void Finalize();

    // 外部からJoltのコアシステムにアクセスするためのゲッター
    JPH::PhysicsSystem* GetJoltSystem() { return m_physicsSystem.get(); }
    JPH::BodyInterface& GetBodyInterface() { return m_physicsSystem->GetBodyInterface(); }

    PhysicsRaycastResult CastRay(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance = 1000.0f);
private:
    PhysicsManager();
    ~PhysicsManager();

    // Joltの必須コンポーネント群
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;

    // Joltの衝突フィルター（cpp側で実装）
    class BPLayerInterfaceImpl;
    class ObjectVsBroadPhaseLayerFilterImpl;
    class ObjectLayerPairFilterImpl;

    std::unique_ptr<BPLayerInterfaceImpl> m_bpLayerInterface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objVsBpFilter;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_objPairFilter;
};