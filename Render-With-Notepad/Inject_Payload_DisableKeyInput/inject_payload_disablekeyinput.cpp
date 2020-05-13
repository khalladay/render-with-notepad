#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include "inject_payload_disablekeyinput.h"

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	return 1;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD ul_reason_for_call, LPVOID lpvReserved)
{
	return true;
}