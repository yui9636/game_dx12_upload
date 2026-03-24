#pragma once
#include "Component/Component.h"
#include <string>
#include <memory>

class AudioSource;

class BGMComponent : public Component
{
public:
    const char* GetName() const override { return "BGM"; }

    void Start() override;
    void OnDestroy() override;

    void OnGUI() override;
    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;

    void PlayPreview();
    void StopPreview();

private:
    std::string filePath;
    float volume = 0.5f;
    float pitch = 1.0f;
    bool loop = true;

    std::shared_ptr<AudioSource> playingSource;
};
