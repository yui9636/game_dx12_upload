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
#include "Gameplay/StateMachineAssetComponent.h"
#include "Gameplay/StateMachineParamsComponent.h"
#include "Gameplay/StateMachineSystem.h"
#include "Gameplay/TimelineAssetRuntimeBuilder.h"
#include "Gameplay/TimelineLibraryComponent.h"
#include "Gameplay/TimelineComponent.h"
#include "Gameplay/TimelineItemBuffer.h"
#include "Input/InputActionMapComponent.h"
#include "Input/InputBindingComponent.h"
#include "Model/Model.h"
#include "Registry/Registry.h"
#include "System/Dialog.h"
#include "System/ResourceManager.h"

namespace
{
    // Timeline繝輔ぃ繧､繝ｫ繧帝幕縺・菫晏ｭ倥☆繧九→縺阪・繝繧､繧｢繝ｭ繧ｰ逕ｨ繝輔ぅ繝ｫ繧ｿ縲・
    static constexpr const char* kTimelineFileFilter =
        "Timeline (*.timeline.json)\0*.timeline.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // StateMachine繝輔ぃ繧､繝ｫ繧帝幕縺・菫晏ｭ倥☆繧九→縺阪・繝繧､繧｢繝ｭ繧ｰ逕ｨ繝輔ぅ繝ｫ繧ｿ縲・
    static constexpr const char* kStateMachineFileFilter =
        "StateMachine (*.statemachine.json)\0*.statemachine.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // InputMap繝輔ぃ繧､繝ｫ繧帝幕縺・菫晏ｭ倥☆繧九→縺阪・繝繧､繧｢繝ｭ繧ｰ逕ｨ繝輔ぅ繝ｫ繧ｿ縲・
    static constexpr const char* kInputMapFileFilter =
        "Input Map (*.inputmap.json)\0*.inputmap.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // Prefab繝輔ぃ繧､繝ｫ繧剃ｿ晏ｭ倥☆繧九→縺阪・繝繧､繧｢繝ｭ繧ｰ逕ｨ繝輔ぅ繝ｫ繧ｿ縲・
    static constexpr const char* kPrefabFileFilter =
        "Prefab (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";

    // 謖・ｮ壹＆繧後◆繝代せ縺ｮ諡｡蠑ｵ蟄舌′縲∬ｨｱ蜿ｯ縺輔ｌ縺滓僑蠑ｵ蟄舌Μ繧ｹ繝医↓蜷ｫ縺ｾ繧後※縺・ｋ縺狗｢ｺ隱阪☆繧九・
    static bool HasExtension(const std::string& path, std::initializer_list<const char*> extensions)
    {
        // 繝代せ縺九ｉ諡｡蠑ｵ蟄舌□縺代ｒ蜿悶ｊ蜃ｺ縺吶・
        std::string ext = std::filesystem::path(path).extension().string();

        // 諡｡蠑ｵ蟄舌ｒ蟆乗枚蟄励↓螟画鋤縺励※縲∝､ｧ譁・ｭ怜ｰ乗枚蟄励・驕輔＞縺ｧ螟ｱ謨励＠縺ｪ縺・ｈ縺・↓縺吶ｋ縲・
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // 險ｱ蜿ｯ縺輔ｌ縺滓僑蠑ｵ蟄舌→荳閾ｴ縺吶ｋ縺玖ｪｿ縺ｹ繧九・
        for (const char* candidate : extensions) {
            if (ext == candidate) {
                return true;
            }
        }

        return false;
    }

    static bool HasTimelineAssetContent(const TimelineAsset& asset)
    {
        return asset.id != 0
            || !asset.name.empty()
            || !asset.tracks.empty()
            || asset.animationIndex >= 0
            || asset.duration > 0.0f;
    }

    static void EnsureTimelineAssetIdentity(TimelineAsset& asset, TimelineLibraryComponent& library)
    {
        if (asset.id != 0) {
            if (asset.id >= library.nextTimelineId) {
                library.nextTimelineId = asset.id + 1;
            }
            return;
        }

        if (library.nextTimelineId == 0) {
            library.nextTimelineId = 1;
        }

        asset.id = library.nextTimelineId++;
    }

    static TimelineAsset* FindTimelineAssetById(TimelineLibraryComponent& library, uint32_t timelineId)
    {
        if (timelineId == 0) {
            return nullptr;
        }

        for (auto& asset : library.assets) {
            if (asset.id == timelineId) {
                return &asset;
            }
        }
        return nullptr;
    }

    static const TimelineAsset* FindTimelineAssetById(const TimelineLibraryComponent& library, uint32_t timelineId)
    {
        if (timelineId == 0) {
            return nullptr;
        }

        for (const auto& asset : library.assets) {
            if (asset.id == timelineId) {
                return &asset;
            }
        }
        return nullptr;
    }

    static void SyncEditingTimelineIntoLibrary(TimelineLibraryComponent& library, TimelineAsset& editingAsset)
    {
        if (!HasTimelineAssetContent(editingAsset)) {
            return;
        }

        EnsureTimelineAssetIdentity(editingAsset, library);
        TimelineAsset* existing = FindTimelineAssetById(library, editingAsset.id);
        if (existing) {
            *existing = editingAsset;
            return;
        }

        library.assets.push_back(editingAsset);
    }
}

