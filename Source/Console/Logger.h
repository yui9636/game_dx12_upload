#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

// ??????????
enum class LogLevel {
    Info,
    Warning,
    Error
};

// ImGui???????????
struct LogEntry {
    LogLevel level;
    std::string message;
};

class Logger
{
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    // ?????ImGui??VS????????????????
    void Print(LogLevel level, const char* format, ...);

    // ImGui?????????????????????
    const std::vector<LogEntry>& GetLogs() { return m_logs; }
    void ClearLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.clear();
    }

private:
    Logger() = default;
    ~Logger() = default;

    std::filesystem::path GetLogFilePath() const;

    std::vector<LogEntry> m_logs; // ???????
    std::mutex m_mutex;           // ???????????
};

// ==========================================================
// ?????????????Info, Warn, Error????????
// ==========================================================
#if defined(_DEBUG)
#define LOG_INFO(...)  { Logger::Instance().Print(LogLevel::Info, __VA_ARGS__); }
#define LOG_WARN(...)  { Logger::Instance().Print(LogLevel::Warning, __VA_ARGS__); }
#define LOG_ERROR(...) { Logger::Instance().Print(LogLevel::Error, __VA_ARGS__); }
#else
#define LOG_INFO(...)  {}
#define LOG_WARN(...)  {}
#define LOG_ERROR(...) {}
#endif
