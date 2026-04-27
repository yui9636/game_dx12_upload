#include "EnemyConfigAsset.h"

#include <fstream>

#include <nlohmann/json.hpp>

bool EnemyConfigAsset::LoadFromFile(const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    if (!ifs) return false;
    nlohmann::json j;
    try { ifs >> j; } catch (...) { return false; }

    EnemyConfigAsset out;
    out.version          = j.value("version", 1);
    out.name             = j.value("name", std::string{});
    out.behaviorTreePath = j.value("behaviorTreePath", std::string{});
    out.stateMachinePath = j.value("stateMachinePath", std::string{});
    out.timelinePath     = j.value("timelinePath",     std::string{});
    out.modelPath        = j.value("modelPath",        std::string{});
    out.animatorPath     = j.value("animatorPath",     std::string{});
    out.maxHealth        = j.value("maxHealth",     100.0f);
    out.walkSpeed        = j.value("walkSpeed",     2.0f);
    out.runSpeed         = j.value("runSpeed",      4.5f);
    out.turnSpeed        = j.value("turnSpeed",     540.0f);
    out.sightRadius      = j.value("sightRadius",   10.0f);
    out.sightFOV         = j.value("sightFOV",      1.5708f);
    out.hearingRadius    = j.value("hearingRadius", 0.0f);
    out.baseAttack       = j.value("baseAttack",    10.0f);
    *this = std::move(out);
    return true;
}

bool EnemyConfigAsset::SaveToFile(const std::filesystem::path& path) const
{
    nlohmann::json j;
    j["version"]          = version;
    j["name"]             = name;
    j["behaviorTreePath"] = behaviorTreePath;
    j["stateMachinePath"] = stateMachinePath;
    j["timelinePath"]     = timelinePath;
    j["modelPath"]        = modelPath;
    j["animatorPath"]     = animatorPath;
    j["maxHealth"]        = maxHealth;
    j["walkSpeed"]        = walkSpeed;
    j["runSpeed"]         = runSpeed;
    j["turnSpeed"]        = turnSpeed;
    j["sightRadius"]      = sightRadius;
    j["sightFOV"]         = sightFOV;
    j["hearingRadius"]    = hearingRadius;
    j["baseAttack"]       = baseAttack;

    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << j.dump(2);
    return true;
}

EnemyConfigAsset EnemyConfigAsset::CreateAggressiveKnight()
{
    EnemyConfigAsset c;
    c.name             = "AggressiveKnight";
    c.behaviorTreePath = "Data/AI/BehaviorTrees/AggressiveKnight.bt";
    c.maxHealth        = 100.0f;
    c.walkSpeed        = 2.0f;
    c.runSpeed         = 4.5f;
    c.turnSpeed        = 540.0f;
    c.sightRadius      = 10.0f;
    c.sightFOV         = 1.5708f;
    c.hearingRadius    = 0.0f;
    c.baseAttack       = 10.0f;
    return c;
}
