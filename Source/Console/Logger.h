#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <filesystem>
#include <cstdint>

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

    // `[Category] message` 形式から抽出したカテゴリ。無い場合は空文字列。
    std::string category;

    // Unix epoch milliseconds。
    int64_t timestampMs = 0;

    // プロセス内で単調増加するログ番号。
    uint64_t sequence = 0;
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

    // 現在保持しているログ履歴を返す (非 lock; 単一スレッド使用のみ安全)。
    // 旧 API。後方互換のため残すが、別スレッドから Print が呼ばれる環境では使わないこと。
    const std::vector<LogEntry>& GetLogs() { return m_logs; }

    // Thread-safe: 現在保持しているログ履歴のスナップショットを返す。
    // 戻り値は copy なので、別スレッドが Print してもイテレートが壊れない。
    std::vector<LogEntry> SnapshotLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_logs;
    }

    // Thread-safe: 指定 sequence より後の entry だけコピーする (cursor pull 用)。
    std::vector<LogEntry> SnapshotLogsSince(uint64_t minExclusiveSequence) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<LogEntry> out;
        out.reserve(m_logs.size());
        for (const auto& e : m_logs) {
            if (e.sequence > minExclusiveSequence) out.push_back(e);
        }
        return out;
    }

    // Thread-safe: 現在の最新 sequence を返す。pull cursor の初期化に使う。
    uint64_t GetLatestSequence() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return (m_nextSequence == 0) ? 0 : (m_nextSequence - 1);
    }

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

    // ログ履歴内の単調増加 sequence。
    uint64_t m_nextSequence = 1;

    // メモリ上 m_logs / m_nextSequence の同時アクセス保護。短時間の lock のみ。
    std::mutex m_mutex;

    // ファイル I/O 専用 mutex。Snapshot 系が file 書き込み中に待たされないよう分離。
    std::mutex m_fileMutex;
};
// デバッグビルド時だけ有効なログ出力マクロ。
// Info / Warn / Error の3種類を簡単に呼べるようにする。
#if defined(_DEBUG)
#define LOG_INFO(...)  { Logger::Instance().Print(LogLevel::Info, __VA_ARGS__); }
#define LOG_WARN(...)  { Logger::Instance().Print(LogLevel::Warning, __VA_ARGS__); }
#define LOG_ERROR(...) { Logger::Instance().Print(LogLevel::Error, __VA_ARGS__); }
#else
#define LOG_INFO(...)  {}
#define LOG_WARN(...)  {}
#define LOG_ERROR(...) {}
#endif