// PlayerEditor縺ｮ繝励Ξ繝薙Η繝ｼ迥ｶ諷九ｒ荳譎ょ●豁｢縺吶ｋ縲・
// Timeline蜀咲函縲￣reviewState縲∵園譛臼review Entity繧呈ｭ｢繧√ｋ縲・
void PlayerEditorSession::Suspend(PlayerEditorPanel& panel)
{
    // 蜀咲函繝倥ャ繝峨ｒ蜈磯ｭ縺ｫ謌ｻ縺吶・
    panel.m_playheadFrame = 0;

    // Timeline蜀咲函迥ｶ諷九ｒ蛛懈ｭ｢縺ｫ縺吶ｋ縲・
    panel.m_isPlaying = false;

    // PreviewState縺ｫ迴ｾ蝨ｨ縺ｮTimeline蜀咲函菴咲ｽｮ繧貞渚譏縺吶ｋ縲・
    SyncPreviewTimelinePlayback(panel);

    // Animation preview縺悟虚縺・※縺・ｋ縺ｪ繧臥ｵゆｺ・＠縺ｦ蜈・・Animator迥ｶ諷九∈謌ｻ縺吶・
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // PlayerEditor縺瑚・蛻・〒菴懊▲縺蘖review Entity繧堤ｴ譽・☆繧九・
    DestroyOwnedPreviewEntity(panel);
}

// PlayerEditor縺梧園譛峨＠縺ｦ縺・ｋPreview Entity繧堤ｴ譽・☆繧九・
// 螟夜Κ縺九ｉ驕ｸ謚槭＆繧後◆Entity縺ｯ遐ｴ譽・○縺壹∬・蛻・〒菴懊▲縺溘ｂ縺ｮ縺縺代ｒ豸医☆縲・
void PlayerEditorSession::DestroyOwnedPreviewEntity(PlayerEditorPanel& panel)
{
    // PlayerEditor謇譛峨・Entity縺ｧ縺ｪ縺代ｌ縺ｰ遐ｴ譽・＠縺ｪ縺・・
    if (!panel.m_previewEntityOwned) {
        return;
    }

    // PreviewState縺悟虚縺・※縺・ｋ縺ｪ繧牙・縺ｫ豁｢繧√ｋ縲・
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // Registry縺後≠繧翫￣review Entity縺檎函蟄倥＠縺ｦ縺・ｋ縺ｪ繧臥ｴ譽・☆繧九・
    if (panel.m_registry && !Entity::IsNull(panel.m_previewEntity) && panel.m_registry->IsAlive(panel.m_previewEntity)) {
        panel.m_registry->DestroyEntity(panel.m_previewEntity);
    }

    // Preview Entity諠・ｱ繧堤ｩｺ縺ｫ謌ｻ縺吶・
    panel.m_previewEntity = Entity::NULL_ID;
    panel.m_previewEntityOwned = false;
}

// PlayerEditor蟆ら畑縺ｮPreview Entity繧剃ｽ懈・縺吶ｋ縲・
// 繝｢繝・Ν繧帝幕縺・◆縺ｨ縺阪ヾcene荳翫↓繝励Ξ繝薙Η繝ｼ逕ｨEntity繧剃ｽ懊▲縺ｦMeshComponent繧剃ｻ倥￠繧九・
void PlayerEditorSession::EnsureOwnedPreviewEntity(PlayerEditorPanel& panel)
{
    // Registry縺檎┌縺・√∪縺溘・繝｢繝・Ν縺碁幕縺九ｌ縺ｦ縺・↑縺・↑繧我ｽ懊ｌ縺ｪ縺・・
    if (!panel.m_registry || !panel.HasOpenModel()) {
        return;
    }

    // 譌｢縺ｫ菴ｿ逕ｨ蜿ｯ閭ｽ縺ｪPreview Entity縺後≠繧九↑繧画眠縺励￥菴懊ｉ縺ｪ縺・・
    if (panel.CanUsePreviewEntity()) {
        return;
    }

    // 蜿､縺・園譛臼review Entity縺梧ｮ九▲縺ｦ縺・ｋ蝣ｴ蜷医・蜈医↓遐ｴ譽・☆繧九・
    DestroyOwnedPreviewEntity(panel);

    // 譁ｰ縺励＞Entity繧坦egistry縺ｫ菴懊ｋ縲・
    panel.m_previewEntity = panel.m_registry->CreateEntity();

    // 縺薙・Entity縺ｯPlayerEditor縺御ｽ懊▲縺溘ｂ縺ｮ縺ｨ縺励※邂｡逅・☆繧九・
    panel.m_previewEntityOwned = true;

    // 繝・ヰ繝・げ繧Зierarchy陦ｨ遉ｺ逕ｨ縺ｮ蜷榊燕繧剃ｻ倥￠繧九・
    panel.m_registry->AddComponent(panel.m_previewEntity, NameComponent{ "Player Preview" });

    // 蜴溽せ縺ｫ鄂ｮ縺上◆繧√・TransformComponent繧剃ｽ懊ｋ縲・
    TransformComponent transform{};

    // Preview Entity縺ｮ菴咲ｽｮ縺ｯ蜴溽せ縲・
    transform.localPosition = { 0.0f, 0.0f, 0.0f };

    // Preview Entity縺ｮ繧ｹ繧ｱ繝ｼ繝ｫ縺ｯ遲牙阪・
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



    // Transform譖ｴ譁ｰ縺悟ｿ・ｦ√□縺ｨ遏･繧峨○繧九・
    transform.isDirty = true;

    // Entity縺ｫTransformComponent繧定ｿｽ蜉縺吶ｋ縲・
    panel.m_registry->AddComponent(panel.m_previewEntity, transform);

    // 謠冗判逕ｨ縺ｮMeshComponent繧剃ｽ懊ｋ縲・
    MeshComponent mesh{};

    // 繝｢繝・Ν繝輔ぃ繧､繝ｫ繝代せ繧貞・繧後ｋ縲・
    mesh.modelFilePath = panel.m_currentModelPath;

    // ResourceManager縺九ｉ蜿門ｾ励＠縺溘Δ繝・Ν譛ｬ菴薙ｒ蜈･繧後ｋ縲・
    mesh.model = panel.m_ownedModel;

    // 陦ｨ遉ｺ迥ｶ諷九ｒ譛牙柑縺ｫ縺吶ｋ縲・
    mesh.isVisible = true;

    // 蠖ｱ繧定誠縺ｨ縺呵ｨｭ螳壹↓縺吶ｋ縲・
    mesh.castShadow = true;

    // Entity縺ｫMeshComponent繧定ｿｽ蜉縺吶ｋ縲・
    // 縺薙％縺ｾ縺ｧ譚･繧後・縲後Δ繝・Ν繧呈戟縺｣縺檸ntity縲阪・菴懊ｉ繧後※縺・ｋ縲・
    panel.m_registry->AddComponent(panel.m_previewEntity, mesh);

    // EffectPreview逕ｨ縺ｮ逶ｮ蜊ｰComponent繧剃ｻ倥￠繧九・
    panel.m_registry->AddComponent(panel.m_previewEntity, EffectPreviewTagComponent{});

    // Timeline縲ヾtateMachine縲！nputMap縲ヾocket縺ｪ縺ｩ縺ｮEditor蛛ｴ繝・・繧ｿ繧単review Entity縺ｸ蜿肴丐縺吶ｋ縲・
    ApplyEditorBindingsToPreviewEntity(panel);
}

