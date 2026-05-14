#pragma once

#include <string>
#include <vector>
// FlowEvent は name/value を中心に、実行時やエディターで共有する状態を保持する。

struct FlowEvent
{
    std::string name;
    std::string value;
};

class FlowEventQueue
{
public:
    void Push(const std::string& name, const std::string& value = std::string{})
    {
        if (name.empty()) return;
        FlowEvent event{ name, value };
        m_events.push_back(event);
        m_recentEvents.push_back(event);
        if (m_recentEvents.size() > 64) {
            m_recentEvents.erase(m_recentEvents.begin(), m_recentEvents.begin() + (m_recentEvents.size() - 64));
        }
    }

    bool Contains(const std::string& name, const std::string& value = std::string{}) const
    {
        if (name.empty()) return false;
        for (const FlowEvent& event : m_events) {
            if (event.name != name) continue;
            if (value.empty() || event.value == value) return true;
        }
        return false;
    }

    void Clear()
    {
        m_events.clear();
    }

    const std::vector<FlowEvent>& GetEvents() const { return m_events; }
    const std::vector<FlowEvent>& GetRecentEvents() const { return m_recentEvents; }

private:
    std::vector<FlowEvent> m_events;
    std::vector<FlowEvent> m_recentEvents;
};
