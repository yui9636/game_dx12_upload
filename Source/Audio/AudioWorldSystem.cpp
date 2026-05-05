// 音声ワールドシステムの実装です。
// miniaudio を使い、ECS の AudioEmitter / AudioListener / AudioSettings と実音声再生を同期します。
#include "AudioWorldSystem.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../../External/miniaudio-master/miniaudio.h"

#include "AudioAssetSystem.h"
#include "Component/AudioBusSendComponent.h"
#include "Component/AudioListenerComponent.h"
#include "Component/AudioOneShotRequestComponent.h"
#include "Component/AudioSettingsComponent.h"
#include "Component/AudioStateComponent.h"
#include "Component\HierarchyComponent.h"
#include "Component\NameComponent.h"
#include "Component\TransformComponent.h"
#include "Console/Logger.h"
#include "Registry/Registry.h"
#include "System/Query.h"
#include "System/PathResolver.h"

namespace
{
    // 再生中の1ボイス分の状態を保持します。
    // miniaudio の ma_sound と、エンジン側の管理情報をまとめています。
    struct VoiceState
    {
        // miniaudio が扱う実際の再生オブジェクトです。
        ma_sound sound{};
        // エンジン側でこのボイスを識別するハンドルです。
        AudioVoiceHandle handle = 0;
        // ECS の Entity に紐付く場合、その EntityID を保持します。
        EntityID entity = Entity::NULL_ID;
        // 再生している音声ファイルの正規化済みパスです。
        std::string clipPath;
        // このボイスが所属している音声バスです。
        AudioBusType bus = AudioBusType::SFX;
        // ma_sound の初期化が完了しているかどうかです。
        bool initialized = false;
        // 一時再生音かどうかです。終了後に自動破棄されます。
        bool transient = false;
        // エディタの音声プレビュー用ボイスかどうかです。
        bool preview = false;
        // Play / Pause などのエンジンモードに連動して制御されるかどうかです。
        bool runtimeControlled = false;
        // 3D 空間音声として扱うかどうかです。
        bool is3D = false;
        // ループ再生するかどうかです。
        bool loop = false;
        // Pause 中に停止扱いになっているかどうかです。
        bool paused = false;
        // ストリーミング再生として扱うかどうかです。
        bool streaming = false;
        // AudioBusSendComponent などから来る送信音量倍率です。
        float sendVolume = 1.0f;
        // ボイス単体の音量です。
        float volume = 1.0f;
        // ボイス単体のピッチです。
        float pitch = 1.0f;
        // 3D 音声の最小距離です。
        float minDistance = 1.0f;
        // 3D 音声の最大距離です。
        float maxDistance = 50.0f;
    };

    // AudioEmitterComponent と実際のボイスを結び付けるランタイム状態です。
    struct EmitterRuntimeState
    {
        // エンジン側でこのボイスを識別するハンドルです。
        AudioVoiceHandle handle = 0;
        std::string resolvedClipPath;
        bool startedOnce = false;
        // Pause 中に停止扱いになっているかどうかです。
        bool paused = false;
    };

    // 音声ファイルの指定パスを、実際にアクセス可能なファイルシステムパスへ解決します。
    std::filesystem::path ResolveAudioFilesystemPath(const std::string& clipPath)
    {
        if (clipPath.empty()) {
            return {};
        }

        std::filesystem::path path = std::filesystem::path(clipPath).lexically_normal();
        if (path.is_absolute()) {
            return path;
        }

        const std::string resolved = PathResolver::Resolve(clipPath);
        if (!resolved.empty()) {
            return std::filesystem::path(resolved).lexically_normal();
        }

        std::error_code ec;
        return (std::filesystem::current_path(ec) / path).lexically_normal();
    }

    // 音声パスを比較やキャッシュキーに使いやすい形へ正規化します。
    std::string NormalizeAudioPath(const std::string& clipPath)
    {
        if (clipPath.empty()) {
            return {};
        }

        std::error_code ec;
        std::filesystem::path path = ResolveAudioFilesystemPath(clipPath);
        if (path.empty()) {
            return {};
        }

        const std::filesystem::path weak = std::filesystem::weakly_canonical(path, ec);
        if (!ec) {
            return weak.string();
        }
        return path.string();
    }

    // Entity とその親階層が runtime 上で有効かどうかを確認します。
    bool IsEntityRuntimeActive(EntityID entity, Registry& registry)
    {
        const HierarchyComponent* hierarchy = registry.GetComponent<HierarchyComponent>(entity);
        EntityID current = entity;
        while (!Entity::IsNull(current)) {
            hierarchy = registry.GetComponent<HierarchyComponent>(current);
            if (hierarchy && !hierarchy->isActive) {
                return false;
            }
            current = hierarchy ? hierarchy->parent : Entity::NULL_ID;
        }
        return true;
    }

