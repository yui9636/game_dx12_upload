#include "Profiler.h"

// ---------------------------------------------------------
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
// ---------------------------------------------------------
ScopedTimer::ScopedTimer(const std::string& name) : m_name(name) {
    m_start = std::chrono::high_resolution_clock::now();
}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    float ms = std::chrono::duration<float, std::milli>(end - m_start).count();

    Profiler::Instance().PushResult(m_name, ms);
}
