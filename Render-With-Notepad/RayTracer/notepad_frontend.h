void initializeNotepadFrontend(const char* pathToNotepadExe, const char* pathToHookDLL, int windowWidth, int windowHeight, int charsPerLine, int charBufferSize);
void shutdownNotepadFrontend();

void drawChar(int x, int y, char c);
void clearScreen();
void swapBuffersAndRedraw();