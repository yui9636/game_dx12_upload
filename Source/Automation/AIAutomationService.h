#pragma once

#include <filesystem>

class EngineKernel;

class AIAutomationService
{
public:
    void Initialize();
    void Finalize();
    void ProcessPendingCommands(EngineKernel& kernel);

private:
    std::filesystem::path m_rootDir;
    std::filesystem::path m_commandsDir;
    std::filesystem::path m_processingDir;
    std::filesystem::path m_resultsDir;
    std::filesystem::path m_screenshotsDir;
    std::filesystem::path m_stateDir;
};
