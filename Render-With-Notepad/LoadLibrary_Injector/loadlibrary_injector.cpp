//Injector_LoadLibrary is a dll injector that uses LoadLibraryA to inject a dll into a running process
// usage: Injector_LoadLibrary <process name> <path to dll> 

#include <stdio.h>
#include <Windows.h>
#include <TlHelp32.h> //for PROCESSENTRY32, needs to be included after windows.h

void printHelp()
{
	printf("Injector_LoadLibrary\nUsage: Injector_LoadLibrary <process name> <path to dll>\n");
}

void createRemoteThread(DWORD processID, const char* dllPath)
{
	HANDLE handle = OpenProcess(
		PROCESS_QUERY_INFORMATION | //Needed to get a process' token
		PROCESS_CREATE_THREAD |		//for obvious reasons
		PROCESS_VM_OPERATION |		//required to perform operations on address space of process (like WriteProcessMemory)
		PROCESS_VM_WRITE,			//required for WriteProcessMemory
		FALSE,						//don't inherit handle
		processID);

	if (handle == NULL)
	{
		fprintf(stderr, "Could not open process with pid: %lu\n", processID);
		return;
	}

	//once the process is open, we need to write the name of our dll to that process' memory
	size_t dllPathLen = strlen(dllPath);
	void* dllPathRemote = VirtualAllocEx(
		handle,
		NULL, //let the system decide where to allocate the memory
		dllPathLen,
		MEM_COMMIT, //actually commit the virtual memory
		PAGE_READWRITE); //mem access for committed page

	if (!dllPathRemote)
	{
		fprintf(stderr, "Could not allocate %zd bytes in process with pid: %lu\n", dllPathLen, processID);
		return;
	}

	BOOL writeSucceeded = WriteProcessMemory(
		handle,
		dllPathRemote,
		dllPath,
		dllPathLen,
		NULL);

	if (!writeSucceeded)
	{
		fprintf(stderr, "Could not write %zd bytes to process with pid %lu\n", dllPathLen, processID);
		return;
	}

	//now get address of LoadLibraryW function inside Kernel32.dll
	//TEXT macro "Identifies a string as Unicode when UNICODE is defined by a preprocessor directive during compilation. Otherwise, ANSI string"
	PTHREAD_START_ROUTINE loadLibraryFunc = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(TEXT("Kernel32.dll")), "LoadLibraryA");
	if (loadLibraryFunc == NULL)
	{
		fprintf(stderr, "Could not find LoadLibraryA function inside kernel32.dll\n");
		return;
	}

	//now create a thread in remote process that loads our target dll using LoadLibraryA

	HANDLE remoteThread = CreateRemoteThread(
		handle,
		NULL, //default thread security
		0, //stack size for thread
		loadLibraryFunc, //pointer to start of thread function (for us, LoadLibraryA)
		dllPathRemote, //pointer to variable being passed to thread function
		0, //0 means the thread runs immediately after creation
		NULL); //we don't care about getting back the thread identifier

	if (remoteThread == NULL)
	{
		fprintf(stderr, "Could not create remote thread.\n");
		return;
	}
	else
	{
		fprintf(stdout, "Success! remote thread started in process %d\n", processID);
	}

	// Wait for the remote thread to terminate
	WaitForSingleObject(remoteThread, INFINITE);

	//once we're done, free the memory we allocated in the remote process for the dllPathname, and shut down
	VirtualFreeEx(handle, dllPathRemote, 0, MEM_RELEASE);
	CloseHandle(remoteThread);
	CloseHandle(handle);
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
	if (argc != 3)
	{
		printHelp();
		return 1;
	}

	createRemoteThread(findPidByName(argv[1]), argv[2]);

	return 0;
}