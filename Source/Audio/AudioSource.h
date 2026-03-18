#pragma once

#include <memory>
#include <xaudio2.h>
#include <x3daudio.h>
#include <DirectXMath.h>
#include "AudioResource.h"

// オーディオソース (再生インスタンス)
class AudioSource
{
public:
    // コンストラクタで初期設定を行う
    AudioSource(IXAudio2* xaudio, std::shared_ptr<AudioResource> resource);
    ~AudioSource();

    // 再生・停止
    void Play();
    void Stop();

    // 状態取得
    bool IsPlaying() const;
    bool IsStopped() const;

    // パラメータ設定
    void SetVolume(float volume);
    void SetPitch(float pitch);
    void SetLoop(bool loop);

    // 3D座標設定 (ワールド座標)
    void SetPosition(const DirectX::XMFLOAT3& position);

    // 3D演算更新 (マネージャーから呼ばれる)
    // listener: カメラの位置と向き, x3dHandle: 計算用ハンドル
    void Update3D(const X3DAUDIO_LISTENER& listener, const X3DAUDIO_HANDLE& x3dHandle);

private:
    IXAudio2SourceVoice* sourceVoice = nullptr;
    std::shared_ptr<AudioResource> resource;

    // 再生パラメータ
    float volume = 1.0f;
    float pitch = 1.0f; // 1.0 = 等速
    bool loop = false;

    // 3D用パラメータ
    bool is3D = false; // SetPositionされたらtrueになる
    DirectX::XMFLOAT3 position = { 0, 0, 0 };

    // X3DAudio用構造体
    X3DAUDIO_EMITTER emitter = { 0 };
    X3DAUDIO_DSP_SETTINGS dspSettings = { 0 };
    FLOAT32 matrixCoefficients[8] = { 0 }; // チャンネルマトリクス計算用バッファ (最大7.1ch想定)
};