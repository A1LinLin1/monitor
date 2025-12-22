// myhook/config.cpp
#include "config.h"

#pragma data_seg(".shared")
bool Config::recordKeyboard = true;
bool Config::recordMouse = true;
wchar_t Config::targetWindowTitle[256] = L"";
HWND Config::hMainWnd = NULL;
#pragma data_seg()

#pragma comment(linker, "/SECTION:.shared,RWS")