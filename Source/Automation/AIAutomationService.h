#pragma once

#include <filesystem>
#include <memory>

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
    std::unique_ptr<WebSocketServer> m_webSocketServer;

    std::filesystem::path m_rootDir;
    std::filesystem::path m_commandsDir;
    std::filesystem::path m_processingDir;
    std::filesystem::path m_resultsDir;
    std::filesystem::path m_screenshotsDir;
    std::filesystem::path m_stateDir;
};
