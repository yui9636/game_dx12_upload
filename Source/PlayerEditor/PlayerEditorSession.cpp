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
#include "Component/HierarchyComponent.h"
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
#include "Undo/EntitySnapshot.h"

namespace
{
    // Timeline郢晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢ｧ蟶晏ｹ慕ｸｺ繝ｻ闖ｫ譎擾ｽｭ蛟･笘・ｹｧ荵昶・邵ｺ髦ｪ繝ｻ郢敖郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ騾包ｽｨ郢晁ｼ斐≦郢晢ｽｫ郢ｧ・ｿ邵ｲ繝ｻ
    static constexpr const char* kTimelineFileFilter =
        "Timeline (*.timeline.json)\0*.timeline.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // StateMachine郢晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢ｧ蟶晏ｹ慕ｸｺ繝ｻ闖ｫ譎擾ｽｭ蛟･笘・ｹｧ荵昶・邵ｺ髦ｪ繝ｻ郢敖郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ騾包ｽｨ郢晁ｼ斐≦郢晢ｽｫ郢ｧ・ｿ邵ｲ繝ｻ
    static constexpr const char* kStateMachineFileFilter =
        "StateMachine (*.statemachine.json)\0*.statemachine.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // InputMap郢晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢ｧ蟶晏ｹ慕ｸｺ繝ｻ闖ｫ譎擾ｽｭ蛟･笘・ｹｧ荵昶・邵ｺ髦ｪ繝ｻ郢敖郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ騾包ｽｨ郢晁ｼ斐≦郢晢ｽｫ郢ｧ・ｿ邵ｲ繝ｻ
    static constexpr const char* kInputMapFileFilter =
        "Input Map (*.inputmap.json)\0*.inputmap.json\0JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";

    // Prefab郢晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢ｧ蜑・ｽｿ譎擾ｽｭ蛟･笘・ｹｧ荵昶・邵ｺ髦ｪ繝ｻ郢敖郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ騾包ｽｨ郢晁ｼ斐≦郢晢ｽｫ郢ｧ・ｿ邵ｲ繝ｻ
    static constexpr const char* kPrefabFileFilter =
        "Prefab (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0";

