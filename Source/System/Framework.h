//#pragma once
//
//#include <windows.h>
//#include"Scene/Scene.h"
//#include "HighResolutionTimer.h"
//#include "Input/input.h"
//#include <Audio\Audio.h>
//
//
//class Framework
//{
//public:
//    Framework(HWND hWnd);
//    ~Framework();
//
//public:
//    int Run();
//    LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//
//private:
//    //---------------------------------------------
//    // Update : フレーム更新（dt 秒）
//    //---------------------------------------------
//    void Update(float dt);
//
//    //---------------------------------------------
//    // Render : フレーム描画（dt 秒）
//    //---------------------------------------------
//    void Render(float dt);
//
//    //---------------------------------------------
//    // CalculateFrameStats : FPSとフレーム時間の表示文字列を更新
//    //---------------------------------------------
//    void CalculateFrameStats();
//
//    //---------------------------------------------
//    // DrawSceneSwitcher : 左上に常駐するシーン切替ミニランチャーを描画
//    //---------------------------------------------
//    void DrawSceneSwitcher();
//
//    //---------------------------------------------
//    // CreateSceneByIndex : ランチャーの選択Indexからシーンを生成
//    //---------------------------------------------
//    Scene* CreateSceneByIndex(int index);
//
//private:
//    const HWND              hWnd;
//    HighResolutionTimer     timer;
//    Input                   input;
//    Audio                   audio;
//
//    // ※ 既存の unique_ptr<Scene> scene は SceneManager に移行済みのため未使用
//    std::unique_ptr<Scene>  scene;
//
//    // ─ ランチャー用ステート ─
//    bool    showSceneSwitcher = true; // 表示のON/OFF（常時trueで表示）
//    int     selectedSceneIndex = 0;   // コンボの現在選択
//};
#pragma once

#include <windows.h>
#include "HighResolutionTimer.h"
#include "Input/input.h"
#include <Audio/Audio.h>

class Framework
{
public:
    Framework(HWND hWnd);
    ~Framework();

    int Run();
    LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    //---------------------------------------------
    // Update : フレーム更新
    //---------------------------------------------
    void Update(float dt);

    //---------------------------------------------
    // Render : フレーム描画
    //---------------------------------------------
    void Render(float dt);

    //---------------------------------------------
    // CalculateFrameStats : FPS表示（ウィンドウタイトル等）
    //---------------------------------------------
    void CalculateFrameStats();

private:
    const HWND              hWnd;
    HighResolutionTimer     timer;
    Input                   input;
    Audio                   audio;

};