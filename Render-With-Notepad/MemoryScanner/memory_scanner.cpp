//MemoryScanner searches for a string in the allocated memory of a process
//Usage: MemoryScanner <process name> <string> 

#include <stdio.h>
#include <stdint.h>
#include <Windows.h>
#include <TlHelp32.h> //for PROCESSENTRY32, needs to be included after windows.h
#include <psapi.h>
#include <ctype.h>

const char* GetErrorDescription(DWORD err)
{
	switch (err)
	{
	case 0: return "Success";
	case 87: return "Invalid Parameter";
	}

	return "";
}

void printHelp()
{
	printf("MemoryScanner\nUsage: MemoryScanner <process name> <\"string to search for\">\n");
	printf("MemoryScanner\nExample: MemoryScanner notepad.exe BananaBread\n");
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

char* FindPattern(char* src, size_t srcLen, const char* pattern, size_t patternLen)
{
	char* cur = src;
	size_t curPos = 0;

	while (curPos < srcLen)
	{
		if (memcmp(cur, pattern, patternLen) == 0)
		{
			return cur;
		}

		curPos++;
		cur = &src[curPos];
	}
	return nullptr;
}

void PrintAddressesThatMatchPattern(HANDLE process, const char* pattern, size_t patternLen)
{
	MEMORY_BASIC_INFORMATION memInfo;
	char* basePtr = (char*)0x0;
	
	while (VirtualQueryEx(process, (void*)basePtr, &memInfo, sizeof(MEMORY_BASIC_INFORMATION)))
	{
		const DWORD mem_commit = 0x1000;
		const DWORD page_readwrite = 0x04;
		if (memInfo.State == mem_commit && memInfo.Protect == page_readwrite)
		{
			char* remoteMemRegionPtr = (char*)memInfo.BaseAddress;
			char* localCopyContents = (char*)malloc(memInfo.RegionSize);

			SIZE_T bytesRead = 0;
			if (ReadProcessMemory(process, memInfo.BaseAddress, localCopyContents, memInfo.RegionSize, &bytesRead))
			{
				char* match = FindPattern(localCopyContents, memInfo.RegionSize, pattern, patternLen);
				
				if (match)
				{
					uint64_t diff = (uint64_t)match - (uint64_t)(localCopyContents);
					char* processPtr = remoteMemRegionPtr + diff;
					printf("pattern found at %p\n", processPtr);
				}
			}
			free(localCopyContents);
		}
		basePtr = (char*)memInfo.BaseAddress + memInfo.RegionSize;
	}
}

int main(int argc, const char** argv)
{
	if (argc != 3)
	{
		printHelp();
	}

	DWORD processID = findPidByName(argv[1]);
	if (!processID)
	{
		fprintf(stderr, "Could not find process %s\n", argv[1]);
		return 1;
	}

	HANDLE handle = OpenProcess(
		PROCESS_ALL_ACCESS,
		FALSE,
		processID);

	if (handle == NULL)
	{
		fprintf(stderr, "Could not open process with pid: %lu\n", processID);
		return 1;
	}

	//convert input string to UTF16 (hackily)
	const size_t patternLen = strlen(argv[2]);
	char* pattern = new char[patternLen*2];
	for (int i = 0; i < patternLen; ++i)
	{
		pattern[i*2] = argv[2][i];
		pattern[i*2 + 1] = 0x0;
	}

	PrintAddressesThatMatchPattern(handle, pattern, patternLen*2);

	return 0;
}