    // 隰悶・・ｮ螢ｹ・・ｹｧ蠕娯螺郢昜ｻ｣縺帷ｸｺ・ｮ隲｡・｡陟托ｽｵ陝・・窶ｲ邵ｲ竏ｬ・ｨ・ｱ陷ｿ・ｯ邵ｺ霈費ｽ檎ｸｺ貊灘ヱ陟托ｽｵ陝・・ﾎ懃ｹｧ・ｹ郢晏現竊楢惺・ｫ邵ｺ・ｾ郢ｧ蠕娯ｻ邵ｺ繝ｻ・狗ｸｺ迢暦ｽ｢・ｺ髫ｱ髦ｪ笘・ｹｧ荵敖繝ｻ
    static bool HasExtension(const std::string& path, std::initializer_list<const char*> extensions)
    {
        // 郢昜ｻ｣縺帷ｸｺ荵晢ｽ芽ｫ｡・｡陟托ｽｵ陝・・笆｡邵ｺ莉｣・定愾謔ｶ・願怎・ｺ邵ｺ蜷ｶﾂ繝ｻ
        std::string ext = std::filesystem::path(path).extension().string();

        // 隲｡・｡陟托ｽｵ陝・・・定氣荵玲椢陝・干竊楢棔逕ｻ驪､邵ｺ蜉ｱ窶ｻ邵ｲ竏晢ｽ､・ｧ隴√・・ｭ諤懶ｽｰ荵玲椢陝・干繝ｻ鬩戊ｼ費ｼ樒ｸｺ・ｧ陞滂ｽｱ隰ｨ蜉ｱ・邵ｺ・ｪ邵ｺ繝ｻ・育ｸｺ繝ｻ竊鍋ｸｺ蜷ｶ・狗ｸｲ繝ｻ
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // 髫ｪ・ｱ陷ｿ・ｯ邵ｺ霈費ｽ檎ｸｺ貊灘ヱ陟托ｽｵ陝・・竊定叉ﾂ髢ｾ・ｴ邵ｺ蜷ｶ・狗ｸｺ邇厄ｽｪ・ｿ邵ｺ・ｹ郢ｧ荵敖繝ｻ
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

    static EntityID FindFirstMeshEntityRecursive(EntityID entity, Registry& registry)
    {
        if (Entity::IsNull(entity) || !registry.IsAlive(entity)) {
            return Entity::NULL_ID;
        }

        if (registry.GetComponent<MeshComponent>(entity)) {
            return entity;
        }

        const HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        if (!hierarchy) {
            return Entity::NULL_ID;
        }

        EntityID child = hierarchy->firstChild;
        while (!Entity::IsNull(child)) {
            EntityID found = FindFirstMeshEntityRecursive(child, registry);
            if (!Entity::IsNull(found)) {
                return found;
            }

            const HierarchyComponent* childHierarchy = registry.GetComponent<HierarchyComponent>(child);
            child = childHierarchy ? childHierarchy->nextSibling : Entity::NULL_ID;
        }

        return Entity::NULL_ID;
    }

    static float ComputePreviewFitRadius(const Model& model)
    {
        const auto bounds = model.GetWorldBounds();
        const DirectX::XMFLOAT3 ex = bounds.Extents;
        const float radius = std::sqrt(ex.x * ex.x + ex.y * ex.y + ex.z * ex.z);
        return radius > 0.01f ? radius : 1.0f;
    }

}

// PlayerEditor邵ｺ・ｮ郢晏干ﾎ樒ｹ晁侭ﾎ礼ｹ晢ｽｼ霑･・ｶ隲ｷ荵晢ｽ定叉ﾂ隴弱ｇ笳剰ｱ・ｽ｢邵ｺ蜷ｶ・狗ｸｲ繝ｻ
// Timeline陷蜥ｲ蜃ｽ邵ｲ・｣reviewState邵ｲ竏ｵ蝨定ｭ幄・review Entity郢ｧ蜻茨ｽｭ・｢郢ｧ竏夲ｽ狗ｸｲ繝ｻ
void PlayerEditorSession::Suspend(PlayerEditorPanel& panel)
{
    // 陷蜥ｲ蜃ｽ郢晏･繝｣郢晏ｳｨ・定怦逎ｯ・ｰ・ｭ邵ｺ・ｫ隰鯉ｽｻ邵ｺ蜷ｶﾂ繝ｻ
    panel.m_playheadFrame = 0;

    // Timeline陷蜥ｲ蜃ｽ霑･・ｶ隲ｷ荵晢ｽ定屁諛茨ｽｭ・｢邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    panel.m_isPlaying = false;

    // PreviewState邵ｺ・ｫ霑ｴ・ｾ陜ｨ・ｨ邵ｺ・ｮTimeline陷蜥ｲ蜃ｽ闖ｴ蜥ｲ・ｽ・ｮ郢ｧ雋樊ｸ夊ｭ擾｣ｰ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    SyncPreviewTimelinePlayback(panel);

    // Animation preview邵ｺ謔溯劒邵ｺ繝ｻ窶ｻ邵ｺ繝ｻ・狗ｸｺ・ｪ郢ｧ閾･・ｵ繧・ｽｺ繝ｻ・邵ｺ・ｦ陷医・繝ｻAnimator霑･・ｶ隲ｷ荵昶・隰鯉ｽｻ邵ｺ蜷ｶﾂ繝ｻ
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // PlayerEditor邵ｺ迹壹・陋ｻ繝ｻ縲定抄諛岩夢邵ｺ陂睦eview Entity郢ｧ蝣､・ｰ・ｴ隴ｽ繝ｻ笘・ｹｧ荵敖繝ｻ
    DestroyOwnedPreviewEntity(panel);
}

// PlayerEditor邵ｺ譴ｧ蝨定ｭ帛ｳｨ・邵ｺ・ｦ邵ｺ繝ｻ・輝review Entity郢ｧ蝣､・ｰ・ｴ隴ｽ繝ｻ笘・ｹｧ荵敖繝ｻ
// 陞溷､慚夂ｸｺ荵晢ｽ蛾ｩ包ｽｸ隰壽ｧｭ・・ｹｧ蠕娯螺Entity邵ｺ・ｯ驕撰ｽｴ隴ｽ繝ｻ笳狗ｸｺ螢ｹﾂ竏ｬ繝ｻ陋ｻ繝ｻ縲定抄諛岩夢邵ｺ貅假ｽらｸｺ・ｮ邵ｺ・ｰ邵ｺ莉｣・定ｱｸ蛹ｻ笘・ｸｲ繝ｻ
void PlayerEditorSession::DestroyOwnedPreviewEntity(PlayerEditorPanel& panel)
{
    // PlayerEditor隰・隴帛ｳｨ繝ｻEntity邵ｺ・ｧ邵ｺ・ｪ邵ｺ莉｣・檎ｸｺ・ｰ驕撰ｽｴ隴ｽ繝ｻ・邵ｺ・ｪ邵ｺ繝ｻﾂ繝ｻ
    if (!panel.m_previewEntityOwned) {
        return;
    }

    // PreviewState邵ｺ謔溯劒邵ｺ繝ｻ窶ｻ邵ｺ繝ｻ・狗ｸｺ・ｪ郢ｧ迚吶・邵ｺ・ｫ雎・ｽ｢郢ｧ竏夲ｽ狗ｸｲ繝ｻ
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // Registry邵ｺ蠕娯旺郢ｧ鄙ｫﾂ・｣review Entity邵ｺ讙主・陝・･・邵ｺ・ｦ邵ｺ繝ｻ・狗ｸｺ・ｪ郢ｧ閾･・ｰ・ｴ隴ｽ繝ｻ笘・ｹｧ荵敖繝ｻ
    if (panel.m_registry && !Entity::IsNull(panel.m_previewEntity) && panel.m_registry->IsAlive(panel.m_previewEntity)) {
        EntitySnapshot::DestroySubtree(panel.m_previewEntity, *panel.m_registry);
    }

    // Preview Entity隲繝ｻ・ｰ・ｱ郢ｧ蝣､・ｩ・ｺ邵ｺ・ｫ隰鯉ｽｻ邵ｺ蜷ｶﾂ繝ｻ
    panel.m_previewEntity = Entity::NULL_ID;
    panel.m_previewEntityOwned = false;
}

// PlayerEditor陝・ｉ逡醍ｸｺ・ｮPreview Entity郢ｧ蜑・ｽｽ諛医・邵ｺ蜷ｶ・狗ｸｲ繝ｻ
// 郢晢ｽ｢郢昴・ﾎ晉ｹｧ蟶晏ｹ慕ｸｺ繝ｻ笳・ｸｺ・ｨ邵ｺ髦ｪﾂ繝ｾcene闕ｳ鄙ｫ竊鍋ｹ晏干ﾎ樒ｹ晁侭ﾎ礼ｹ晢ｽｼ騾包ｽｨEntity郢ｧ蜑・ｽｽ諛岩夢邵ｺ・ｦMeshComponent郢ｧ蜑・ｽｻ蛟･・郢ｧ荵敖繝ｻ
void PlayerEditorSession::EnsureOwnedPreviewEntity(PlayerEditorPanel& panel)
{
    // Registry邵ｺ讙寂伯邵ｺ繝ｻﾂ竏壺穐邵ｺ貅倥・郢晢ｽ｢郢昴・ﾎ晉ｸｺ遒∝ｹ慕ｸｺ荵晢ｽ檎ｸｺ・ｦ邵ｺ繝ｻ竊醍ｸｺ繝ｻ竊醍ｹｧ謌托ｽｽ諛奇ｽ檎ｸｺ・ｪ邵ｺ繝ｻﾂ繝ｻ
    if (!panel.m_registry || !panel.HasOpenModel()) {
        return;
    }

    // 隴鯉ｽ｢邵ｺ・ｫ闖ｴ・ｿ騾包ｽｨ陷ｿ・ｯ髢ｭ・ｽ邵ｺ・ｪPreview Entity邵ｺ蠕娯旺郢ｧ荵昶・郢ｧ逕ｻ逵邵ｺ蜉ｱ・･闖ｴ諛奇ｽ臥ｸｺ・ｪ邵ｺ繝ｻﾂ繝ｻ
    if (panel.CanUsePreviewEntity()) {
        return;
    }

    // 陷ｿ・､邵ｺ繝ｻ蝨定ｭ幄・review Entity邵ｺ譴ｧ・ｮ荵昶夢邵ｺ・ｦ邵ｺ繝ｻ・玖撻・ｴ陷ｷ蛹ｻ繝ｻ陷亥現竊馴＄・ｴ隴ｽ繝ｻ笘・ｹｧ荵敖繝ｻ
    DestroyOwnedPreviewEntity(panel);

    // 隴・ｽｰ邵ｺ蜉ｱ・昿ntity郢ｧ蝮ｦegistry邵ｺ・ｫ闖ｴ諛奇ｽ狗ｸｲ繝ｻ
    panel.m_previewEntity = panel.m_registry->CreateEntity();

    // 邵ｺ阮吶・Entity邵ｺ・ｯPlayerEditor邵ｺ蠕｡・ｽ諛岩夢邵ｺ貅假ｽらｸｺ・ｮ邵ｺ・ｨ邵ｺ蜉ｱ窶ｻ驍ゑｽ｡騾・・笘・ｹｧ荵敖繝ｻ
    panel.m_previewEntityOwned = true;

    // 郢昴・繝ｰ郢昴・縺堤ｹｧﾐ擁erarchy髯ｦ・ｨ驕会ｽｺ騾包ｽｨ邵ｺ・ｮ陷ｷ讎顔√郢ｧ蜑・ｽｻ蛟･・郢ｧ荵敖繝ｻ
    panel.m_registry->AddComponent(panel.m_previewEntity, NameComponent{ "Player Preview" });

    // 陷ｴ貅ｽ縺帷ｸｺ・ｫ驗ゑｽｮ邵ｺ荳岩螺郢ｧ竏壹・TransformComponent郢ｧ蜑・ｽｽ諛奇ｽ狗ｸｲ繝ｻ
    TransformComponent transform{};

    // Preview Entity邵ｺ・ｮ闖ｴ蜥ｲ・ｽ・ｮ邵ｺ・ｯ陷ｴ貅ｽ縺帷ｸｲ繝ｻ
    transform.localPosition = { 0.0f, 0.0f, 0.0f };

    // Preview Entity邵ｺ・ｮ郢ｧ・ｹ郢ｧ・ｱ郢晢ｽｼ郢晢ｽｫ邵ｺ・ｯ驕ｲ迚卍髦ｪﾂ繝ｻ
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



    // Transform隴厄ｽｴ隴・ｽｰ邵ｺ謔滂ｽｿ繝ｻ・ｦ竏壺味邵ｺ・ｨ驕擾ｽ･郢ｧ蟲ｨ笳狗ｹｧ荵敖繝ｻ
    transform.isDirty = true;

    // Entity邵ｺ・ｫTransformComponent郢ｧ螳夲ｽｿ・ｽ陷会｣ｰ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    panel.m_registry->AddComponent(panel.m_previewEntity, transform);

    // 隰蜀怜愛騾包ｽｨ邵ｺ・ｮMeshComponent郢ｧ蜑・ｽｽ諛奇ｽ狗ｸｲ繝ｻ
    MeshComponent mesh{};

    // 郢晢ｽ｢郢昴・ﾎ晉ｹ晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢昜ｻ｣縺帷ｹｧ雋槭・郢ｧ蠕鯉ｽ狗ｸｲ繝ｻ
    mesh.modelFilePath = panel.m_currentModelPath;

    // ResourceManager邵ｺ荵晢ｽ芽愾髢・ｾ蜉ｱ・邵ｺ貅佩皮ｹ昴・ﾎ晁ｭ幢ｽｬ闖ｴ阮呻ｽ定怦・･郢ｧ蠕鯉ｽ狗ｸｲ繝ｻ
    mesh.model = panel.m_ownedModel;

    // 髯ｦ・ｨ驕会ｽｺ霑･・ｶ隲ｷ荵晢ｽ定ｭ帷甥譟醍ｸｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    mesh.isVisible = true;

    // 陟厄ｽｱ郢ｧ螳夊ｪ邵ｺ・ｨ邵ｺ蜻ｵ・ｨ・ｭ陞ｳ螢ｹ竊鍋ｸｺ蜷ｶ・狗ｸｲ繝ｻ
    mesh.castShadow = true;

    // Entity邵ｺ・ｫMeshComponent郢ｧ螳夲ｽｿ・ｽ陷会｣ｰ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    // 邵ｺ阮呻ｼ・ｸｺ・ｾ邵ｺ・ｧ隴夲ｽ･郢ｧ蠕後・邵ｲ蠕湖皮ｹ昴・ﾎ晉ｹｧ蜻域亜邵ｺ・｣邵ｺ讙ｸntity邵ｲ髦ｪ繝ｻ闖ｴ諛奇ｽ臥ｹｧ蠕娯ｻ邵ｺ繝ｻ・狗ｸｲ繝ｻ
    panel.m_registry->AddComponent(panel.m_previewEntity, mesh);

    // EffectPreview騾包ｽｨ邵ｺ・ｮ騾ｶ・ｮ陷奇ｽｰComponent郢ｧ蜑・ｽｻ蛟･・郢ｧ荵敖繝ｻ
    panel.m_registry->AddComponent(panel.m_previewEntity, EffectPreviewTagComponent{});

    // Timeline邵ｲ繝ｾtateMachine邵ｲ・］putMap邵ｲ繝ｾocket邵ｺ・ｪ邵ｺ・ｩ邵ｺ・ｮEditor陋幢ｽｴ郢昴・繝ｻ郢ｧ・ｿ郢ｧ蜊腕eview Entity邵ｺ・ｸ陷ｿ閧ｴ荳千ｸｺ蜷ｶ・狗ｸｲ繝ｻ
    ApplyEditorBindingsToPreviewEntity(panel);
}

// PlayerEditor邵ｺ・ｮPreview陝・ｽｾ髮趣ｽ｡郢ｧ蛛ｵﾂ竏晢ｽ､螟慚哘ntity邵ｺ・ｫ陝ｾ・ｮ邵ｺ邇ｲ蟠帷ｸｺ蛹ｻ・狗ｸｲ繝ｻ
// 邵ｺ・､邵ｺ・ｾ郢ｧ鄙ｫﾂ險斗ayerEditor陝・ｉ逡薦ntity邵ｲ髦ｪ縲堤ｸｺ・ｯ邵ｺ・ｪ邵ｺ荳環竏ｵ驥瑚氛迚｢cene闕ｳ鄙ｫ繝ｻEntity郢ｧ蝣､・ｷ・ｨ鬮ｮ繝ｻ・ｯ・ｾ髮趣ｽ｡邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
void PlayerEditorSession::SetPreviewEntity(PlayerEditorPanel& panel, EntityID entity)
{
    // 隴鯉ｽ｢邵ｺ・ｫ陷ｷ蠕個ｧ陞溷､慚哘ntity郢ｧ蜊腕eview陝・ｽｾ髮趣ｽ｡邵ｺ・ｫ邵ｺ蜉ｱ窶ｻ邵ｺ繝ｻ・狗ｸｺ・ｪ郢ｧ謌托ｽｽ霈費ｽらｸｺ蜉ｱ竊醍ｸｺ繝ｻﾂ繝ｻ
    if (panel.m_previewEntity == entity && !panel.m_previewEntityOwned) {
        return;
    }

    // 陷蜥ｲ蜃ｽ闕ｳ・ｭ邵ｺ・ｪ郢ｧ逕ｻ・ｭ・｢郢ｧ竏夲ｽ狗ｸｲ繝ｻ
    panel.m_isPlaying = false;

    // PreviewState邵ｺ謔溯劒邵ｺ繝ｻ窶ｻ邵ｺ繝ｻ・狗ｸｺ・ｪ郢ｧ閾･・ｵ繧・ｽｺ繝ｻ笘・ｹｧ荵敖繝ｻ
    if (panel.m_previewState.IsActive()) {
        panel.m_previewState.ExitPreview();
    }

    // 隰・隴幄・review Entity邵ｺ蠕娯旺郢ｧ荵昶・郢ｧ閾･・ｰ・ｴ隴ｽ繝ｻ笘・ｹｧ荵敖繝ｻ
    DestroyOwnedPreviewEntity(panel);

    // 陞溷､慚哘ntity郢ｧ蜊腕eview陝・ｽｾ髮趣ｽ｡邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    panel.m_previewEntity = entity;

    // 陞溷､慚哘ntity邵ｺ・ｪ邵ｺ・ｮ邵ｺ・ｧPlayerEditor隰・隴帛ｳｨ縲堤ｸｺ・ｯ邵ｺ・ｪ邵ｺ繝ｻﾂ繝ｻ
    panel.m_previewEntityOwned = false;

    // 陞溷､慚哘ntity邵ｺ荵晢ｽ唄ocket隲繝ｻ・ｰ・ｱ郢ｧ螳夲ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ邵ｲ繝ｻ
    ImportSocketsFromPreviewEntity(panel);
}

// EditorLayer邵ｺ・ｪ邵ｺ・ｩ陞溷､慚夐ｩ包ｽｸ隰壽ｧｭﾂｰ郢ｧ蟲ｨﾂ・｣layerEditor邵ｺ・ｸ霑ｴ・ｾ陜ｨ・ｨ鬩包ｽｸ隰壽仭ntity隲繝ｻ・ｰ・ｱ郢ｧ雋樣・隴帶ｺ倪・郢ｧ荵敖繝ｻ
void PlayerEditorSession::SyncExternalSelection(PlayerEditorPanel& panel, EntityID entity, const std::string& modelPath)
{
    // 霑ｴ・ｾ陜ｨ・ｨ鬩包ｽｸ隰壽ｧｭ・・ｹｧ蠕娯ｻ邵ｺ繝ｻ・畿ntity郢ｧ蜑・ｽｿ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
    panel.m_selectedEntity = entity;

    // 邵ｺ譏ｴ繝ｻEntity邵ｺ・ｮ郢晢ｽ｢郢昴・ﾎ晉ｹ昜ｻ｣縺帷ｹｧ蜑・ｽｿ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
    panel.m_selectedEntityModelPath = modelPath;

    // Preview Entity邵ｺ譴ｧ驥檎ｸｺ・ｫ雎・ｽｻ郢ｧ阮吶堤ｸｺ繝ｻ・狗ｸｺ・ｪ郢ｧ迚咏崟霎｣・ｧ郢ｧ蝣､・ｩ・ｺ邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    if (!Entity::IsNull(panel.m_previewEntity) && panel.m_registry && !panel.m_registry->IsAlive(panel.m_previewEntity)) {
        panel.m_previewEntity = Entity::NULL_ID;
        panel.m_previewEntityOwned = false;
    }
}

