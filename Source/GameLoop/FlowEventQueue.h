#pragma once

#include <string>
#include <vector>

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
        m_events.push_back({ name, value });
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

private:
    std::vector<FlowEvent> m_events;
};
