#include "PlayerEditorSession.h"

#include "PlayerEditorPanel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <initializer_list>

#include "Asset/PrefabSystem.h"
#include "Animator/AnimatorService.h"
#include "Component/EffectPreviewTagComponent.h"
#include "Component/MeshComponent.h"
#include "Component/NameComponent.h"
#include "Component/NodeSocketComponent.h"
#include "Component/PrefabInstanceComponent.h"
#include "Component/TransformComponent.h"
#include "StateMachineAssetSerializer.h"
#include "TimelineAssetSerializer.h"
#include "Gameplay/PlayerRuntimeSetup.h"
#include "Gameplay/PlaybackComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/StateMachineSystem.h"
#include "Gameplay/TimelineAssetRuntimeBuilder.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Input/InputBindingComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "System/ResourceManager.h"

namespace
{
    // Timelineファイルを開く/保存するときのダイアログ用フィルタ。
    static constexpr const char* kTimelineFileFilter =
        "Timeline (*.timeline.json)\0*.timeline.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // StateMachineファイルを開く/保存するときのダイアログ用フィルタ。
    static constexpr const char* kStateMachineFileFilter =
        "StateMachine (*.statemachine.json)\0*.statemachine.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // InputMapファイルを開く/保存するときのダイアログ用フィルタ。
    static constexpr const char* kInputMapFileFilter =
        "Input Map (*.inputmap.json)\0*.inputmap.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // Prefabファイルを保存するときのダイアログ用フィルタ。
    static constexpr const char* kPrefabFileFilter =
        "Prefab (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";

    // 指定されたパスの拡張子が、許可された拡張子リストに含まれているか確認する。
    static bool HasExtension(const std::string& path, std::initializer_list<const char*> extensions)
    {
        // パスから拡張子だけを取り出す。
        std::string ext = std::filesystem::path(path).extension().string();

        // 拡張子を小文字に変換して、大文字小文字の違いで失敗しないようにする。
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // 許可された拡張子と一致するか調べる。
        for (const char* candidate : extensions) {
            if (ext == candidate) {
                return true;
            }
        }

        // 一致する拡張子が無ければ不正なファイルとして扱う。
        return false;
    }
}

// PlayerEditorのプレビュー状態を一時停止する。
// Timeline再生、PreviewState、所有Preview Entityを止める。
void PlayerEditorSession::Suspend(PlayerEditorPanel& panel)
{
    // 再生ヘッドを先頭に戻す。
    panel.m_playheadFrame = 0;

    // Timeline再生状態を停止にする。
    panel.m_isPlaying = false;

    // PreviewStateに現在のTimeline再生位置を反映する。
    SyncPreviewTimelinePlayback(panel);

    // Animation previewが動いているなら終了して元のAnimator状態へ戻す。
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // PlayerEditorが自分で作ったPreview Entityを破棄する。
    DestroyOwnedPreviewEntity(panel);
}

// PlayerEditorが所有しているPreview Entityを破棄する。
// 外部から選択されたEntityは破棄せず、自分で作ったものだけを消す。
void PlayerEditorSession::DestroyOwnedPreviewEntity(PlayerEditorPanel& panel)
{
    // PlayerEditor所有のEntityでなければ破棄しない。
    if (!panel.m_previewEntityOwned) {
        return;
    }

    // PreviewStateが動いているなら先に止める。
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // Registryがあり、Preview Entityが生存しているなら破棄する。
    if (panel.m_registry && !Entity::IsNull(panel.m_previewEntity) && panel.m_registry->IsAlive(panel.m_previewEntity)) {
        panel.m_registry->DestroyEntity(panel.m_previewEntity);
    }

    // Preview Entity情報を空に戻す。
    panel.m_previewEntity = Entity::NULL_ID;
    panel.m_previewEntityOwned = false;
}

