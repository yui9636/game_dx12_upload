#include <sstream>
#include <cstdio>
#include <imgui.h>
#include "ImGuiRenderer.h"
#include "Framework.h"
#include "Graphics.h"
#include <shellapi.h>
#include "Asset/AssetManager.h"
#include "Engine/EngineKernel.h"
#include "RHI/DX11/DX11CommandList.h"
#include "Console/Logger.h"

static const int syncInterval = 1; // エディタは 60fps 前提で扱いたいので VSync を有効化

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Framework::Framework(HWND hWnd)
    : hWnd(hWnd)
{
    // 1. グラフィックス初期化
    Graphics::Instance().Initialize(hWnd, GraphicsAPI::DX12);

    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX11) {
        // DX11: 完全な ImGui 初期化
        ImGuiRenderer::Initialize(
            hWnd,
            Graphics::Instance().GetDevice(),
            Graphics::Instance().GetDeviceContext()
        );

        ImGuiIO& io = ImGui::GetIO();
#if defined(IMGUI_HAS_VIEWPORT)
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
#endif
    } else {
        // DX12: ImGui DX12 バックエンドで完全初期化
        ImGuiRenderer::InitializeDX12(hWnd, Graphics::Instance().GetDX12Device());
    }

    LOG_INFO("[Framework] Graphics initialized API=%s", Graphics::Instance().GetAPI() == GraphicsAPI::DX12 ? "DX12" : "DX11");

    // 2. エンジンカーネルの初期化
    EngineKernel::Instance().Initialize();
}

Framework::~Framework()
{
    EngineKernel::Instance().Finalize();
    // DX11/DX12 共通（内部で API 分岐）
    ImGuiRenderer::Finalize();
}

int Framework::Run()
{
    MSG msg = {};

    if (!IsWindowVisible(hWnd))
        ShowWindow(hWnd, SW_SHOW);

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            timer.Tick();
            CalculateFrameStats();

            float dt = (syncInterval == 0)
                ? timer.TimeInterval()
                : (1.0f / 60.0f);

            Update(dt);
            Render(dt);
        }
    }
    return (int)msg.wParam;
}

void Framework::Update(float dt)
{
    EngineKernel::Instance().PollInput();
    // ImGui NewFrame
    ImGuiRenderer::Begin();

    EngineKernel::Instance().Update(dt);
}

void Framework::Render(float dt)
{
    // DX12 パス: EngineKernel::Render() 内で ImGui 描画 + SubmitFrame まで完結
    if (Graphics::Instance().GetAPI() == GraphicsAPI::DX12) {
        EngineKernel::Instance().Render();
        Graphics::Instance().Present(syncInterval);
        return;
    }

    // DX11 パス (既存)
    ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
    DX11CommandList commandList(dc);

    FrameBuffer* displayFB = Graphics::Instance().GetFrameBuffer(FrameBufferId::Display);
    displayFB->Clear(&commandList, 0, 0, 0, 1);
    displayFB->SetRenderTargets(&commandList);

    EngineKernel::Instance().Render();

    ImGuiRenderer::End();

    Graphics::Instance().Present(syncInterval);
}

void Framework::CalculateFrameStats()
{
    static int frameCnt = 0;
    static double timeElapsed = 0.0f;

    frameCnt++;

    if ((timer.TimeStamp() - timeElapsed) >= 1.0f)
    {
        float fps = (float)frameCnt;
        float mspf = 1000.0f / fps;

        std::ostringstream outs;
        outs.precision(6);
        outs << "MyEngine | FPS: " << fps << " / " << "Frame Time: " << mspf << " (ms)";
        SetWindowTextA(hWnd, outs.str().c_str());

        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}

LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // ImGui メッセージハンドリング（DX11/DX12 共通 - imgui_impl_win32 は API 非依存）
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

    switch (msg)
    {
    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);

        for (UINT i = 0; i < fileCount; i++) {
            char filePath[MAX_PATH];
            DragQueryFileA(hDrop, i, filePath, MAX_PATH);

            AssetManager::Instance().ImportExternalFile(
                filePath,
                AssetManager::Instance().GetRootDirectory()
            );
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY: PostQuitMessage(0); break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostMessage(hWnd, WM_CLOSE, 0, 0);
        break;
    case WM_ENTERSIZEMOVE: timer.Stop();  break;
    case WM_EXITSIZEMOVE:  timer.Start(); break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}
