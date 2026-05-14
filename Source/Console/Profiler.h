#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <mutex>

// 1 つの計測結果を表すデータです。
struct ProfileResult {
    // 計測対象の名前です。
    std::string name;

    // 計測にかかった時間です。単位はミリ秒です。
    float timeMs;
};
// フレーム中の処理時間を収集する簡易プロファイラーです。
class Profiler {
public:
    // Profiler の唯一のインスタンスを取得します。
    static Profiler& Instance();

    // 計測結果を追加します。
    void PushResult(const std::string& name, float timeMs);

    // 蓄積された計測結果をすべて削除します。
    void Clear();

    // 現在蓄積されている計測結果一覧を取得します。
    const std::vector<ProfileResult>& GetResults() const { return m_results; }

private:
    // シングルトンとして使うため、外部からの生成を禁止します。
    Profiler() = default;

    // シングルトンとして使うため、外部からの破棄を禁止します。
    ~Profiler() = default;

    // 記録された計測結果一覧です。
    std::vector<ProfileResult> m_results;

    // 複数スレッドから計測結果が追加されても壊れないように保護します。
    mutable std::mutex m_mutex;
};
// スコープに入ってから抜けるまでの時間を自動計測する RAII タイマーです。
class ScopedTimer {
public:
    // 計測名を受け取り、作成時点の時刻を記録します。
    ScopedTimer(const std::string& name);

    // 破棄時に経過時間を計算し、Profiler へ結果を登録します。
    ~ScopedTimer();
private:
    // 計測対象の名前です。
    std::string m_name;

    // 計測開始時刻です。
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

// 現在のスコープを簡単に計測するためのマクロです。
#define PROFILE_SCOPE(name) ScopedTimer timer##__LINE__(name)
