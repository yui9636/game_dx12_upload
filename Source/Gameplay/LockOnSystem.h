#pragma once

class Registry;

// Player-side lock-on. On rising-edge of the "LockOn" input action,
// either acquires the closest enemy in fov and binds the third-person
// camera target to it, or releases the lock and rebinds the camera to
// the player. Run before TransformSystem so the camera can react the
// same frame.
class LockOnSystem {
public:
    static void Update(Registry& registry, float dt);
};
