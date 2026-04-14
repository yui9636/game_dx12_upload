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

class EngineKernel
{
    friend class Framework;
private:
    EngineKernel();
    ~EngineKernel();

public:
    static EngineKernel& Instance();

    void Update(float rawDt);
    void Render();
    void PollInput();

    void Play();
    void Stop();
    void Pause();
    void Step();
    void ResetRenderStateForSceneChange();

    EngineMode GetMode() const { return mode; }
    const EngineTime& GetTime() const { return time; }
    AudioWorldSystem& GetAudioWorld() { return *m_audioWorld; }
    const AudioWorldSystem& GetAudioWorld() const { return *m_audioWorld; }

    IInputBackend& GetInputBackend() { return *m_inputBackend; }
    const InputEventQueue& GetInputEventQueue() const { return m_inputQueue; }

private:
    void Initialize();
    void Finalize();
    void RenderPlayerPreviewOffscreen();

    EngineTime time;
    EngineMode mode = EngineMode::Editor;
    bool m_stepFrameRequested = false;

    std::unique_ptr<RenderPipeline> m_renderPipeline;
    std::unique_ptr<ReflectionProbeBaker> m_probeBaker;
    RenderQueue m_renderQueue;
    GridRenderSystem m_editorGridRenderSystem;
    std::unique_ptr<AudioWorldSystem> m_audioWorld;

    std::unique_ptr<GameLayer> m_gameLayer;
    std::unique_ptr<EditorLayer> m_editorLayer;

    // Shared offscreen renderer for thumbnails / previews
    std::unique_ptr<OffscreenRenderer> m_sharedOffscreen;

    // Input system
    std::unique_ptr<IInputBackend> m_inputBackend;
    InputEventQueue m_inputQueue;

    Registry m_emptyRegistry;
};
