#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class EngineKernel;
class WebSocketServer;

// =============================================================================
// ARCHITECTURAL RULE -- ALL EDITOR AUTOMATION
//
// All AI automation commands MUST simulate UI-level editor operations.
// Direct modification of internal editor member variables is FORBIDDEN.
//
// WHY: Every editor has render-pass gates (e.g. ShouldRenderEffectPreview,
// ShouldRenderUIEditorPreview, ...). These gates are only open when the
// corresponding workspace tab is visible and active. Writing state behind
// the gate -- e.g. camera variables, transform overrides, environment color
// -- has NO visible effect: the output texture remains black/stale and any
// screenshot taken will be wrong.
//
// CORRECT PATTERN:
//   1. Open / activate the target editor workspace first.
//   2. Issue compile / play / build commands through the editor pipeline.
//   3. Verify state via the corresponding get_state command before capture.
//   4. NEVER call *SetCamera*, *SetEnvironment*, *SetPreviewCamera*, or
//      similar "Automation" setters unless the workspace gate is confirmed
//      open, and even then prefer the higher-level editor commands.
// =============================================================================
class AIAutomationService
{
public:
    AIAutomationService();
    ~AIAutomationService();

    void Initialize();
    void Finalize();
    void ProcessPendingCommands(EngineKernel& kernel);

private:
    struct PendingEffectMultiTimeReview
    {
        bool active = false;
        std::string clientId;
        nlohmann::json command;
        nlohmann::json params;
        nlohmann::json frames = nlohmann::json::array();
        std::vector<float> times;
        size_t index = 0;
        int waitFrames = 0;
        std::string stem;
        std::filesystem::path dir;
        std::string format;
        std::string target;
    };

    bool TryStartPendingEffectMultiTimeReview(EngineKernel& kernel, const nlohmann::json& command, const std::string& clientId);
    void ProcessPendingEffectMultiTimeReview(EngineKernel& kernel);

    std::unique_ptr<WebSocketServer> m_webSocketServer;
    PendingEffectMultiTimeReview m_pendingEffectMultiTimeReview;

    std::filesystem::path m_rootDir;
    std::filesystem::path m_commandsDir;
    std::filesystem::path m_processingDir;
    std::filesystem::path m_resultsDir;
    std::filesystem::path m_screenshotsDir;
    std::filesystem::path m_stateDir;

    std::chrono::steady_clock::time_point m_lastStateWriteTime;
    std::chrono::steady_clock::time_point m_lastEcsBroadcastTime;
};