// PlayerEditor縺ｮPreview蟇ｾ雎｡繧偵∝､夜ΚEntity縺ｫ蟾ｮ縺玲崛縺医ｋ縲・
// 縺､縺ｾ繧翫訓layerEditor蟆ら畑Entity縲阪〒縺ｯ縺ｪ縺上∵里蟄牢cene荳翫・Entity繧堤ｷｨ髮・ｯｾ雎｡縺ｫ縺吶ｋ縲・
void PlayerEditorSession::SetPreviewEntity(PlayerEditorPanel& panel, EntityID entity)
{
    // 譌｢縺ｫ蜷後§螟夜ΚEntity繧単review蟇ｾ雎｡縺ｫ縺励※縺・ｋ縺ｪ繧我ｽ輔ｂ縺励↑縺・・
    if (panel.m_previewEntity == entity && !panel.m_previewEntityOwned) {
        return;
    }

    // 蜀咲函荳ｭ縺ｪ繧画ｭ｢繧√ｋ縲・
    panel.m_isPlaying = false;

    // PreviewState縺悟虚縺・※縺・ｋ縺ｪ繧臥ｵゆｺ・☆繧九・
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // 謇譛臼review Entity縺後≠繧九↑繧臥ｴ譽・☆繧九・
    DestroyOwnedPreviewEntity(panel);

    // 螟夜ΚEntity繧単review蟇ｾ雎｡縺ｫ縺吶ｋ縲・
    panel.m_previewEntity = entity;

    // 螟夜ΚEntity縺ｪ縺ｮ縺ｧPlayerEditor謇譛峨〒縺ｯ縺ｪ縺・・
    panel.m_previewEntityOwned = false;

    // 螟夜ΚEntity縺九ｉSocket諠・ｱ繧定ｪｭ縺ｿ霎ｼ繧縲・
    ImportSocketsFromPreviewEntity(panel);
}

// EditorLayer縺ｪ縺ｩ螟夜Κ驕ｸ謚槭°繧峨￣layerEditor縺ｸ迴ｾ蝨ｨ驕ｸ謚昿ntity諠・ｱ繧貞酔譛溘☆繧九・
void PlayerEditorSession::SyncExternalSelection(PlayerEditorPanel& panel, EntityID entity, const std::string& modelPath)
{
    // 迴ｾ蝨ｨ驕ｸ謚槭＆繧後※縺・ｋEntity繧剃ｿ晏ｭ倥☆繧九・
    panel.m_selectedEntity = entity;

    // 縺昴・Entity縺ｮ繝｢繝・Ν繝代せ繧剃ｿ晏ｭ倥☆繧九・
    panel.m_selectedEntityModelPath = modelPath;

    // Preview Entity縺梧里縺ｫ豁ｻ繧薙〒縺・ｋ縺ｪ繧牙盾辣ｧ繧堤ｩｺ縺ｫ縺吶ｋ縲・
    if (!Entity::IsNull(panel.m_previewEntity) && panel.m_registry && !panel.m_registry->IsAlive(panel.m_previewEntity)) {
        panel.m_previewEntity = Entity::NULL_ID;
        panel.m_previewEntityOwned = false;
    }
}

