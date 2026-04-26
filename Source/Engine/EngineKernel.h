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
#include "GameLoop/GameLoopRuntime.h"
#include "GameLoop/UIButtonClickEventQueue.h"
#include <memory>

class GameLayer;
class EditorLayer;

// �G���W���S�̂̒��j��S������J�[�l���N���X�B
// �X�V�E�`��E���͎擾�E���s���[�h�Ǘ��E�e��v�T�u�V�X�e���̕ێ����s���B
class EngineKernel
{
    // Framework ����̂ݏ������E�I�������փA�N�Z�X������B
    friend class Framework;

private:
    // singleton �p�Ȃ̂ŃR���X�g���N�^�� private�B
    EngineKernel();

    // singleton �p�Ȃ̂Ńf�X�g���N�^�� private�B
    ~EngineKernel();

public:
    // singleton �C���X�^���X��Ԃ��B
    static EngineKernel& Instance();

    // 1 �t���[���Ԃ�̍X�V�������s���B
    void Update(float rawDt);

    // 1 �t���[���Ԃ�̕`�揈�����s���B
    void Render();

    // ���̓f�o�C�X����C�x���g�����W����B
    void PollInput();

    // Editor / Pause ��Ԃ��� Play ���J�n����B
    void Play();

    // Play / Pause ���~���� Editor �֖߂��B
    void Stop();

    // Play �� Pause ��؂�ւ���B
    void Pause();

    // Pause ���� 1 �t���[�������i�߂�B
    void Step();

    // �V�[���؂�ւ����ɕ`��֘A��Ԃ����S�Ƀ��Z�b�g����B
    void ResetRenderStateForSceneChange();

    // ���݂̎��s���[�h��Ԃ��B
    EngineMode GetMode() const { return mode; }

    // ���݂̃G���W�����ԏ���Ԃ��B
    const EngineTime& GetTime() const { return time; }

    // AudioWorldSystem �Q�Ƃ�Ԃ��B
    AudioWorldSystem& GetAudioWorld() { return *m_audioWorld; }

    // const �� AudioWorldSystem �Q�Ƃ�Ԃ��B
    const AudioWorldSystem& GetAudioWorld() const { return *m_audioWorld; }

    // ���̓o�b�N�G���h�Q�Ƃ�Ԃ��B
    IInputBackend& GetInputBackend() { return *m_inputBackend; }

    // GameLoop authoring data (scene transition graph).
    GameLoopAsset& GetGameLoopAsset() { return m_gameLoopAsset; }
    const GameLoopAsset& GetGameLoopAsset() const { return m_gameLoopAsset; }

    // GameLoop runtime state (persistent across scene loads).
    GameLoopRuntime& GetGameLoopRuntime() { return m_gameLoopRuntime; }
    const GameLoopRuntime& GetGameLoopRuntime() const { return m_gameLoopRuntime; }

    // Registry that owns the GameLoop persistent input owner entity.
    Registry& GetGameLoopRegistry() { return m_gameLoopRegistry; }

    // Per-frame UI button click queue (cleared at end of frame).
    UIButtonClickEventQueue& GetUIButtonClickQueue() { return m_uiButtonClickQueue; }

    // ���t���[�����W�������̓C�x���g�L���[��Ԃ��B
    const InputEventQueue& GetInputEventQueue() const { return m_inputQueue; }

private:
    // �G���W���S�̂�����������B
    void Initialize();

    // �G���W���S�̂��I����������B
    void Finalize();

    // PlayerEditor �p�̃v���r���[�� offscreen �`�悷��B
    void RenderPlayerPreviewOffscreen();

    // ���ԊǗ����B
    EngineTime time;

    // ���݂̎��s���[�h�B
    EngineMode mode = EngineMode::Editor;

    // Pause ���� 1 �t���[�������i�߂�v���t���O�B
    bool m_stepFrameRequested = false;

    // �`��p�C�v���C���{�́B
    std::unique_ptr<RenderPipeline> m_renderPipeline;

    // ReflectionProbe �x�C�N�S���B
    std::unique_ptr<ReflectionProbeBaker> m_probeBaker;

    // �t���[�����̕`��p�P�b�g���W�߂�L���[�B
    RenderQueue m_renderQueue;

    // Editor �p�O���b�h�`��V�X�e���B
    GridRenderSystem m_editorGridRenderSystem;

    // �I�[�f�B�I���[���h�X�V�V�X�e���B
    std::unique_ptr<AudioWorldSystem> m_audioWorld;

    // �Q�[���{�҃��C���[�B
    std::unique_ptr<GameLayer> m_gameLayer;

    // �G�f�B�^���C���[�B
    std::unique_ptr<EditorLayer> m_editorLayer;

    // �T���l�C����e��v���r���[�p�̋��L offscreen renderer�B
    std::unique_ptr<OffscreenRenderer> m_sharedOffscreen;

    // ���̓o�b�N�G���h�{�́B
    std::unique_ptr<IInputBackend> m_inputBackend;

    // ���t���[���̓��̓C�x���g�~�ϐ�B
    InputEventQueue m_inputQueue;

    // GameLayer ���������̑�֗p�� Registry�B
    Registry m_emptyRegistry;

    // ---- GameLoop ----
    // GameLoop authoring data.
    GameLoopAsset m_gameLoopAsset;

    // GameLoop runtime state (persistent across scene loads).
    GameLoopRuntime m_gameLoopRuntime;

    // Registry hosting the GameLoop persistent input owner entity.
    Registry m_gameLoopRegistry;

    // Per-frame UI button click queue.
    UIButtonClickEventQueue m_uiButtonClickQueue;

    // True once GameLoop input owner entity has been created.
    bool m_gameLoopInputOwnerInitialized = false;

    // Editor scene path at the moment Play was pressed (restored on Stop).
    std::string m_savedEditorScenePath;
};