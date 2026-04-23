#include <windows.h>
#include <stdio.h>
#include <cstdarg>
#include <fstream>
#include "Logger.h"

// ログ出力先ファイルのパスを返す。
// 実行ディレクトリ配下の Saved/Logs/runtime.log を使う。
std::filesystem::path Logger::GetLogFilePath() const
{
    // Saved/Logs ディレクトリを作成する。
    auto dir = std::filesystem::current_path() / "Saved" / "Logs";
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

    // Visual Studio の出力ウィンドウへ送る。
    ::OutputDebugStringA(vsOutput.c_str());

    // 複数スレッドから安全に書けるようロックする。
    std::lock_guard<std::mutex> lock(m_mutex);

    // 初回だけログファイルを空にしてリセットする。
    static bool fileReset = false;
    const std::filesystem::path logFilePath = GetLogFilePath();
    if (!fileReset) {
        std::ofstream resetFile(logFilePath, std::ios::out | std::ios::trunc);
        fileReset = true;
    }

    // 以降は追記モードでファイルへ書き込む。
    std::ofstream file(logFilePath, std::ios::out | std::ios::app);
    if (file.is_open()) {
        file << vsOutput;
    }

    // メモリ上のログ履歴にも保存する。
    m_logs.push_back({ level, finalMessage });
}