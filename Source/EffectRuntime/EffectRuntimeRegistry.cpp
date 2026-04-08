#include "EffectRuntimeRegistry.h"

#include "EffectCompiler.h"
#include "EffectGraphSerializer.h"

EffectRuntimeRegistry& EffectRuntimeRegistry::Instance()
{
    static EffectRuntimeRegistry instance;
    return instance;
}

std::shared_ptr<CompiledEffectAsset> EffectRuntimeRegistry::LoadCompiledAsset(const std::string& assetKey)
{
    auto transientIt = m_transientAssets.find(assetKey);
    if (transientIt != m_transientAssets.end()) {
        return transientIt->second;
    }

    auto cacheIt = m_compiledAssetCache.find(assetKey);
    if (cacheIt != m_compiledAssetCache.end()) {
        return cacheIt->second;
    }

    EffectGraphAsset graphAsset;
    if (!EffectGraphSerializer::Load(assetKey, graphAsset)) {
        return nullptr;
    }

    auto compiled = EffectCompiler::Compile(graphAsset, assetKey);
    m_compiledAssetCache[assetKey] = compiled;
    return compiled;
}

std::shared_ptr<CompiledEffectAsset> EffectRuntimeRegistry::GetCompiledAsset(const std::string& assetKey)
{
    return LoadCompiledAsset(assetKey);
}

void EffectRuntimeRegistry::RegisterTransientAsset(const std::string& assetKey, const std::shared_ptr<CompiledEffectAsset>& compiledAsset)
{
    if (!compiledAsset) {
        m_transientAssets.erase(assetKey);
        return;
    }
    m_transientAssets[assetKey] = compiledAsset;
}

uint32_t EffectRuntimeRegistry::Spawn(const std::string& assetKey, uint32_t seed)
{
    auto compiled = LoadCompiledAsset(assetKey);
    if (!compiled || !compiled->valid) {
        return 0;
    }

    EffectRuntimeInstance instance;
    instance.id = m_nextRuntimeInstanceId++;
    instance.assetKey = assetKey;
    instance.compiledAsset = compiled;
    instance.time = 0.0f;
    instance.seed = seed;
    instance.active = true;
    m_instances[instance.id] = instance;
    return instance.id;
}

EffectRuntimeInstance* EffectRuntimeRegistry::GetRuntimeInstance(uint32_t instanceId)
{
    auto it = m_instances.find(instanceId);
    return it != m_instances.end() ? &it->second : nullptr;
}

void EffectRuntimeRegistry::Destroy(uint32_t instanceId)
{
    m_instances.erase(instanceId);
}
