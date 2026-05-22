#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class EngineKernel;
class WebSocketServer;

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
};
