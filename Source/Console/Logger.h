#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

// ログの重要度を表す列挙型。
enum class LogLevel {
    Info,     // 通常情報
    Warning,  // 警告
    Error     // エラー
};

// 1件ぶんのログ情報。
// ImGui 上のログビューなどで表示するために使う。
struct LogEntry {
    // ログの種類。
    LogLevel level;

    // ログ本文。
    std::string message;
};

// ログ出力を管理する singleton クラス。
// Visual Studio 出力ウィンドウ、ログファイル、ImGui 表示用履歴の3か所を管理する。
class Logger
{
public:
    // singleton インスタンスを返す。
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    // 可変長引数付きでログを出力する。
    // ImGui 履歴、VS 出力、ログファイルへ同時に書き込む。
    void Print(LogLevel level, const char* format, ...);

    // 現在保持しているログ履歴を返す。
    const std::vector<LogEntry>& GetLogs() { return m_logs; }

    // 保持中のログ履歴を消去する。
    void ClearLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.clear();
    }

private:
    // singleton 用なのでコンストラクタは private。
    Logger() = default;

    // 特別な破棄処理は不要。
    ~Logger() = default;

    // 実際のログファイルパスを返す。
    std::filesystem::path GetLogFilePath() const;

    // メモリ上に保持しているログ履歴。
    std::vector<LogEntry> m_logs;

    // 複数スレッドからの同時書き込みを守るための mutex。
    std::mutex m_mutex;
};

// ==========================================================
// デバッグビルド時だけ有効なログ出力マクロ。
// Info / Warn / Error の3種類を簡単に呼べるようにする。
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