    // Quaternion 回転から前方向ベクトルを計算します。
    DirectX::XMFLOAT3 GetForwardFromRotation(const DirectX::XMFLOAT4& rotation)
    {
        using namespace DirectX;
        const XMVECTOR quat = XMLoadFloat4(&rotation);
        const XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), quat);
        XMFLOAT3 out{};
        XMStoreFloat3(&out, XMVector3Normalize(forward));
        return out;
    }

    // miniaudio に渡す再生フラグを、ストリーミング設定と 3D 設定から作ります。
    ma_uint32 MakeSoundFlags(bool streaming, bool is3D)
    {
        ma_uint32 flags = 0;
        if (streaming) {
            flags |= MA_SOUND_FLAG_STREAM;
        } else {
            flags |= MA_SOUND_FLAG_DECODE;
        }
        if (!is3D) {
            flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
        }
        return flags;
    }
}

// AudioWorldSystem の内部実装です。
// miniaudio の engine / group / voice 管理をまとめ、ヘッダーから詳細を隠します。
struct AudioWorldSystem::Impl
{
    // miniaudio の中心となるエンジンオブジェクトです。
    ma_engine engine{};
    // BGM 用のサウンドグループです。
    ma_sound_group bgmGroup{};
    // SFX 用のサウンドグループです。
    ma_sound_group sfxGroup{};
    // UI 用のサウンドグループです。
    ma_sound_group uiGroup{};

    // miniaudio engine が初期化済みかどうかです。
    bool engineInitialized = false;
    bool bgmGroupInitialized = false;
    bool sfxGroupInitialized = false;
    bool uiGroupInitialized = false;

    // 音声クリップのメタ情報とキャッシュを管理します。
    AudioAssetSystem assets;
    // 次に発行するボイスハンドルです。0 は無効値として扱います。
    AudioVoiceHandle nextHandle = 1;
    // 現在管理している全ボイスです。
    std::unordered_map<AudioVoiceHandle, std::unique_ptr<VoiceState>> voices;
    // Entity ごとの AudioEmitterComponent 実行状態です。
    std::unordered_map<EntityID, EmitterRuntimeState> emitterStates;
    // エディタプレビュー再生中のボイスハンドルです。
    AudioVoiceHandle previewHandle = 0;
    // 現在プレビュー中の音声ファイルパスです。
    std::string previewClipPath;
    // プレビュー再生に使っている音声バスです。
    AudioBusType previewBus = AudioBusType::UI;
    // 前回 Update 時のエンジンモードです。
    EngineMode lastMode = EngineMode::Editor;
    // 最後に反映した AudioSettingsComponent の値です。
    AudioSettingsComponent lastAppliedSettings{};
    // バスごとのミュート状態です。
    std::unordered_map<AudioBusType, bool> mutedBuses;
    // ソロ再生対象のバスです。nullopt ならソロ無しです。
    std::optional<AudioBusType> soloBus;

    // AudioBusType に対応する miniaudio の sound group を返します。
    ma_sound_group* GetGroup(AudioBusType bus)
    {
        switch (bus) {
        case AudioBusType::BGM: return &bgmGroup;
        case AudioBusType::UI: return &uiGroup;
        case AudioBusType::Master:
        case AudioBusType::SFX:
        default:
            return &sfxGroup;
        }
    }

    // 1ボイス分の ma_sound を安全に解放します。
    void UninitVoice(VoiceState& voice)
    {
        if (voice.initialized) {
            ma_sound_uninit(&voice.sound);
            voice.initialized = false;
        }
    }

    // 全ボイスとエミッター状態、プレビュー状態をまとめて破棄します。
    void ClearAllVoices()
    {
        for (auto& entry : voices) {
            UninitVoice(*entry.second);
        }
        voices.clear();
        emitterStates.clear();
        previewHandle = 0;
        previewClipPath.clear();
    }

    // AudioSettingsComponent の値から、バスの基本音量を取得します。
    float GetBaseBusVolume(AudioBusType bus) const
    {
        switch (bus) {
        case AudioBusType::BGM: return lastAppliedSettings.bgmVolume;
        case AudioBusType::UI: return lastAppliedSettings.uiVolume;
        case AudioBusType::Master:
        case AudioBusType::SFX:
        default:
            return lastAppliedSettings.sfxVolume;
        }
    }

    // mute / solo / muteAll を反映した実効バス音量を取得します。
    float GetEffectiveBusVolume(AudioBusType bus) const
    {
        float base = GetBaseBusVolume(bus);
        if (lastAppliedSettings.muteAll) {
            return 0.0f;
        }

        if (soloBus.has_value()) {
            return (*soloBus == bus) ? base : 0.0f;
        }

        const auto mutedIt = mutedBuses.find(bus);
        if (mutedIt != mutedBuses.end() && mutedIt->second) {
            return 0.0f;
        }

        return base;
    }
};

// AudioBusType を UI 表示向けの文字列へ変換します。
const char* GetAudioBusTypeLabel(AudioBusType bus)
{
    switch (bus) {
    case AudioBusType::Master: return "Master";
    case AudioBusType::BGM: return "BGM";
    case AudioBusType::SFX: return "SFX";
    case AudioBusType::UI: return "UI";
    default: return "Unknown";
    }
}

// AudioWorldSystem を生成します。実際の初期化は Initialize で行います。
AudioWorldSystem::AudioWorldSystem() = default;

// 破棄時に Finalize を呼び、音声リソースを解放します。
AudioWorldSystem::~AudioWorldSystem()
{
    Finalize();
}