// 繝｢繝・Ν繝輔ぃ繧､繝ｫ繧帝幕縺阪￣layerEditor縺ｮPreview逕ｨ繝｢繝・Ν縺ｨ縺励※險ｭ螳壹☆繧九・
bool PlayerEditorSession::OpenModelFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // 遨ｺ繝代せ縺ｪ繧牙､ｱ謨励・
    if (path.empty()) {
        return false;
    }

 
    // PlayerEditor縺ｮPreview Entity縺ｯ騾壼ｸｸScene謠冗判縺ｫ荵励○繧九◆繧√・
    std::shared_ptr<Model> model = ResourceManager::Instance().CreateModelInstance(path);
    if (!model) {
        return false;
    }


    // 譌｢蟄榔review繧・・逕溽憾諷九ｒ蛛懈ｭ｢繝ｻ遐ｴ譽・☆繧九・
    Suspend(panel);

    // PlayerEditor縺梧園譛峨☆繧九Δ繝・Ν縺ｨ縺励※菫晏ｭ倥☆繧九・
    panel.m_ownedModel = std::move(model);

    // 逕溘・繧､繝ｳ繧ｿ蜿ら・繧ゆｿ晄戟縺吶ｋ縲・
    panel.m_model = panel.m_ownedModel.get();

    // 迴ｾ蝨ｨ髢九＞縺ｦ縺・ｋ繝｢繝・Ν繝代せ繧剃ｿ晏ｭ倥☆繧九・
    panel.m_currentModelPath = path;

    // Bone/Socket/Timeline驕ｸ謚槭↑縺ｩ繧偵Μ繧ｻ繝・ヨ縺吶ｋ縲・
    panel.ResetSelectionState();

    // 譛蛻昴・Animation繧帝∈謚樒憾諷九↓縺吶ｋ縲・
    panel.m_selectedAnimIndex = 0;

    // 繝｢繝・Ν陦ｨ遉ｺ繧ｹ繧ｱ繝ｼ繝ｫ繧貞・譛溷､縺ｫ謌ｻ縺吶・
    panel.m_previewModelScale = 1.0f;

    // Preview謠冗判繧ｵ繧､繧ｺ繧偵Μ繧ｻ繝・ヨ縺吶ｋ縲・
    panel.m_previewRenderSize = { 0.0f, 0.0f };

    // Socket繝ｪ繧ｹ繝医ｒ遨ｺ縺ｫ縺吶ｋ縲・
    panel.m_sockets.clear();

    // Dirty繝輔Λ繧ｰ繧貞・譛溷喧縺吶ｋ縲・
    panel.m_socketDirty = false;
    panel.m_timelineDirty = false;
    panel.m_stateMachineDirty = false;

    // Preview Entity繧剃ｽ懈・縺吶ｋ縲・
    EnsureOwnedPreviewEntity(panel);

    // 繝｢繝・Ν繧帝幕縺代◆縺ｮ縺ｧ謌仙粥縲・
    return true;
}

// Timeline繝輔ぃ繧､繝ｫ繧定ｪｭ縺ｿ霎ｼ繧縲・
bool PlayerEditorSession::OpenTimelineFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // 遨ｺ繝代せ縲√∪縺溘・荳肴ｭ｣縺ｪ諡｡蠑ｵ蟄舌↑繧牙､ｱ謨励・
    if (path.empty() || !HasExtension(path, { ".timeline.json", ".json" })) {
        return false;
    }

    // TimelineAsset繧谷SON縺九ｉ隱ｭ縺ｿ霎ｼ繧縲・
    if (!TimelineAssetSerializer::Load(path, panel.m_timelineAsset)) {
        return false;
    }

    if (panel.m_timelineAsset.id == 0) {
        panel.m_timelineAsset.id = 1;
    }

    // 隱ｭ縺ｿ霎ｼ繧薙□Timeline繝代せ繧剃ｿ晏ｭ倥☆繧九・
    panel.m_timelineAssetPath = path;

    // 隱ｭ縺ｿ霎ｼ縺ｿ逶ｴ蠕後↑縺ｮ縺ｧDirty縺ｧ縺ｯ縺ｪ縺・・
    panel.m_timelineDirty = false;

    // Preview Entity逕ｨ縺ｮTimeline runtime data繧貞・讒狗ｯ峨☆繧九・
    RebuildPreviewTimelineRuntimeData(panel);

    return true;
}

// StateMachine繝輔ぃ繧､繝ｫ繧定ｪｭ縺ｿ霎ｼ繧縲・
bool PlayerEditorSession::OpenStateMachineFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // 遨ｺ繝代せ縲√∪縺溘・荳肴ｭ｣縺ｪ諡｡蠑ｵ蟄舌↑繧牙､ｱ謨励・
    if (path.empty() || !HasExtension(path, { ".statemachine.json", ".json" })) {
        return false;
    }

    // StateMachineAsset繧谷SON縺九ｉ隱ｭ縺ｿ霎ｼ繧縲・
    if (!StateMachineAssetSerializer::Load(path, panel.m_stateMachineAsset)) {
        return false;
    }

    // 隱ｭ縺ｿ霎ｼ繧薙□StateMachine繝代せ繧剃ｿ晏ｭ倥☆繧九・
    panel.m_stateMachineAssetPath = path;

    // 隱ｭ縺ｿ霎ｼ縺ｿ逶ｴ蠕後↑縺ｮ縺ｧDirty縺ｧ縺ｯ縺ｪ縺・・
    panel.m_stateMachineDirty = false;

    return true;
}

// InputMap繝輔ぃ繧､繝ｫ繧定ｪｭ縺ｿ霎ｼ繧縲・
bool PlayerEditorSession::OpenInputMapFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // InputMappingTab蛛ｴ縺ｮOpen蜃ｦ逅・∈蟋碑ｭｲ縺吶ｋ縲・
    return panel.m_inputMappingTab.OpenActionMap(path);
}

