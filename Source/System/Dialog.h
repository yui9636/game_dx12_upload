#pragma once

#include <Windows.h>
// DialogResult はこの機能の公開インターフェースを定義し、実装側が具体的な処理を行う。

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
