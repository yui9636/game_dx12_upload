#include "System/Misc.h" // 既存のヘルパーがあれば
#include "Audio.h"
#include "AudioResource.h"
#include "AudioSource.h"
#include <algorithm>

using namespace DirectX;

Audio* Audio::instance = nullptr;

Audio::Audio()
{
    // インスタンス設定
    if (instance == nullptr)
    {
        instance = this;
    }
    Initialize();
}

Audio::~Audio()
{
    Finalize();
    if (instance == this)
    {
        instance = nullptr;
    }
}

void Audio::Initialize()
{
    if (xAudio2) return; // 既に初期化済み

    HRESULT hr;

    // COM初期化
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // XAudio2エンジンの作成
    // デバッグビルド時はデバッグ情報を有効にするフラグなどを入れても良い
    UINT32 flags = 0;
#if defined(_DEBUG)
    // flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    hr = XAudio2Create(xAudio2.GetAddressOf(), flags, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create XAudio2 engine.\n");
        return;
    }

    // マスターボイスの作成
    hr = xAudio2->CreateMasteringVoice(&masteringVoice);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create mastering voice.\n");
        return;
    }

    // -------------------------------------------------------
    // 3Dオーディオ (X3DAudio) の初期化
    // -------------------------------------------------------
    DWORD channelMask;
    masteringVoice->GetChannelMask(&channelMask);

    // スピーカー構成に基づいて X3DAudio を初期化
    X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, x3DHandle);

    // リスナーの初期化
    memset(&listener, 0, sizeof(X3DAUDIO_LISTENER));
    listener.OrientFront = { 0, 0, 1 };
    listener.OrientTop = { 0, 1, 0 };
    listener.Position = { 0, 0, 0 };
    listener.Velocity = { 0, 0, 0 };
}

void Audio::Finalize()
{
    // アクティブなソースを全停止
    for (auto& source : activeSources)
    {
        if (source) source->Stop();
    }
    activeSources.clear();

    // キャッシュクリア
    ClearCache();

    // マスターボイス破棄
    if (masteringVoice)
    {
        masteringVoice->DestroyVoice();
        masteringVoice = nullptr;
    }

    xAudio2.Reset();
    CoUninitialize();
}

void Audio::Update()
{
    // 再生が終了したソースをリストから削除する
    // (これをしないとメモリリーク、あるいは無限に増え続ける)
    auto it = std::remove_if(activeSources.begin(), activeSources.end(),
        [](const std::shared_ptr<AudioSource>& s) {
            // 停止しているものは削除対象
            return s->IsStopped();
        });

    activeSources.erase(it, activeSources.end());

    // 必要ならここで3D更新を行っても良いが、
    // 基本的にSetListenerやPlay3Dのタイミングで更新される設計にしている
}

// -------------------------------------------------------
// リソースキャッシュシステム
// -------------------------------------------------------
std::shared_ptr<AudioResource> Audio::GetResource(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(mutex);

    // 1. キャッシュを検索
    auto it = resourceCache.find(filename);
    if (it != resourceCache.end())
    {
        // ヒットしたらそれを返す (ロード不要！)
        return it->second;
    }

    // 2. なければ新規ロード
    auto resource = std::make_shared<AudioResource>(filename.c_str());

    // 正常にロードできていればキャッシュに登録
    if (resource->GetAudioBytes() > 0)
    {
        resourceCache[filename] = resource;
    }
    else
    {
        // ロード失敗時はnullptrを返すか、エラー用リソースを返す
        // ここでは安全のため nullptr を返しておく
        return nullptr;
    }

    return resource;
}

void Audio::ClearCache()
{
    std::lock_guard<std::mutex> lock(mutex);
    resourceCache.clear();
}

// -------------------------------------------------------
// 再生関数
// -------------------------------------------------------
std::shared_ptr<AudioSource> Audio::Play2D(const std::string& filename, float volume, float pitch, bool loop)
{
    auto resource = GetResource(filename);
    if (!resource) return nullptr; // 失敗時はnullptr

    // 音源インスタンス生成
    auto source = std::make_shared<AudioSource>(xAudio2.Get(), resource);

    // パラメータ設定
    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLoop(loop);

    // 再生開始
    source->Play();

    // 管理リストに追加
    activeSources.push_back(source);

    return source; // ★作成したインスタンスを返す
}

std::shared_ptr<AudioSource> Audio::Play3D(const std::string& filename, const DirectX::XMFLOAT3& position, float volume, float pitch, bool loop)
{
    auto resource = GetResource(filename);
    if (!resource) return nullptr;

    auto source = std::make_shared<AudioSource>(xAudio2.Get(), resource);

    // 3D設定
    source->SetPosition(position);
    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLoop(loop);

    // 再生開始
    source->Play();

    // 初回の3D計算を適用 (これをしないと一瞬変な位置で鳴る可能性がある)
    source->Update3D(listener, x3DHandle);

    activeSources.push_back(source);

    return source;
}

void Audio::SetListener(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front, const DirectX::XMFLOAT3& up)
{
    listener.Position = { position.x, position.y, position.z };
    listener.OrientFront = { front.x, front.y, front.z };
    listener.OrientTop = { up.x, up.y, up.z };

    // リスナーが動いたら、すべての再生中3D音源に対して計算結果を更新する
    for (auto& source : activeSources)
    {
        source->Update3D(listener, x3DHandle);
    }
}

void Audio::StopAll()
{
    // アクティブなソースを全停止
    for (auto& source : activeSources)
    {
        if (source) source->Stop();
    }
    // リストをクリアして参照を外す（AudioSourceのデストラクタが呼ばれる）
    activeSources.clear();
}