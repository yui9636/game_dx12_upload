#pragma once
#include <cstdint>
#include <cstddef> // size_t

// コンポーネントの種類を表すID
using ComponentTypeID = uint32_t;

// エンティティが持てるコンポーネントの最大数（Bitsetのサイズに直結します）
constexpr uint32_t MAX_COMPONENTS = 64;

// コンポーネントのメモリレイアウト情報を保持する構造体
struct ComponentMetadata {
    size_t size;
    size_t alignment;
};

class TypeManager {
private:
    // 内部でIDをカウントアップするための静的変数
    static ComponentTypeID GetNextTypeID() {
        static ComponentTypeID s_componentCounter = 0;
        return s_componentCounter++;
    }

public:
    // テンプレート関数: 型 T を渡すたびに、その型専用の固定IDを返す
    // 例: GetComponentTypeID<Transform>() -> 常に 0
    // 例: GetComponentTypeID<Velocity>()  -> 常に 1
    template <typename T>
    static ComponentTypeID GetComponentTypeID() {
        // 関数内の静的ローカル変数は、型 T ごとに独立して1度だけ初期化される特性を利用
        static const ComponentTypeID id = GetNextTypeID();
        return id;
    }

    // 型 T のメモリサイズとアライメントを取得する
    template <typename T>
    static ComponentMetadata GetComponentMetadata() {
        return { sizeof(T), alignof(T) };
    }
};