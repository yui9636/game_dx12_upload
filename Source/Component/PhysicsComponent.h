#pragma once
#include "Jolt/jolt.h"
#include <Jolt/Physics/Body/BodyID.h>

// Joltの世界における肉体のIDだけを保持する、純粋なデータ
struct PhysicsComponent {
    JPH::BodyID bodyID;
};