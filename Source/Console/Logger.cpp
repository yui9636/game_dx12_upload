#include <windows.h>
#include <stdio.h>
#include <cstdarg>
#include <chrono>
#include <fstream>
#include <utility>
#include "Logger.h"
#include "System/PathResolver.h"

namespace
{
    int64_t NowUnixMilliseconds()
    {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }

    std::string ExtractCategory(const std::string& message)
    {
        if (message.size() < 3 || message.front() != '[') {
            return {};
        }
        const size_t close = message.find(']');
        if (close == std::string::npos || close <= 1 || close > 64) {
            return {};
        }
        const std::string category = message.substr(1, close - 1);
        if (category == "INFO" || category == "WARN" || category == "ERROR") {
            return {};
        }
        return category;
    }
}

// ログ出力先ファイルのパスを返す。
// プロジェクトルート配下の Saved/Logs/runtime.log を使う。
std::filesystem::path Logger::GetLogFilePath() const
{
    // Saved/Logs ディレクトリを作成する。
    auto dir = std::filesystem::path(PathResolver::Resolve("Saved/Logs"));
    std::filesystem::create_directories(dir);

    // 実際のログファイルパスを返す。
    return dir / "runtime.log";
}

// 可変長引数付きでログを出力する。
// OutputDebugStringA と runtime.log の両方へ書き込む。
void Logger::Print(LogLevel level, const char* format, ...)
{
    // printf 形式文字列を一時バッファへ展開する。
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // std::string に変換する。
    std::string finalMessage = message;

    // 末尾に改行が無ければ補う。
    if (finalMessage.empty() || finalMessage.back() != '\n') {
        finalMessage += "\n";
    }

    // ログレベルに応じたプレフィックス付き文字列を作る。
    std::string vsOutput;
    switch (level) {
    case LogLevel::Info:
        vsOutput = "[INFO] " + finalMessage;
        break;

    case LogLevel::Warning:
        vsOutput = "[WARN] " + finalMessage;
        break;

    case LogLevel::Error:
        vsOutput = "[ERROR] " + finalMessage;
        break;
    }

    // Visual Studio の出力ウィンドウへ送る (lock 外。OutputDebugString は thread-safe)。
    ::OutputDebugStringA(vsOutput.c_str());

    // メモリ上 entry の登録だけ短時間 lock で行う (Snapshot 系の競合を最小化)。
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        LogEntry entry;
        entry.level = level;
        entry.message = finalMessage;
        entry.category = ExtractCategory(finalMessage);
        entry.timestampMs = NowUnixMilliseconds();
        entry.sequence = m_nextSequence++;
        m_logs.push_back(std::move(entry));

        // Ring buffer cap で無制限増加を防ぐ。古い entry を先頭から落とす。
        // この削除は SnapshotLogsSince の cursor 値より古いものを切り捨てる効果。
        // 4096 件保持で、64 文字平均なら 256KB 程度。
        constexpr size_t kMaxRetained = 4096;
        if (m_logs.size() > kMaxRetained) {
            m_logs.erase(m_logs.begin(),
                         m_logs.begin() + (m_logs.size() - kMaxRetained));
        }
    }

    // ファイル I/O は別 mutex で直列化。SnapshotLogs を block しない。
    {
        std::lock_guard<std::mutex> fileLock(m_fileMutex);
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
    }
}
