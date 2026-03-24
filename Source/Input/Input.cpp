#include "Input/Input.h"

Input* Input::instance = nullptr;

Input::Input(HWND hWnd)
	: mouse(hWnd)
{
	instance = this;
}

void Input::Update()
{
	gamePad.Update();
	mouse.Update();
}
