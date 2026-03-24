#include "System/Misc.h"
#include "Audio.h"
#include "AudioResource.h"
#include "AudioSource.h"
#include <algorithm>

using namespace DirectX;

Audio* Audio::instance = nullptr;

Audio::Audio()
{
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
    if (xAudio2) return;

    HRESULT hr;

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

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

    hr = xAudio2->CreateMasteringVoice(&masteringVoice);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create mastering voice.\n");
        return;
    }

    // -------------------------------------------------------
    // -------------------------------------------------------
    DWORD channelMask;
    masteringVoice->GetChannelMask(&channelMask);

    X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, x3DHandle);

    memset(&listener, 0, sizeof(X3DAUDIO_LISTENER));
    listener.OrientFront = { 0, 0, 1 };
    listener.OrientTop = { 0, 1, 0 };
    listener.Position = { 0, 0, 0 };
    listener.Velocity = { 0, 0, 0 };
}

void Audio::Finalize()
{
    for (auto& source : activeSources)
    {
        if (source) source->Stop();
    }
    activeSources.clear();

    ClearCache();

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
    auto it = std::remove_if(activeSources.begin(), activeSources.end(),
        [](const std::shared_ptr<AudioSource>& s) {
            return s->IsStopped();
        });

    activeSources.erase(it, activeSources.end());

}

// -------------------------------------------------------
// -------------------------------------------------------
std::shared_ptr<AudioResource> Audio::GetResource(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto it = resourceCache.find(filename);
    if (it != resourceCache.end())
    {
        return it->second;
    }

    auto resource = std::make_shared<AudioResource>(filename.c_str());

    if (resource->GetAudioBytes() > 0)
    {
        resourceCache[filename] = resource;
    }
    else
    {
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
// -------------------------------------------------------
std::shared_ptr<AudioSource> Audio::Play2D(const std::string& filename, float volume, float pitch, bool loop)
{
    auto resource = GetResource(filename);
    if (!resource) return nullptr;

    auto source = std::make_shared<AudioSource>(xAudio2.Get(), resource);

    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLoop(loop);

    source->Play();

    activeSources.push_back(source);

    return source;
}

std::shared_ptr<AudioSource> Audio::Play3D(const std::string& filename, const DirectX::XMFLOAT3& position, float volume, float pitch, bool loop)
{
    auto resource = GetResource(filename);
    if (!resource) return nullptr;

    auto source = std::make_shared<AudioSource>(xAudio2.Get(), resource);

    source->SetPosition(position);
    source->SetVolume(volume);
    source->SetPitch(pitch);
    source->SetLoop(loop);

    source->Play();

    source->Update3D(listener, x3DHandle);

    activeSources.push_back(source);

    return source;
}

void Audio::SetListener(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front, const DirectX::XMFLOAT3& up)
{
    listener.Position = { position.x, position.y, position.z };
    listener.OrientFront = { front.x, front.y, front.z };
    listener.OrientTop = { up.x, up.y, up.z };

    for (auto& source : activeSources)
    {
        source->Update3D(listener, x3DHandle);
    }
}

void Audio::StopAll()
{
    for (auto& source : activeSources)
    {
        if (source) source->Stop();
    }
    activeSources.clear();
}