// miniaudio engine と BGM/SFX/UI の各 sound group を初期化します。
bool AudioWorldSystem::Initialize()
{
    if (m_initialized) {
        return true;
    }

    m_impl = std::make_unique<Impl>();

    ma_engine_config config = ma_engine_config_init();
    const ma_result engineResult = ma_engine_init(&config, &m_impl->engine);
    if (engineResult != MA_SUCCESS) {
        LOG_ERROR("[AudioWorldSystem] Failed to initialize miniaudio engine. result=%d", static_cast<int>(engineResult));
        m_impl.reset();
        return false;
    }
    m_impl->engineInitialized = true;

    if (ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->bgmGroup) == MA_SUCCESS) {
        m_impl->bgmGroupInitialized = true;
    }
    if (ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->sfxGroup) == MA_SUCCESS) {
        m_impl->sfxGroupInitialized = true;
    }
    if (ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->uiGroup) == MA_SUCCESS) {
        m_impl->uiGroupInitialized = true;
    }

    m_initialized = m_impl->bgmGroupInitialized && m_impl->sfxGroupInitialized && m_impl->uiGroupInitialized;
    if (!m_initialized) {
        LOG_ERROR("[AudioWorldSystem] Failed to initialize sound groups.");
        Finalize();
        return false;
    }

    LOG_INFO("[AudioWorldSystem] Initialized with miniaudio backend.");
    return true;
}

// 全音声停止後、sound group と miniaudio engine を解放します。
void AudioWorldSystem::Finalize()
{
    if (!m_impl) {
        m_initialized = false;
        return;
    }

    m_impl->ClearAllVoices();

    if (m_impl->uiGroupInitialized) {
        ma_sound_group_uninit(&m_impl->uiGroup);
        m_impl->uiGroupInitialized = false;
    }
    if (m_impl->sfxGroupInitialized) {
        ma_sound_group_uninit(&m_impl->sfxGroup);
        m_impl->sfxGroupInitialized = false;
    }
    if (m_impl->bgmGroupInitialized) {
        ma_sound_group_uninit(&m_impl->bgmGroup);
        m_impl->bgmGroupInitialized = false;
    }
    if (m_impl->engineInitialized) {
        ma_engine_uninit(&m_impl->engine);
        m_impl->engineInitialized = false;
    }

    m_impl.reset();
    m_activeListenerEntity = Entity::NULL_ID;
    m_initialized = false;
}

// シーン変更時に、再生中ボイスやエミッター状態をリセットします。
void AudioWorldSystem::ResetForSceneChange()
{
    if (!m_impl) {
        return;
    }

    m_impl->ClearAllVoices();
    m_activeListenerEntity = Entity::NULL_ID;
}

namespace
{
    // 2D/3D、runtime/editor、transient/preview をまとめて扱う共通ボイス生成関数です。
    AudioVoiceHandle CreateVoiceInternal(AudioWorldSystem::Impl& impl,
                                         const std::string& clipPath,
                                         AudioBusType bus,
                                         bool is3D,
                                         bool loop,
                                         float volume,
                                         float sendVolume,
                                         float pitch,
                                         float minDistance,
                                         float maxDistance,
                                         bool streaming,
                                         bool startImmediately,
                                         bool transient,
                                         bool preview,
                                         bool runtimeControlled,
                                         EntityID entity,
                                         const DirectX::XMFLOAT3* position)
    {
        const AudioClipAsset clip = impl.assets.GetClipOrDefault(clipPath);
        const std::string resolvedPath = clip.importedPath.empty() ? NormalizeAudioPath(clipPath) : clip.importedPath;
        if (resolvedPath.empty() || !clip.valid) {
            LOG_WARN("[AudioWorldSystem] Missing audio clip: %s", clipPath.c_str());
            return 0;
        }

        auto voice = std::make_unique<VoiceState>();
        voice->handle = impl.nextHandle++;
        voice->entity = entity;
        voice->clipPath = resolvedPath;
        voice->bus = bus;
        voice->transient = transient;
        voice->preview = preview;
        voice->runtimeControlled = runtimeControlled;
        voice->is3D = is3D;
        voice->loop = loop;
        voice->sendVolume = sendVolume;
        voice->volume = volume;
        voice->pitch = pitch;
        voice->minDistance = minDistance;
        voice->maxDistance = maxDistance;
        voice->streaming = streaming;

        ma_sound_group* group = impl.GetGroup(bus);
        const ma_uint32 flags = MakeSoundFlags(streaming, is3D);
        const ma_result result = ma_sound_init_from_file(&impl.engine, resolvedPath.c_str(), flags, group, nullptr, &voice->sound);
        if (result != MA_SUCCESS) {
            LOG_ERROR("[AudioWorldSystem] Failed to load clip '%s'. result=%d", resolvedPath.c_str(), static_cast<int>(result));
            return 0;
        }

        voice->initialized = true;
        ma_sound_set_looping(&voice->sound, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&voice->sound, volume * sendVolume);
        ma_sound_set_pitch(&voice->sound, pitch);

        if (is3D) {
            ma_sound_set_spatialization_enabled(&voice->sound, MA_TRUE);
            ma_sound_set_min_distance(&voice->sound, minDistance);
            ma_sound_set_max_distance(&voice->sound, maxDistance);
            if (position) {
                ma_sound_set_position(&voice->sound, position->x, position->y, position->z);
            }
        } else {
            ma_sound_set_spatialization_enabled(&voice->sound, MA_FALSE);
        }

        if (startImmediately) {
            ma_sound_seek_to_pcm_frame(&voice->sound, 0);
            ma_sound_start(&voice->sound);
        }

        const AudioVoiceHandle handle = voice->handle;
        impl.voices.emplace(handle, std::move(voice));
        return handle;
    }
}

