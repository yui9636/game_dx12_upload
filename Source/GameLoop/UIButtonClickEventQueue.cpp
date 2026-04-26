#include "UIButtonClickEventQueue.h"

#include <algorithm>

void UIButtonClickEventQueue::Push(const std::string& buttonId)
{
    if (buttonId.empty()) return;
    m_clickedButtonIds.push_back(buttonId);
}

bool UIButtonClickEventQueue::Contains(const std::string& buttonId) const
{
    if (buttonId.empty()) return false;
    return std::find(m_clickedButtonIds.begin(), m_clickedButtonIds.end(), buttonId)
        != m_clickedButtonIds.end();
}

void UIButtonClickEventQueue::Clear()
{
    m_clickedButtonIds.clear();
}
