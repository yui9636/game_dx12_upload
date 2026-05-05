#include "Profiler.h"

// ---------------------------------------------------------
// Profiler の唯一のインスタンスを返します。
// ---------------------------------------------------------
Profiler& Profiler::Instance() {
    static Profiler instance;
    return instance;
}

// 計測結果をスレッドセーフに追加します。
void Profiler::PushResult(const std::string& name, float timeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_results.push_back({ name, timeMs });
}

// 蓄積された計測結果をスレッドセーフに削除します。
void Profiler::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_results.clear();
}

// ---------------------------------------------------------
// ScopedTimer
// ---------------------------------------------------------

// 計測名を保存し、現在時刻を開始時刻として記録します。
ScopedTimer::ScopedTimer(const std::string& name) : m_name(name) {
    m_start = std::chrono::high_resolution_clock::now();
}

// スコープ終了時に経過時間をミリ秒へ変換し、Profiler に登録します。
ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    float ms = std::chrono::duration<float, std::milli>(end - m_start).count();

    Profiler::Instance().PushResult(m_name, ms);
}