// 郢晢ｽ｢郢昴・ﾎ晉ｹ晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢ｧ蟶晏ｹ慕ｸｺ髦ｪﾂ・｣layerEditor邵ｺ・ｮPreview騾包ｽｨ郢晢ｽ｢郢昴・ﾎ晉ｸｺ・ｨ邵ｺ蜉ｱ窶ｻ髫ｪ・ｭ陞ｳ螢ｹ笘・ｹｧ荵敖繝ｻ
bool PlayerEditorSession::OpenModelFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    const bool isPrefab = HasExtension(path, { ".prefab" });

    if (isPrefab) {
        if (!panel.m_registry) {
            return false;
        }

        EntitySnapshot::Snapshot snapshot;
        if (!PrefabSystem::LoadPrefabSnapshot(path, snapshot)) {
            return false;
        }

        Suspend(panel);

        EntitySnapshot::RestoreResult restore = EntitySnapshot::RestoreSubtree(snapshot, *panel.m_registry);
        if (Entity::IsNull(restore.root) || !panel.m_registry->IsAlive(restore.root)) {
            return false;
        }

        if (!panel.m_registry->GetComponent<EffectPreviewTagComponent>(restore.root)) {
            panel.m_registry->AddComponent(restore.root, EffectPreviewTagComponent{});
        }

        if (auto* prefabInstance = panel.m_registry->GetComponent<PrefabInstanceComponent>(restore.root)) {
            prefabInstance->prefabAssetPath = path;
            prefabInstance->hasOverrides = false;
        }
        else {
            PrefabInstanceComponent newPrefabInstance{};
            newPrefabInstance.prefabAssetPath = path;
            newPrefabInstance.hasOverrides = false;
            panel.m_registry->AddComponent(restore.root, newPrefabInstance);
        }

        EntityID meshEntity = FindFirstMeshEntityRecursive(restore.root, *panel.m_registry);
        if (Entity::IsNull(meshEntity)) {
            EntitySnapshot::DestroySubtree(restore.root, *panel.m_registry);
            return false;
        }

        MeshComponent* mesh = panel.m_registry->GetComponent<MeshComponent>(meshEntity);
        if (!mesh || mesh->modelFilePath.empty()) {
            EntitySnapshot::DestroySubtree(restore.root, *panel.m_registry);
            return false;
        }

        std::shared_ptr<Model> model = mesh->model;
        if (!model) {
            model = ResourceManager::Instance().CreateModelInstance(mesh->modelFilePath);
            if (!model) {
                EntitySnapshot::DestroySubtree(restore.root, *panel.m_registry);
                return false;
            }
            mesh->model = model;
        }

        panel.m_previewEntity = restore.root;
        panel.m_previewEntityOwned = true;
        panel.m_ownedModel = std::move(model);
        panel.m_model = panel.m_ownedModel.get();
        panel.m_currentModelPath = mesh->modelFilePath;
        panel.ResetSelectionState();
        panel.m_selectedAnimIndex = 0;
        panel.m_previewRenderSize = { 0.0f, 0.0f };
        panel.m_previewModelScale = 1.0f;
        panel.m_sockets.clear();

        if (const auto* embeddedStateMachine = panel.m_registry->GetComponent<StateMachineAssetComponent>(restore.root)) {
            panel.m_stateMachineAsset = embeddedStateMachine->asset;
        }
        else {
            panel.m_stateMachineAsset = StateMachineAsset{};
        }
        panel.m_stateMachineAssetPath.clear();
        panel.m_stateMachineDirty = false;

        if (const auto* timelineLibrary = panel.m_registry->GetComponent<TimelineLibraryComponent>(restore.root)) {
            if (!timelineLibrary->assets.empty()) {
                panel.m_timelineAsset = timelineLibrary->assets.front();
            }
            else {
                panel.m_timelineAsset = TimelineAsset{};
            }
        }
        else {
            panel.m_timelineAsset = TimelineAsset{};
        }
        panel.m_timelineAssetPath.clear();
        panel.m_timelineDirty = false;

        if (const auto* embeddedInputMap = panel.m_registry->GetComponent<InputActionMapComponent>(restore.root)) {
            panel.m_inputMappingTab.SetEditingMap(embeddedInputMap->asset);
        }
        else {
            panel.m_inputMappingTab.ClearEditingMap();
        }

        PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*panel.m_registry, restore.root);
        PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, restore.root);
        PlayerRuntimeSetup::ResetPlayerRuntimeState(*panel.m_registry, restore.root);

        ImportSocketsFromPreviewEntity(panel);
        RebuildPreviewTimelineRuntimeData(panel);
        const auto bounds = panel.m_model->GetWorldBounds();
        panel.m_pendingCameraFitTarget = bounds.Center;
        panel.m_pendingCameraFitRadius = ComputePreviewFitRadius(*panel.m_model);
        panel.m_hasPendingCameraFit = true;
        return true;
    }

    std::shared_ptr<Model> model = ResourceManager::Instance().CreateModelInstance(path);
    if (!model) {
        return false;
    }

    Suspend(panel);
    panel.m_ownedModel = std::move(model);
    panel.m_model = panel.m_ownedModel.get();
    panel.m_currentModelPath = path;
    panel.ResetSelectionState();
    panel.m_selectedAnimIndex = 0;
    panel.m_previewModelScale = 1.0f;
    panel.m_previewRenderSize = { 0.0f, 0.0f };
    panel.m_sockets.clear();
    panel.m_socketDirty = false;
    panel.m_timelineDirty = false;
    panel.m_stateMachineDirty = false;
    EnsureOwnedPreviewEntity(panel);
    const auto bounds = panel.m_model->GetWorldBounds();
    panel.m_pendingCameraFitTarget = bounds.Center;
    panel.m_pendingCameraFitRadius = ComputePreviewFitRadius(*panel.m_model);
    panel.m_hasPendingCameraFit = true;
    return true;
}