// Entity に紐付かない 2D 一時音声を再生します。
AudioVoiceHandle AudioWorldSystem::PlayTransient2D(const std::string& clipPath,
                                                   float volume,
                                                   float pitch,
                                                   bool loop,
                                                   AudioBusType bus,
                                                   bool streaming)
{
    if (!m_impl) {
        return 0;
    }
    return CreateVoiceInternal(*m_impl, clipPath, bus, false, loop, volume, 1.0f, pitch, 1.0f, 50.0f, streaming, true, true, false, true, Entity::NULL_ID, nullptr);
}

// Entity に紐付かない 3D 一時音声を指定位置で再生します。
AudioVoiceHandle AudioWorldSystem::PlayTransient3D(const std::string& clipPath,
                                                   const DirectX::XMFLOAT3& position,
                                                   float volume,
                                                   float pitch,
                                                   bool loop,
                                                   AudioBusType bus,
                                                   float minDistance,
                                                   float maxDistance,
                                                   bool streaming)
{
    if (!m_impl) {
        return 0;
    }
    return CreateVoiceInternal(*m_impl, clipPath, bus, true, loop, volume, 1.0f, pitch, minDistance, maxDistance, streaming, true, true, false, true, Entity::NULL_ID, &position);
}

// エディタ用の 2D 一時音声を再生します。
AudioVoiceHandle AudioWorldSystem::PlayEditorTransient2D(const std::string& clipPath,
                                                         float volume,
                                                         float pitch,
                                                         bool loop,
                                                         AudioBusType bus,
                                                         bool streaming)
{
    if (!m_impl) {
        return 0;
    }
    return CreateVoiceInternal(*m_impl, clipPath, bus, false, loop, volume, 1.0f, pitch, 1.0f, 50.0f, streaming, true, true, false, false, Entity::NULL_ID, nullptr);
}

// エディタ用の 3D 一時音声を指定位置で再生します。
AudioVoiceHandle AudioWorldSystem::PlayEditorTransient3D(const std::string& clipPath,
                                                         const DirectX::XMFLOAT3& position,
                                                         float volume,
                                                         float pitch,
                                                         bool loop,
                                                         AudioBusType bus,
                                                         float minDistance,
                                                         float maxDistance,
                                                         bool streaming)
{
    if (!m_impl) {
        return 0;
    }
    return CreateVoiceInternal(*m_impl, clipPath, bus, true, loop, volume, 1.0f, pitch, minDistance, maxDistance, streaming, true, true, false, false, Entity::NULL_ID, &position);
}

// 指定したボイスを停止し、管理リストから削除します。
void AudioWorldSystem::StopVoice(AudioVoiceHandle handle)
{
    if (!m_impl || handle == 0) {
        return;
    }

    auto it = m_impl->voices.find(handle);
    if (it == m_impl->voices.end()) {
        return;
    }

    if (it->second->initialized) {
        ma_sound_stop(&it->second->sound);
        m_impl->UninitVoice(*it->second);
    }

    if (m_impl->previewHandle == handle) {
        m_impl->previewHandle = 0;
        m_impl->previewClipPath.clear();
    }

    m_impl->voices.erase(it);
}

// 管理中の全ボイスを停止します。
void AudioWorldSystem::StopAllVoices()
{
    if (!m_impl) {
        return;
    }

    std::vector<AudioVoiceHandle> handles;
    handles.reserve(m_impl->voices.size());
    for (const auto& entry : m_impl->voices) {
        handles.push_back(entry.first);
    }

    for (AudioVoiceHandle handle : handles) {
        StopVoice(handle);
    }
}

// 3D ボイスの現在位置を更新します。
void AudioWorldSystem::SetVoicePosition(AudioVoiceHandle handle, const DirectX::XMFLOAT3& position)
{
    if (!m_impl || handle == 0) {
        return;
    }

    auto it = m_impl->voices.find(handle);
    if (it == m_impl->voices.end() || !it->second->initialized || !it->second->is3D) {
        return;
    }

    ma_sound_set_position(&it->second->sound, position.x, position.y, position.z);
}

// 指定ハンドルのボイスがまだ存在するかどうかを返します。
bool AudioWorldSystem::IsVoiceAlive(AudioVoiceHandle handle) const
{
    return m_impl && handle != 0 && m_impl->voices.find(handle) != m_impl->voices.end();
}

