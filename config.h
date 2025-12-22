#pragma once
#include <windows.h>

struct Config {
    static bool recordKeyboard;
    static bool recordMouse;
    static wchar_t targetWindowTitle[256];
    static HWND hMainWnd; // 共享主窗口句柄，供 DLL 发送消息
};