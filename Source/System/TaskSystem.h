#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class TaskSystem
{
public:
    struct TaskGraphNode
    {
        std::function<void()> task;
        std::vector<size_t> dependencies;
    };

    static TaskSystem& Instance()
    {
        static TaskSystem instance;
        return instance;
    }

    size_t GetWorkerCount() const
    {
        return m_workers.size();
    }

    template<typename Func>
    void ParallelFor(size_t itemCount, size_t grainSize, Func&& func)
    {
        if (itemCount == 0) {
            return;
        }

        const size_t effectiveGrain = (std::max<size_t>)(1, grainSize);
        const size_t chunkCount = (itemCount + effectiveGrain - 1) / effectiveGrain;
        if (m_workers.empty() || chunkCount <= 1) {
            for (size_t i = 0; i < itemCount; ++i) {
                func(i);
            }
            return;
        }

        auto counter = std::make_shared<CompletionCounter>(chunkCount);
        for (size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex) {
            const size_t begin = chunkIndex * effectiveGrain;
            const size_t end = (std::min)(itemCount, begin + effectiveGrain);
            Enqueue([begin, end, func, counter]() mutable {
                for (size_t i = begin; i < end; ++i) {
                    func(i);
                }
                counter->Signal();
            });
        }
        counter->Wait();
    }

    void RunTaskGraph(const std::vector<TaskGraphNode>& nodes)
    {
        const size_t nodeCount = nodes.size();
        if (nodeCount == 0) {
            return;
        }

        if (m_workers.empty() || nodeCount == 1) {
            std::vector<bool> finished(nodeCount, false);
            size_t finishedCount = 0;
            while (finishedCount < nodeCount) {
                bool progressed = false;
                for (size_t i = 0; i < nodeCount; ++i) {
                    if (finished[i]) {
                        continue;
                    }

                    bool ready = true;
                    for (size_t dep : nodes[i].dependencies) {
                        if (dep >= nodeCount || !finished[dep]) {
                            ready = false;
                            break;
                        }
                    }

                    if (!ready) {
                        continue;
                    }

                    if (nodes[i].task) {
                        nodes[i].task();
                    }
                    finished[i] = true;
                    ++finishedCount;
                    progressed = true;
                }

                if (!progressed) {
                    break;
                }
            }
            return;
        }

        struct SharedState
        {
            explicit SharedState(size_t count)
                : remainingDependencies(count)
            {
            }

            std::vector<std::atomic<int>> remainingDependencies;
            std::vector<std::vector<size_t>> dependents;
            std::mutex completionMutex;
            std::condition_variable completionCv;
            std::atomic<size_t> completedCount = 0;
        };

        auto state = std::make_shared<SharedState>(nodeCount);
        state->dependents.resize(nodeCount);

        for (size_t i = 0; i < nodeCount; ++i) {
            state->remainingDependencies[i].store(static_cast<int>(nodes[i].dependencies.size()), std::memory_order_relaxed);
            for (size_t dep : nodes[i].dependencies) {
                if (dep < nodeCount) {
                    state->dependents[dep].push_back(i);
                }
            }
        }

        auto scheduleNode = [this, state, &nodes](auto&& self, size_t index) -> void {
            Enqueue([this, state, &nodes, index, self]() mutable {
                if (nodes[index].task) {
                    nodes[index].task();
                }

                for (size_t dependent : state->dependents[index]) {
                    if (state->remainingDependencies[dependent].fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        self(self, dependent);
                    }
                }

                const size_t completed = state->completedCount.fetch_add(1, std::memory_order_acq_rel) + 1;
                if (completed == nodes.size()) {
                    std::lock_guard<std::mutex> lock(state->completionMutex);
                    state->completionCv.notify_all();
                }
            });
        };

        for (size_t i = 0; i < nodeCount; ++i) {
            if (state->remainingDependencies[i].load(std::memory_order_acquire) == 0) {
                scheduleNode(scheduleNode, i);
            }
        }

        std::unique_lock<std::mutex> lock(state->completionMutex);
        state->completionCv.wait(lock, [&]() {
            return state->completedCount.load(std::memory_order_acquire) >= nodeCount;
        });
    }

private:
    struct CompletionCounter
    {
        explicit CompletionCounter(size_t targetCount)
            : target(targetCount)
        {
        }

        void Signal()
        {
            const size_t value = completed.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (value >= target) {
                std::lock_guard<std::mutex> lock(mutex);
                cv.notify_all();
            }
        }

        void Wait()
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&]() {
                return completed.load(std::memory_order_acquire) >= target;
            });
        }

        const size_t target;
        std::atomic<size_t> completed = 0;
        std::mutex mutex;
        std::condition_variable cv;
    };

    TaskSystem()
    {
        const unsigned int hw = std::thread::hardware_concurrency();
        const size_t workerCount = hw > 2 ? static_cast<size_t>(hw - 1) : 0;
        m_workers.reserve(workerCount);
        for (size_t i = 0; i < workerCount; ++i) {
            m_workers.emplace_back([this]() { WorkerLoop(); });
        }
    }

    ~TaskSystem()
    {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_stopping = true;
        }
        m_queueCv.notify_all();
        for (std::thread& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    TaskSystem(const TaskSystem&) = delete;
    TaskSystem& operator=(const TaskSystem&) = delete;

    void Enqueue(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_tasks.push(std::move(task));
        }
        m_queueCv.notify_one();
    }

    void WorkerLoop()
    {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueCv.wait(lock, [&]() {
                    return m_stopping || !m_tasks.empty();
                });

                if (m_stopping && m_tasks.empty()) {
                    return;
                }

                task = std::move(m_tasks.front());
                m_tasks.pop();
            }

            if (task) {
                task();
            }
        }
    }

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    bool m_stopping = false;
};
