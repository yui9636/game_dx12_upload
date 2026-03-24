#pragma once
#include "Registry/Registry.h"

class PhysicsSyncSystem {
public:
    void Update(Registry& registry, bool isSimulation);
};
