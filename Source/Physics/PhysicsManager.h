#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <memory>
#include <Entity\Entity.h>
#include <d3d11.h>
#include <DirectXMath.h>


namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

struct PhysicsRaycastResult {
    bool hasHit = false;
    uint32_t entityID = 0;
    float distance = 0.0f;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
};

class PhysicsManager {
public:
    static PhysicsManager& Instance();


    void Initialize();
    void Update(float deltaTime);
    void Finalize();

    JPH::PhysicsSystem* GetJoltSystem() { return m_physicsSystem.get(); }
    JPH::BodyInterface& GetBodyInterface() { return m_physicsSystem->GetBodyInterface(); }

    PhysicsRaycastResult CastRay(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance = 1000.0f);
private:
    PhysicsManager();
    ~PhysicsManager();

    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;

    class BPLayerInterfaceImpl;
    class ObjectVsBroadPhaseLayerFilterImpl;
    class ObjectLayerPairFilterImpl;

    std::unique_ptr<BPLayerInterfaceImpl> m_bpLayerInterface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objVsBpFilter;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_objPairFilter;
};
