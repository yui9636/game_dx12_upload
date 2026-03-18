#pragma once
#include "Registry/Registry.h"

class PhysicsSyncSystem {
public:
    // isSimulation: •¨—‰‰Z‚ğ‘–‚ç‚¹‚Ä‚¢‚é‚©‚Ç‚¤‚©iGameMode or EditorModej
    void Update(Registry& registry, bool isSimulation);
};