// PlayerEditor専用のPreview Entityを作成する。
// モデルを開いたとき、Scene上にプレビュー用Entityを作ってMeshComponentを付ける。
void PlayerEditorSession::EnsureOwnedPreviewEntity(PlayerEditorPanel& panel)
{
    // Registryが無い、またはモデルが開かれていないなら作れない。
    if (!panel.m_registry || !panel.HasOpenModel()) {
        return;
    }

    // 既に使用可能なPreview Entityがあるなら新しく作らない。
    if (panel.CanUsePreviewEntity()) {
        return;
    }

    // 古い所有Preview Entityが残っている場合は先に破棄する。
    DestroyOwnedPreviewEntity(panel);

    // 新しいEntityをRegistryに作る。
    panel.m_previewEntity = panel.m_registry->CreateEntity();

    // このEntityはPlayerEditorが作ったものとして管理する。
    panel.m_previewEntityOwned = true;

    // デバッグやHierarchy表示用の名前を付ける。
    panel.m_registry->AddComponent(panel.m_previewEntity, NameComponent{ "Player Preview" });

    // 原点に置くためのTransformComponentを作る。
    TransformComponent transform{};

    // Preview Entityの位置は原点。
    transform.localPosition = { 0.0f, 0.0f, 0.0f };

    // Preview Entityのスケールは等倍。
    transform.localScale = { 1.0f, 1.0f, 1.0f };

    DirectX::XMStoreFloat4x4(
        &transform.worldMatrix,
        DirectX::XMMatrixIdentity());

    DirectX::XMStoreFloat4x4(
        &transform.prevWorldMatrix,
        DirectX::XMMatrixIdentity());

    transform.worldPosition = transform.localPosition;
    transform.worldRotation = transform.localRotation;
    transform.worldScale = transform.localScale;



    // Transform更新が必要だと知らせる。
    transform.isDirty = true;

    // EntityにTransformComponentを追加する。
    panel.m_registry->AddComponent(panel.m_previewEntity, transform);

    // 描画用のMeshComponentを作る。
    MeshComponent mesh{};

    // モデルファイルパスを入れる。
    mesh.modelFilePath = panel.m_currentModelPath;

    // ResourceManagerから取得したモデル本体を入れる。
    mesh.model = panel.m_ownedModel;

    // 表示状態を有効にする。
    mesh.isVisible = true;

    // 影を落とす設定にする。
    mesh.castShadow = true;

    // EntityにMeshComponentを追加する。
    // ここまで来れば「モデルを持ったEntity」は作られている。
    panel.m_registry->AddComponent(panel.m_previewEntity, mesh);

    // EffectPreview用の目印Componentを付ける。
    panel.m_registry->AddComponent(panel.m_previewEntity, EffectPreviewTagComponent{});

    // Timeline、StateMachine、InputMap、SocketなどのEditor側データをPreview Entityへ反映する。
    ApplyEditorBindingsToPreviewEntity(panel);
}

// PlayerEditorのPreview対象を、外部Entityに差し替える。
// つまり「PlayerEditor専用Entity」ではなく、既存Scene上のEntityを編集対象にする。
void PlayerEditorSession::SetPreviewEntity(PlayerEditorPanel& panel, EntityID entity)
{
    // 既に同じ外部EntityをPreview対象にしているなら何もしない。
    if (panel.m_previewEntity == entity && !panel.m_previewEntityOwned) {
        return;
    }

    // 再生中なら止める。
    panel.m_isPlaying = false;

    // PreviewStateが動いているなら終了する。
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // 所有Preview Entityがあるなら破棄する。
    DestroyOwnedPreviewEntity(panel);

    // 外部EntityをPreview対象にする。
    panel.m_previewEntity = entity;

    // 外部EntityなのでPlayerEditor所有ではない。
    panel.m_previewEntityOwned = false;

    // 外部EntityからSocket情報を読み込む。
    ImportSocketsFromPreviewEntity(panel);
}

// EditorLayerなど外部選択から、PlayerEditorへ現在選択Entity情報を同期する。
void PlayerEditorSession::SyncExternalSelection(PlayerEditorPanel& panel, EntityID entity, const std::string& modelPath)
{
    // 現在選択されているEntityを保存する。
    panel.m_selectedEntity = entity;

    // そのEntityのモデルパスを保存する。
    panel.m_selectedEntityModelPath = modelPath;

    // Preview Entityが既に死んでいるなら参照を空にする。
    if (!Entity::IsNull(panel.m_previewEntity) && panel.m_registry && !panel.m_registry->IsAlive(panel.m_previewEntity)) {
        panel.m_previewEntity = Entity::NULL_ID;
        panel.m_previewEntityOwned = false;
    }
}

