#pragma once
#include "Component/Component.h"
#include "Storage/GameplayAsset.h"
#include <memory>
#include <vector>

class RunnerComponent;
class ColliderComponent;
class Actor;

class TimelineCharacter : public Component
{
public:
    const char* GetName() const override { return "TimelineCharacter"; }

    void Start() override;
    void Update(float elapsedTime) override;

    void SetGameplayData(const GameplayAsset& data);

    void SetRunner(std::shared_ptr<RunnerComponent> runner);

    void OnAnimationChange(int newAnimIndex);

private:
    DirectX::XMMATRIX CalcWorldMatrixForItem(const GESequencerItem& item);
    int SecondsToFrames(float seconds) const;
    float FramesToSeconds(int frames) const;

private:
    std::shared_ptr<RunnerComponent> runner;
    std::shared_ptr<ColliderComponent> collider;

    GameplayAsset gameplayData;

    const std::vector<GESequencerItem>* currentTimeline = nullptr;

    std::vector<GESequencerItem> activeItems;

    int currentAnimIndex = -1;
    float fps = 60.0f;
};
