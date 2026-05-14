#pragma once

#include <windows.h>
#include "HighResolutionTimer.h"
// Framework は hWnd/timer/m_minimized/m_needsResize を中心に、実行時やエディターで共有する状態を保持する。

class Framework
{
public:
    Framework(HWND hWnd);
    ~Framework();

    int Run();
    LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void Update(float dt);
    void Render(float dt);
    void CalculateFrameStats();

private:
    const HWND              hWnd;
    HighResolutionTimer     timer;
    bool                    m_minimized = false;
    bool                    m_needsResize = false;
    bool                    m_inSizeMove = false;
    bool                    m_ready = false;
    uint32_t                m_pendingWidth = 0;
    uint32_t                m_pendingHeight = 0;
};
