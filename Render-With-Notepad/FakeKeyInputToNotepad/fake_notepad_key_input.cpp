//Sends a single char input ("E") to a running notepad.exe process 
//Thanks to spy++, I know that notepad has 3 windows, with 3 different classes: "Notepad" (the parent window), "Edit", and "msctls_statusbar32"

#include <stdio.h>
#include <Windows.h>
#include <TlHelp32.h> //for PROCESSENTRY32, needs to be included after windows.h
#include <psapi.h>


HWND GetWindowForProcessAndClassName(DWORD pid, const char* className)
{
	HWND curWnd = GetTopWindow(0); //0 arg means to get the window at the top of the Z order
	char classNameBuf[256];

	while (curWnd != NULL)
	{
		DWORD curPid;
		DWORD dwThreadId = GetWindowThreadProcessId(curWnd, &curPid);

		if (curPid == pid)
		{
			GetClassName(curWnd, classNameBuf, 256);
			if (strcmp(className, classNameBuf) == 0)
			{
				return curWnd;
			}

			HWND childWindow = FindWindowEx(curWnd, NULL, className, NULL);

			if (childWindow != NULL)
			{
				return childWindow;
			}
		}
		curWnd = GetNextWindow(curWnd, GW_HWNDNEXT);
	}
	return NULL;
}

DWORD findPidByName(const char* name)
{
	HANDLE h;
	PROCESSENTRY32 singleProcess;
	h = CreateToolhelp32Snapshot( //takes a snapshot of specified processes
		TH32CS_SNAPPROCESS, //get all processes
		0); //ignored for SNAPPROCESS

	singleProcess.dwSize = sizeof(PROCESSENTRY32);

	do {
		if (strcmp(singleProcess.szExeFile, name) == 0)
		{
			DWORD pid = singleProcess.th32ProcessID;
			printf("PID Found: %lu\n", pid);
			CloseHandle(h);
			return pid;
		}

	} while (Process32Next(h, &singleProcess));

	CloseHandle(h);

	return 0;
}

int main(int argc, const char** argv)
{
	DWORD notepadPID = findPidByName("notepad.exe");
	HWND editControl = GetWindowForProcessAndClassName(notepadPID, "Edit"); //case sensitive
	PostMessage(editControl, WM_CHAR, 0x45, 0);

	return 0;
}