#pragma once

#include <Windows.h>

using MouseButton = unsigned int;

class Mouse
{
public:
	static const MouseButton BTN_LEFT = (1 << 0);
	static const MouseButton BTN_MIDDLE = (1 << 1);
	static const MouseButton BTN_RIGHT = (1 << 2);

public:
	Mouse(HWND hWnd);
	~Mouse() {}

	void Update();

	MouseButton GetButton() const { return buttonState[0]; }

	MouseButton GetButtonDown() const { return buttonDown; }

	MouseButton GetButtonUp() const { return buttonUp; }

	void SetWheel(int wheel) { this->wheel[0] += wheel; }

	int GetWheel() const { return wheel[1]; }

	int GetPositionX() const { return positionX[0]; }

	int GetPositionY() const { return positionY[0]; }

	int GetOldPositionX() const { return positionX[1]; }

	int GetOldPositionY() const { return positionY[1]; }

	void SetScreenWidth(int width) { screenWidth = width; }

	void SetScreenHeight(int height) { screenHeight = height; }

	int GetScreenWidth() const { return screenWidth; }

	int GetScreenHeight() const { return screenHeight; }

private:
	MouseButton		buttonState[2] = { 0 };
	MouseButton		buttonDown = 0;
	MouseButton		buttonUp = 0;
	int				positionX[2];
	int				positionY[2];
	int				wheel[2];
	int				screenWidth = 0;
	int				screenHeight = 0;
	HWND			hWnd = nullptr;
};
