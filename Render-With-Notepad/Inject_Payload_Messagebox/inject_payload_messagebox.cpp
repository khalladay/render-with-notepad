//a small dll payload that spawns a message box in whatever process loads the dll
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD ul_reason_for_call, LPVOID lpvReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		MessageBox(NULL, "Process attach!", "Woohoo", 0);
		break;
	}
}