// 指定クリップをエディタプレビューとして再生します。
void AudioWorldSystem::PreviewClip(const std::string& clipPath, AudioBusType bus)
{
    if (!m_impl) {
        return;
    }

    StopPreview();
    m_impl->previewHandle = PlayTransient2D(clipPath, 1.0f, 1.0f, false, bus, false);
    if (m_impl->previewHandle != 0) {
        m_impl->previewClipPath = NormalizeAudioPath(clipPath);
        m_impl->previewBus = bus;
        auto it = m_impl->voices.find(m_impl->previewHandle);
        if (it != m_impl->voices.end()) {
            it->second->preview = true;
            it->second->runtimeControlled = false;
        }
    }
}

// 同じクリップをプレビュー中なら停止し、違う場合は再生します。
void AudioWorldSystem::TogglePreviewClip(const std::string& clipPath, AudioBusType bus)
{
    if (!m_impl) {
        return;
    }

    const std::string normalized = NormalizeAudioPath(clipPath);
    if (!normalized.empty() && normalized == m_impl->previewClipPath && IsVoiceAlive(m_impl->previewHandle)) {
        StopPreview();
        return;
    }
    PreviewClip(clipPath, bus);
}

// 現在のプレビュー再生を停止します。
void AudioWorldSystem::StopPreview()
{
    if (!m_impl) {
        return;
    }
    if (m_impl->previewHandle != 0) {
        StopVoice(m_impl->previewHandle);
    }
    m_impl->previewHandle = 0;
    m_impl->previewClipPath.clear();
}

// 指定クリップが現在プレビュー再生中かどうかを確認します。
bool AudioWorldSystem::IsPreviewing(const std::string& clipPath) const
{
    if (!m_impl || m_impl->previewHandle == 0) {
        return false;
    }
    return NormalizeAudioPath(clipPath) == m_impl->previewClipPath && IsVoiceAlive(m_impl->previewHandle);
}

// 現在プレビュー再生中のクリップパスを返します。
std::string AudioWorldSystem::GetPreviewClipPath() const
{
    return m_impl ? m_impl->previewClipPath : std::string{};
}

// プレビュー再生の現在位置と総時間を取得します。
bool AudioWorldSystem::GetPreviewPlaybackProgress(float& cursorSeconds, float& lengthSeconds) const
{
    cursorSeconds = 0.0f;
    lengthSeconds = 0.0f;

    if (!m_impl || m_impl->previewHandle == 0) {
        return false;
    }

    auto it = m_impl->voices.find(m_impl->previewHandle);
    if (it == m_impl->voices.end() || !it->second->initialized) {
        return false;
    }

    ma_sound_get_cursor_in_seconds(&it->second->sound, &cursorSeconds);
    ma_sound_get_length_in_seconds(&it->second->sound, &lengthSeconds);
    if (lengthSeconds < 0.0f) {
        lengthSeconds = 0.0f;
    }

    return true;
}

// プレビュー再生位置を指定秒へ移動します。
void AudioWorldSystem::SeekPreview(float seconds)
{
    if (!m_impl || m_impl->previewHandle == 0) {
        return;
    }

    auto it = m_impl->voices.find(m_impl->previewHandle);
    if (it == m_impl->voices.end() || !it->second->initialized) {
        return;
    }

    float lengthSeconds = 0.0f;
    ma_sound_get_length_in_seconds(&it->second->sound, &lengthSeconds);
    if (lengthSeconds > 0.0f) {
        seconds = (std::max)(0.0f, (std::min)(seconds, lengthSeconds));
    } else {
        seconds = (std::max)(0.0f, seconds);
    }

    ma_sound_seek_to_second(&it->second->sound, seconds);
}

namespace
{
    // Registry から AudioSettingsComponent を探し、マスター音量と各バス音量へ反映します。
    void ApplySettingsComponent(AudioWorldSystem::Impl& impl, Registry& registry)
    {
        AudioSettingsComponent settings{};
        bool found = false;

        Query<AudioSettingsComponent> settingsQuery(registry);
        settingsQuery.ForEach([&](AudioSettingsComponent& candidate) {
            if (!found) {
                settings = candidate;
                found = true;
            }
        });

        impl.lastAppliedSettings = settings;
        ma_engine_set_volume(&impl.engine, settings.muteAll ? 0.0f : settings.masterVolume);
        ma_sound_group_set_volume(&impl.bgmGroup, impl.GetEffectiveBusVolume(AudioBusType::BGM));
        ma_sound_group_set_volume(&impl.sfxGroup, impl.GetEffectiveBusVolume(AudioBusType::SFX));
        ma_sound_group_set_volume(&impl.uiGroup, impl.GetEffectiveBusVolume(AudioBusType::UI));
    }

    // AudioListenerComponent と TransformComponent から、miniaudio の listener 情報を更新します。
    EntityID SyncListener(AudioWorldSystem::Impl& impl, Registry& registry)
    {
        EntityID chosenEntity = Entity::NULL_ID;
        DirectX::XMFLOAT3 chosenPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 chosenForward = { 0.0f, 0.0f, 1.0f };
        float chosenVolumeScale = 1.0f;

        Query<AudioListenerComponent, TransformComponent> listenerQuery(registry);
        listenerQuery.ForEachWithEntity([&](EntityID entity, AudioListenerComponent& listener, TransformComponent& transform) {
            if (!registry.IsAlive(entity) || !IsEntityRuntimeActive(entity, registry)) {
                return;
            }

            if (Entity::IsNull(chosenEntity) || listener.isPrimary) {
                chosenEntity = entity;
                chosenPosition = transform.worldPosition;
                chosenForward = GetForwardFromRotation(transform.worldRotation);
                chosenVolumeScale = listener.volumeScale;
            }
        });

        ma_engine_listener_set_position(&impl.engine, 0, chosenPosition.x, chosenPosition.y, chosenPosition.z);
        ma_engine_listener_set_direction(&impl.engine, 0, chosenForward.x, chosenForward.y, chosenForward.z);
        ma_engine_listener_set_world_up(&impl.engine, 0, 0.0f, 1.0f, 0.0f);
        ma_engine_listener_set_cone(&impl.engine, 0, 6.283185f, 6.283185f, chosenVolumeScale);
        return chosenEntity;
    }

