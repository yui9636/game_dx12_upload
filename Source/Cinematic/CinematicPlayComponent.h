#pragma once
#include "Component/Component.h"
#include "Cinematic/CinematicSequence.h"

class CinematicPlayComponent : public Component
{
public:
    const char* GetName() const override { return "CinematicPlay"; }

    void Start() override;
    void Update(float dt) override;

    // エディター設定用
    void Serialize(json& outJson) const override;
    void Deserialize(const json& inJson) override;
    void OnGUI() override;

    // 再生制御
    void Play();
    void Stop();

private:
    std::string sequenceFilePath; // 再生するファイル
    bool playOnStart = false;     // 開始時に自動再生するか
    bool isPlaying = false;
    float currentTime = 0.0f;

    std::shared_ptr<Cinematic::Sequence> sequence;
};