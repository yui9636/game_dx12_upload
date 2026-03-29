#pragma once
#include "EngineTime.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderPipeline.h"
#include "RenderContext/RenderQueue.h"
#include "ReflectionProbe/ReflectionProbeBaker.h"
#include "Render/OffscreenRenderer.h"
#include "Grid/GridRenderSystem.h"
#include "Registry/Registry.h"
#include <memory>

class GameLayer;
class EditorLayer;

enum class EngineMode { Editor, Play, Pause };

class EngineKernel
{
    friend class Framework;
private:
    EngineKernel() = default;
    ~EngineKernel() = default;

public:
    static EngineKernel& Instance();

    void Update(float rawDt);
    void Render();

    void Play();
    void Stop();
    void Pause();
    void Step();
    void ResetRenderStateForSceneChange();

    EngineMode GetMode() const { return mode; }
    const EngineTime& GetTime() const { return time; }

private:
    void Initialize();
    void Finalize();

    EngineTime time;
    EngineMode mode = EngineMode::Editor;
    bool m_stepFrameRequested = false;

    std::unique_ptr<RenderPipeline> m_renderPipeline;
    std::unique_ptr<ReflectionProbeBaker> m_probeBaker;
    RenderQueue m_renderQueue;
    GridRenderSystem m_editorGridRenderSystem;

    std::unique_ptr<GameLayer> m_gameLayer;
    std::unique_ptr<EditorLayer> m_editorLayer;

    // Shared offscreen renderer for thumbnails / material preview
    std::unique_ptr<OffscreenRenderer> m_sharedOffscreen;

    Registry m_emptyRegistry;
};
