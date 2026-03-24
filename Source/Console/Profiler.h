#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <mutex>

struct ProfileResult {
    std::string name;
    float timeMs;
};

// ==========================================================
// ==========================================================
class Profiler {
public:
    static Profiler& Instance();

    void PushResult(const std::string& name, float timeMs);

    void Clear();

    const std::vector<ProfileResult>& GetResults() const { return m_results; }

private:
    Profiler() = default;
    ~Profiler() = default;

    std::vector<ProfileResult> m_results;
    mutable std::mutex m_mutex;
};

// ==========================================================
// ==========================================================
class ScopedTimer {
public:
    ScopedTimer(const std::string& name);
    ~ScopedTimer();
private:
    std::string m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

#define PROFILE_SCOPE(name) ScopedTimer timer##__LINE__(name)
