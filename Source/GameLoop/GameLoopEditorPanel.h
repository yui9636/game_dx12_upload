#pragma once

#include <filesystem>
#include <memory>
#include "GameLoopAsset.h"

class GameLoopEditorPanelInternal;

class GameLoopEditorPanel
{
public:
    GameLoopEditorPanel();
    ~GameLoopEditorPanel();

    GameLoopEditorPanel(const GameLoopEditorPanel&) = delete;
    GameLoopEditorPanel& operator=(const GameLoopEditorPanel&) = delete;
    GameLoopEditorPanel(GameLoopEditorPanel&&) = delete;
    GameLoopEditorPanel& operator=(GameLoopEditorPanel&&) = delete;

    void Draw(bool* p_open = nullptr, bool* outFocused = nullptr);

    const std::filesystem::path& GetCurrentPath() const;
    void SetCurrentPath(const std::filesystem::path& p);

    // ---- Automation API ----
    GameLoopAsset&       GetAsset();
    const GameLoopAsset& GetAsset() const;

    bool     IsDirty()    const;
    void     MarkDirty();
    void     RequestFitGraph();

    uint32_t GetSelectedNodeId()       const;
    uint32_t GetSelectedTransitionId() const;
    void     SelectNodeById(uint32_t id);
    void     SelectTransitionById(uint32_t id);
    void     ClearSelection();

    uint32_t AddSceneNodeAutomation(const std::string& scenePath, const DirectX::XMFLOAT2& pos);
    uint32_t AddFlowNodeAutomation(GameLoopNodeType type, const std::string& name, const DirectX::XMFLOAT2& pos);
    uint32_t AddTransitionAutomation(uint32_t from, uint32_t to);
    void     DeleteNodeAutomation(uint32_t id);
    void     DeleteTransitionByIdAutomation(uint32_t id);

    void     ReverseTransitionByIdAutomation(uint32_t id);
    void     ValidateAutomation();
    void     SaveAutomation();
    void     LoadAutomation(const std::filesystem::path& path);

    const GameLoopValidateResult& GetValidateResult() const;
    const std::string&            GetStatusMessage()  const;

private:
    std::unique_ptr<GameLoopEditorPanelInternal> m_internal;
};