// Timeline繝峨く繝･繝｡繝ｳ繝医ｒ菫晏ｭ倥☆繧九・
bool PlayerEditorSession::SaveTimelineDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 迴ｾ蝨ｨ縺ｮTimeline菫晏ｭ伜・繧貞叙蠕励☆繧九・
    std::string path = panel.m_timelineAssetPath;

    // SaveAs謖・ｮ壹√∪縺溘・菫晏ｭ伜・譛ｪ險ｭ螳壹↑繧我ｿ晏ｭ倥ム繧､繧｢繝ｭ繧ｰ繧貞・縺吶・
    if (saveAs || path.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 譌｢蟄倥ヱ繧ｹ縺後≠繧後・蛻晄悄蛟､縺ｫ菴ｿ縺・・
        if (!path.empty()) {
            strcpy_s(pathBuffer, path.c_str());
        }
        // Timeline蜷阪′縺ゅｌ縺ｰ縲√◎繧後ｒ菴ｿ縺｣縺ｦ蛻晄悄菫晏ｭ伜・繧剃ｽ懊ｋ縲・
        else if (!panel.m_timelineAsset.name.empty()) {
            strcpy_s(pathBuffer, ("Assets/Timeline/" + panel.m_timelineAsset.name + ".timeline.json").c_str());
        }

        // 菫晏ｭ倥ム繧､繧｢繝ｭ繧ｰ繧帝幕縺上・
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kTimelineFileFilter, "Save Timeline", "json") != DialogResult::OK) {
            return false;
        }

        // 菫晏ｭ伜・繧堤｢ｺ螳壹☆繧九・
        path = pathBuffer;
    }

    // TimelineAsset繧谷SON縺ｸ菫晏ｭ倥☆繧九・
    if (!TimelineAssetSerializer::Save(path, panel.m_timelineAsset)) {
        return false;
    }

    // 菫晏ｭ伜・繧定ｨ倬鹸縺励．irty繧定ｧ｣髯､縺吶ｋ縲・
    panel.m_timelineAssetPath = path;
    panel.m_timelineDirty = false;

    // StateMachine/Timeline runtime cache繧堤┌蜉ｹ蛹悶☆繧九・
    StateMachineSystem::InvalidateAssetCache(path.c_str());

    return true;
}

// StateMachine繝峨く繝･繝｡繝ｳ繝医ｒ菫晏ｭ倥☆繧九・
bool PlayerEditorSession::SaveStateMachineDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 迴ｾ蝨ｨ縺ｮStateMachine菫晏ｭ伜・繧貞叙蠕励☆繧九・
    std::string path = panel.m_stateMachineAssetPath;

    // SaveAs謖・ｮ壹√∪縺溘・菫晏ｭ伜・譛ｪ險ｭ螳壹↑繧我ｿ晏ｭ倥ム繧､繧｢繝ｭ繧ｰ繧貞・縺吶・
    if (saveAs || path.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 譌｢蟄倥ヱ繧ｹ縺後≠繧後・蛻晄悄蛟､縺ｫ菴ｿ縺・・
        if (!path.empty()) {
            strcpy_s(pathBuffer, path.c_str());
        }
        // StateMachine蜷阪′縺ゅｌ縺ｰ縲√◎繧後ｒ菴ｿ縺｣縺ｦ蛻晄悄菫晏ｭ伜・繧剃ｽ懊ｋ縲・
        else if (!panel.m_stateMachineAsset.name.empty()) {
            strcpy_s(pathBuffer, ("Assets/StateMachine/" + panel.m_stateMachineAsset.name + ".statemachine.json").c_str());
        }

        // 菫晏ｭ倥ム繧､繧｢繝ｭ繧ｰ繧帝幕縺上・
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kStateMachineFileFilter, "Save State Machine", "json") != DialogResult::OK) {
            return false;
        }

        // 菫晏ｭ伜・繧堤｢ｺ螳壹☆繧九・
        path = pathBuffer;
    }

    // StateMachineAsset繧谷SON縺ｸ菫晏ｭ倥☆繧九・
    if (!StateMachineAssetSerializer::Save(path, panel.m_stateMachineAsset)) {
        return false;
    }

    // 菫晏ｭ伜・繧定ｨ倬鹸縺励．irty繧定ｧ｣髯､縺吶ｋ縲・
    panel.m_stateMachineAssetPath = path;
    panel.m_stateMachineDirty = false;

    // StateMachine cache繧堤┌蜉ｹ蛹悶☆繧九・
    StateMachineSystem::InvalidateAssetCache(path.c_str());

    return true;
}

// InputMap繝峨く繝･繝｡繝ｳ繝医ｒ菫晏ｭ倥☆繧九・
bool PlayerEditorSession::SaveInputMapDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 騾壼ｸｸ菫晏ｭ倥↑繧迂nputMappingTab蛛ｴ縺ｸ蟋碑ｭｲ縺吶ｋ縲・
    if (!saveAs) {
        return panel.m_inputMappingTab.SaveActionMap();
    }

    // SaveAs逕ｨ縺ｮ菫晏ｭ伜・繝舌ャ繝輔ぃ縲・
    char pathBuffer[MAX_PATH] = {};

    // 迴ｾ蝨ｨ縺ｮInputMap繝代せ繧貞・譛溷､縺ｫ縺吶ｋ縲・
    const std::string& currentPath = panel.m_inputMappingTab.GetActionMapPath();
    if (!currentPath.empty()) {
        strcpy_s(pathBuffer, currentPath.c_str());
    }

    // 菫晏ｭ倥ム繧､繧｢繝ｭ繧ｰ繧帝幕縺上・
    if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kInputMapFileFilter, "Save Input Map", "json") != DialogResult::OK) {
        return false;
    }

    // 謖・ｮ壹ヱ繧ｹ縺ｸInputMap繧剃ｿ晏ｭ倥☆繧九・
    return panel.m_inputMappingTab.SaveActionMapAs(pathBuffer);
}

// Timeline / StateMachine / InputMap / Socket 繧偵∪縺ｨ繧√※菫晏ｭ倥☆繧九・
bool PlayerEditorSession::SaveAllDocuments(PlayerEditorPanel& panel, bool saveAs)
{
    (void)saveAs;

    if (!panel.CanUsePreviewEntity()) {
        return false;
    }

    ApplyEditorBindingsToPreviewEntity(panel);
    ExportSocketsToPreviewEntity(panel);
    return true;
}

