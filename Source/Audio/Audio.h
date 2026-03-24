#pragma once

#include <d3d11.h>
#include <xaudio2.h>
#include <x3daudio.h>

#include <wrl/client.h>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <mutex>
#include <DirectXMath.h>

#pragma comment(lib, "xaudio2.lib")

class AudioResource;
class AudioSource;

class Audio
{
public:
    static Audio* Instance()
    {
        return instance;
    }

    Audio();
    ~Audio();

    void Initialize();
    void Finalize();
    void StopAll();
    void Update();

    // -----------------------------------------------------------
    // -----------------------------------------------------------
    std::shared_ptr<AudioResource> GetResource(const std::string& filename);

    void ClearCache();

    std::shared_ptr<AudioSource> Play2D(const std::string& filename, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

    std::shared_ptr<AudioSource> Play3D(const std::string& filename, const DirectX::XMFLOAT3& position, float volume = 1.0f, float pitch = 0.0f, bool loop = false);

    // -----------------------------------------------------------
    // -----------------------------------------------------------
    void SetListener(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front, const DirectX::XMFLOAT3& up);

    IXAudio2* GetXAudio2() const { return xAudio2.Get(); }



    const X3DAUDIO_HANDLE& GetX3DHandle() const { return x3DHandle; }
    const X3DAUDIO_LISTENER& GetListener() const { return listener; }

private:
    static Audio* instance;

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masteringVoice = nullptr;

    X3DAUDIO_HANDLE x3DHandle = {};
    X3DAUDIO_LISTENER listener = {};

    std::map<std::string, std::shared_ptr<AudioResource>> resourceCache;
    std::mutex mutex;

    std::vector<std::shared_ptr<AudioSource>> activeSources;
};
