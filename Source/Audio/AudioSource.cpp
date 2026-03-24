#include "AudioSource.h"
#include <cassert>

using namespace DirectX;

AudioSource::AudioSource(IXAudio2* xaudio, std::shared_ptr<AudioResource> resource)
    : resource(resource)
{
    HRESULT hr;

    hr = xaudio->CreateSourceVoice(&sourceVoice, &resource->GetWaveFormat());
    assert(SUCCEEDED(hr));

    // ---------------------------------------------------
    // ---------------------------------------------------
    const WAVEFORMATEX& wfx = resource->GetWaveFormat();

    emitter.ChannelCount = wfx.nChannels;
    emitter.CurveDistanceScaler = 1.0f;
    emitter.DopplerScaler = 1.0f;

    if (wfx.nChannels == 1)
    {
        emitter.ChannelRadius = 0.0f;
    }
    else
    {
        emitter.ChannelRadius = 0.0f;
    }

    dspSettings.SrcChannelCount = wfx.nChannels;
    dspSettings.DstChannelCount = 2;
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

    Stop();

    XAUDIO2_BUFFER buffer = { 0 };
    buffer.pAudioData = resource->GetAudioData();
    buffer.AudioBytes = resource->GetAudioBytes();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    sourceVoice->SubmitSourceBuffer(&buffer);

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
}

void AudioSource::SetPosition(const DirectX::XMFLOAT3& pos)
{
    position = pos;
    is3D = true;

    emitter.Position = { pos.x, pos.y, pos.z };
    emitter.OrientFront = { 0, 0, 1 };
    emitter.OrientTop = { 0, 1, 0 };
    emitter.Velocity = { 0, 0, 0 };
}

void AudioSource::Update3D(const X3DAUDIO_LISTENER& listener, const X3DAUDIO_HANDLE& x3dHandle)
{
    if (!sourceVoice || !is3D) return;

    UINT32 flags = X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER;

    X3DAudioCalculate(x3dHandle, &listener, &emitter, flags, &dspSettings);

    sourceVoice->SetOutputMatrix(nullptr, dspSettings.SrcChannelCount, dspSettings.DstChannelCount, dspSettings.pMatrixCoefficients);

    sourceVoice->SetFrequencyRatio(pitch * dspSettings.DopplerFactor);
}
