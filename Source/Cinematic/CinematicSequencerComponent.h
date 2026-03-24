#pragma once
#include "Cinematic/CinematicSequence.h"
#include "Component/Component.h"
#include "SequencerDriver.h"
#include <memory>
#include <vector>
#include <string>

class Actor;

class CinematicSequencerComponent : public Component
{
public:
    CinematicSequencerComponent();
    ~CinematicSequencerComponent() override;

    const char* GetName() const override { return "CinematicSequencer"; }

    void Update(float dt) override;
    void OnGUI() override;

    void Play();
    void Stop();
    void Pause();
    void SetTime(float time);

    bool IsPlaying() const { return isPlaying; }
    std::shared_ptr<Cinematic::Sequence> GetSequence() const { return sequence; }

    void SetTargetActor(std::shared_ptr<Actor> actor);
private:
    void DrawTrackList();
    void DrawTimelineWindow();
    void DrawGizmo();

    void CaptureInitialState();
    void RestoreInitialState();

    void UpdateGhostCamera();

    struct Selection
    {
        int trackIndex = -1;
        int keyIndex = -1;
        bool isDragging = false;

        bool IsSelected(int t, int k) const { return trackIndex == t && keyIndex == k; }
        bool IsValid() const { return trackIndex != -1 && keyIndex != -1; }
        void Clear() { trackIndex = -1; keyIndex = -1; isDragging = false; }
    } selection;

    std::shared_ptr<Cinematic::Sequence> sequence;
    float currentTime = 0.0f;
    bool isPlaying = false;
    bool isPaused = false;

    std::shared_ptr<Actor> editorGhost;

    struct ActorState {
        std::weak_ptr<Actor> target;
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 rotation;
        DirectX::XMFLOAT3 scale;
    };
    std::vector<ActorState> initialStates;

    SequencerDriver driver;           
    std::weak_ptr<Actor> targetActor;
};