// モデルファイルを開き、PlayerEditorのPreview用モデルとして設定する。
bool PlayerEditorSession::OpenModelFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // 空パスなら失敗。
    if (path.empty()) {
        return false;
    }

 
    // PlayerEditorのPreview Entityは通常Scene描画に乗せるため、
    std::shared_ptr<Model> model = ResourceManager::Instance().CreateModelInstance(path);
    if (!model) {
        return false;
    }


    // 既存Previewや再生状態を停止・破棄する。
    Suspend(panel);

    // PlayerEditorが所有するモデルとして保存する。
    panel.m_ownedModel = std::move(model);

    // 生ポインタ参照も保持する。
    panel.m_model = panel.m_ownedModel.get();

    // 現在開いているモデルパスを保存する。
    panel.m_currentModelPath = path;

    // Bone/Socket/Timeline選択などをリセットする。
    panel.ResetSelectionState();

    // 最初のAnimationを選択状態にする。
    panel.m_selectedAnimIndex = 0;

    // モデル表示スケールを初期値に戻す。
    panel.m_previewModelScale = 1.0f;

    // Preview描画サイズをリセットする。
    panel.m_previewRenderSize = { 0.0f, 0.0f };

    // Socketリストを空にする。
    panel.m_sockets.clear();

    // Dirtyフラグを初期化する。
    panel.m_socketDirty = false;
    panel.m_timelineDirty = false;
    panel.m_stateMachineDirty = false;

    // Preview Entityを作成する。
    EnsureOwnedPreviewEntity(panel);

    // モデルを開けたので成功。
    return true;
}

// Timelineファイルを読み込む。
bool PlayerEditorSession::OpenTimelineFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // 空パス、または不正な拡張子なら失敗。
    if (path.empty() || !HasExtension(path, { ".timeline.json", ".json" })) {
        return false;
    }

    // TimelineAssetをJSONから読み込む。
    if (!TimelineAssetSerializer::Load(path, panel.m_timelineAsset)) {
        return false;
    }

    // 読み込んだTimelineパスを保存する。
    panel.m_timelineAssetPath = path;

    // 読み込み直後なのでDirtyではない。
    panel.m_timelineDirty = false;

    // Preview Entity用のTimeline runtime dataを再構築する。
    RebuildPreviewTimelineRuntimeData(panel);

    return true;
}

// StateMachineファイルを読み込む。
bool PlayerEditorSession::OpenStateMachineFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // 空パス、または不正な拡張子なら失敗。
    if (path.empty() || !HasExtension(path, { ".statemachine.json", ".json" })) {
        return false;
    }

    // StateMachineAssetをJSONから読み込む。
    if (!StateMachineAssetSerializer::Load(path, panel.m_stateMachineAsset)) {
        return false;
    }

    // 読み込んだStateMachineパスを保存する。
    panel.m_stateMachineAssetPath = path;

    // 読み込み直後なのでDirtyではない。
    panel.m_stateMachineDirty = false;

    return true;
}

// InputMapファイルを読み込む。
bool PlayerEditorSession::OpenInputMapFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // InputMappingTab側のOpen処理へ委譲する。
    return panel.m_inputMappingTab.OpenActionMap(path);
}

// Timelineドキュメントを保存する。
bool PlayerEditorSession::SaveTimelineDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 現在のTimeline保存先を取得する。
    std::string path = panel.m_timelineAssetPath;

    // SaveAs指定、または保存先未設定なら保存ダイアログを出す。
    if (saveAs || path.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 既存パスがあれば初期値に使う。
        if (!path.empty()) {
            strcpy_s(pathBuffer, path.c_str());
        }
        // Timeline名があれば、それを使って初期保存先を作る。
        else if (!panel.m_timelineAsset.name.empty()) {
            strcpy_s(pathBuffer, ("Assets/Timeline/" + panel.m_timelineAsset.name + ".timeline.json").c_str());
        }

        // 保存ダイアログを開く。
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kTimelineFileFilter, "Save Timeline", "json") != DialogResult::OK) {
            return false;
        }

        // 保存先を確定する。
        path = pathBuffer;
    }

    // TimelineAssetをJSONへ保存する。
    if (!TimelineAssetSerializer::Save(path, panel.m_timelineAsset)) {
        return false;
    }

    // 保存先を記録し、Dirtyを解除する。
    panel.m_timelineAssetPath = path;
    panel.m_timelineDirty = false;

    // StateMachine/Timeline runtime cacheを無効化する。
    StateMachineSystem::InvalidateAssetCache(path.c_str());

    return true;
}

