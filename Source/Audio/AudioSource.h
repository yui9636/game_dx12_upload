#pragma once

#include <memory>
#include <xaudio2.h>
#include <x3daudio.h>
#include <DirectXMath.h>
#include "AudioResource.h"

class AudioSource
{
public:
    AudioSource(IXAudio2* xaudio, std::shared_ptr<AudioResource> resource);
    ~AudioSource();

    void Play();
    void Stop();

    bool IsPlaying() const;
    bool IsStopped() const;

    void SetVolume(float volume);
    void SetPitch(float pitch);
    void SetLoop(bool loop);

    void SetPosition(const DirectX::XMFLOAT3& position);

    void Update3D(const X3DAUDIO_LISTENER& listener, const X3DAUDIO_HANDLE& x3dHandle);

private:
    IXAudio2SourceVoice* sourceVoice = nullptr;
    std::shared_ptr<AudioResource> resource;

    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;

    bool is3D = false;
    DirectX::XMFLOAT3 position = { 0, 0, 0 };

    X3DAUDIO_EMITTER emitter = { 0 };
    X3DAUDIO_DSP_SETTINGS dspSettings = { 0 };
    FLOAT32 matrixCoefficients[8] = { 0 };
};
