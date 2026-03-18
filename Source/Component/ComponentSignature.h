#pragma once
#include <bitset>
#include "Type/TypeInfo.h" // TypeManagerがあるファイル名に合わせてください

// シグネチャの定義。MAX_COMPONENTS(64)個のON/OFFフラグを保持する
using Signature = std::bitset<MAX_COMPONENTS>;

// ユーティリティ: 複数の型を指定して、合成されたシグネチャを作る（C++17のFold式を使用）
// 例: CreateSignature<Transform, Velocity>()
// ※ 1つの型の場合も、複数の型の場合も、すべてこの1つの関数で処理します！
template <typename... Ts>
inline Signature CreateSignature() {
    Signature sig;

    // sizeof...(Ts) で引数の数をコンパイル時に判定
    if constexpr (sizeof...(Ts) > 0) {
        // 展開して各型のビットを立てる (カンマ演算子の Fold Expression)
        (sig.set(TypeManager::GetComponentTypeID<Ts>()), ...);
    }

    return sig;
}

// シグネチャ A が シグネチャ B の条件を完全に満たしているかチェックする
// Query（検索）システムで「TransformとVelocityを持つアーキタイプか？」を判定するのに使います
inline bool SignatureMatches(const Signature& archetypeSig, const Signature& querySig) {
    // querySig で立っているビットが、archetypeSig でも全て立っていれば true
    return (archetypeSig & querySig) == querySig;
}