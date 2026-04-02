#pragma once
#include "InputEvent.h"
#include <vector>

class InputEventQueue {
public:
    void Clear() { m_events.clear(); m_sequence = 0; }

    void Push(InputEvent event) {
        event.sequence = m_sequence++;
        m_events.push_back(event);
    }

    const std::vector<InputEvent>& GetEvents() const { return m_events; }
    size_t Size() const { return m_events.size(); }
    bool Empty() const { return m_events.empty(); }

private:
    std::vector<InputEvent> m_events;
    uint16_t m_sequence = 0;
};
