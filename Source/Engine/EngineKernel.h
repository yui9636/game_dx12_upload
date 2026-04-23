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
#include <memory>

class GameLayer;
class EditorLayer;

// エンジン全体の中核を担当するカーネルクラス。
// 更新・描画・入力取得・実行モード管理・各主要サブシステムの保持を行う。
class EngineKernel
{
    // Framework からのみ初期化・終了処理へアクセスさせる。
    friend class Framework;

private:
    // singleton 用なのでコンストラクタは private。
    EngineKernel();

    // singleton 用なのでデストラクタも private。
    ~EngineKernel();

public:
    // singleton インスタンスを返す。
    static EngineKernel& Instance();

    // 1 フレームぶんの更新処理を行う。
    void Update(float rawDt);

    // 1 フレームぶんの描画処理を行う。
    void Render();

    // 入力デバイスからイベントを収集する。
    void PollInput();

    // Editor / Pause 状態から Play を開始する。
    void Play();

    // Play / Pause を停止して Editor へ戻す。
    void Stop();

    // Play と Pause を切り替える。
    void Pause();

    // Pause 中に 1 フレームだけ進める。
    void Step();

    // シーン切り替え時に描画関連状態を安全にリセットする。
    void ResetRenderStateForSceneChange();

    // 現在の実行モードを返す。
    EngineMode GetMode() const { return mode; }

    // 現在のエンジン時間情報を返す。
    const EngineTime& GetTime() const { return time; }

    // AudioWorldSystem 参照を返す。
    AudioWorldSystem& GetAudioWorld() { return *m_audioWorld; }

    // const 版 AudioWorldSystem 参照を返す。
    const AudioWorldSystem& GetAudioWorld() const { return *m_audioWorld; }

    // 入力バックエンド参照を返す。
    IInputBackend& GetInputBackend() { return *m_inputBackend; }

    // 今フレーム収集した入力イベントキューを返す。
    const InputEventQueue& GetInputEventQueue() const { return m_inputQueue; }

private:
    // エンジン全体を初期化する。
    void Initialize();

    // エンジン全体を終了処理する。
    void Finalize();

    // PlayerEditor 用のプレビューを offscreen 描画する。
    void RenderPlayerPreviewOffscreen();

    // 時間管理情報。
    EngineTime time;

    // 現在の実行モード。
    EngineMode mode = EngineMode::Editor;

    // Pause 中に 1 フレームだけ進める要求フラグ。
    bool m_stepFrameRequested = false;

    // 描画パイプライン本体。
    std::unique_ptr<RenderPipeline> m_renderPipeline;

    // ReflectionProbe ベイク担当。
    std::unique_ptr<ReflectionProbeBaker> m_probeBaker;

    // フレーム中の描画パケットを集めるキュー。
    RenderQueue m_renderQueue;

    // Editor 用グリッド描画システム。
    GridRenderSystem m_editorGridRenderSystem;

    // オーディオワールド更新システム。
    std::unique_ptr<AudioWorldSystem> m_audioWorld;

    // ゲーム本編レイヤー。
    std::unique_ptr<GameLayer> m_gameLayer;

    // エディタレイヤー。
    std::unique_ptr<EditorLayer> m_editorLayer;

    // サムネイルや各種プレビュー用の共有 offscreen renderer。
    std::unique_ptr<OffscreenRenderer> m_sharedOffscreen;

    // 入力バックエンド本体。
    std::unique_ptr<IInputBackend> m_inputBackend;

    // 今フレームの入力イベント蓄積先。
    InputEventQueue m_inputQueue;

    // GameLayer が無い時の代替用空 Registry。
    Registry m_emptyRegistry;
};