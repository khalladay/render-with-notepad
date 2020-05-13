#include "notepad_frontend.h"
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <windowsx.h>
typedef unsigned __int64 QWORD;


typedef struct
{
	DWORD pid;
	HANDLE process;
	HWND topWindow;
	HWND editWindow;
	int frameBufferCharCount;
	char* frontBufferPtr;
	char* backBuffer;
	int screenWidth;
	int screenHeight;
	int screenWidthChars;

}NotepadFrontendInstance;

NotepadFrontendInstance instance = { 0 };

//********************************************************************************
//		Frontend Initialization Functions
//********************************************************************************

const char* GetErrorDescription(DWORD err)
{
	switch (err)
	{
	case 0: return "Success";
	case 1: return "Invalid Function";
	case 2: return "File Not Found";
	case 3: return "Path Not Found";
	case 4: return "Too Many Open Files";
	case 5: return "Access Denied";
	case 6: return "Invalid Handle";
	case 87: return "Invalid Parameter";
	case 1114: return "DLL Init Failed";
	case 127: return "Proc Not Found";
	}

	return "";
}

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

HANDLE launchNotepad(const char* pathToNotepad)
{
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFO si = { 0 };
	if (CreateProcess(const_cast<char*>(pathToNotepad), NULL, NULL, NULL, false, 0, NULL, NULL, &si, &pi))
	{
		return pi.hProcess;
	}

	printf("LaunchNotepad Error: %s", GetErrorDescription(GetLastError()));

	return 0;
}

HWND GetTopWindowForProcess(DWORD pid)
{
	HWND hCurWnd = GetTopWindow(0); //0 arg means to get the window at the top of the Z order
	while (hCurWnd != NULL)
	{
		DWORD cur_pid;
		DWORD dwThreadId = GetWindowThreadProcessId(hCurWnd, &cur_pid);

		if (cur_pid == pid)
		{
			HWND lastParent = hCurWnd;
			HWND parent = GetParent(hCurWnd);
			while (parent != NULL)
			{
				lastParent = parent;
				parent = GetParent(lastParent);
			}
			return lastParent;
		}
		hCurWnd = GetNextWindow(hCurWnd, GW_HWNDNEXT);
	}

	return NULL;
}

char* FindPattern(char* src, size_t srcLen, const char* pattern, size_t len)
{
	char* cur = src;
	size_t curPos = 0;

	while (curPos < srcLen)
	{
		if (memcmp(cur, pattern, len) == 0)
		{
			return cur;
		}

		curPos++;
		cur = &src[curPos];
	}
	return nullptr;
}

int findBytePatternInProcessMemory(HANDLE handle, const char* bytePattern, size_t patternLen, char** outPointers, size_t outPointerCount)
{
	MEMORY_BASIC_INFORMATION memInfo;
	char* basePtr = (char*)0x0;
	VirtualQueryEx(handle, (void*)basePtr, &memInfo, sizeof(MEMORY_BASIC_INFORMATION));
	int foundPointers = 0;

	do {
		const DWORD mem_commit = 0x1000;
		const DWORD page_readwrite = 0x04;
		if (memInfo.State == mem_commit && memInfo.Protect == page_readwrite)
		{
			char* buffContents = (char*)malloc(memInfo.RegionSize);
			char* baseAddr = (char*)memInfo.BaseAddress;
			size_t bytesRead = 0;

			if (ReadProcessMemory(handle, memInfo.BaseAddress, buffContents, memInfo.RegionSize, &bytesRead))
			{
				char* match = FindPattern(buffContents, memInfo.RegionSize, bytePattern, patternLen);
				if (match)
				{
					QWORD diff = (QWORD)match - (QWORD)buffContents;
					char* patternPtr = (char*)((QWORD)memInfo.BaseAddress + diff);
					outPointers[foundPointers++] = patternPtr;

				}
			}
			free(buffContents);
		}


		basePtr = (char*)memInfo.BaseAddress + memInfo.RegionSize;

	} while (VirtualQueryEx(handle, (void*)basePtr, &memInfo, sizeof(MEMORY_BASIC_INFORMATION)));

	return foundPointers;
}


char* CreateAndAcquireRemoteCharBufferPtr()
{
	size_t notepadCharBufferSize = (size_t)instance.frameBufferCharCount * 2;
	size_t charsToWrite = instance.frameBufferCharCount;

	char* frameBuffer = (char*)malloc(notepadCharBufferSize);
	for (int i = 0; i < charsToWrite; i++)
	{
		char v = 0x41 + (rand() % 26);
		PostMessage(instance.editWindow, WM_CHAR, v, 0);
		frameBuffer[i * 2] = v;
		frameBuffer[i * 2 + 1] = 0x00;
	}

	Sleep(5000); //wait for input messages to finish processing...it's slow. 

	char* pointers[256];
	int foundPatterns = findBytePatternInProcessMemory(instance.process, frameBuffer, notepadCharBufferSize, pointers, 256);

	if (foundPatterns == 0)
	{
		printf("Error: Pattern Not Found\n");
		return NULL;
	}
	if (foundPatterns > 1)
	{
		printf("Error: Found multiple instances of char pattern in memory\n");
		return NULL;
	}

	return pointers[0];

}

void initializeNotepadFrontend(const char* pathToNotepadExe, const char* pathToHookDLL, int windowWidth, int windowHeight, int charsPerLine, int charBufferSize)
{
	srand(time(NULL));

	instance.process = launchNotepad(pathToNotepadExe);
	Sleep(150); //wait for process to start up

	instance.pid = GetProcessId(instance.process);
	instance.topWindow = GetTopWindowForProcess(instance.pid);
	instance.editWindow = GetWindowForProcessAndClassName(instance.pid, "Edit");
	instance.frameBufferCharCount = charBufferSize;
	instance.backBuffer = (char*)malloc(charBufferSize * 2); // *2 for utf16 chars
	instance.screenHeight = windowHeight;
	instance.screenWidth = windowWidth;
	instance.screenWidthChars = charsPerLine;

	MoveWindow(instance.topWindow, 100, 100, instance.screenWidth, instance.screenHeight, true);
	instance.frontBufferPtr = CreateAndAcquireRemoteCharBufferPtr();
	swapBuffersAndRedraw();
}

void shutdownNotepadFrontend()
{
	if (!TerminateProcess(instance.process, 0))
	{
		printf("Error terminating process: %s", GetErrorDescription(GetLastError()));
	}
	CloseHandle(instance.process);

	free(instance.backBuffer);
	Sleep(50);
}

//********************************************************************************
//		Drawing Functions
//********************************************************************************

void swapBuffersAndRedraw()
{
	size_t written = 0;
	WriteProcessMemory(instance.process, instance.frontBufferPtr, instance.backBuffer, instance.frameBufferCharCount * 2, &written);

	RECT r;
	GetClientRect(instance.editWindow, &r);
	InvalidateRect(instance.editWindow, &r, false);
}

void drawChar(int x, int y, char c)
{
	instance.backBuffer[(instance.screenWidthChars * 2) * y + x * 2] = c;
}

void clearScreen()
{
	memset(instance.backBuffer, 0, instance.frameBufferCharCount * 2);
}
