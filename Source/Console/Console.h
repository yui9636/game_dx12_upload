#pragma once

class Console {
public:
    static Console& Instance();

    void Draw(const char* title = "Console", bool* p_open = nullptr);

private:
    Console() = default;
    ~Console() = default;

    bool m_autoScroll = true;
};
