#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "EffectGraphAsset.h"

// 実行中エフェクト 1 件ぶんの runtime 情報。
// どの compiled asset を使っているか、経過時間や seed などを保持する。
struct EffectRuntimeInstance
{
    // runtime instance の一意 ID。
    uint32_t id = 0;

    // 元になったアセットキー。
    std::string assetKey;

    // コンパイル済みエフェクトアセット参照。
    std::shared_ptr<CompiledEffectAsset> compiledAsset;

    // 再生経過時間。
    float time = 0.0f;

    // 乱数 seed。
    uint32_t seed = 1;

    // 現在アクティブかどうか。
    bool active = true;
};

// エフェクトの compiled asset キャッシュと runtime instance を管理するレジストリ。
// 永続キャッシュ、一時登録アセット、実行中インスタンスをまとめて扱う。
class EffectRuntimeRegistry
{
public:
    // singleton インスタンスを返す。
    static EffectRuntimeRegistry& Instance();

    // 指定 assetKey の compiled asset を取得する。
    // 必要ならロードとコンパイルも行う。
    std::shared_ptr<CompiledEffectAsset> GetCompiledAsset(const std::string& assetKey);

    // 一時的な compiled asset を登録する。
    // エディタプレビューなど、ファイル保存前の差し替え用途を想定する。
    // nullptr を渡した場合は一時登録を解除する。
    void RegisterTransientAsset(const std::string& assetKey, const std::shared_ptr<CompiledEffectAsset>& compiledAsset);

    // 指定 assetKey のエフェクトを runtime instance として生成する。
    // 成功時は instance ID、失敗時は 0 を返す。
    uint32_t Spawn(const std::string& assetKey, uint32_t seed);

    // 指定 instanceId の runtime instance を取得する。
    // 見つからなければ nullptr を返す。
    EffectRuntimeInstance* GetRuntimeInstance(uint32_t instanceId);

    // 指定 instanceId の runtime instance を破棄する。
    void Destroy(uint32_t instanceId);

private:
    // 指定 assetKey の compiled asset を読み込む。
    // まず transient、次にキャッシュを見て、無ければ元 graph をロードしてコンパイルする。
    std::shared_ptr<CompiledEffectAsset> LoadCompiledAsset(const std::string& assetKey);

    // 永続的な compiled asset キャッシュ。
    std::unordered_map<std::string, std::shared_ptr<CompiledEffectAsset>> m_compiledAssetCache;

    // 一時差し替え用 compiled asset。
    // 同じ assetKey があれば通常キャッシュより優先される。
    std::unordered_map<std::string, std::shared_ptr<CompiledEffectAsset>> m_transientAssets;

    // 実行中 runtime instance 一覧。
    std::unordered_map<uint32_t, EffectRuntimeInstance> m_instances;

    // 次に払い出す runtime instance ID。
    uint32_t m_nextRuntimeInstanceId = 1;
};