    // エンジンの Pause 状態に合わせて、対象ボイスを停止または再開します。
    void PauseOrResumeVoice(VoiceState& voice, bool paused)
    {
        if (!voice.initialized) {
            return;
        }

        if (paused) {
            if (!voice.paused && ma_sound_is_playing(&voice.sound)) {
                ma_sound_stop(&voice.sound);
                voice.paused = true;
            }
        } else if (voice.paused) {
            ma_sound_start(&voice.sound);
            voice.paused = false;
        }
    }
}

// ECS の音声コンポーネントを読み取り、ボイス生成・更新・停止をまとめて処理します。
void AudioWorldSystem::Update(Registry& registry, EngineMode mode)
{
    if (!m_impl) {
        return;
    }

    // 音量設定とリスナー位置を先に同期します。
    ApplySettingsComponent(*m_impl, registry);
    m_activeListenerEntity = SyncListener(*m_impl, registry);

    const bool runtimeEnabled = (mode == EngineMode::Play || mode == EngineMode::Pause);
    const bool runtimePaused = (mode == EngineMode::Pause);

    std::vector<std::pair<EntityID, AudioStateComponent>> pendingStateUpdates;
    std::vector<EntityID> pendingStateRemovals;
    std::unordered_set<EntityID> aliveEmitters;

    // AudioEmitterComponent を持つ Entity を走査し、継続再生ボイスを同期します。
    Query<AudioEmitterComponent> emitterQuery(registry);
    emitterQuery.ForEachWithEntity([&](EntityID entity, AudioEmitterComponent& emitter) {
        aliveEmitters.insert(entity);
        auto& runtime = m_impl->emitterStates[entity];

        const bool canRun = runtimeEnabled && IsEntityRuntimeActive(entity, registry) && !emitter.clipAssetPath.empty();
        if (!canRun) {
            if (runtime.handle != 0) {
                StopVoice(runtime.handle);
                runtime.handle = 0;
            }
            runtime.startedOnce = false;
            runtime.paused = false;
            runtime.resolvedClipPath.clear();
            pendingStateRemovals.push_back(entity);
            return;
        }

        const std::string resolvedPath = NormalizeAudioPath(emitter.clipAssetPath);
        const bool soundIs3D = emitter.is3D && emitter.spatialBlend > 0.0f;
        const TransformComponent* transform = registry.GetComponent<TransformComponent>(entity);
        const AudioBusSendComponent* busSend = registry.GetComponent<AudioBusSendComponent>(entity);
        const AudioBusType resolvedBus = busSend ? busSend->bus : emitter.bus;
        const float sendVolume = busSend ? (std::max)(0.0f, busSend->sendVolume) : 1.0f;

        bool recreateVoice = runtime.handle == 0 || !IsVoiceAlive(runtime.handle);
        if (!recreateVoice && runtime.resolvedClipPath != resolvedPath) {
            recreateVoice = true;
        }
        if (!recreateVoice) {
            const auto voiceIt = m_impl->voices.find(runtime.handle);
            if (voiceIt == m_impl->voices.end()) {
                recreateVoice = true;
            } else {
                const VoiceState& voice = *voiceIt->second;
                recreateVoice = voice.bus != resolvedBus || voice.streaming != emitter.streaming || voice.is3D != soundIs3D;
            }
        }

        if (recreateVoice) {
            if (runtime.handle != 0) {
                StopVoice(runtime.handle);
                runtime.handle = 0;
            }

            const DirectX::XMFLOAT3 position = transform ? transform->worldPosition : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
            runtime.handle = soundIs3D
                ? CreateVoiceInternal(*m_impl, resolvedPath, resolvedBus, true, emitter.loop, emitter.volume, sendVolume, emitter.pitch, emitter.minDistance, emitter.maxDistance, emitter.streaming, false, false, false, true, entity, &position)
                : CreateVoiceInternal(*m_impl, resolvedPath, resolvedBus, false, emitter.loop, emitter.volume, sendVolume, emitter.pitch, emitter.minDistance, emitter.maxDistance, emitter.streaming, false, false, false, true, entity, nullptr);
            runtime.resolvedClipPath = resolvedPath;
            runtime.startedOnce = false;
            runtime.paused = false;
        }

        if (runtime.handle == 0) {
            pendingStateRemovals.push_back(entity);
            return;
        }

        auto voiceIt = m_impl->voices.find(runtime.handle);
        if (voiceIt == m_impl->voices.end()) {
            runtime.handle = 0;
            runtime.startedOnce = false;
            pendingStateRemovals.push_back(entity);
            return;
        }

        VoiceState& voice = *voiceIt->second;
        voice.loop = emitter.loop;
        voice.volume = emitter.volume;
        voice.sendVolume = sendVolume;
        voice.pitch = emitter.pitch;
        voice.minDistance = emitter.minDistance;
        voice.maxDistance = emitter.maxDistance;
        voice.bus = resolvedBus;
        voice.entity = entity;

        ma_sound_set_looping(&voice.sound, emitter.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&voice.sound, emitter.volume * sendVolume);
        ma_sound_set_pitch(&voice.sound, emitter.pitch);

        if (voice.is3D && transform) {
            ma_sound_set_position(&voice.sound, transform->worldPosition.x, transform->worldPosition.y, transform->worldPosition.z);
            ma_sound_set_min_distance(&voice.sound, emitter.minDistance);
            ma_sound_set_max_distance(&voice.sound, emitter.maxDistance);
        }

        if (!emitter.playOnStart) {
            if (ma_sound_is_playing(&voice.sound) || voice.paused) {
                ma_sound_stop(&voice.sound);
            }
            runtime.startedOnce = false;
            runtime.paused = false;
            voice.paused = false;
            return;
        }

        if (!runtime.startedOnce) {
            ma_sound_seek_to_pcm_frame(&voice.sound, 0);
            ma_sound_start(&voice.sound);
            runtime.startedOnce = true;
            runtime.paused = false;
            voice.paused = false;
        }

        PauseOrResumeVoice(voice, runtimePaused);
        runtime.paused = voice.paused;

        AudioStateComponent state{};
        state.isPlaying = voice.initialized && (ma_sound_is_playing(&voice.sound) == MA_TRUE);
        state.isPaused = voice.paused;
        state.isVirtualized = false;
        state.activeVoiceHandle = runtime.handle;
        ma_sound_get_cursor_in_seconds(&voice.sound, &state.playbackTimeSec);
        ma_sound_get_length_in_seconds(&voice.sound, &state.lengthSec);
        pendingStateUpdates.emplace_back(entity, state);
    });

    std::vector<EntityID> deadEmitterStates;
    for (const auto& entry : m_impl->emitterStates) {
        if (aliveEmitters.find(entry.first) == aliveEmitters.end() || !registry.IsAlive(entry.first)) {
            deadEmitterStates.push_back(entry.first);
        }
    }
    for (EntityID deadEntity : deadEmitterStates) {
        auto it = m_impl->emitterStates.find(deadEntity);
        if (it != m_impl->emitterStates.end()) {
            if (it->second.handle != 0) {
                StopVoice(it->second.handle);
            }
            m_impl->emitterStates.erase(it);
        }
        pendingStateRemovals.push_back(deadEntity);
    }

    // AudioOneShotRequestComponent は、短命の単発再生リクエストとして処理します。
    std::vector<EntityID> oneShotEntitiesToRemove;
    Query<AudioOneShotRequestComponent> oneShotQuery(registry);
    oneShotQuery.ForEachWithEntity([&](EntityID entity, AudioOneShotRequestComponent& request) {
        const bool hasClip = !request.clipAssetPath.empty();
        const bool active = !registry.IsAlive(entity) || IsEntityRuntimeActive(entity, registry);

        if (hasClip && active) {
            if (request.is3D) {
                DirectX::XMFLOAT3 position = request.worldPosition;
                if (TransformComponent* transform = registry.GetComponent<TransformComponent>(entity)) {
                    position = transform->worldPosition;
                }
                PlayTransient3D(request.clipAssetPath,
                                position,
                                request.volume,
                                request.pitch,
                                request.loop,
                                request.bus,
                                request.minDistance,
                                request.maxDistance,
                                request.streaming);
            } else {
                PlayTransient2D(request.clipAssetPath,
                                request.volume,
                                request.pitch,
                                request.loop,
                                request.bus,
                                request.streaming);
            }
        }

        if (request.lifetimeFrames <= 1) {
            oneShotEntitiesToRemove.push_back(entity);
        } else {
            request.lifetimeFrames -= 1;
        }
    });

    for (const auto& stateUpdate : pendingStateUpdates) {
        registry.AddComponent<AudioStateComponent>(stateUpdate.first, stateUpdate.second);
    }
    for (EntityID entity : pendingStateRemovals) {
        registry.RemoveComponent<AudioStateComponent>(entity);
    }
    for (EntityID entity : oneShotEntitiesToRemove) {
        registry.RemoveComponent<AudioOneShotRequestComponent>(entity);
    }

    // 終了済みの一時音声や、runtime 外で不要になったボイスを回収します。
    std::vector<AudioVoiceHandle> voicesToRemove;
    for (auto& entry : m_impl->voices) {
        VoiceState& voice = *entry.second;
        if (voice.preview) {
            continue;
        }

        if (voice.runtimeControlled) {
            if (!runtimeEnabled) {
                const bool allowEditorTransient = voice.transient && Entity::IsNull(voice.entity);
                if (!allowEditorTransient) {
                    voicesToRemove.push_back(entry.first);
                    continue;
                }
            }
            if (voice.transient) {
                PauseOrResumeVoice(voice, runtimePaused);
            }
        }

        if (voice.transient && !voice.loop && !voice.paused && ma_sound_at_end(&voice.sound)) {
            voicesToRemove.push_back(entry.first);
        }
    }

    for (AudioVoiceHandle handle : voicesToRemove) {
        StopVoice(handle);
    }

    if (m_impl->previewHandle != 0) {
        auto it = m_impl->voices.find(m_impl->previewHandle);
        if (it == m_impl->voices.end() || (!it->second->loop && ma_sound_at_end(&it->second->sound))) {
            StopPreview();
        }
    }

    m_impl->lastMode = mode;
}

