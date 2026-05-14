#pragma once
#include <string>
#include <vector>

// 1 frame 分の UI button click queue。
// UIButtonClickSystem が積み、GameLoopSystem が読み、EngineKernel が frame 終端で clear する。
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
