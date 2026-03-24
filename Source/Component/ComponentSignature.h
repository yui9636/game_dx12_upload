#pragma once
#include <bitset>
#include "Type/TypeInfo.h"

using Signature = std::bitset<MAX_COMPONENTS>;

template <typename... Ts>
inline Signature CreateSignature() {
    Signature sig;

    if constexpr (sizeof...(Ts) > 0) {
        (sig.set(TypeManager::GetComponentTypeID<Ts>()), ...);
    }

    return sig;
}

inline bool SignatureMatches(const Signature& archetypeSig, const Signature& querySig) {
    return (archetypeSig & querySig) == querySig;
}