// デバッグ表示用に、現在管理しているボイス情報を収集します。
std::vector<AudioWorldSystem::DebugVoiceInfo> AudioWorldSystem::GetDebugVoices() const
{
    std::vector<DebugVoiceInfo> voices;
    if (!m_impl) {
        return voices;
    }

    voices.reserve(m_impl->voices.size());
    for (const auto& entry : m_impl->voices) {
        const VoiceState& voice = *entry.second;
        DebugVoiceInfo info;
        info.handle = voice.handle;
        info.clipPath = voice.clipPath;
        info.bus = voice.bus;
        info.entity = voice.entity;
        info.is3D = voice.is3D;
        info.loop = voice.loop;
        info.playing = voice.initialized && (ma_sound_is_playing(&voice.sound) == MA_TRUE);
        info.transient = voice.transient;
        info.preview = voice.preview;
        info.volume = voice.volume;
        info.pitch = voice.pitch;
        ma_sound_get_cursor_in_seconds(&voice.sound, &info.cursorSeconds);
        ma_sound_get_length_in_seconds(&voice.sound, &info.lengthSeconds);
        voices.push_back(std::move(info));
    }

    std::sort(voices.begin(), voices.end(), [](const DebugVoiceInfo& a, const DebugVoiceInfo& b) {
        if (a.preview != b.preview) {
            return a.preview > b.preview;
        }
        return a.handle < b.handle;
    });

    return voices;
}

