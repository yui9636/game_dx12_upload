#pragma once

#include <Windows.h>

enum class DialogResult
{
	Yes,
	No,
	OK,
	Cancel
};

class Dialog
{
public:
	static DialogResult OpenFileName(char* filepath, int size, const char* filter = nullptr, const char* title = nullptr, HWND hWnd = NULL, bool multiSelect = false);

	static DialogResult SaveFileName(char* filepath, int size, const char* filter = nullptr, const char* title = nullptr, const char* ext = nullptr, HWND hWnd = NULL);
};
