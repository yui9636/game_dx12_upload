#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "EffectGraphAsset.h"

struct EffectRuntimeInstance
{
    uint32_t id = 0;
    std::string assetKey;
    std::shared_ptr<CompiledEffectAsset> compiledAsset;
    float time = 0.0f;
    uint32_t seed = 1;
    bool active = true;
};

class EffectRuntimeRegistry
{
public:
    static EffectRuntimeRegistry& Instance();

    std::shared_ptr<CompiledEffectAsset> GetCompiledAsset(const std::string& assetKey);
    void RegisterTransientAsset(const std::string& assetKey, const std::shared_ptr<CompiledEffectAsset>& compiledAsset);
    uint32_t Spawn(const std::string& assetKey, uint32_t seed);
    EffectRuntimeInstance* GetRuntimeInstance(uint32_t instanceId);
    void Destroy(uint32_t instanceId);

private:
    std::shared_ptr<CompiledEffectAsset> LoadCompiledAsset(const std::string& assetKey);

    std::unordered_map<std::string, std::shared_ptr<CompiledEffectAsset>> m_compiledAssetCache;
    std::unordered_map<std::string, std::shared_ptr<CompiledEffectAsset>> m_transientAssets;
    std::unordered_map<uint32_t, EffectRuntimeInstance> m_instances;
    uint32_t m_nextRuntimeInstanceId = 1;
};
