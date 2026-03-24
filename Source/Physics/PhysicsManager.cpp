#include "PhysicsManager.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <cstdarg>
#include <iostream>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Body/Body.h>

using namespace JPH;

PhysicsManager& PhysicsManager::Instance() {
    static PhysicsManager instance;
    return instance;
}

static void TraceImpl(const char* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    std::cout << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine) {
    std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage ? inMessage : "") << std::endl;
    return true;
}
#endif

namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS(2);
}

class PhysicsManager::BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }
    virtual uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
        return mObjectToBroadPhase[inLayer];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
        return (inLayer == BroadPhaseLayers::NON_MOVING) ? "NON_MOVING" : "MOVING";
    }
#endif
private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class PhysicsManager::ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
        case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:     return true;
        default:                 return false;
        }
    }
};

class PhysicsManager::ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
        switch (inObject1) {
        case Layers::NON_MOVING: return inObject2 == Layers::MOVING;
        case Layers::MOVING:     return true;
        default:                 return false;
        }
    }
};

void PhysicsManager::Initialize() {
    RegisterDefaultAllocator();

    Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

        Factory::sInstance = new Factory();
    RegisterTypes();

    m_tempAllocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);
    m_jobSystem = std::make_unique<JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

    m_bpLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
    m_objVsBpFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    m_objPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

    m_physicsSystem = std::make_unique<PhysicsSystem>();
    m_physicsSystem->Init(1024, 0, 1024, 1024, *m_bpLayerInterface, *m_objVsBpFilter, *m_objPairFilter);
}

void PhysicsManager::Update(float deltaTime) {
    if (m_physicsSystem) {
        m_physicsSystem->Update(deltaTime, 1, m_tempAllocator.get(), m_jobSystem.get());
    }
}

PhysicsRaycastResult PhysicsManager::CastRay(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance) {
    PhysicsRaycastResult result;
    if (!m_physicsSystem) return result;

    JPH::RVec3 joltOrigin(origin.x, origin.y, origin.z);
    JPH::Vec3 joltDir(direction.x, direction.y, direction.z);

    JPH::RRayCast ray(joltOrigin, joltDir * maxDistance);
    JPH::RayCastResult joltResult;

    if (m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, joltResult)) {
        result.hasHit = true;
        result.distance = joltResult.mFraction * maxDistance;

        JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), joltResult.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            result.entityID = static_cast<uint32_t>(body.GetUserData());

            JPH::RVec3 hitPos = joltOrigin + joltDir * result.distance;
            result.position = { (float)hitPos.GetX(), (float)hitPos.GetY(), (float)hitPos.GetZ() };

            JPH::Vec3 n = body.GetWorldSpaceSurfaceNormal(joltResult.mSubShapeID2, hitPos);
            result.normal = { n.GetX(), n.GetY(), n.GetZ() };
        }
    }
    return result;
}



void PhysicsManager::Finalize() {
    m_physicsSystem.reset();
    m_jobSystem.reset();
    m_tempAllocator.reset();

    m_objPairFilter.reset();
    m_objVsBpFilter.reset();
    m_bpLayerInterface.reset();

    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
}

PhysicsManager::PhysicsManager() = default;
PhysicsManager::~PhysicsManager() = default;
