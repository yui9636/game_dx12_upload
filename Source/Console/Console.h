#pragma once
// Console は m_autoScroll を中心に、実行時やエディターで共有する状態を保持する。

class Console {
public:
    static Console& Instance();

    void Draw(const char* title = "Console", bool* p_open = nullptr, bool* outFocused = nullptr);

private:
    Console() = default;
    ~Console() = default;

    bool m_autoScroll = true;
};
