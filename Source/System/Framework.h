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
//    //---------------------------------------------
//    void Update(float dt);
//
//    //---------------------------------------------
//    //---------------------------------------------
//    void Render(float dt);
//
//    //---------------------------------------------
//    //---------------------------------------------
//    void CalculateFrameStats();
//
//    //---------------------------------------------
//    //---------------------------------------------
//    void DrawSceneSwitcher();
//
//    //---------------------------------------------
//    //---------------------------------------------
//    Scene* CreateSceneByIndex(int index);
//
//private:
//    const HWND              hWnd;
//    HighResolutionTimer     timer;
//    Input                   input;
//    Audio                   audio;
//
//    std::unique_ptr<Scene>  scene;
//
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
    //---------------------------------------------
    void Update(float dt);

    //---------------------------------------------
    //---------------------------------------------
    void Render(float dt);

    //---------------------------------------------
    //---------------------------------------------
    void CalculateFrameStats();

private:
    const HWND              hWnd;
    HighResolutionTimer     timer;
    Input                   input;
    Audio                   audio;

};