// StateMachineドキュメントを保存する。
bool PlayerEditorSession::SaveStateMachineDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 現在のStateMachine保存先を取得する。
    std::string path = panel.m_stateMachineAssetPath;

    // SaveAs指定、または保存先未設定なら保存ダイアログを出す。
    if (saveAs || path.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 既存パスがあれば初期値に使う。
        if (!path.empty()) {
            strcpy_s(pathBuffer, path.c_str());
        }
        // StateMachine名があれば、それを使って初期保存先を作る。
        else if (!panel.m_stateMachineAsset.name.empty()) {
            strcpy_s(pathBuffer, ("Assets/StateMachine/" + panel.m_stateMachineAsset.name + ".statemachine.json").c_str());
        }

        // 保存ダイアログを開く。
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kStateMachineFileFilter, "Save State Machine", "json") != DialogResult::OK) {
            return false;
        }

        // 保存先を確定する。
        path = pathBuffer;
    }

    // StateMachineAssetをJSONへ保存する。
    if (!StateMachineAssetSerializer::Save(path, panel.m_stateMachineAsset)) {
        return false;
    }

    // 保存先を記録し、Dirtyを解除する。
    panel.m_stateMachineAssetPath = path;
    panel.m_stateMachineDirty = false;

    // StateMachine cacheを無効化する。
    StateMachineSystem::InvalidateAssetCache(path.c_str());

    return true;
}

// InputMapドキュメントを保存する。
bool PlayerEditorSession::SaveInputMapDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 通常保存ならInputMappingTab側へ委譲する。
    if (!saveAs) {
        return panel.m_inputMappingTab.SaveActionMap();
    }

    // SaveAs用の保存先バッファ。
    char pathBuffer[MAX_PATH] = {};

    // 現在のInputMapパスを初期値にする。
    const std::string& currentPath = panel.m_inputMappingTab.GetActionMapPath();
    if (!currentPath.empty()) {
        strcpy_s(pathBuffer, currentPath.c_str());
    }

    // 保存ダイアログを開く。
    if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kInputMapFileFilter, "Save Input Map", "json") != DialogResult::OK) {
        return false;
    }

    // 指定パスへInputMapを保存する。
    return panel.m_inputMappingTab.SaveActionMapAs(pathBuffer);
}

// Timeline / StateMachine / InputMap / Socket をまとめて保存する。
bool PlayerEditorSession::SaveAllDocuments(PlayerEditorPanel& panel, bool saveAs)
{
    // 途中で失敗しても他の保存を試すため、boolの積み上げで管理する。
    bool ok = true;

    // TimelineがDirty、またはSaveAs対象なら保存する。
    if (panel.m_timelineDirty || (saveAs && !panel.m_timelineAsset.tracks.empty())) ok &= SaveTimelineDocument(panel, saveAs);

    // StateMachineがDirty、またはSaveAs対象なら保存する。
    if (panel.m_stateMachineDirty || (saveAs && !panel.m_stateMachineAsset.states.empty())) ok &= SaveStateMachineDocument(panel, saveAs);

    // InputMapがDirty、またはSaveAs対象なら保存する。
    if (panel.m_inputMappingTab.IsDirty() || (saveAs && !panel.m_inputMappingTab.GetActionMapPath().empty())) ok &= SaveInputMapDocument(panel, saveAs);

    // SocketがDirtyならPreview Entityへ書き戻す。
    if (panel.m_socketDirty) {
        ExportSocketsToPreviewEntity(panel);
    }

    return ok;
}