// Timeline郢晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢ｧ螳夲ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ邵ｲ繝ｻ
bool PlayerEditorSession::OpenTimelineFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // 驕ｨ・ｺ郢昜ｻ｣縺帷ｸｲ竏壺穐邵ｺ貅倥・闕ｳ閧ｴ・ｭ・｣邵ｺ・ｪ隲｡・｡陟托ｽｵ陝・・竊醍ｹｧ迚呻ｽ､・ｱ隰ｨ蜉ｱﾂ繝ｻ
    if (path.empty() || !HasExtension(path, { ".timeline.json", ".json" })) {
        return false;
    }

    // TimelineAsset郢ｧ隹ｷSON邵ｺ荵晢ｽ蛾坡・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ邵ｲ繝ｻ
    if (!TimelineAssetSerializer::Load(path, panel.m_timelineAsset)) {
        return false;
    }

    if (panel.m_timelineAsset.id == 0) {
        panel.m_timelineAsset.id = 1;
    }

    // 髫ｱ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧ阮吮味Timeline郢昜ｻ｣縺帷ｹｧ蜑・ｽｿ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
    panel.m_timelineAssetPath = path;

    // 髫ｱ・ｭ邵ｺ・ｿ髴趣ｽｼ邵ｺ・ｿ騾ｶ・ｴ陟募ｾ娯・邵ｺ・ｮ邵ｺ・ｧDirty邵ｺ・ｧ邵ｺ・ｯ邵ｺ・ｪ邵ｺ繝ｻﾂ繝ｻ
    panel.m_timelineDirty = false;

    // Preview Entity騾包ｽｨ邵ｺ・ｮTimeline runtime data郢ｧ雋槭・隶堤距・ｯ蟲ｨ笘・ｹｧ荵敖繝ｻ
    RebuildPreviewTimelineRuntimeData(panel);

    return true;
}

