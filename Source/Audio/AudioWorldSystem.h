#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "Component/AudioEmitterComponent.h"
#include "Entity/Entity.h"
#include "Engine/EngineMode.h"

class Registry;

using AudioVoiceHandle = uint64_t;

class AudioWorldSystem
{
public:
    struct Impl;

    struct DebugVoiceInfo
    {
        AudioVoiceHandle handle = 0;
        std::string clipPath;
        AudioBusType bus = AudioBusType::SFX;
        EntityID entity = Entity::NULL_ID;
        bool is3D = false;
        bool loop = false;
        bool playing = false;
        bool transient = false;
        bool preview = false;
        float volume = 1.0f;
        float pitch = 1.0f;
        float cursorSeconds = 0.0f;
        float lengthSeconds = 0.0f;
    };

    AudioWorldSystem();
    ~AudioWorldSystem();

    bool Initialize();
    void Finalize();
    void Update(Registry& registry, EngineMode mode);
    void ResetForSceneChange();

    AudioVoiceHandle PlayTransient2D(const std::string& clipPath,
                                     float volume = 1.0f,
                                     float pitch = 1.0f,
                                     bool loop = false,
                                     AudioBusType bus = AudioBusType::SFX,
                                     bool streaming = false);
    AudioVoiceHandle PlayTransient3D(const std::string& clipPath,
                                     const DirectX::XMFLOAT3& position,
                                     float volume = 1.0f,
                                     float pitch = 1.0f,
                                     bool loop = false,
                                     AudioBusType bus = AudioBusType::SFX,
                                     float minDistance = 1.0f,
                                     float maxDistance = 50.0f,
                                     bool streaming = false);
    void StopVoice(AudioVoiceHandle handle);
    void SetVoicePosition(AudioVoiceHandle handle, const DirectX::XMFLOAT3& position);
    bool IsVoiceAlive(AudioVoiceHandle handle) const;

    void PreviewClip(const std::string& clipPath, AudioBusType bus = AudioBusType::UI);
    void TogglePreviewClip(const std::string& clipPath, AudioBusType bus = AudioBusType::UI);
    void StopPreview();
    bool IsPreviewing(const std::string& clipPath) const;
    std::string GetPreviewClipPath() const;

    std::vector<DebugVoiceInfo> GetDebugVoices() const;
    EntityID GetActiveListenerEntity() const { return m_activeListenerEntity; }
    bool IsInitialized() const { return m_initialized; }

private:
    std::unique_ptr<Impl> m_impl;
    bool m_initialized = false;
    EntityID m_activeListenerEntity = Entity::NULL_ID;
};

const char* GetAudioBusTypeLabel(AudioBusType bus);
