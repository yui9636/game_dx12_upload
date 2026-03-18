#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <mutex>

// 計測結果1つ分のデータ
struct ProfileResult {
    std::string name;
    float timeMs;
};

// ==========================================================
// プロファイラー本体（シングルトン）
// ==========================================================
class Profiler {
public:
    static Profiler& Instance();

    // 計測結果を保存（マルチスレッド対応）
    void PushResult(const std::string& name, float timeMs);

    // 毎フレームの終わりに結果をクリアする
    void Clear();

    // UI描画用に結果を取得
    const std::vector<ProfileResult>& GetResults() const { return m_results; }

private:
    Profiler() = default;
    ~Profiler() = default;

    std::vector<ProfileResult> m_results;
    mutable std::mutex m_mutex; // DX12化時のマルチスレッド記録に必須の鍵
};

// ==========================================================
// スコープタイマー（作成された瞬間から破棄されるまでの時間を自動計測）
// ==========================================================
class ScopedTimer {
public:
    ScopedTimer(const std::string& name);
    ~ScopedTimer();
private:
    std::string m_name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

// 実際のコードで使うマクロ
// 例: PROFILE_SCOPE("GBufferPass");
#define PROFILE_SCOPE(name) ScopedTimer timer##__LINE__(name)