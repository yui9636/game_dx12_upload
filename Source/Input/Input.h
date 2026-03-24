#pragma once

#include "Input/GamePad.h"
#include "Input/Mouse.h"

class Input
{
public:
	Input(HWND hWnd);
	~Input() {}

public:
	static Input& Instance() { return *instance; }

	void Update();

	GamePad& GetGamePad() { return gamePad; }

	Mouse& GetMouse() { return mouse; }

private:
	static Input*		instance;
	GamePad				gamePad;
	Mouse				mouse;
};