// Preview EntityをPrefabとして保存する。
bool PlayerEditorSession::SavePrefabDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // Registryが無い、またはPreview Entityが無いなら保存できない。
    if (!panel.m_registry || Entity::IsNull(panel.m_previewEntity)) {
        return false;
    }

    // Dirtyな関連ドキュメントがあれば先に保存する。
    if (panel.HasAnyDirtyDocument()) {
        if (!SaveAllDocuments(panel, false) && panel.HasAnyDirtyDocument()) {
            if (!SaveAllDocuments(panel, true)) {
                return false;
            }
        }
    }

    // Editorで設定したTimeline/StateMachine/Input/SocketをPreview Entityへ反映する。
    ApplyEditorBindingsToPreviewEntity(panel);

    // Socket情報もPreview Entityへ書き戻す。
    ExportSocketsToPreviewEntity(panel);

    // PlayerとしてPrefabに必要な永続Componentを保証する。
    PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*panel.m_registry, panel.m_previewEntity);

    // Runtime用Componentも保証する。
    PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, panel.m_previewEntity);

    // Runtime状態を初期化する。
    PlayerRuntimeSetup::ResetPlayerRuntimeState(*panel.m_registry, panel.m_previewEntity);

    // 保存先Prefabパス。
    std::string prefabPath;

    // SaveAsでなければ、PrefabInstanceComponentの既存パスを使う。
    if (!saveAs) {
        if (const PrefabInstanceComponent* prefab = panel.m_registry->GetComponent<PrefabInstanceComponent>(panel.m_previewEntity)) {
            prefabPath = prefab->prefabAssetPath;
        }
    }

    // 保存先がまだ無ければ保存ダイアログで決める。
    if (prefabPath.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 既存Prefabパスがあるなら初期値にする。
        if (const PrefabInstanceComponent* prefab = panel.m_registry->GetComponent<PrefabInstanceComponent>(panel.m_previewEntity);
            prefab && !prefab->prefabAssetPath.empty()) {
            strcpy_s(pathBuffer, prefab->prefabAssetPath.c_str());
        }
        // 無ければモデル名からPrefab保存先を作る。
        else {
            const std::string defaultName = panel.m_currentModelPath.empty()
                ? "Assets/Prefab/Player.prefab"
                : ("Assets/Prefab/" + std::filesystem::path(panel.m_currentModelPath).stem().string() + ".prefab");
            strcpy_s(pathBuffer, defaultName.c_str());
        }

        // 保存ダイアログを開く。
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kPrefabFileFilter, "Save Player Prefab", "prefab") != DialogResult::OK) {
            return false;
        }

        // 保存先を確定する。
        prefabPath = pathBuffer;
    }

    // Preview EntityをPrefabとして保存する。
    return PrefabSystem::SaveEntityToPrefabPath(panel.m_previewEntity, *panel.m_registry, prefabPath);
}

// すべてのドキュメントを保存前状態に戻す。
void PlayerEditorSession::RevertAllDocuments(PlayerEditorPanel& panel)
{
    // Timelineに保存済みパスがあるならファイルから再読み込みする。
    if (!panel.m_timelineAssetPath.empty()) {
        TimelineAssetSerializer::Load(panel.m_timelineAssetPath, panel.m_timelineAsset);
        panel.m_timelineDirty = false;
    }

    // StateMachineに保存済みパスがあるならファイルから再読み込みする。
    if (!panel.m_stateMachineAssetPath.empty()) {
        StateMachineAssetSerializer::Load(panel.m_stateMachineAssetPath, panel.m_stateMachineAsset);
        panel.m_stateMachineDirty = false;
    }

    // InputMapを再読み込みする。
    panel.m_inputMappingTab.ReloadActionMap();

    // Preview EntityからSocketを再取得する。
    ImportSocketsFromPreviewEntity(panel);

    // Socket Dirtyを解除する。
    panel.m_socketDirty = false;
}

