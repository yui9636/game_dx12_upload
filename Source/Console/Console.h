#pragma once

class Console {
public:
    static Console& Instance();

    // 毎フレームImGuiで統合ウィンドウを描画する
    void Draw(const char* title = "Console", bool* p_open = nullptr);

private:
    Console() = default;
    ~Console() = default;

    // UIの状態保持
    bool m_autoScroll = true;
};