// StateMachine郢晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢ｧ螳夲ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ邵ｲ繝ｻ
bool PlayerEditorSession::OpenStateMachineFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // 驕ｨ・ｺ郢昜ｻ｣縺帷ｸｲ竏壺穐邵ｺ貅倥・闕ｳ閧ｴ・ｭ・｣邵ｺ・ｪ隲｡・｡陟托ｽｵ陝・・竊醍ｹｧ迚呻ｽ､・ｱ隰ｨ蜉ｱﾂ繝ｻ
    if (path.empty() || !HasExtension(path, { ".statemachine.json", ".json" })) {
        return false;
    }

    // StateMachineAsset郢ｧ隹ｷSON邵ｺ荵晢ｽ蛾坡・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ邵ｲ繝ｻ
    if (!StateMachineAssetSerializer::Load(path, panel.m_stateMachineAsset)) {
        return false;
    }

    // 髫ｱ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧ阮吮味StateMachine郢昜ｻ｣縺帷ｹｧ蜑・ｽｿ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
    panel.m_stateMachineAssetPath = path;

    // 髫ｱ・ｭ邵ｺ・ｿ髴趣ｽｼ邵ｺ・ｿ騾ｶ・ｴ陟募ｾ娯・邵ｺ・ｮ邵ｺ・ｧDirty邵ｺ・ｧ邵ｺ・ｯ邵ｺ・ｪ邵ｺ繝ｻﾂ繝ｻ
    panel.m_stateMachineDirty = false;

    return true;
}

// InputMap郢晁ｼ斐＜郢ｧ・､郢晢ｽｫ郢ｧ螳夲ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ邵ｲ繝ｻ
bool PlayerEditorSession::OpenInputMapFromPath(PlayerEditorPanel& panel, const std::string& path)
{
    // InputMappingTab陋幢ｽｴ邵ｺ・ｮOpen陷・ｽｦ騾・・竏郁沂遒托ｽｭ・ｲ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    return panel.m_inputMappingTab.OpenActionMap(path);
}

// Timeline郢晏ｳｨ縺冗ｹ晢ｽ･郢晢ｽ｡郢晢ｽｳ郢晏現・定将譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
bool PlayerEditorSession::SaveTimelineDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 霑ｴ・ｾ陜ｨ・ｨ邵ｺ・ｮTimeline闖ｫ譎擾ｽｭ莨懊・郢ｧ雋槫徐陟募干笘・ｹｧ荵敖繝ｻ
    std::string path = panel.m_timelineAssetPath;

    // SaveAs隰悶・・ｮ螢ｹﾂ竏壺穐邵ｺ貅倥・闖ｫ譎擾ｽｭ莨懊・隴幢ｽｪ髫ｪ・ｭ陞ｳ螢ｹ竊醍ｹｧ謌托ｽｿ譎擾ｽｭ蛟･繝郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ郢ｧ雋槭・邵ｺ蜷ｶﾂ繝ｻ
    if (saveAs || path.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 隴鯉ｽ｢陝・･繝ｱ郢ｧ・ｹ邵ｺ蠕娯旺郢ｧ蠕後・陋ｻ譎・ｄ陋滂ｽ､邵ｺ・ｫ闖ｴ・ｿ邵ｺ繝ｻﾂ繝ｻ
        if (!path.empty()) {
            strcpy_s(pathBuffer, path.c_str());
        }
        // Timeline陷ｷ髦ｪ窶ｲ邵ｺ繧・ｽ檎ｸｺ・ｰ邵ｲ竏壺落郢ｧ蠕鯉ｽ定抄・ｿ邵ｺ・｣邵ｺ・ｦ陋ｻ譎・ｄ闖ｫ譎擾ｽｭ莨懊・郢ｧ蜑・ｽｽ諛奇ｽ狗ｸｲ繝ｻ
        else if (!panel.m_timelineAsset.name.empty()) {
            strcpy_s(pathBuffer, ("Assets/Timeline/" + panel.m_timelineAsset.name + ".timeline.json").c_str());
        }

        // 闖ｫ譎擾ｽｭ蛟･繝郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ郢ｧ蟶晏ｹ慕ｸｺ荳環繝ｻ
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kTimelineFileFilter, "Save Timeline", "json") != DialogResult::OK) {
            return false;
        }

        // 闖ｫ譎擾ｽｭ莨懊・郢ｧ蝣､・｢・ｺ陞ｳ螢ｹ笘・ｹｧ荵敖繝ｻ
        path = pathBuffer;
    }

    // TimelineAsset郢ｧ隹ｷSON邵ｺ・ｸ闖ｫ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
    if (!TimelineAssetSerializer::Save(path, panel.m_timelineAsset)) {
        return false;
    }

    // 闖ｫ譎擾ｽｭ莨懊・郢ｧ螳夲ｽｨ蛟ｬ鮖ｸ邵ｺ蜉ｱﾂ・司rty郢ｧ螳夲ｽｧ・｣鬮ｯ・､邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    panel.m_timelineAssetPath = path;
    panel.m_timelineDirty = false;

    // StateMachine/Timeline runtime cache郢ｧ蝣､笏瑚怏・ｹ陋ｹ謔ｶ笘・ｹｧ荵敖繝ｻ
    StateMachineSystem::InvalidateAssetCache(path.c_str());

    return true;
}

// StateMachine郢晏ｳｨ縺冗ｹ晢ｽ･郢晢ｽ｡郢晢ｽｳ郢晏現・定将譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
bool PlayerEditorSession::SaveStateMachineDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 霑ｴ・ｾ陜ｨ・ｨ邵ｺ・ｮStateMachine闖ｫ譎擾ｽｭ莨懊・郢ｧ雋槫徐陟募干笘・ｹｧ荵敖繝ｻ
    std::string path = panel.m_stateMachineAssetPath;

    // SaveAs隰悶・・ｮ螢ｹﾂ竏壺穐邵ｺ貅倥・闖ｫ譎擾ｽｭ莨懊・隴幢ｽｪ髫ｪ・ｭ陞ｳ螢ｹ竊醍ｹｧ謌托ｽｿ譎擾ｽｭ蛟･繝郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ郢ｧ雋槭・邵ｺ蜷ｶﾂ繝ｻ
    if (saveAs || path.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 隴鯉ｽ｢陝・･繝ｱ郢ｧ・ｹ邵ｺ蠕娯旺郢ｧ蠕後・陋ｻ譎・ｄ陋滂ｽ､邵ｺ・ｫ闖ｴ・ｿ邵ｺ繝ｻﾂ繝ｻ
        if (!path.empty()) {
            strcpy_s(pathBuffer, path.c_str());
        }
        // StateMachine陷ｷ髦ｪ窶ｲ邵ｺ繧・ｽ檎ｸｺ・ｰ邵ｲ竏壺落郢ｧ蠕鯉ｽ定抄・ｿ邵ｺ・｣邵ｺ・ｦ陋ｻ譎・ｄ闖ｫ譎擾ｽｭ莨懊・郢ｧ蜑・ｽｽ諛奇ｽ狗ｸｲ繝ｻ
        else if (!panel.m_stateMachineAsset.name.empty()) {
            strcpy_s(pathBuffer, ("Assets/StateMachine/" + panel.m_stateMachineAsset.name + ".statemachine.json").c_str());
        }

        // 闖ｫ譎擾ｽｭ蛟･繝郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ郢ｧ蟶晏ｹ慕ｸｺ荳環繝ｻ
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kStateMachineFileFilter, "Save State Machine", "json") != DialogResult::OK) {
            return false;
        }

        // 闖ｫ譎擾ｽｭ莨懊・郢ｧ蝣､・｢・ｺ陞ｳ螢ｹ笘・ｹｧ荵敖繝ｻ
        path = pathBuffer;
    }

    // StateMachineAsset郢ｧ隹ｷSON邵ｺ・ｸ闖ｫ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
    if (!StateMachineAssetSerializer::Save(path, panel.m_stateMachineAsset)) {
        return false;
    }

    // 闖ｫ譎擾ｽｭ莨懊・郢ｧ螳夲ｽｨ蛟ｬ鮖ｸ邵ｺ蜉ｱﾂ・司rty郢ｧ螳夲ｽｧ・｣鬮ｯ・､邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    panel.m_stateMachineAssetPath = path;
    panel.m_stateMachineDirty = false;

    // StateMachine cache郢ｧ蝣､笏瑚怏・ｹ陋ｹ謔ｶ笘・ｹｧ荵敖繝ｻ
    StateMachineSystem::InvalidateAssetCache(path.c_str());

    return true;
}

