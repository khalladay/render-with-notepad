#pragma once
#define DllExport   __declspec( dllexport )

extern "C"
{
	DllExport LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
}
