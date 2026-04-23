#include "EffectRuntimeRegistry.h"

#include "EffectCompiler.h"
#include "EffectGraphSerializer.h"

// singleton インスタンスを返す。
EffectRuntimeRegistry& EffectRuntimeRegistry::Instance()
{
    static EffectRuntimeRegistry instance;
    return instance;
}

// 指定 assetKey の CompiledEffectAsset を読み込む。
// まず transient、次にキャッシュを見て、無ければ .effect graph をロードしてコンパイルする。
std::shared_ptr<CompiledEffectAsset> EffectRuntimeRegistry::LoadCompiledAsset(const std::string& assetKey)
{
    // 一時登録アセットがあればそれを優先する。
    auto transientIt = m_transientAssets.find(assetKey);
    if (transientIt != m_transientAssets.end()) {
        return transientIt->second;
    }

    // 既にコンパイル済みキャッシュがあればそれを返す。
    auto cacheIt = m_compiledAssetCache.find(assetKey);
    if (cacheIt != m_compiledAssetCache.end()) {
        return cacheIt->second;
    }

    // 元の EffectGraphAsset をロードする。
    EffectGraphAsset graphAsset;
    if (!EffectGraphSerializer::Load(assetKey, graphAsset)) {
        return nullptr;
    }

    // 読み込んだグラフをコンパイルしてキャッシュする。
    auto compiled = EffectCompiler::Compile(graphAsset, assetKey);
    m_compiledAssetCache[assetKey] = compiled;
    return compiled;
}

// 指定 assetKey の CompiledEffectAsset を取得する。
// 現状は LoadCompiledAsset にそのまま委譲する。
std::shared_ptr<CompiledEffectAsset> EffectRuntimeRegistry::GetCompiledAsset(const std::string& assetKey)
{
    return LoadCompiledAsset(assetKey);
}

// 一時的な CompiledEffectAsset を登録する。
// compiledAsset が nullptr の場合は登録解除として扱う。
void EffectRuntimeRegistry::RegisterTransientAsset(const std::string& assetKey, const std::shared_ptr<CompiledEffectAsset>& compiledAsset)
{
    // nullptr が来たら transient 登録を消す。
    if (!compiledAsset) {
        m_transientAssets.erase(assetKey);
        return;
    }

    // 一時アセットを登録する。
    m_transientAssets[assetKey] = compiledAsset;
}

// 指定 assetKey のエフェクトを runtime instance として生成する。
// 成功時は instance ID、失敗時は 0 を返す。
uint32_t EffectRuntimeRegistry::Spawn(const std::string& assetKey, uint32_t seed)
{
    // 対象アセットをロードする。
    auto compiled = LoadCompiledAsset(assetKey);

    // 無効またはコンパイル失敗済みなら生成できない。
    if (!compiled || !compiled->valid) {
        return 0;
    }

    // 新しい runtime instance を作成する。
    EffectRuntimeInstance instance;
    instance.id = m_nextRuntimeInstanceId++;
    instance.assetKey = assetKey;
    instance.compiledAsset = compiled;
    instance.time = 0.0f;
    instance.seed = seed;
    instance.active = true;

    // 管理テーブルへ登録する。
    m_instances[instance.id] = instance;
    return instance.id;
}

// 指定 instanceId の runtime instance を取得する。
// 見つからなければ nullptr を返す。
EffectRuntimeInstance* EffectRuntimeRegistry::GetRuntimeInstance(uint32_t instanceId)
{
    auto it = m_instances.find(instanceId);
    return it != m_instances.end() ? &it->second : nullptr;
}

// 指定 instanceId の runtime instance を破棄する。
void EffectRuntimeRegistry::Destroy(uint32_t instanceId)
{
    m_instances.erase(instanceId);
}