// InputMap郢晏ｳｨ縺冗ｹ晢ｽ･郢晢ｽ｡郢晢ｽｳ郢晏現・定将譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
bool PlayerEditorSession::SaveInputMapDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // 鬨ｾ螢ｼ・ｸ・ｸ闖ｫ譎擾ｽｭ蛟･竊醍ｹｧ霑ＯputMappingTab陋幢ｽｴ邵ｺ・ｸ陝狗｢托ｽｭ・ｲ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    if (!saveAs) {
        return panel.m_inputMappingTab.SaveActionMap();
    }

    // SaveAs騾包ｽｨ邵ｺ・ｮ闖ｫ譎擾ｽｭ莨懊・郢晁・繝｣郢晁ｼ斐＜邵ｲ繝ｻ
    char pathBuffer[MAX_PATH] = {};

    // 霑ｴ・ｾ陜ｨ・ｨ邵ｺ・ｮInputMap郢昜ｻ｣縺帷ｹｧ雋槭・隴帶ｺｷﾂ・､邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    const std::string& currentPath = panel.m_inputMappingTab.GetActionMapPath();
    if (!currentPath.empty()) {
        strcpy_s(pathBuffer, currentPath.c_str());
    }

    // 闖ｫ譎擾ｽｭ蛟･繝郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ郢ｧ蟶晏ｹ慕ｸｺ荳環繝ｻ
    if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kInputMapFileFilter, "Save Input Map", "json") != DialogResult::OK) {
        return false;
    }

    // 隰悶・・ｮ螢ｹ繝ｱ郢ｧ・ｹ邵ｺ・ｸInputMap郢ｧ蜑・ｽｿ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
    return panel.m_inputMappingTab.SaveActionMapAs(pathBuffer);
}

// Timeline / StateMachine / InputMap / Socket 郢ｧ蛛ｵ竏ｪ邵ｺ・ｨ郢ｧ竏壺ｻ闖ｫ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
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

// Preview Entity郢ｧ蜊腕efab邵ｺ・ｨ邵ｺ蜉ｱ窶ｻ闖ｫ譎擾ｽｭ蛟･笘・ｹｧ荵敖繝ｻ
bool PlayerEditorSession::SavePrefabDocument(PlayerEditorPanel& panel, bool saveAs)
{
    // Registry邵ｺ讙寂伯邵ｺ繝ｻﾂ竏壺穐邵ｺ貅倥・Preview Entity邵ｺ讙寂伯邵ｺ繝ｻ竊醍ｹｧ謌托ｽｿ譎擾ｽｭ蛟･縲堤ｸｺ髦ｪ竊醍ｸｺ繝ｻﾂ繝ｻ
    if (!panel.m_registry || Entity::IsNull(panel.m_previewEntity)) {
        return false;
    }

    // Editor邵ｺ・ｧ髫ｪ・ｭ陞ｳ螢ｹ・邵ｺ鬯ｱimeline/StateMachine/Input/Socket郢ｧ蜊腕eview Entity邵ｺ・ｸ陷ｿ閧ｴ荳千ｸｺ蜷ｶ・狗ｸｲ繝ｻ    ApplyEditorBindingsToPreviewEntity(panel);

    // Socket隲繝ｻ・ｰ・ｱ郢ｧ・喪eview Entity邵ｺ・ｸ隴厄ｽｸ邵ｺ閧ｴ邯ｾ邵ｺ蜷ｶﾂ繝ｻ
    ExportSocketsToPreviewEntity(panel);

    // Player邵ｺ・ｨ邵ｺ蜉ｱ窶ｻPrefab邵ｺ・ｫ陟｢繝ｻ・ｦ竏壺・雎鯉ｽｸ驍ｯ蜥ｾomponent郢ｧ蜑・ｽｿ譎・ｽｨ・ｼ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    PlayerRuntimeSetup::EnsurePlayerPersistentComponents(*panel.m_registry, panel.m_previewEntity);

    // Runtime騾包ｽｨComponent郢ｧ繧・ｽｿ譎・ｽｨ・ｼ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, panel.m_previewEntity);

    // Runtime霑･・ｶ隲ｷ荵晢ｽ定崕譎・ｄ陋ｹ謔ｶ笘・ｹｧ荵敖繝ｻ
    PlayerRuntimeSetup::ResetPlayerRuntimeState(*panel.m_registry, panel.m_previewEntity);

    // 闖ｫ譎擾ｽｭ莨懊・Prefab郢昜ｻ｣縺帷ｸｲ繝ｻ
    std::string prefabPath;

    // SaveAs邵ｺ・ｧ邵ｺ・ｪ邵ｺ莉｣・檎ｸｺ・ｰ邵ｲ・｣refabInstanceComponent邵ｺ・ｮ隴鯉ｽ｢陝・･繝ｱ郢ｧ・ｹ郢ｧ蜑・ｽｽ・ｿ邵ｺ繝ｻﾂ繝ｻ
    if (!saveAs) {
        if (const PrefabInstanceComponent* prefab = panel.m_registry->GetComponent<PrefabInstanceComponent>(panel.m_previewEntity)) {
            prefabPath = prefab->prefabAssetPath;
        }
    }

    // 闖ｫ譎擾ｽｭ莨懊・邵ｺ蠕娯穐邵ｺ・ｰ霎滂ｽ｡邵ｺ莉｣・檎ｸｺ・ｰ闖ｫ譎擾ｽｭ蛟･繝郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ邵ｺ・ｧ雎趣ｽｺ郢ｧ竏夲ｽ狗ｸｲ繝ｻ
    if (prefabPath.empty()) {
        char pathBuffer[MAX_PATH] = {};

        // 隴鯉ｽ｢陝・ｦ排efab郢昜ｻ｣縺帷ｸｺ蠕娯旺郢ｧ荵昶・郢ｧ迚吶・隴帶ｺｷﾂ・､邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
        if (const PrefabInstanceComponent* prefab = panel.m_registry->GetComponent<PrefabInstanceComponent>(panel.m_previewEntity);
            prefab && !prefab->prefabAssetPath.empty()) {
            strcpy_s(pathBuffer, prefab->prefabAssetPath.c_str());
        }
        // 霎滂ｽ｡邵ｺ莉｣・檎ｸｺ・ｰ郢晢ｽ｢郢昴・ﾎ晁惺髦ｪﾂｰ郢ｧ閾ｼrefab闖ｫ譎擾ｽｭ莨懊・郢ｧ蜑・ｽｽ諛奇ｽ狗ｸｲ繝ｻ
        else {
            const std::string defaultName = panel.m_currentModelPath.empty()
                ? "Assets/Prefab/Player.prefab"
                : ("Assets/Prefab/" + std::filesystem::path(panel.m_currentModelPath).stem().string() + ".prefab");
            strcpy_s(pathBuffer, defaultName.c_str());
        }

        // 闖ｫ譎擾ｽｭ蛟･繝郢ｧ・､郢ｧ・｢郢晢ｽｭ郢ｧ・ｰ郢ｧ蟶晏ｹ慕ｸｺ荳環繝ｻ
        if (Dialog::SaveFileName(pathBuffer, MAX_PATH, kPrefabFileFilter, "Save Player Prefab", "prefab") != DialogResult::OK) {
            return false;
        }

        // 闖ｫ譎擾ｽｭ莨懊・郢ｧ蝣､・｢・ｺ陞ｳ螢ｹ笘・ｹｧ荵敖繝ｻ
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

// 邵ｺ蜷ｶ竏狗ｸｺ・ｦ邵ｺ・ｮ郢晏ｳｨ縺冗ｹ晢ｽ･郢晢ｽ｡郢晢ｽｳ郢晏現・定将譎擾ｽｭ莨懃√霑･・ｶ隲ｷ荵昶・隰鯉ｽｻ邵ｺ蜷ｶﾂ繝ｻ
void PlayerEditorSession::RevertAllDocuments(PlayerEditorPanel& panel)
{
    // Timeline邵ｺ・ｫ闖ｫ譎擾ｽｭ菫ｶ・ｸ蛹ｻ竏ｩ郢昜ｻ｣縺帷ｸｺ蠕娯旺郢ｧ荵昶・郢ｧ蟲ｨ繝ｵ郢ｧ・｡郢ｧ・､郢晢ｽｫ邵ｺ荵晢ｽ芽怙蟠趣ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ邵ｺ・ｿ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    if (!panel.m_timelineAssetPath.empty()) {
        TimelineAssetSerializer::Load(panel.m_timelineAssetPath, panel.m_timelineAsset);
        panel.m_timelineDirty = false;
    }

    // StateMachine邵ｺ・ｫ闖ｫ譎擾ｽｭ菫ｶ・ｸ蛹ｻ竏ｩ郢昜ｻ｣縺帷ｸｺ蠕娯旺郢ｧ荵昶・郢ｧ蟲ｨ繝ｵ郢ｧ・｡郢ｧ・､郢晢ｽｫ邵ｺ荵晢ｽ芽怙蟠趣ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ邵ｺ・ｿ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    if (!panel.m_stateMachineAssetPath.empty()) {
        StateMachineAssetSerializer::Load(panel.m_stateMachineAssetPath, panel.m_stateMachineAsset);
        panel.m_stateMachineDirty = false;
    }

    if (!panel.m_inputMappingTab.GetActionMapPath().empty()) {
        panel.m_inputMappingTab.ReloadActionMap();
    }

    // Preview Entity邵ｺ荵晢ｽ唄ocket郢ｧ雋槭・陷ｿ髢・ｾ蜉ｱ笘・ｹｧ荵敖繝ｻ
    ImportSocketsFromPreviewEntity(panel);

    // Socket Dirty郢ｧ螳夲ｽｧ・｣鬮ｯ・､邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    panel.m_socketDirty = false;
}

