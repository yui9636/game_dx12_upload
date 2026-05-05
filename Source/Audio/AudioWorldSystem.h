#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "Audio/AudioClipAsset.h"
#include "Component/AudioEmitterComponent.h"
#include "Entity/Entity.h"
#include "Engine/EngineMode.h"

class Registry;

// 再生中の音声を識別するためのハンドルです。
using AudioVoiceHandle = uint64_t;

// エンジン全体の音声再生を管理するシステムです。
// miniaudio を内部実装に隠し、ゲーム側からは 2D/3D 音声・プレビュー・バス制御として扱えるようにします。
class AudioWorldSystem
{
public:
    // miniaudio などの詳細を隠すための内部実装です。
    struct Impl;

    // デバッグ表示用の再生ボイス情報です。
    struct DebugVoiceInfo
    {
        // 音声ボイスの識別ハンドルです。
        AudioVoiceHandle handle = 0;

        // 再生中の音声ファイルパスです。
        std::string clipPath;

        // 所属している音声バスです。
        AudioBusType bus = AudioBusType::SFX;

        // ECS Entity に紐付く音声の場合、その EntityID が入ります。
        EntityID entity = Entity::NULL_ID;

        // 3D 空間音声かどうかです。
        bool is3D = false;

        // ループ再生しているかどうかです。
        bool loop = false;

        // 現在再生中かどうかです。
        bool playing = false;

        // 一時再生音かどうかです。
        bool transient = false;

        // エディタのプレビュー再生かどうかです。
        bool preview = false;

        // ボイス単体の音量です。
        float volume = 1.0f;

        // ボイス単体のピッチです。
        float pitch = 1.0f;

        // 現在の再生位置です。単位は秒です。
        float cursorSeconds = 0.0f;

        // 音声の総再生時間です。単位は秒です。
        float lengthSeconds = 0.0f;
    };

    // デバッグ表示用の音声バス情報です。
    struct DebugBusInfo
    {
        // 対象の音声バスです。
        AudioBusType bus = AudioBusType::SFX;

        // 設定値としての基本音量です。
        float baseVolume = 1.0f;

        // mute / solo を反映した実効音量です。
        float effectiveVolume = 1.0f;

        // このバスがミュートされているかどうかです。
        bool muted = false;

        // このバスがソロ再生対象かどうかです。
        bool solo = false;

        // このバスで管理している有効ボイス数です。
        int activeVoiceCount = 0;

        // このバスでストリーミング再生しているボイス数です。
        int streamingVoiceCount = 0;
    };

    // 音声システムを生成します。
    AudioWorldSystem();

    // 音声システムを破棄します。内部で Finalize も呼ばれます。
    ~AudioWorldSystem();

    // miniaudio エンジンと各バスを初期化します。
    bool Initialize();

    // 全ボイス停止後、音声エンジンとバスを解放します。
    void Finalize();

    // ECS の AudioEmitter / AudioListener / AudioSettings を読み取り、音声状態を更新します。
    void Update(Registry& registry, EngineMode mode);

    // シーン切り替え時に再生中ボイスやエミッター状態を破棄します。
    void ResetForSceneChange();

    // 2D の一時音声を再生します。UI音や効果音など、Entity に紐付かない音に使います。
    AudioVoiceHandle PlayTransient2D(const std::string& clipPath,
                                     float volume = 1.0f,
                                     float pitch = 1.0f,
                                     bool loop = false,
                                     AudioBusType bus = AudioBusType::SFX,
                                     bool streaming = false);

    // 3D の一時音声を指定ワールド位置で再生します。
    AudioVoiceHandle PlayTransient3D(const std::string& clipPath,
                                     const DirectX::XMFLOAT3& position,
                                     float volume = 1.0f,
                                     float pitch = 1.0f,
                                     bool loop = false,
                                     AudioBusType bus = AudioBusType::SFX,
                                     float minDistance = 1.0f,
                                     float maxDistance = 50.0f,
                                     bool streaming = false);