// Preview Entity繧単refab縺ｨ縺励※菫晏ｭ倥☆繧九・
bool PlayerEditorSession::SavePrefabDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // Registry縺檎┌縺・√∪縺溘・Preview Entity縺檎┌縺・↑繧我ｿ晏ｭ倥〒縺阪↑縺・・
    if (!panel.m_registry || Entity::IsNull(panel.m_previewEntity)) {
        return false;
    }

    // Editor縺ｧ險ｭ螳壹＠縺鬱imeline/StateMachine/Input/Socket繧単review Entity縺ｸ蜿肴丐縺吶ｋ縲・    ApplyEditorBindingsToPreviewEntity(panel);

    // Socket諠・ｱ繧１review Entity縺ｸ譖ｸ縺肴綾縺吶・
    ExportSocketsToPreviewEntity(panel);

    // Player縺ｨ縺励※Prefab縺ｫ蠢・ｦ√↑豌ｸ邯咾omponent繧剃ｿ晁ｨｼ縺吶ｋ縲・
    PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*panel.m_registry, panel.m_previewEntity);

    // Runtime逕ｨComponent繧ゆｿ晁ｨｼ縺吶ｋ縲・
    PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, panel.m_previewEntity);

    // Runtime迥ｶ諷九ｒ蛻晄悄蛹悶☆繧九・
    PlayerRuntimeSetup::ResetPlayerRuntimeState(*panel.m_registry, panel.m_previewEntity);

    // 菫晏ｭ伜・Prefab繝代せ縲・
    std::string prefabPath;

    // SaveAs縺ｧ縺ｪ縺代ｌ縺ｰ縲￣refabInstanceComponent縺ｮ譌｢蟄倥ヱ繧ｹ繧剃ｽｿ縺・・
    if (!saveAs) {
        if (const PrefabInstanceComponent* prefab = panel.m_registry->GetComponent<PrefabInstanceComponent>(panel.m_previewEntity)) {
            prefabPath = prefab->prefabAssetPath;
        }
    }

    // 菫晏ｭ伜・縺後∪縺辟｡縺代ｌ縺ｰ菫晏ｭ倥ム繧､繧｢繝ｭ繧ｰ縺ｧ豎ｺ繧√ｋ縲・
    if (prefabPath.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 譌｢蟄榔refab繝代せ縺後≠繧九↑繧牙・譛溷､縺ｫ縺吶ｋ縲・
        if (const PrefabInstanceComponent* prefab = panel.m_registry->GetComponent<PrefabInstanceComponent>(panel.m_previewEntity);
            prefab && !prefab->prefabAssetPath.empty()) {
            strcpy_s(pathBuffer, prefab->prefabAssetPath.c_str());
        }
        // 辟｡縺代ｌ縺ｰ繝｢繝・Ν蜷阪°繧臼refab菫晏ｭ伜・繧剃ｽ懊ｋ縲・
        else {
            const std::string defaultName = panel.m_currentModelPath.empty()
                ? "Assets/Prefab/Player.prefab"
                : ("Assets/Prefab/" + std::filesystem::path(panel.m_currentModelPath).stem().string() + ".prefab");
            strcpy_s(pathBuffer, defaultName.c_str());
        }

        // 菫晏ｭ倥ム繧､繧｢繝ｭ繧ｰ繧帝幕縺上・
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kPrefabFileFilter, "Save Player Prefab", "prefab") != DialogResult::OK) {
            return false;
        }

        // 菫晏ｭ伜・繧堤｢ｺ螳壹☆繧九・
        prefabPath = pathBuffer;
    }

    if (!PrefabSystem::SaveEntityToPrefabPath(panel.m_previewEntity, *panel.m_registry, prefabPath)) {
        return false;
    }

    panel.m_timelineDirty = false;
    panel.m_stateMachineDirty = false;
    panel.m_socketDirty = false;
    panel.m_inputMappingTab.SetEditingMap(panel.m_inputMappingTab.GetEditingMap());
    return true;
}

// 縺吶∋縺ｦ縺ｮ繝峨く繝･繝｡繝ｳ繝医ｒ菫晏ｭ伜燕迥ｶ諷九↓謌ｻ縺吶・
void PlayerEditorSession::RevertAllDocuments(PlayerEditorPanel& panel)
{
    // Timeline縺ｫ菫晏ｭ俶ｸ医∩繝代せ縺後≠繧九↑繧峨ヵ繧｡繧､繝ｫ縺九ｉ蜀崎ｪｭ縺ｿ霎ｼ縺ｿ縺吶ｋ縲・
    if (!panel.m_timelineAssetPath.empty()) {
        TimelineAssetSerializer::Load(panel.m_timelineAssetPath, panel.m_timelineAsset);
        panel.m_timelineDirty = false;
    }

    // StateMachine縺ｫ菫晏ｭ俶ｸ医∩繝代せ縺後≠繧九↑繧峨ヵ繧｡繧､繝ｫ縺九ｉ蜀崎ｪｭ縺ｿ霎ｼ縺ｿ縺吶ｋ縲・
    if (!panel.m_stateMachineAssetPath.empty()) {
        StateMachineAssetSerializer::Load(panel.m_stateMachineAssetPath, panel.m_stateMachineAsset);
        panel.m_stateMachineDirty = false;
    }

    if (!panel.m_inputMappingTab.GetActionMapPath().empty()) {
        panel.m_inputMappingTab.ReloadActionMap();
    }

    // Preview Entity縺九ｉSocket繧貞・蜿門ｾ励☆繧九・
    ImportSocketsFromPreviewEntity(panel);

    // Socket Dirty繧定ｧ｣髯､縺吶ｋ縲・
    panel.m_socketDirty = false;
}