// PlayerEditor邵ｺ・ｮ陷ｷ繝ｻ・ｨ・ｮ髫ｪ・ｭ陞ｳ螢ｹ・単review Entity邵ｺ・ｸ陷ｿ閧ｴ荳千ｸｺ蜷ｶ・狗ｸｲ繝ｻ
void PlayerEditorSession::ApplyEditorBindingsToPreviewEntity(PlayerEditorPanel& panel)
{
    // Preview Entity邵ｺ蠕｡・ｽ・ｿ邵ｺ蛹ｻ竊醍ｸｺ繝ｻ竊醍ｹｧ謌托ｽｽ霈費ｽらｸｺ蜉ｱ竊醍ｸｺ繝ｻﾂ繝ｻ
    if (!panel.CanUsePreviewEntity()) {
        return;
    }

    // MeshComponent邵ｺ・ｸ霑ｴ・ｾ陜ｨ・ｨ邵ｺ・ｮ郢晢ｽ｢郢昴・ﾎ晁ｫ繝ｻ・ｰ・ｱ郢ｧ雋樊ｸ夊ｭ擾｣ｰ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
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

    // Preview郢晢ｽ｢郢昴・ﾎ晉ｸｺ・ｮ郢ｧ・ｹ郢ｧ・ｱ郢晢ｽｼ郢晢ｽｫ郢ｧ謗｢ransformComponent邵ｺ・ｸ陷ｿ閧ｴ荳千ｸｺ蜷ｶ・狗ｸｲ繝ｻ
    if (auto* transform = panel.m_registry->GetComponent<TransformComponent>(panel.m_previewEntity)) {
        transform->localScale = { panel.m_previewModelScale, panel.m_previewModelScale, panel.m_previewModelScale };
        transform->isDirty = true;
    }

    // Socket驍ｱ・ｨ鬮ｮ繝ｻ・ｵ蜈域｣｡郢ｧ蜊腕eview Entity邵ｺ・ｸ隴厄ｽｸ邵ｺ閧ｴ邯ｾ邵ｺ蜷ｶﾂ繝ｻ
    ExportSocketsToPreviewEntity(panel);

    // TimelineAsset郢ｧ蝮ｦuntime騾包ｽｨComponent邵ｺ・ｸ陞溽判驪､邵ｺ蜉ｱ窶ｻPreview Entity邵ｺ・ｸ陷ｿ閧ｴ荳千ｸｺ蜷ｶ・狗ｸｲ繝ｻ
    RebuildPreviewTimelineRuntimeData(panel);
}

// TimelineAsset邵ｺ荵晢ｽ欝imelineComponent邵ｺ・ｨTimelineItemBuffer郢ｧ蜑・ｽｽ諛奇ｽ企ｶ・ｴ邵ｺ蜷ｶﾂ繝ｻ
void PlayerEditorSession::RebuildPreviewTimelineRuntimeData(PlayerEditorPanel& panel)
{
    // Preview Entity邵ｺ讙寂伯邵ｺ繝ｻ竊醍ｹｧ謌托ｽｽ霈費ｽらｸｺ蜉ｱ竊醍ｸｺ繝ｻﾂ繝ｻ
    if (Entity::IsNull(panel.m_previewEntity)) {
        return;
    }

    // Player Runtime邵ｺ・ｫ陟｢繝ｻ・ｦ竏壺・Component郢ｧ蜑・ｽｿ譎・ｽｨ・ｼ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    PlayerRuntimeSetup::EnsurePlayerRuntimeComponents(*panel.m_registry, panel.m_previewEntity);

    // Timeline runtime騾包ｽｨComponent郢ｧ蜑・ｽｸﾂ隴弱ｆ・ｽ諛医・邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    TimelineComponent timeline{};

    // Timeline item runtime buffer郢ｧ蜑・ｽｸﾂ隴弱ｆ・ｽ諛医・邵ｺ蜷ｶ・狗ｸｲ繝ｻ
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

    // 隴鯉ｽ｢邵ｺ・ｫTimelineComponent邵ｺ蠕娯旺郢ｧ荵昶・郢ｧ謌托ｽｸ鬆大ｶ檎ｸｺ髦ｪ笘・ｹｧ荵敖繝ｻ
    if (auto* existing = panel.m_registry->GetComponent<TimelineComponent>(panel.m_previewEntity)) {
        *existing = timeline;
    }
    // 霎滂ｽ｡邵ｺ莉｣・檎ｸｺ・ｰ髴托ｽｽ陷会｣ｰ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    else {
        panel.m_registry->AddComponent(panel.m_previewEntity, timeline);
    }

    // 隴鯉ｽ｢邵ｺ・ｫTimelineItemBuffer邵ｺ蠕娯旺郢ｧ荵昶・郢ｧ謌托ｽｸ鬆大ｶ檎ｸｺ髦ｪ笘・ｹｧ荵敖繝ｻ
    if (auto* existingBuffer = panel.m_registry->GetComponent<TimelineItemBuffer>(panel.m_previewEntity)) {
        *existingBuffer = buffer;
    }
    // 霎滂ｽ｡邵ｺ莉｣・檎ｸｺ・ｰ髴托ｽｽ陷会｣ｰ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    else {
        panel.m_registry->AddComponent(panel.m_previewEntity, buffer);
    }
}

