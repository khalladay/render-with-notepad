# Let's Turn Notepad.exe into a Game Platform
This is a collection of proof of concept applications for using Memory Scanning, API Hooking, and DLL Injection to allow you to write real time games that draw in Notepad at 30 fps (or more) and allow you to intercept user input sent to a Notepad window to allow your game to be driven via keyboard controls. 

The end result is a game that seems to live entirely inside an instance of stock Notepad.exe. 

## Included Projects
There are a number of projects included as part of the render-in-notepad solution (located inside the Render-In-Notepad directory). They are as follows: 

### RayTracer
A simple real time ray tracer which renders a 3D scene in Notepad, using memory scanning to get the address of Notepad's text buffer Requires you pass the path to Notepad.exe as a command line argument  [Link](https://github.com/khalladay/render-with-notepad/tree/master/Render-With-Notepad/RayTracer)

It looks better when full size, but here's a low quality gif of what this looks like (a full size gif was really obnoxious in the readme)

![gif of ray tracer](https://github.com/khalladay/render-with-notepad/blob/master/rt2.gif)

### Snake
A snake game which is played entirely in Notepad.exe. Uses memory scanning to get access to Notepad's text buffer, and uses the Innject_Payload_HookKeyboard dll to install a hook which redirects output from that Notepad instance to the Snake process so it can be used as game input. Requires the path to notepad.exe and the path to Inject_Payload_HookKeyboard's dll on the command line. [Link](https://github.com/khalladay/render-with-notepad/tree/master/Render-With-Notepad/Snake)

It looks better when full size, but here's a low quality gif of what this looks like (a full size gif was really obnoxious in the readme)

![gif of snake](https://github.com/khalladay/render-with-notepad/blob/master/snake3.gif)

### MemoryScanner
A quick and dirty memory scanning app which searches a target process' address space for a chosen byte pattern and prints that address to stdout. Requires the name of the (already running) process you wish to scan, and a string "byte pattern" as command line args. [Link](https://github.com/khalladay/render-with-notepad/tree/master/Render-With-Notepad/MemoryScanner)

### FakeKeyboardInputToNotepad
This program finds a running Notepad process and sends a WM_CHAR message to its edit control to print the letter 'E.' Requires that you already have notepad running. [Link](https://github.com/khalladay/render-with-notepad/tree/master/Render-With-Notepad/FakeKeyInputToNotepad)

### Inject_Payload_Messagebox
This is a dll payload that can be injected into a running process to trigger a system message box popup. Requires a different program to actually inject the dll. [Link](https://github.com/khalladay/render-with-notepad/tree/master/Render-With-Notepad/Inject_Payload_Messagebox)

### LoadLibrary_Injector
A simple dll injector which uses LoadLibrary as its injection mechanism. Intended to be used with the Inject_Payload_Messagebox payload. 

### Inject_Payload_DisableKeyInput
A dll containing a dummy KeyboardProc callback, intended to be used with SetWindowsHook to prevent a Win32 window from receiving keyboard input. Requires a different program to actually install this hook. [Link](https://github.com/khalladay/render-with-notepad/tree/master/Render-With-Notepad/Inject_Payload_DisableKeyInput)

### Inject_Payload_HookKeyboard
A dll payload containing a KeyboardProc callback that opens a socket on localhost and redirects keyboard output to that socket. Requires a different program to actually install this hook. [Link](https://github.com/khalladay/render-with-notepad/tree/master/Render-With-Notepad/Inject_Payload_HookKeyboard)

### HookKeyboard_Injector
Installs a WH_KEYBOARD hook into an already running instance of notepad. Can be used with both hook payloads described above. If used with Inject_Payload_HookKeyboard, this program will first start a listen socket, and will receive (and print) the keyboard events sent by the installed hook function. Requires you pass the path to the dll containing your desired hook function on the command line. [Link](https://github.com/khalladay/render-with-notepad/tree/master/Render-With-Notepad/HookKeyboard_Injector)
