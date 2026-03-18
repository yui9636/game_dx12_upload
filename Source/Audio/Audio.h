#pragma once

#include <d3d11.h>
#include <xaudio2.h>
#include <x3daudio.h> // 3D音響計算用

#include <wrl/client.h>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <mutex>
#include <DirectXMath.h>

// リンクライブラリ
#pragma comment(lib, "xaudio2.lib")

// 前方宣言
class AudioResource;
class AudioSource;

class Audio
{
public:
    // シングルトン
    static Audio* Instance()
    {
        // インスタンスはCpp側で生成・保持するスタイル（既存踏襲）
        // またはここでstatic生成しても良いが、既存コードに合わせてポインタアクセスにする
        return instance;
    }

    Audio();
    ~Audio();

    // 初期化・終了（コンストラクタ/デストラクタで呼ばれるが明示的にも呼べるように）
    void Initialize();
    void Finalize();
    void StopAll();
    // 毎フレーム更新（終わった音の削除など）
    void Update();

    // -----------------------------------------------------------
    // リソース管理
    // -----------------------------------------------------------
    // ファイルから読み込む（キャッシュにあればそれを返す）
    std::shared_ptr<AudioResource> GetResource(const std::string& filename);

    // キャッシュを全クリア（シーン遷移時などに呼ぶ）
    void ClearCache();

    std::shared_ptr<AudioSource> Play2D(const std::string& filename, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

    // 3D再生 (効果音など)
    // 戻り値でインスタンスを返すので、後から位置を変えたり止めたりできる
    std::shared_ptr<AudioSource> Play3D(const std::string& filename, const DirectX::XMFLOAT3& position, float volume = 1.0f, float pitch = 0.0f, bool loop = false);

    // -----------------------------------------------------------
    // 3Dリスナー制御
    // -----------------------------------------------------------
    // プレイヤーの耳（カメラ）の位置をセットする
    void SetListener(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front, const DirectX::XMFLOAT3& up);

    // ゲッター
    IXAudio2* GetXAudio2() const { return xAudio2.Get(); }



    // 3D計算用ハンドル取得
    const X3DAUDIO_HANDLE& GetX3DHandle() const { return x3DHandle; }
    const X3DAUDIO_LISTENER& GetListener() const { return listener; }

private:
    static Audio* instance;

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masteringVoice = nullptr;

    // 3Dオーディオ関連
    X3DAUDIO_HANDLE x3DHandle = {};
    X3DAUDIO_LISTENER listener = {};

    // リソースキャッシュ
    // Key: ファイルパス, Value: 音声データ
    std::map<std::string, std::shared_ptr<AudioResource>> resourceCache;
    std::mutex mutex; // スレッドセーフ用

    // 再生中のソース管理（更新・削除用）
    std::vector<std::shared_ptr<AudioSource>> activeSources;
};