// PlayerEditor縺ｮ蜷・ｨｮ險ｭ螳壹ｒPreview Entity縺ｸ蜿肴丐縺吶ｋ縲・
void PlayerEditorSession::ApplyEditorBindingsToPreviewEntity(PlayerEditorPanel& panel)
{
    // Preview Entity縺御ｽｿ縺医↑縺・↑繧我ｽ輔ｂ縺励↑縺・・
    if (!panel.CanUsePreviewEntity()) {
        return;
    }

    // MeshComponent縺ｸ迴ｾ蝨ｨ縺ｮ繝｢繝・Ν諠・ｱ繧貞渚譏縺吶ｋ縲・
    if (auto* mesh = panel.m_registry->GetComponent<MeshComponent>(panel.m_previewEntity)) {
        mesh->model = panel.m_ownedModel;
        mesh->modelFilePath = panel.m_currentModelPath;
        mesh->isVisible = true;
    }

    PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*panel.m_registry, panel.m_previewEntity);

    if (auto* embeddedStateMachine = panel.m_registry->GetComponent<StateMachineAssetComponent>(panel.m_previewEntity)) {
        embeddedStateMachine->asset = panel.m_stateMachineAsset;
    }

    if (auto* timelineLibrary = panel.m_registry->GetComponent<TimelineLibraryComponent>(panel.m_previewEntity)) {
        SyncEditingTimelineIntoLibrary(*timelineLibrary, panel.m_timelineAsset);
    }

    if (auto* embeddedInputMap = panel.m_registry->GetComponent<InputActionMapComponent>(panel.m_previewEntity)) {
        embeddedInputMap->asset = panel.m_inputMappingTab.GetEditingMap();
    }

    // Preview繝｢繝・Ν縺ｮ繧ｹ繧ｱ繝ｼ繝ｫ繧探ransformComponent縺ｸ蜿肴丐縺吶ｋ縲・
    if (auto* transform = panel.m_registry->GetComponent<TransformComponent>(panel.m_previewEntity)) {
        transform->localScale = { panel.m_previewModelScale, panel.m_previewModelScale, panel.m_previewModelScale };
        transform->isDirty = true;
    }

    // Socket邱ｨ髮・ｵ先棡繧単review Entity縺ｸ譖ｸ縺肴綾縺吶・
    ExportSocketsToPreviewEntity(panel);

    // TimelineAsset繧坦untime逕ｨComponent縺ｸ螟画鋤縺励※Preview Entity縺ｸ蜿肴丐縺吶ｋ縲・
    RebuildPreviewTimelineRuntimeData(panel);
}

// TimelineAsset縺九ｉTimelineComponent縺ｨTimelineItemBuffer繧剃ｽ懊ｊ逶ｴ縺吶・
void PlayerEditorSession::RebuildPreviewTimelineRuntimeData(PlayerEditorPanel& panel)
{
    // Preview Entity縺檎┌縺・↑繧我ｽ輔ｂ縺励↑縺・・
    if (Entity::IsNull(panel.m_previewEntity)) {
        return;
    }

    // Player Runtime縺ｫ蠢・ｦ√↑Component繧剃ｿ晁ｨｼ縺吶ｋ縲・
    PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, panel.m_previewEntity);

    // Timeline runtime逕ｨComponent繧剃ｸ譎ゆｽ懈・縺吶ｋ縲・
    TimelineComponent timeline{};

    // Timeline item runtime buffer繧剃ｸ譎ゆｽ懈・縺吶ｋ縲・
    TimelineItemBuffer buffer{};

    const TimelineAsset* sourceTimeline = nullptr;
    if (const StateNode* selectedState = panel.m_stateMachineAsset.FindState(panel.m_selectedNodeId)) {
        if (selectedState->timelineId != 0) {
            if (const auto* timelineLibrary = panel.m_registry->GetComponent<TimelineLibraryComponent>(panel.m_previewEntity)) {
                sourceTimeline = FindTimelineAssetById(*timelineLibrary, selectedState->timelineId);
            }
        }
    }

    if (!sourceTimeline && HasTimelineAssetContent(panel.m_timelineAsset)) {
        sourceTimeline = &panel.m_timelineAsset;
    }

    if (!sourceTimeline) {
        if (auto* existing = panel.m_registry->GetComponent<TimelineComponent>(panel.m_previewEntity)) {
            *existing = TimelineComponent{};
        }
        if (auto* existingBuffer = panel.m_registry->GetComponent<TimelineItemBuffer>(panel.m_previewEntity)) {
            existingBuffer->items.clear();
        }
        return;
    }

    if (!TimelineAssetRuntimeBuilder::Build(*sourceTimeline, panel.m_selectedAnimIndex, timeline, buffer)) {
        return;
    }

    // 譌｢縺ｫTimelineComponent縺後≠繧九↑繧我ｸ頑嶌縺阪☆繧九・
    if (auto* existing = panel.m_registry->GetComponent<TimelineComponent>(panel.m_previewEntity)) {
        *existing = timeline;
    }
    // 辟｡縺代ｌ縺ｰ霑ｽ蜉縺吶ｋ縲・
    else {
        panel.m_registry->AddComponent(panel.m_previewEntity, timeline);
    }

    // 譌｢縺ｫTimelineItemBuffer縺後≠繧九↑繧我ｸ頑嶌縺阪☆繧九・
    if (auto* existingBuffer = panel.m_registry->GetComponent<TimelineItemBuffer>(panel.m_previewEntity)) {
        *existingBuffer = buffer;
    }
    // 辟｡縺代ｌ縺ｰ霑ｽ蜉縺吶ｋ縲・
    else {
        panel.m_registry->AddComponent(panel.m_previewEntity, buffer);
    }
}

