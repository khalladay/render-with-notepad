//this will hook the arrow key and enter key presses from a notepad app and log whenever they get pressed (and prevent them from happening in notepad)

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <TlHelp32.h> //for PROCESSENTRY32, needs to be included after windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

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
			CloseHandle(h);
			return pid;
		}

	} while (Process32Next(h, &singleProcess));

	CloseHandle(h);

	return 0;
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

const char* GetErrorDescription(DWORD err)
{
	switch (err)
	{
	case 0: return "Success";
	case 87: return "Invalid Parameter";
	case 1114: return "DLL Init Failed";
	case 127: return "Proc Not Found";
	}

	return "";
}

bool installRemoteHook(DWORD threadId, const char* hookDLL)
{
	HMODULE hookLib = LoadLibrary(hookDLL);
	if (hookLib == NULL) return false;
	
	HOOKPROC hookFunc = (HOOKPROC)GetProcAddress(hookLib, "KeyboardProc");
	if (hookFunc == NULL) return false;
	
	SetWindowsHookEx(WH_KEYBOARD, hookFunc, hookLib, threadId);
	return true;
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


void printHelp()
{
	printf("HookKeyboard_Injector\nUsage: HookKeyboard_Injector  <path to dll>\n");
	printf("HookKeyboard_Injector\nUsage: Requires a separate instance of Notepad.exe to be running\n");
}

int main(int argc, const char** argv)
{
	if (argc != 2)
	{
		printHelp();
		return 1;
	}

	DWORD notepadPID = findPidByName("notepad.exe");
	HWND notepadEditWindow = GetWindowForProcessAndClassName(notepadPID, "edit");
	DWORD notepadEditThreadId = GetWindowThreadProcessId(notepadEditWindow, &notepadPID);
	installRemoteHook(notepadEditThreadId, argv[1]);

	SOCKET sock = CreateServerSocket("1337");

	if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
	{
		printf("Listen failed with error: %ld\n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	//create a temp socket for incoming connection
	SOCKET ClientSocket = INVALID_SOCKET;

	// Accept a client socket
	ClientSocket = accept(sock, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET)
	{
		printf("accept failed: %d\n", WSAGetLastError());
		closesocket(sock);
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
			printf("Received: %s\n", recvbuf);
		}
		else if (iResult == 0)
		{
			printf("Connection closing...\n");
		}
		else
		{
			printf("recv failed: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return 1;
		}

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