    // エディタ用の 2D 一時音声を再生します。Play 中でなくても消されにくいプレビュー用途です。
    AudioVoiceHandle PlayEditorTransient2D(const std::string& clipPath,
                                           float volume = 1.0f,
                                           float pitch = 1.0f,
                                           bool loop = false,
                                           AudioBusType bus = AudioBusType::UI,
                                           bool streaming = false);

    // エディタ用の 3D 一時音声を再生します。
    AudioVoiceHandle PlayEditorTransient3D(const std::string& clipPath,
                                           const DirectX::XMFLOAT3& position,
                                           float volume = 1.0f,
                                           float pitch = 1.0f,
                                           bool loop = false,
                                           AudioBusType bus = AudioBusType::UI,
                                           float minDistance = 1.0f,
                                           float maxDistance = 50.0f,
                                           bool streaming = false);

    // 指定したボイスを停止して破棄します。
    void StopVoice(AudioVoiceHandle handle);

    // 全てのボイスを停止して破棄します。
    void StopAllVoices();

    // 3D ボイスの再生位置を更新します。
    void SetVoicePosition(AudioVoiceHandle handle, const DirectX::XMFLOAT3& position);

    // 指定したボイスがまだ管理下に存在しているかを返します。
    bool IsVoiceAlive(AudioVoiceHandle handle) const;

    // 音声クリップをエディタ上でプレビュー再生します。
    void PreviewClip(const std::string& clipPath, AudioBusType bus = AudioBusType::UI);

    // 同じクリップが再生中なら停止し、違う場合はプレビュー再生します。
    void TogglePreviewClip(const std::string& clipPath, AudioBusType bus = AudioBusType::UI);

    // 現在のプレビュー再生を停止します。
    void StopPreview();

    // 指定クリップが現在プレビュー再生中かどうかを返します。
    bool IsPreviewing(const std::string& clipPath) const;

    // 現在プレビュー中のクリップパスを返します。
    std::string GetPreviewClipPath() const;

    // プレビュー再生の現在位置と長さを取得します。
    bool GetPreviewPlaybackProgress(float& cursorSeconds, float& lengthSeconds) const;

    // プレビュー再生位置を指定秒数へ移動します。
    void SeekPreview(float seconds);

    // デバッグ表示用のボイス一覧を取得します。
    std::vector<DebugVoiceInfo> GetDebugVoices() const;

    // デバッグ表示用のバス一覧を取得します。
    std::vector<DebugBusInfo> GetDebugBuses() const;

    // 指定クリップのメタ情報を取得します。
    AudioClipAsset DescribeClip(const std::string& clipPath);

    // キャッシュ済みクリップ一覧を取得します。
    std::vector<AudioClipAsset> GetCachedClips() const;

    // キャッシュ済みクリップ数を取得します。
    size_t GetCachedClipCount() const;

    // クリップメタ情報キャッシュを破棄します。
    void ClearClipCache();

    // 指定した音声バスのミュート状態を設定します。
    void SetBusMuted(AudioBusType bus, bool muted);

    // 指定した音声バスがミュート中かどうかを返します。
    bool IsBusMuted(AudioBusType bus) const;

    // ソロ再生するバスを設定します。nullopt の場合はソロ解除です。
    void SetSoloBus(std::optional<AudioBusType> bus);

    // 現在ソロ再生対象になっているバスを取得します。
    std::optional<AudioBusType> GetSoloBus() const;

    // 現在有効なリスナー Entity を取得します。
    EntityID GetActiveListenerEntity() const { return m_activeListenerEntity; }

    // 音声システムが初期化済みかどうかを返します。
    bool IsInitialized() const { return m_initialized; }

private:
    // miniaudio やボイス管理をまとめた内部実装です。
    std::unique_ptr<Impl> m_impl;

    // 初期化済みかどうかです。
    bool m_initialized = false;

    // 現在採用されている AudioListenerComponent の Entity です。
    EntityID m_activeListenerEntity = Entity::NULL_ID;
};

// AudioBusType を UI 表示用の文字列へ変換します。
const char* GetAudioBusTypeLabel(AudioBusType bus);
