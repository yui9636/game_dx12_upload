#pragma once
#include "Component/Component.h"
#include "Cinematic/CinematicSequence.h"

class CinematicPlayComponent : public Component
{
public:
    const char* GetName() const override { return "CinematicPlay"; }

    void Start() override;
    void Update(float dt) override;

    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;
    void OnGUI() override;

    void Play();
    void Stop();

private:
    std::string sequenceFilePath;
    bool playOnStart = false;
    bool isPlaying = false;
    float currentTime = 0.0f;

    std::shared_ptr<Cinematic::Sequence> sequence;
};
