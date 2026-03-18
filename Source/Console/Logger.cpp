#include <windows.h>
#include <stdio.h>
#include <cstdarg>
#include <fstream>
#include "Logger.h"

std::filesystem::path Logger::GetLogFilePath() const
{
    auto dir = std::filesystem::current_path() / "Saved" / "Logs";
    std::filesystem::create_directories(dir);
    return dir / "runtime.log";
}

void Logger::Print(LogLevel level, const char* format, ...)
{
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    std::string finalMessage = message;
    if (finalMessage.empty() || finalMessage.back() != '\n') {
        finalMessage += "\n";
    }

    std::string vsOutput;
    switch (level) {
    case LogLevel::Info:    vsOutput = "[INFO] " + finalMessage; break;
    case LogLevel::Warning: vsOutput = "[WARN] " + finalMessage; break;
    case LogLevel::Error:   vsOutput = "[ERROR] " + finalMessage; break;
    }
    ::OutputDebugStringA(vsOutput.c_str());

    std::lock_guard<std::mutex> lock(m_mutex);

    static bool fileReset = false;
    const std::filesystem::path logFilePath = GetLogFilePath();
    if (!fileReset) {
        std::ofstream resetFile(logFilePath, std::ios::out | std::ios::trunc);
        fileReset = true;
    }

    std::ofstream file(logFilePath, std::ios::out | std::ios::app);
    if (file.is_open()) {
        file << vsOutput;
    }

    m_logs.push_back({ level, finalMessage });
}
