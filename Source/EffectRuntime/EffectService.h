#pragma once

#include <cstdint>
#include <string>
#include <DirectXMath.h>

#include "Entity/Entity.h"

class Registry;

// 再生中エフェクトを指す軽量ハンドル。
// 実体は effect runtime entity の EntityID を保持する。
struct EffectHandle
{
    // effect runtime entity。
    EntityID entity = Entity::NULL_ID;

    // 有効な handle かどうかを返す。
    bool IsValid() const { return !Entity::IsNull(entity); }

    // handle を無効状態へ戻す。
    void Reset() { entity = Entity::NULL_ID; }
};

// エフェクト再生要求時に渡す記述子。
// ワールド座標、回転、スケール、seed、loop などをまとめる。
struct EffectPlayDesc
{
    // 再生する effect asset パス。
    std::string assetPath;

    // 初期位置。
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };

    // 初期回転。
    DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };

    // 初期スケール。
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

    // 乱数 seed。
    uint32_t seed = 1;

    // ループ再生するかどうか。
    bool loop = false;

    // デバッグ表示用の entity 名。
    const char* debugName = "Effect Runtime";
};

// effect runtime entity の生成・停止・再生位置変更・パラメータ override などを行うサービス。
// 内部 Registry を使う形と、明示 Registry を渡す形の両方を提供する。
class EffectService
{
public:
    // singleton インスタンスを返す。
    static EffectService& Instance();

    // 内部で使う Registry を設定する。
    void SetRegistry(Registry* registry) { m_registry = registry; }

    // 現在設定されている Registry を返す。
    Registry* GetRegistry() const { return m_registry; }

    // 内部 Registry 上でワールドエフェクトを再生する。
    EffectHandle PlayWorld(const EffectPlayDesc& desc);

    // 明示された Registry 上でワールドエフェクトを再生する。
    EffectHandle PlayWorld(Registry& registry, const EffectPlayDesc& desc);

    // 内部 Registry 上で handle の生存を判定する。
    bool IsAlive(const EffectHandle& handle) const;

    // 明示された Registry 上で handle の生存を判定する。
    bool IsAlive(Registry& registry, const EffectHandle& handle) const;

    // 内部 Registry 上で effect を停止する。
    // destroyEntity が true なら entity 自体も削除する。
    void Stop(const EffectHandle& handle, bool destroyEntity = true);

    // 明示された Registry 上で effect を停止する。
    void Stop(Registry& registry, const EffectHandle& handle, bool destroyEntity = true);

    // 内部 Registry 上で effect の再生位置を変更する。
    void Seek(const EffectHandle& handle, float time, float duration, bool loop);

    // 明示された Registry 上で effect の再生位置を変更する。
    void Seek(Registry& registry, const EffectHandle& handle, float time, float duration, bool loop);

    // 内部 Registry 上で effect の transform を直接設定する。
    void SetWorldTransform(const EffectHandle& handle,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& rotation,
        const DirectX::XMFLOAT3& scale);

    // 明示された Registry 上で effect の transform を直接設定する。
    void SetWorldTransform(Registry& registry,
        const EffectHandle& handle,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& rotation,
        const DirectX::XMFLOAT3& scale);

    // 内部 Registry 上で worldMatrix を分解して transform を設定する。
    void SetWorldMatrix(const EffectHandle& handle, const DirectX::XMFLOAT4X4& worldMatrix);

    // 明示された Registry 上で worldMatrix を分解して transform を設定する。
    void SetWorldMatrix(Registry& registry, const EffectHandle& handle, const DirectX::XMFLOAT4X4& worldMatrix);

    // 内部 Registry 上で float パラメータ override を設定する。
    void SetScalar(const EffectHandle& handle, const std::string& parameterName, float value);

    // 明示された Registry 上で float パラメータ override を設定する。
    void SetScalar(Registry& registry, const EffectHandle& handle, const std::string& parameterName, float value);

    // 内部 Registry 上で color パラメータ override を設定する。
    void SetColor(const EffectHandle& handle, const std::string& parameterName, const DirectX::XMFLOAT4& value);

    // 明示された Registry 上で color パラメータ override を設定する。
    void SetColor(Registry& registry, const EffectHandle& handle, const std::string& parameterName, const DirectX::XMFLOAT4& value);

private:
    // 実際に effect runtime entity を生成する内部処理。
    EffectHandle PlayWorldInternal(Registry& registry, const EffectPlayDesc& desc);

    // effect を停止する内部処理。
    void StopInternal(Registry& registry, const EffectHandle& handle, bool destroyEntity);

    // effect の再生位置を変更する内部処理。
    void SeekInternal(Registry& registry, const EffectHandle& handle, float time, float duration, bool loop);

    // effect の transform を更新する内部処理。
    void SetWorldTransformInternal(Registry& registry,
        const EffectHandle& handle,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& rotation,
        const DirectX::XMFLOAT3& scale);

    // float パラメータ override を設定する内部処理。
    void SetScalarInternal(Registry& registry, const EffectHandle& handle, const std::string& parameterName, float value);

    // color パラメータ override を設定する内部処理。
    void SetColorInternal(Registry& registry, const EffectHandle& handle, const std::string& parameterName, const DirectX::XMFLOAT4& value);

    // effect runtime entity に最低限必要な component を付与する。
    void EnsureBaseComponents(Registry& registry, EntityID entity, const char* debugName) const;

    // 内部保持している Registry。
    Registry* m_registry = nullptr;
};