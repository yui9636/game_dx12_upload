#pragma once
// Actor に付けてシネマティックシーケンスを編集・再生するコンポーネント。
#include "Cinematic/CinematicSequence.h"
#include "Component/Component.h"
#include "SequencerDriver.h"
#include <memory>
#include <vector>
#include <string>

// 対象 Actor は前方宣言で依存を軽くする。
class Actor;

// シネマティックのタイムライン編集・再生・ターゲット反映を担当する。
class CinematicSequencerComponent : public Component
{
public:
    // シーケンスを初期化する。
    CinematicSequencerComponent();
    // 作成した編集用ゴーストなどを破棄する。
    ~CinematicSequencerComponent() override;

    // エディタに表示するコンポーネント名を返す。
    const char* GetName() const override { return "CinematicSequencer"; }

    // 毎フレームの再生時間更新とトラック評価を行う。
    void Update(float dt) override;
    // ImGui でシーケンサー編集 UI を描画する。
    void OnGUI() override;

    // 先頭から再生を開始する。
    void Play();
    // 再生を停止して時間を先頭へ戻す。
    void Stop();
    // 再生中の一時停止状態を切り替える。
    void Pause();
    // 現在時間を直接設定し、シーケンスをその時間で評価する。
    void SetTime(float time);

    // 再生中かどうかを返す。
    bool IsPlaying() const { return isPlaying; }
    // 編集対象のシーケンスを取得する。
    std::shared_ptr<Cinematic::Sequence> GetSequence() const { return sequence; }

    // トラック評価を反映する対象 Actor を設定する。
    void SetTargetActor(std::shared_ptr<Actor> actor);
private:
    // 左側のトラック一覧 UI を描画する。
    void DrawTrackList();
    // 右側のタイムライン UI を描画する。
    void DrawTimelineWindow();
    // カメラキー編集用のギズモを描画する。
    void DrawGizmo();

    // 再生開始前の Actor 状態を保存する。
    void CaptureInitialState();
    // 保存しておいた Actor 状態へ戻す。
    void RestoreInitialState();

    // 選択中のカメラキーを表示する編集用ゴーストを更新する。
    void UpdateGhostCamera();

    // 現在選択中のトラック・キー情報。
    struct Selection
    {
        // 選択中トラック番号。-1 は未選択。
        int trackIndex = -1;
        // 選択中キー番号。-1 は未選択。
        int keyIndex = -1;
        // キーをドラッグ中かどうか。
        bool isDragging = false;

        // 指定したトラック・キーが現在選択されているか調べる。
        bool IsSelected(int t, int k) const { return trackIndex == t && keyIndex == k; }
        // トラックとキーの両方が選択されているか調べる。
        bool IsValid() const { return trackIndex != -1 && keyIndex != -1; }
        // 選択状態を解除する。
        void Clear() { trackIndex = -1; keyIndex = -1; isDragging = false; }
    } selection;

    // 編集・再生対象のシーケンス。
    std::shared_ptr<Cinematic::Sequence> sequence;
    // 現在の再生時間。
    float currentTime = 0.0f;
    // 再生中フラグ。
    bool isPlaying = false;
    // 一時停止中フラグ。
    bool isPaused = false;

    // カメラキー位置を可視化する編集用 Actor。
    std::shared_ptr<Actor> editorGhost;

    // 再生前の Actor 状態を戻すための保存データ。
    struct ActorState {
        std::weak_ptr<Actor> target;
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 rotation;
        DirectX::XMFLOAT3 scale;
    };
    std::vector<ActorState> initialStates;

    // シーケンサー結果を外部システムへ渡すドライバー。
    SequencerDriver driver;           
    // トラックの反映対象 Actor。弱参照にして寿命管理を Actor 側へ任せる。
    std::weak_ptr<Actor> targetActor;
};