// Timeline邵ｺ・ｮ陷蜥ｲ蜃ｽ闖ｴ蜥ｲ・ｽ・ｮ郢ｧ蜊腕eviewState邵ｺ・ｸ陷ｷ譴ｧ謔・ｸｺ蜷ｶ・狗ｸｲ繝ｻ
void PlayerEditorSession::SyncPreviewTimelinePlayback(PlayerEditorPanel& panel)
{
    // PreviewState邵ｺ謔溯劒邵ｺ繝ｻ窶ｻ邵ｺ繝ｻ竊醍ｸｺ繝ｻ竊醍ｹｧ謌托ｽｽ霈費ｽらｸｺ蜉ｱ竊醍ｸｺ繝ｻﾂ繝ｻ
    if (!panel.m_previewState.IsActive()) {
        return;
    }

    // frame郢ｧ逞ｴeconds邵ｺ・ｸ陞溽判驪､邵ｺ蜉ｱ窶ｻPreviewState邵ｺ・ｸ雋ゑｽ｡邵ｺ蜷ｶﾂ繝ｻ
    panel.m_previewState.SetTime(panel.m_playheadFrame / (panel.m_timelineAsset.fps > 0.0f ? panel.m_timelineAsset.fps : 60.0f));
}

// 霑ｴ・ｾ陜ｨ・ｨ鬩包ｽｸ隰壽ｨ費ｽｸ・ｭ邵ｺ・ｮScene Entity邵ｺ荵晢ｽ臼layerEditor邵ｺ・ｸ髫ｪ・ｭ陞ｳ螢ｹ・定愾謔ｶ・企恷・ｼ郢ｧﾂ邵ｲ繝ｻ
void PlayerEditorSession::ImportFromSelectedEntity(PlayerEditorPanel& panel)
{
    // Registry邵ｺ讙寂伯邵ｺ繝ｻﾂ竏壺穐邵ｺ貅倥・鬩包ｽｸ隰壽仭ntity邵ｺ讙寂伯邵ｺ繝ｻ竊醍ｹｧ謌托ｽｽ霈費ｽらｸｺ蜉ｱ竊醍ｸｺ繝ｻﾂ繝ｻ
    if (!panel.m_registry || Entity::IsNull(panel.m_selectedEntity)) {
        return;
    }

    // 鬩包ｽｸ隰壽仭ntity邵ｺ・ｮ郢晢ｽ｢郢昴・ﾎ晉ｹ昜ｻ｣縺帷ｸｺ蠕娯旺郢ｧ荵昶・郢ｧ蟲ｨﾂ竏壺落郢ｧ蠕鯉ｽ単layerEditor邵ｺ・ｧ鬮｢荵晢ｿ･邵ｲ繝ｻ
    if (!panel.m_selectedEntityModelPath.empty()) {
        OpenModelFromPath(panel, panel.m_selectedEntityModelPath);
    }

    // 鬩包ｽｸ隰壽仭ntity郢ｧ蜊腕eview陝・ｽｾ髮趣ｽ｡邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
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

    // 鬩包ｽｸ隰壽仭ntity邵ｺ荵晢ｽ唄ocket隲繝ｻ・ｰ・ｱ郢ｧ螳夲ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ邵ｲ繝ｻ
    ImportSocketsFromPreviewEntity(panel);
}

// Preview Entity邵ｺ荵晢ｽ唄ocket隲繝ｻ・ｰ・ｱ郢ｧ螳夲ｽｪ・ｭ邵ｺ・ｿ髴趣ｽｼ郢ｧﾂ邵ｲ繝ｻ
void PlayerEditorSession::ImportSocketsFromPreviewEntity(PlayerEditorPanel& panel)
{
    // Registry邵ｺ讙寂伯邵ｺ繝ｻﾂ竏壺穐邵ｺ貅倥・Preview Entity邵ｺ蠕｡・ｽ・ｿ邵ｺ蛹ｻ竊醍ｸｺ繝ｻ竊醍ｹｧ蜚・cket郢ｧ蝣､・ｩ・ｺ邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    if (!panel.m_registry || !panel.CanUsePreviewEntity()) {
        panel.m_sockets.clear();
        panel.m_socketDirty = false;
        return;
    }

    // NodeSocketComponent邵ｺ蠕娯旺郢ｧ蠕後・邵ｲ竏壺落邵ｺ・ｮSocket鬩滓ｦ翫・郢ｧ閼ｱditor陋幢ｽｴ邵ｺ・ｸ郢ｧ・ｳ郢晄鱒繝ｻ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    if (const auto* sockets = panel.m_registry->GetComponent<NodeSocketComponent>(panel.m_previewEntity)) {
        panel.m_sockets = sockets->sockets;
    }
    // 霎滂ｽ｡邵ｺ莉｣・檎ｸｺ・ｰSocket郢晢ｽｪ郢ｧ・ｹ郢晏現・帝→・ｺ邵ｺ・ｫ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    else {
        panel.m_sockets.clear();
    }

    // 髫ｱ・ｭ邵ｺ・ｿ髴趣ｽｼ邵ｺ・ｿ騾ｶ・ｴ陟募ｾ娯・邵ｺ・ｮ邵ｺ・ｧDirty邵ｺ・ｧ邵ｺ・ｯ邵ｺ・ｪ邵ｺ繝ｻﾂ繝ｻ
    panel.m_socketDirty = false;
}

// Editor陋幢ｽｴ邵ｺ・ｧ驍ｱ・ｨ鬮ｮ繝ｻ・邵ｺ谺撲cket隲繝ｻ・ｰ・ｱ郢ｧ蜊腕eview Entity邵ｺ・ｸ隴厄ｽｸ邵ｺ閧ｴ邯ｾ邵ｺ蜷ｶﾂ繝ｻ
void PlayerEditorSession::ExportSocketsToPreviewEntity(PlayerEditorPanel& panel)
{
    // Registry邵ｺ讙寂伯邵ｺ繝ｻﾂ竏壺穐邵ｺ貅倥・Preview Entity邵ｺ蠕｡・ｽ・ｿ邵ｺ蛹ｻ竊醍ｸｺ繝ｻ竊醍ｹｧ謌托ｽｽ霈費ｽらｸｺ蜉ｱ竊醍ｸｺ繝ｻﾂ繝ｻ
    if (!panel.m_registry || !panel.CanUsePreviewEntity()) {
        return;
    }

    // 隴鯉ｽ｢陝・･繝ｻNodeSocketComponent郢ｧ雋槫徐陟募干笘・ｹｧ荵敖繝ｻ
    auto* sockets = panel.m_registry->GetComponent<NodeSocketComponent>(panel.m_previewEntity);

    // 霎滂ｽ｡邵ｺ莉｣・檎ｸｺ・ｰ隴・ｽｰ邵ｺ蜉ｱ・･髴托ｽｽ陷会｣ｰ邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    if (!sockets) {
        panel.m_registry->AddComponent(panel.m_previewEntity, NodeSocketComponent{ panel.m_sockets });
        return;
    }

    // 邵ｺ繧・ｽ檎ｸｺ・ｰSocket鬩滓ｦ翫・郢ｧ蜑・ｽｸ鬆大ｶ檎ｸｺ髦ｪ笘・ｹｧ荵敖繝ｻ
    sockets->sockets = panel.m_sockets;

    // 隴厄ｽｸ邵ｺ閧ｴ邯ｾ邵ｺ蜉ｱ笳・ｸｺ・ｮ邵ｺ・ｧDirty郢ｧ螳夲ｽｧ・｣鬮ｯ・､邵ｺ蜷ｶ・狗ｸｲ繝ｻ
    panel.m_socketDirty = false;
}
