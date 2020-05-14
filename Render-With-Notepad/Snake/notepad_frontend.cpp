#include "notepad_frontend.h"
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
typedef unsigned __int64 QWORD;

const char* SERVER_PORT = "1337";

typedef struct
{
	int data[64];
	int front = 0;
	int back = -1;
	int size = 0;
	CRITICAL_SECTION queueCritical = { 0 };
}iqueue;

typedef struct
{
	DWORD pid;
	HANDLE process;
	HWND topWindow;
	HWND editWindow;
	int frameBufferCharCount;
	char* frontBufferPtr;
	char* backBuffer;
	HANDLE inputThread;
	SOCKET serverSocket;
	int screenWidth;
	int screenWidthPixels;
	int screenHeight;
	void(*keyDownCallback)(int);

}NotepadFrontendInstance;

NotepadFrontendInstance instance = { 0 };

//********************************************************************************
//		Input Thread
//********************************************************************************

DWORD InputThreadMain(LPVOID lpParameter)
{
	if (listen(instance.serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		closesocket(instance.serverSocket);
		WSACleanup();
		return 1;
	}

	//create a temp socket for incoming connection
	SOCKET ClientSocket = INVALID_SOCKET;

	// Accept a client socket
	ClientSocket = accept(instance.serverSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET)
	{
		printf("accept failed: %d\n", WSAGetLastError());
		closesocket(instance.serverSocket);
		WSACleanup();
		return 1;
	}

	const int DEFAULT_BUFLEN = 512;
	char recvbuf[DEFAULT_BUFLEN];
	memset(recvbuf, '\0', DEFAULT_BUFLEN);
	int iResult;
	int recvbuflen = DEFAULT_BUFLEN;

	// Receive until the peer shuts down the connection
	do {

		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0)
		{
			if (instance.keyDownCallback)
			{
				instance.keyDownCallback(atoi(recvbuf));
			}
		}
		else if (iResult == 0)
		{
			printf("Connection closing...\n");
		}
		//else
		//{
		//	printf("recv failed: %d\n", WSAGetLastError());
		//	closesocket(ClientSocket);
		//	WSACleanup();
		//	return 1;
		//}

	} while (iResult > 0);

	// shutdown the send half of the connection since no more data will be sent
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR)
	{
		printf("shutdown failed: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();
	return 0;
}

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

SOCKET CreateServerSocket(const char* port)
{
	//setting up a client socket for WINSOCK, mostly copied from the msdn example code
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET ListenSocket = INVALID_SOCKET;

	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return ListenSocket;
	}

	struct addrinfo* result = NULL;
	struct addrinfo* ptr = NULL;
	struct addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //use ipv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the local address and port to be used by the server
	iResult = getaddrinfo(NULL, port, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}


	// Create a SOCKET for the server to listen for client connections
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (ListenSocket == INVALID_SOCKET)
	{
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);


	return ListenSocket;
}


void installRemoteHook(DWORD threadId, const char* hookDLL)
{
	HMODULE hookLib = LoadLibrary(hookDLL);
	if (hookLib == NULL)
	{
		printf("Err: %s\n", GetErrorDescription(GetLastError()));
	}
	HOOKPROC hookFunc = (HOOKPROC)GetProcAddress(hookLib, "KeyboardProc");

	if (hookFunc == NULL)
	{
		printf("Err: %s\n", GetErrorDescription(GetLastError()));

	}

	SetWindowsHookEx(WH_KEYBOARD, hookFunc, hookLib, threadId);
}

void installRemoteHook2(DWORD threadId, const char* hookDLL)
{
	HMODULE hookLib = LoadLibrary(hookDLL);
	if (hookLib == NULL)
	{
		printf("Err: %s\n", GetErrorDescription(GetLastError()));
	}
	HOOKPROC hookFunc = (HOOKPROC)GetProcAddress(hookLib, "MsgProc");

	if (hookFunc == NULL)
	{
		printf("Err: %s\n", GetErrorDescription(GetLastError()));

	}

	SetWindowsHookEx(WH_GETMESSAGE, hookFunc, hookLib, threadId);
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
					printf("pattern found at %p@", patternPtr);
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
	char* frameBuffer = (char*)malloc(instance.frameBufferCharCount * 2);
	for (int i = 0; i < instance.frameBufferCharCount; i++)
	{
		char v = 0x41 + (rand() % 26);
		PostMessage(instance.editWindow, WM_CHAR, v, 0);
		frameBuffer[i * 2] = v;
		frameBuffer[i * 2 + 1] = 0x00;
	}

	Sleep(7000); //wait for input messages to finish processing...it's slow. 

	char* pointers[256];
	int foundPatterns = findBytePatternInProcessMemory(instance.process, frameBuffer, instance.frameBufferCharCount * 2, pointers, 256);

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

void initializeNotepadFrontend(const char* pathToNotepadExe, const char* pathToHookDLL, int windowWidth, int windowHeight, int screenWidthPixels, int charBufferSize)
{
	srand((unsigned int)time(NULL));

	instance.process = launchNotepad(pathToNotepadExe);
	Sleep(150); //wait for process to start up

	instance.pid = GetProcessId(instance.process);
	instance.topWindow = GetTopWindowForProcess(instance.pid);
	instance.editWindow = GetWindowForProcessAndClassName(instance.pid, "edit");
	instance.frameBufferCharCount = charBufferSize;
	instance.backBuffer = (char*)malloc(charBufferSize * 2);
	instance.screenHeight = windowHeight;
	instance.screenWidth = windowWidth;
	instance.screenWidthPixels = screenWidthPixels;

	MoveWindow(instance.topWindow, 100, 100, instance.screenWidth, instance.screenHeight, true);

	instance.frontBufferPtr = CreateAndAcquireRemoteCharBufferPtr();
	instance.serverSocket = CreateServerSocket(SERVER_PORT);
	instance.inputThread = CreateThread(NULL, 0, &InputThreadMain, NULL, 0, NULL);

	DWORD notepadEditThreadId = GetWindowThreadProcessId(instance.editWindow, &instance.pid);
	installRemoteHook(notepadEditThreadId, pathToHookDLL);
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

void setKeydownFunc(void(*func)(int))
{
	instance.keyDownCallback = func;
}

//********************************************************************************
//		Drawing Functions
//********************************************************************************

void redrawScreen()
{
	RECT r;
	GetClientRect(instance.editWindow, &r);
	InvalidateRect(instance.editWindow, &r, false);

}

void drawChar(int x, int y, char c)
{
	instance.backBuffer[(instance.screenWidthPixels * 2) * y + x * 2] = c;
}

char getChar(int x, int y)
{
	return instance.backBuffer[(instance.screenWidthPixels * 2) * y + x * 2];
}

void clearScreen()
{
	memset(instance.backBuffer, 0, instance.frameBufferCharCount * 2);
}

void swapBuffersAndRedraw()
{
	size_t written = 0;
	WriteProcessMemory(instance.process, instance.frontBufferPtr, instance.backBuffer, instance.frameBufferCharCount * 2, &written);

	RECT r;
	GetClientRect(instance.editWindow, &r);
	InvalidateRect(instance.editWindow, &r, false); 
}