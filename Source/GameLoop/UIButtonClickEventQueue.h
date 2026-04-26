#pragma once
#include <string>
#include <vector>

// 1 frame 分の UI Button click event を蓄えるキュー。
// GameLayer::Update 末尾で UIButtonClickSystem が Push し、
// GameLoopSystem が Contains で参照、frame 末尾で EngineKernel が Clear する。
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