// Timeline縺ｮ蜀咲函菴咲ｽｮ繧単reviewState縺ｸ蜷梧悄縺吶ｋ縲・
void PlayerEditorSession::SyncPreviewTimelinePlayback(PlayerEditorPanel& panel)
{
    // PreviewState縺悟虚縺・※縺・↑縺・↑繧我ｽ輔ｂ縺励↑縺・・
    if (!panel.m_previewState.IsActive()) {
        return;
    }

    // frame繧痴econds縺ｸ螟画鋤縺励※PreviewState縺ｸ貂｡縺吶・
    panel.m_previewState.SetTime(panel.m_playheadFrame / (panel.m_timelineAsset.fps > 0.0f ? panel.m_timelineAsset.fps : 60.0f));
}

// 迴ｾ蝨ｨ驕ｸ謚樔ｸｭ縺ｮScene Entity縺九ｉPlayerEditor縺ｸ險ｭ螳壹ｒ蜿悶ｊ霎ｼ繧縲・
void PlayerEditorSession::ImportFromSelectedEntity(PlayerEditorPanel& panel)
{
    // Registry縺檎┌縺・√∪縺溘・驕ｸ謚昿ntity縺檎┌縺・↑繧我ｽ輔ｂ縺励↑縺・・
    if (!panel.m_registry || Entity::IsNull(panel.m_selectedEntity)) {
        return;
    }

    // 驕ｸ謚昿ntity縺ｮ繝｢繝・Ν繝代せ縺後≠繧九↑繧峨√◎繧後ｒPlayerEditor縺ｧ髢九￥縲・
    if (!panel.m_selectedEntityModelPath.empty()) {
        OpenModelFromPath(panel, panel.m_selectedEntityModelPath);
    }

    // 驕ｸ謚昿ntity繧単review蟇ｾ雎｡縺ｫ縺吶ｋ縲・
    SetPreviewEntity(panel, panel.m_selectedEntity);

    if (const auto* embeddedStateMachine = panel.m_registry->GetComponent<StateMachineAssetComponent>(panel.m_selectedEntity)) {
        panel.m_stateMachineAsset = embeddedStateMachine->asset;
        panel.m_stateMachineAssetPath.clear();
        panel.m_stateMachineDirty = false;
    }

    if (const auto* timelineLibrary = panel.m_registry->GetComponent<TimelineLibraryComponent>(panel.m_selectedEntity)) {
        if (!timelineLibrary->assets.empty()) {
            panel.m_timelineAsset = timelineLibrary->assets.front();
            panel.m_timelineAssetPath.clear();
            panel.m_timelineDirty = false;
        }
    }

    if (const auto* embeddedInputMap = panel.m_registry->GetComponent<InputActionMapComponent>(panel.m_selectedEntity)) {
        panel.m_inputMappingTab.SetEditingMap(embeddedInputMap->asset);
    }

    // 驕ｸ謚昿ntity縺九ｉSocket諠・ｱ繧定ｪｭ縺ｿ霎ｼ繧縲・
    ImportSocketsFromPreviewEntity(panel);
}

// Preview Entity縺九ｉSocket諠・ｱ繧定ｪｭ縺ｿ霎ｼ繧縲・
void PlayerEditorSession::ImportSocketsFromPreviewEntity(PlayerEditorPanel& panel)
{
    // Registry縺檎┌縺・√∪縺溘・Preview Entity縺御ｽｿ縺医↑縺・↑繧唄ocket繧堤ｩｺ縺ｫ縺吶ｋ縲・
    if (!panel.m_registry || !panel.CanUsePreviewEntity()) {
        panel.m_sockets.clear();
        panel.m_socketDirty = false;
        return;
    }

    // NodeSocketComponent縺後≠繧後・縲√◎縺ｮSocket驟榊・繧脱ditor蛛ｴ縺ｸ繧ｳ繝斐・縺吶ｋ縲・
    if (const auto* sockets = panel.m_registry->GetComponent<NodeSocketComponent>(panel.m_previewEntity)) {
        panel.m_sockets = sockets->sockets;
    }
    // 辟｡縺代ｌ縺ｰSocket繝ｪ繧ｹ繝医ｒ遨ｺ縺ｫ縺吶ｋ縲・
    else {
        panel.m_sockets.clear();
    }

    // 隱ｭ縺ｿ霎ｼ縺ｿ逶ｴ蠕後↑縺ｮ縺ｧDirty縺ｧ縺ｯ縺ｪ縺・・
    panel.m_socketDirty = false;
}

// Editor蛛ｴ縺ｧ邱ｨ髮・＠縺欖ocket諠・ｱ繧単review Entity縺ｸ譖ｸ縺肴綾縺吶・
void PlayerEditorSession::ExportSocketsToPreviewEntity(PlayerEditorPanel& panel)
{
    // Registry縺檎┌縺・√∪縺溘・Preview Entity縺御ｽｿ縺医↑縺・↑繧我ｽ輔ｂ縺励↑縺・・
    if (!panel.m_registry || !panel.CanUsePreviewEntity()) {
        return;
    }

    // 譌｢蟄倥・NodeSocketComponent繧貞叙蠕励☆繧九・
    auto* sockets = panel.m_registry->GetComponent<NodeSocketComponent>(panel.m_previewEntity);

    // 辟｡縺代ｌ縺ｰ譁ｰ縺励￥霑ｽ蜉縺吶ｋ縲・
    if (!sockets) {
        panel.m_registry->AddComponent(panel.m_previewEntity, NodeSocketComponent{ panel.m_sockets });
        return;
    }

    // 縺ゅｌ縺ｰSocket驟榊・繧剃ｸ頑嶌縺阪☆繧九・
    sockets->sockets = panel.m_sockets;

    // 譖ｸ縺肴綾縺励◆縺ｮ縺ｧDirty繧定ｧ｣髯､縺吶ｋ縲・
    panel.m_socketDirty = false;
}
