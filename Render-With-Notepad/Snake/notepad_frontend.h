void initializeNotepadFrontend(const char* pathToNotepadExe, const char* pathToHookDLL, int windowWidth, int windowHeight, int charsPerLine, int charBufferSize);
void shutdownNotepadFrontend();
void setKeydownFunc(void(*func)(int));

void drawChar(int x, int y, char c);
char getChar(int x, int y);
void clearScreen();
void swapBuffersAndRedraw();