// PlayerEditorの各種設定をPreview Entityへ反映する。
void PlayerEditorSession::ApplyEditorBindingsToPreviewEntity(PlayerEditorPanel& panel)
{
    // Preview Entityが使えないなら何もしない。
    if (!panel.CanUsePreviewEntity()) {
        return;
    }

    // MeshComponentへ現在のモデル情報を反映する。
    if (auto* mesh = panel.m_registry->GetComponent<MeshComponent>(panel.m_previewEntity)) {
        mesh->model = panel.m_ownedModel;
        mesh->modelFilePath = panel.m_currentModelPath;
        mesh->isVisible = true;
    }

    // StateMachine asset pathをPreview Entityへ反映する。
    {
        StateMachineParamsComponent* stateMachine = panel.m_registry->GetComponent<StateMachineParamsComponent>(panel.m_previewEntity);

        // StateMachineパスがあるのにComponentが無ければ追加する。
        if (!stateMachine && !panel.m_stateMachineAssetPath.empty()) {
            panel.m_registry->AddComponent<StateMachineParamsComponent>(panel.m_previewEntity, StateMachineParamsComponent{});
            stateMachine = panel.m_registry->GetComponent<StateMachineParamsComponent>(panel.m_previewEntity);
        }

        // StateMachineComponentがあり、パスもあるならassetPathへコピーする。
        if (stateMachine && !panel.m_stateMachineAssetPath.empty()) {
            strcpy_s(stateMachine->assetPath, panel.m_stateMachineAssetPath.c_str());
        }
        // Componentはあるがパスが無いなら空にする。
        else if (stateMachine) {
            stateMachine->assetPath[0] = '\0';
        }
    }

    // InputMap asset pathをPreview Entityへ反映する。
    {
        InputBindingComponent* inputBinding = panel.m_registry->GetComponent<InputBindingComponent>(panel.m_previewEntity);

        // InputMapパスがあるのにComponentが無ければ追加する。
        if (!inputBinding && !panel.m_inputMappingTab.GetActionMapPath().empty()) {
            panel.m_registry->AddComponent<InputBindingComponent>(panel.m_previewEntity, InputBindingComponent{});
            inputBinding = panel.m_registry->GetComponent<InputBindingComponent>(panel.m_previewEntity);
        }

        // InputBindingComponentがあり、パスもあるならactionMapAssetPathへコピーする。
        if (inputBinding && !panel.m_inputMappingTab.GetActionMapPath().empty()) {
            strcpy_s(inputBinding->actionMapAssetPath, panel.m_inputMappingTab.GetActionMapPath().c_str());
        }
        // Componentはあるがパスが無いなら空にする。
        else if (inputBinding) {
            inputBinding->actionMapAssetPath[0] = '\0';
        }
    }

    // PreviewモデルのスケールをTransformComponentへ反映する。
    if (auto* transform = panel.m_registry->GetComponent<TransformComponent>(panel.m_previewEntity)) {
        transform->localScale = { panel.m_previewModelScale, panel.m_previewModelScale, panel.m_previewModelScale };
        transform->isDirty = true;
    }

    // Socket編集結果をPreview Entityへ書き戻す。
    ExportSocketsToPreviewEntity(panel);

    // TimelineAssetをRuntime用Componentへ変換してPreview Entityへ反映する。
    RebuildPreviewTimelineRuntimeData(panel);
}

// TimelineAssetからTimelineComponentとTimelineItemBufferを作り直す。
void PlayerEditorSession::RebuildPreviewTimelineRuntimeData(PlayerEditorPanel& panel)
{
    // Preview Entityが無いなら何もしない。
    if (Entity::IsNull(panel.m_previewEntity)) {
        return;
    }

    // Player Runtimeに必要なComponentを保証する。
    PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, panel.m_previewEntity);

    // Timeline runtime用Componentを一時作成する。
    TimelineComponent timeline{};

    // Timeline item runtime bufferを一時作成する。
    TimelineItemBuffer buffer{};

    // Editor用TimelineAssetからRuntime用データを構築する。
    if (!TimelineAssetRuntimeBuilder::Build(panel.m_timelineAsset, panel.m_selectedAnimIndex, timeline, buffer)) {
        return;
    }

    // 既にTimelineComponentがあるなら上書きする。
    if (auto* existing = panel.m_registry->GetComponent<TimelineComponent>(panel.m_previewEntity)) {
        *existing = timeline;
    }
    // 無ければ追加する。
    else {
        panel.m_registry->AddComponent(panel.m_previewEntity, timeline);
    }

    // 既にTimelineItemBufferがあるなら上書きする。
    if (auto* existingBuffer = panel.m_registry->GetComponent<TimelineItemBuffer>(panel.m_previewEntity)) {
        *existingBuffer = buffer;
    }
    // 無ければ追加する。
    else {
        panel.m_registry->AddComponent(panel.m_previewEntity, buffer);
    }
}

