#include "AudioSource.h"
#include <cassert>

using namespace DirectX;

AudioSource::AudioSource(IXAudio2* xaudio, std::shared_ptr<AudioResource> resource)
    : resource(resource)
{
    HRESULT hr;

    // ソースボイス生成
    hr = xaudio->CreateSourceVoice(&sourceVoice, &resource->GetWaveFormat());
    assert(SUCCEEDED(hr));

    // ---------------------------------------------------
    // 3Dオーディオ用の初期化 (Emitter設定)
    // ---------------------------------------------------
    const WAVEFORMATEX& wfx = resource->GetWaveFormat();

    emitter.ChannelCount = wfx.nChannels;
    emitter.CurveDistanceScaler = 1.0f;
    emitter.DopplerScaler = 1.0f;

    // モノラル音源だけ3D定位が可能 (ステレオはL/Rがあるため定位させにくい)
    // ここでは簡易的に全音源を3D計算対象として設定できるようにしておく
    if (wfx.nChannels == 1)
    {
        emitter.ChannelRadius = 0.0f;
    }
    else
    {
        // ステレオの場合、少し半径を持たせるなどの調整があるが今回は0
        emitter.ChannelRadius = 0.0f;
    }

    // 計算結果受け取り用設定
    // 出力先(マスタリングボイス)は基本2ch(ステレオ)と仮定して初期化
    dspSettings.SrcChannelCount = wfx.nChannels;
    dspSettings.DstChannelCount = 2; // ※マネージャーから貰うのが厳密だが一旦ステレオ固定
    dspSettings.pMatrixCoefficients = matrixCoefficients;
}

AudioSource::~AudioSource()
{
    if (sourceVoice)
    {
        sourceVoice->Stop();
        sourceVoice->DestroyVoice();
        sourceVoice = nullptr;
    }
}

void AudioSource::Play()
{
    if (!sourceVoice) return;

    // 停止＆バッファクリア
    Stop();

    // バッファ設定
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.pAudioData = resource->GetAudioData();
    buffer.AudioBytes = resource->GetAudioBytes();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    sourceVoice->SubmitSourceBuffer(&buffer);

    // パラメータ適用
    sourceVoice->SetVolume(volume);
    sourceVoice->SetFrequencyRatio(pitch);

    sourceVoice->Start();
}

void AudioSource::Stop()
{
    if (!sourceVoice) return;
    sourceVoice->Stop();
    sourceVoice->FlushSourceBuffers();
}

bool AudioSource::IsPlaying() const
{
    if (!sourceVoice) return false;
    XAUDIO2_VOICE_STATE state;
    sourceVoice->GetState(&state);
    return state.BuffersQueued > 0;
}

bool AudioSource::IsStopped() const
{
    return !IsPlaying();
}

void AudioSource::SetVolume(float vol)
{
    volume = vol;
    if (sourceVoice) sourceVoice->SetVolume(volume);
}

void AudioSource::SetPitch(float p)
{
    pitch = p;
    if (sourceVoice) sourceVoice->SetFrequencyRatio(pitch);
}

void AudioSource::SetLoop(bool l)
{
    loop = l;
    // 再生中の変更は即座には反映されないため、必要なら再Playが必要だが、
    // 今回は設定のみ保持し、次回のPlayで適用する運用とする。
}

void AudioSource::SetPosition(const DirectX::XMFLOAT3& pos)
{
    position = pos;
    is3D = true; // 位置が設定されたら3Dモードとみなす

    // エミッターの位置更新
    emitter.Position = { pos.x, pos.y, pos.z };
    emitter.OrientFront = { 0, 0, 1 }; // 音源の向き（無指向性なら固定でOK）
    emitter.OrientTop = { 0, 1, 0 };
    emitter.Velocity = { 0, 0, 0 };    // ドップラー効果用（今回は省略）
}

void AudioSource::Update3D(const X3DAUDIO_LISTENER& listener, const X3DAUDIO_HANDLE& x3dHandle)
{
    if (!sourceVoice || !is3D) return;

    // 3D計算 (リスナーとエミッターの位置関係からDSP設定を算出)
    // 計算フラグ: マトリクス(パンニング/減衰) と ドップラー を計算
    UINT32 flags = X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER;

    X3DAudioCalculate(x3dHandle, &listener, &emitter, flags, &dspSettings);

    // 計算結果をボイスに適用
    // 1. マトリクス (音量バランス・距離減衰)
    sourceVoice->SetOutputMatrix(nullptr, dspSettings.SrcChannelCount, dspSettings.DstChannelCount, dspSettings.pMatrixCoefficients);

    // 2. ドップラー効果 (ピッチ変化)
    sourceVoice->SetFrequencyRatio(pitch * dspSettings.DopplerFactor);
}