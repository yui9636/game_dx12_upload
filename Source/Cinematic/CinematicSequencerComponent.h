#pragma once
#include "Cinematic/CinematicSequence.h"
#include "Component/Component.h"
#include "SequencerDriver.h"
#include <memory>
#include <vector>
#include <string>

// 前方宣言
class Actor;

class CinematicSequencerComponent : public Component
{
public:
    CinematicSequencerComponent();
    ~CinematicSequencerComponent() override;

    const char* GetName() const override { return "CinematicSequencer"; }

    void Update(float dt) override;
    void OnGUI() override;

    // 再生制御
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
    void DrawGizmo(); // ギズモ操作

    void CaptureInitialState(); // 再生前の状態を保存
    void RestoreInitialState(); // 再生終了時に戻す

    // ★修正: ゴーストカメラは1つだけにする
    void UpdateGhostCamera();

    // 選択情報構造体
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

    // ★修正: 単一の編集用ゴーストアクター
    std::shared_ptr<Actor> editorGhost;

    // Undo/Redo的挙動のための状態保存
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