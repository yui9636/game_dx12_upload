#pragma once
#include <filesystem>
#include <string>

// Bundle of asset paths + numeric stats describing one enemy archetype.
// Read-only at runtime. EnemyRuntimeSetup uses this to construct entities.
struct EnemyConfigAsset
{
    int          version = 1;
    std::string  name;

    // Asset references
    std::string  behaviorTreePath;
    std::string  stateMachinePath;
    std::string  timelinePath;
    std::string  modelPath;
    std::string  animatorPath;

    // Stats
    float        maxHealth     = 100.0f;
    float        walkSpeed     = 2.0f;
    float        runSpeed      = 4.5f;
    float        turnSpeed     = 540.0f;   // deg/sec

    // Perception default
    float        sightRadius   = 10.0f;
    float        sightFOV      = 1.5708f;
    float        hearingRadius = 0.0f;

    // Combat
    float        baseAttack    = 10.0f;

    bool LoadFromFile(const std::filesystem::path& path);
    bool SaveToFile(const std::filesystem::path& path) const;

    // Built-in preset.
    static EnemyConfigAsset CreateAggressiveKnight();
};
