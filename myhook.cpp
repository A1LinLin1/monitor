#include <windows.h>
#include <string>
#include <cstdio>
#include "logger.h"
#include "config.h"

HHOOK hKeyboardHook = NULL;
HHOOK hMouseHook = NULL;
Logger logger("logs"); 
POINT lastMousePos = { 0 };

// 实时同步日志到主界面
void SendLogToUI(const std::string& logEntry) {
    if (Config::hMainWnd == NULL) return;
    COPYDATASTRUCT cds;
    cds.dwData = 1; 
    cds.cbData = (DWORD)logEntry.size() + 1;
    cds.lpData = (PVOID)logEntry.c_str();
    SendMessageA(Config::hMainWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
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
            
            // 键盘状态转真实字符
            BYTE keyState[256];
            GetKeyboardState(keyState);
            wchar_t wch[5];
            int res = ToUnicode(p->vkCode, p->scanCode, keyState, wch, 4, 0);
            
            char buf[512];
            std::string winName = GetActiveWindowTitleStr();
            if (res > 0) {
                wch[res] = L'\0';
                char utf8Char[16];
                WideCharToMultiByte(CP_UTF8, 0, wch, -1, utf8Char, 16, NULL, NULL);
                sprintf_s(buf, "[%s] Char: %s", winName.c_str(), utf8Char);
            } else {
                sprintf_s(buf, "[%s] KeyCode: %d", winName.c_str(), p->vkCode);
            }
            
            logger.log(buf);
            SendLogToUI(buf);
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
            if (abs(dx) > 10 || abs(dy) > 10) {
                char mbuf[64]; sprintf_s(mbuf, "Move: %d,%d", dx, dy);
                action = mbuf; lastMousePos = p->pt;
            }
        }
        if (!action.empty()) {
            char buf[512];
            sprintf_s(buf, "[%s] %s", GetActiveWindowTitleStr().c_str(), action.c_str());
            logger.log(buf);
            SendLogToUI(buf);
        }
    }
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

extern "C" __declspec(dllexport) void SetRecordConfig(bool k, bool m, const wchar_t* t) {
    Config::recordKeyboard = k; Config::recordMouse = m;
    if (t) wcscpy_s(Config::targetWindowTitle, t);
}

extern "C" __declspec(dllexport) void InstallHook() {
    GetCursorPos(&lastMousePos);
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandleW(L"myhook.dll"), 0);
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandleW(L"myhook.dll"), 0);
}

extern "C" __declspec(dllexport) void UninstallHook() {
    if (hKeyboardHook) UnhookWindowsHookEx(hKeyboardHook);
    if (hMouseHook) UnhookWindowsHookEx(hMouseHook);
}