// Timelineの再生位置をPreviewStateへ同期する。
void PlayerEditorSession::SyncPreviewTimelinePlayback(PlayerEditorPanel& panel)
{
    // PreviewStateが動いていないなら何もしない。
    if (!panel.m_previewState.IsActive()) {
        return;
    }

    // frameをsecondsへ変換してPreviewStateへ渡す。
    panel.m_previewState.SetTime(panel.m_playheadFrame / (panel.m_timelineAsset.fps > 0.0f ? panel.m_timelineAsset.fps : 60.0f));
}

// 現在選択中のScene EntityからPlayerEditorへ設定を取り込む。
void PlayerEditorSession::ImportFromSelectedEntity(PlayerEditorPanel& panel)
{
    // Registryが無い、または選択Entityが無いなら何もしない。
    if (!panel.m_registry || Entity::IsNull(panel.m_selectedEntity)) {
        return;
    }

    // 選択Entityのモデルパスがあるなら、それをPlayerEditorで開く。
    if (!panel.m_selectedEntityModelPath.empty()) {
        OpenModelFromPath(panel, panel.m_selectedEntityModelPath);
    }

    // 選択EntityをPreview対象にする。
    SetPreviewEntity(panel, panel.m_selectedEntity);

    // 選択EntityにStateMachineParamsComponentがあり、assetPathがあるならStateMachineを開く。
    if (StateMachineParamsComponent* stateMachine = panel.m_registry->GetComponent<StateMachineParamsComponent>(panel.m_selectedEntity);
        stateMachine && stateMachine->assetPath[0] != '\0') {
        OpenStateMachineFromPath(panel, stateMachine->assetPath);
    }

    // 選択EntityにInputBindingComponentがあり、actionMapAssetPathがあるならInputMapを開く。
    if (InputBindingComponent* inputBinding = panel.m_registry->GetComponent<InputBindingComponent>(panel.m_selectedEntity);
        inputBinding && inputBinding->actionMapAssetPath[0] != '\0') {
        OpenInputMapFromPath(panel, inputBinding->actionMapAssetPath);
    }

    // 選択EntityからSocket情報を読み込む。
    ImportSocketsFromPreviewEntity(panel);
}

// Preview EntityからSocket情報を読み込む。
void PlayerEditorSession::ImportSocketsFromPreviewEntity(PlayerEditorPanel& panel)
{
    // Registryが無い、またはPreview Entityが使えないならSocketを空にする。
    if (!panel.m_registry || !panel.CanUsePreviewEntity()) {
        panel.m_sockets.clear();
        panel.m_socketDirty = false;
        return;
    }

    // NodeSocketComponentがあれば、そのSocket配列をEditor側へコピーする。
    if (const auto* sockets = panel.m_registry->GetComponent<NodeSocketComponent>(panel.m_previewEntity)) {
        panel.m_sockets = sockets->sockets;
    }
    // 無ければSocketリストを空にする。
    else {
        panel.m_sockets.clear();
    }

    // 読み込み直後なのでDirtyではない。
    panel.m_socketDirty = false;
}

// Editor側で編集したSocket情報をPreview Entityへ書き戻す。
void PlayerEditorSession::ExportSocketsToPreviewEntity(PlayerEditorPanel& panel)
{
    // Registryが無い、またはPreview Entityが使えないなら何もしない。
    if (!panel.m_registry || !panel.CanUsePreviewEntity()) {
        return;
    }

    // 既存のNodeSocketComponentを取得する。
    auto* sockets = panel.m_registry->GetComponent<NodeSocketComponent>(panel.m_previewEntity);

    // 無ければ新しく追加する。
    if (!sockets) {
        panel.m_registry->AddComponent(panel.m_previewEntity, NodeSocketComponent{ panel.m_sockets });
        return;
    }

    // あればSocket配列を上書きする。
    sockets->sockets = panel.m_sockets;

    // 書き戻したのでDirtyを解除する。
    panel.m_socketDirty = false;
}