// デバッグ表示用に、各音声バスの状態を収集します。
std::vector<AudioWorldSystem::DebugBusInfo> AudioWorldSystem::GetDebugBuses() const
{
    std::vector<DebugBusInfo> buses;
    if (!m_impl) {
        return buses;
    }

    const AudioBusType orderedBuses[] = { AudioBusType::BGM, AudioBusType::SFX, AudioBusType::UI };
    for (AudioBusType bus : orderedBuses) {
        DebugBusInfo info;
        info.bus = bus;
        info.baseVolume = m_impl->GetBaseBusVolume(bus);
        info.effectiveVolume = m_impl->GetEffectiveBusVolume(bus);
        info.muted = IsBusMuted(bus);
        info.solo = m_impl->soloBus.has_value() && *m_impl->soloBus == bus;

        for (const auto& entry : m_impl->voices) {
            const VoiceState& voice = *entry.second;
            if (voice.bus != bus) {
                continue;
            }

            info.activeVoiceCount += 1;
            if (voice.streaming) {
                info.streamingVoiceCount += 1;
            }
        }

        buses.push_back(info);
    }

    return buses;
}

// 指定クリップのメタ情報を取得します。
AudioClipAsset AudioWorldSystem::DescribeClip(const std::string& clipPath)
{
    if (!m_impl) {
        return {};
    }
    return m_impl->assets.GetClipOrDefault(clipPath);
}

// AudioAssetSystem がキャッシュしているクリップ一覧を取得します。
std::vector<AudioClipAsset> AudioWorldSystem::GetCachedClips() const
{
    if (!m_impl) {
        return {};
    }
    return m_impl->assets.GetCachedClips();
}

// キャッシュ済みクリップ数を取得します。
size_t AudioWorldSystem::GetCachedClipCount() const
{
    return m_impl ? m_impl->assets.GetCachedClipCount() : 0;
}

// 音声クリップメタ情報のキャッシュを破棄します。
void AudioWorldSystem::ClearClipCache()
{
    if (!m_impl) {
        return;
    }
    m_impl->assets.ClearCache();
}

// 指定バスのミュート状態を設定します。
void AudioWorldSystem::SetBusMuted(AudioBusType bus, bool muted)
{
    if (!m_impl) {
        return;
    }
    m_impl->mutedBuses[bus] = muted;
}

// 指定バスがミュートされているかどうかを返します。
bool AudioWorldSystem::IsBusMuted(AudioBusType bus) const
{
    if (!m_impl) {
        return false;
    }
    const auto it = m_impl->mutedBuses.find(bus);
    return it != m_impl->mutedBuses.end() && it->second;
}

// ソロ再生対象のバスを設定します。
void AudioWorldSystem::SetSoloBus(std::optional<AudioBusType> bus)
{
    if (!m_impl) {
        return;
    }
    m_impl->soloBus = bus;
}

// 現在のソロ再生対象バスを取得します。
std::optional<AudioBusType> AudioWorldSystem::GetSoloBus() const
{
    return m_impl ? m_impl->soloBus : std::optional<AudioBusType>{};
}
