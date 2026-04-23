#pragma once

#include <filesystem>
#include <string>

#include "Entity/Entity.h"
#include "Undo/EntitySnapshot.h"

class Registry;

// scene ファイルに付随する簡単なメタデータ。
// 現状は editor 側の scene view mode だけを保持する。
struct SceneFileMetadata
{
    // SceneView の表示モード。
    // 例: "3D"
    std::string sceneViewMode = "3D";
};

// Prefab / Scene の保存・読込・適用・差し戻しを担当するユーティリティクラス。
// すべて static 関数で構成される。
class PrefabSystem
{
public:
    // 指定 entity を prefab として保存する。
    // 保存ファイル名は entity 名から自動決定し、重複時は連番を付ける。
    // outPath が指定されていれば最終保存先パスを返す。
    static bool SaveEntityAsPrefab(EntityID root,
        Registry& registry,
        const std::filesystem::path& destinationDir,
        std::filesystem::path* outPath = nullptr);

    // prefab ファイルを Registry へインスタンス化する。
    // parentEntity が指定されていれば、その子として復元する。
    static EntityID InstantiatePrefab(const std::filesystem::path& prefabPath,
        Registry& registry,
        EntityID parentEntity = Entity::NULL_ID);

    // 指定 entity の subtree を明示した prefabPath へ保存する。
    static bool SaveEntityToPrefabPath(EntityID root,
        Registry& registry,
        const std::filesystem::path& prefabPath);

    // Registry 全体を scene ファイルとして保存する。
    // metadata があれば editor 用の追加情報も保存する。
    static bool SaveRegistryAsScene(Registry& registry,
        const std::filesystem::path& scenePath,
        const SceneFileMetadata* metadata = nullptr);

    // scene ファイルを読み込み、Registry をその内容へ置き換える。
    // outMetadata があれば editor 用メタデータも返す。
    static bool LoadSceneIntoRegistry(const std::filesystem::path& scenePath,
        Registry& registry,
        SceneFileMetadata* outMetadata = nullptr);

    // prefab ファイルを読み込み、EntitySnapshot として返す。
    static bool LoadPrefabSnapshot(const std::filesystem::path& prefabPath,
        EntitySnapshot::Snapshot& outSnapshot);

    // 現在の entity 内容を prefab 元ファイルへ上書き保存する。
    static bool ApplyPrefab(EntityID root, Registry& registry);

    // prefab を元ファイル内容で差し戻す。
    // 現在の entity を消し、元 prefab から再インスタンス化する。
    static EntityID RevertPrefab(EntityID root, Registry& registry);

    // prefab との紐付けだけを外す。
    // entity 自体はそのまま残す。
    static bool UnpackPrefab(EntityID root, Registry& registry);

    // 指定 entity が属する prefab のルート entity を探す。
    // 見つからなければ NULL_ID を返す。
    static EntityID FindPrefabRoot(EntityID entity, Registry& registry);

    // entity が prefab 配下なら、その prefab ルートに override フラグを立てる。
    static void MarkPrefabOverride(EntityID entity, Registry& registry);

    // entity を newParent の子へ付け替えてよいか判定する。
    static bool CanReparent(EntityID entity, EntityID newParent, Registry& registry);

    // parentEntity の子を新規作成してよいか判定する。
    static bool CanCreateChild(EntityID parentEntity, Registry& registry);

    // entity を削除してよいか判定する。
    static bool CanDelete(EntityID entity, Registry& registry);

    // entity を複製してよいか判定する。
    static bool CanDuplicate(EntityID entity, Registry& registry);
};