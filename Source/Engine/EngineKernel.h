#pragma once
#include "EngineTime.h"
#include "EngineMode.h"
#include "Audio/AudioWorldSystem.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderPipeline.h"
#include "RenderContext/RenderQueue.h"
#include "ReflectionProbe/ReflectionProbeBaker.h"
#include "Render/OffscreenRenderer.h"
#include "Grid/GridRenderSystem.h"
#include "Input/IInputBackend.h"
#include "Input/InputEventQueue.h"
#include "Registry/Registry.h"
#include "GameLoop/GameLoopAsset.h"
#include "GameLoop/FlowEventQueue.h"
#include "GameLoop/GameLoopRuntime.h"
#include "GameLoop/UIButtonClickEventQueue.h"
#include <filesystem>
#include <memory>

class AIAutomationService;
class GameLayer;
class EditorLayer;

// エンジン全体の中核を担当するカーネルクラス。
// 更新、描画、入力取得、実行モード管理、各種サブシステムの保持を行う。
class EngineKernel
{
    // Framework からのみ初期化と終了処理へアクセスさせる。
    friend class Framework;

private:
    // singleton 用なのでコンストラクタは private にする。
    EngineKernel();

    // singleton 用なのでデストラクタは private にする。
    ~EngineKernel();

public:
    // singleton インスタンスを取得する。
    static EngineKernel& Instance();

    // 1 フレーム分の更新処理を実行する。
    void Update(float rawDt);

    // 1 フレーム分の描画処理を実行する。
    void Render();

    // 入力デバイスからイベントを収集する。
    void PollInput();

    // Editor または Pause 状態から Play を開始する。
    void Play();

    // Play または Pause を終了し、Editor 状態へ戻す。
    void Stop();

    // Play と Pause を切り替える。
    void Pause();

    // Pause 状態から 1 フレームだけ進める。
    void Step();

    // シーン切り替え時に描画関連の状態を安全にリセットする。
    void ResetRenderStateForSceneChange();

    // 現在の実行モードを取得する。
    EngineMode GetMode() const { return mode; }

    // 現在のエンジン時間情報を取得する。
    const EngineTime& GetTime() const { return time; }

    // AudioWorldSystem への参照を取得する。
    AudioWorldSystem& GetAudioWorld() { return *m_audioWorld; }

    // const 版の AudioWorldSystem への参照を取得する。
    const AudioWorldSystem& GetAudioWorld() const { return *m_audioWorld; }

    // 入力バックエンドへの参照を取得する。
    IInputBackend& GetInputBackend() { return *m_inputBackend; }

    // GameLoop の編集用データを取得する。
    GameLoopAsset& GetGameLoopAsset() { return m_gameLoopAsset; }

    // const 版の GameLoop 編集用データを取得する。
    const GameLoopAsset& GetGameLoopAsset() const { return m_gameLoopAsset; }

    // シーンロードをまたいで保持される GameLoop の実行状態を取得する。
    GameLoopRuntime& GetGameLoopRuntime() { return m_gameLoopRuntime; }

    // const 版の GameLoop 実行状態を取得する。
    const GameLoopRuntime& GetGameLoopRuntime() const { return m_gameLoopRuntime; }

    // GameLoop の永続入力用 Entity を保持する Registry を取得する。
    Registry& GetGameLoopRegistry() { return m_gameLoopRegistry; }

    // 現在の GameLayer が保持する Registry を取得する。
    Registry* GetGameRegistry();

    // const 版の GameLayer Registry を取得する。
    const Registry* GetGameRegistry() const;

    // 現在の EditorLayer を取得する。
    EditorLayer* GetEditorLayer() { return m_editorLayer.get(); }

    // const 版の EditorLayer を取得する。
    const EditorLayer* GetEditorLayer() const { return m_editorLayer.get(); }

    // 1 フレーム分の UI ボタンクリックイベントキューを取得する。
    UIButtonClickEventQueue& GetUIButtonClickQueue() { return m_uiButtonClickQueue; }

    // 1 フレーム分の GameFlow イベントキューを取得する。
    FlowEventQueue& GetFlowEventQueue() { return m_flowEventQueue; }

