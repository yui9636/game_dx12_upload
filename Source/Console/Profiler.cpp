#include "Profiler.h"

// ---------------------------------------------------------
// Profiler本体の実装
// ---------------------------------------------------------
Profiler& Profiler::Instance() {
    static Profiler instance;
    return instance;
}

void Profiler::PushResult(const std::string& name, float timeMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_results.push_back({ name, timeMs });
}

void Profiler::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_results.clear();
}

// ---------------------------------------------------------
// ScopedTimerの実装
// ---------------------------------------------------------
ScopedTimer::ScopedTimer(const std::string& name) : m_name(name) {
    // コンストラクタが呼ばれた瞬間の時間を記録
    m_start = std::chrono::high_resolution_clock::now();
}

ScopedTimer::~ScopedTimer() {
    // デストラクタが呼ばれた（スコープを抜けた）瞬間の時間を記録し、差分を計算
    auto end = std::chrono::high_resolution_clock::now();
    float ms = std::chrono::duration<float, std::milli>(end - m_start).count();

    // プロファイラーに結果を送信
    Profiler::Instance().PushResult(m_name, ms);
}