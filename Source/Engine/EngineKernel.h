#pragma once
#include "EngineTime.h"
#include "RenderContext/RenderContext.h"
#include "RenderContext/RenderPipeline.h"
#include "RenderContext/RenderQueue.h"
#include "ReflectionProbe/ReflectionProbeBaker.h"
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

    EngineMode GetMode() const { return mode; }
    const EngineTime& GetTime() const { return time; }

private:
    void Initialize();
    void Finalize();

    EngineTime time;
    EngineMode mode = EngineMode::Editor;

    std::unique_ptr<RenderPipeline> m_renderPipeline;
    std::unique_ptr<ReflectionProbeBaker> m_probeBaker;
    RenderQueue m_renderQueue; // �p�C�v���C���ɓn���`�[�̑�

    // ���ɂ�2�w�A�[�L�e�N�`��
    std::unique_ptr<GameLayer> m_gameLayer;
    std::unique_ptr<EditorLayer> m_editorLayer;

    // DX12テスト用: GameLayerが存在しない場合の空Registry
    Registry m_emptyRegistry;
};