    // 1 フレームで収集された入力イベントキューを取得する。
    const InputEventQueue& GetInputEventQueue() const { return m_inputQueue; }

    // GameFlowEditor の Load を経由して実行用 GameLoop を登録する。
    void RegisterGameLoopAssetFromEditor(const GameLoopAsset& asset, const std::filesystem::path& path);

    // 実行用 GameLoop の登録を解除し、Play 時の自動シーン遷移を止める。
    void ClearGameLoopAssetRegistration();

    // 実行用 GameLoop が登録済みかを返す。
    bool IsGameLoopAssetRegistered() const { return m_gameLoopAssetRegistered; }

    // 指定パスが現在登録されている GameLoop かを返す。
    bool IsRegisteredGameLoopAssetPath(const std::filesystem::path& path) const;

    // 登録済み GameLoop のパスを返す。
    const std::filesystem::path& GetRegisteredGameLoopAssetPath() const { return m_gameLoopAssetPath; }

private:
    // エンジン全体を初期化する。
    void Initialize();

    // エンジン全体を終了処理する。
    void Finalize();

    // PlayerEditor 用のプレビューを offscreen に描画する。
    void RenderPlayerPreviewOffscreen();

    // 停止要求を即座に反映する。
    void StopImmediate();

    // 時間管理情報。
    EngineTime time;

    // 現在の実行モード。
    EngineMode mode = EngineMode::Editor;

    // Pause 状態から 1 フレームだけ進めるためのフラグ。
    bool m_stepFrameRequested = false;

    // Stop が要求されたことを示すフラグ。
    bool m_stopRequested = false;

    // Render 中であることを示すフラグ。
    bool m_renderInProgress = false;

    // 描画パイプライン本体。
    std::unique_ptr<RenderPipeline> m_renderPipeline;

    // ReflectionProbe のベイク担当。
    std::unique_ptr<ReflectionProbeBaker> m_probeBaker;

    // フレーム内の描画コマンドを集めるキュー。
    RenderQueue m_renderQueue;

    // Editor 用のグリッド描画システム。
    GridRenderSystem m_editorGridRenderSystem;

    // オーディオワールド更新システム。
    std::unique_ptr<AudioWorldSystem> m_audioWorld;

    // ゲーム本編用レイヤー。
    std::unique_ptr<GameLayer> m_gameLayer;

    // エディタ用レイヤー。
    std::unique_ptr<EditorLayer> m_editorLayer;

    // AI / external tool 用の自動化インターフェース。
    std::unique_ptr<AIAutomationService> m_aiAutomationService;

    // サムネイルや各種プレビュー用の共有 offscreen renderer。
    std::unique_ptr<OffscreenRenderer> m_sharedOffscreen;

    // 入力バックエンド本体。
    std::unique_ptr<IInputBackend> m_inputBackend;

    // 1 フレーム分の入力イベントを蓄積するキュー。
    InputEventQueue m_inputQueue;

    // GameLayer が存在しない場合の代替 Registry。
    Registry m_emptyRegistry;

    // GameLoop 関連の状態。
    // GameLoop の編集用データ。
    GameLoopAsset m_gameLoopAsset;

    // GameFlowEditor の Load を通して実行登録されたかどうか。
    bool m_gameLoopAssetRegistered = false;

    // 実行登録された GameFlow ファイルパス。
    std::filesystem::path m_gameLoopAssetPath;

    // シーンロードをまたいで保持される GameLoop の実行状態。
    GameLoopRuntime m_gameLoopRuntime;

    // GameLoop の永続入力用 Entity を保持する Registry。
    Registry m_gameLoopRegistry;

    // 1 フレーム分の UI ボタンクリックイベントキュー。
    UIButtonClickEventQueue m_uiButtonClickQueue;

    // 1 フレーム分の GameFlow イベントキュー。
    FlowEventQueue m_flowEventQueue;

    // GameLoop の入力オーナー Entity を作成済みかどうか。
    bool m_gameLoopInputOwnerInitialized = false;

    // Play を押した時点の Editor シーンパス。Stop 時に復元する。
    std::string m_savedEditorScenePath;
};
