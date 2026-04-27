#pragma once
#include <string>
#include <vector>

// Per-frame UI button click queue.
// UIButtonClickSystem pushes, GameLoopSystem reads, EngineKernel clears at end-of-frame.
class UIButtonClickEventQueue
{
public:
    void Push(const std::string& buttonId);

    bool Contains(const std::string& buttonId) const;

    void Clear();

    const std::vector<std::string>& GetAll() const { return m_clickedButtonIds; }

private:
    std::vector<std::string> m_clickedButtonIds;
};
