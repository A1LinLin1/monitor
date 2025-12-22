#include <windows.h>
#include <string>
#include <cstdio>
#include "logger.h"
#include "config.h"

HHOOK hKeyboardHook = NULL;
HHOOK hMouseHook = NULL;
Logger logger("logs"); 
POINT lastMousePos = { 0 };

// 辅助函数：判断当前窗口是否需要记录
bool ShouldRecordCurrentWindow() {
    // 关键修正：确保 Config::targetWindowTitle 已经由 SetRecordConfig 填充
    if (wcslen(Config::targetWindowTitle) == 0) return true;

    HWND hwnd = GetForegroundWindow();
    wchar_t currentTitle[256];
    if (GetWindowTextW(hwnd, currentTitle, 256) > 0) {
        // 模糊匹配：只要当前标题包含目标标题即可
        return (wcsstr(currentTitle, Config::targetWindowTitle) != NULL);
    }
    return false;
}

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
            // 只有符合过滤条件时才继续执行记录
            if (!ShouldRecordCurrentWindow()) return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);

            KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
            BYTE keyState[256];
            GetKeyboardState(keyState);
            keyState[VK_SHIFT] = GetKeyState(VK_SHIFT);
            keyState[VK_CAPITAL] = GetKeyState(VK_CAPITAL);

            wchar_t wch[5];
            int res = ToUnicode(p->vkCode, p->scanCode, keyState, wch, 4, 0);

            char buf[512];
            std::string windowName = GetActiveWindowTitleStr();
            
            if (res > 0) { 
                wch[res] = L'\0';
                char utf8Char[16];
                WideCharToMultiByte(CP_UTF8, 0, wch, -1, utf8Char, 16, NULL, NULL);
                sprintf_s(buf, "[%s] Key: %s", windowName.c_str(), utf8Char);
            } else { 
                std::string keyName = GetVirtualKeyName(p->vkCode);
                sprintf_s(buf, "[%s] Special: [%s]", windowName.c_str(), keyName.c_str());
            }
            logger.log(buf); 
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && Config::recordMouse) {
        if (!ShouldRecordCurrentWindow()) return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
        
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

// 修正：将 UI 传来的标题拷贝到共享配置中
extern "C" __declspec(dllexport) void SetRecordConfig(bool k, bool m, const wchar_t* t) {
    Config::recordKeyboard = k; 
    Config::recordMouse = m;
    if (t) {
        wcscpy_s(Config::targetWindowTitle, t); // 核心修正点
    } else {
        Config::targetWindowTitle[0] = L'\0';
    }
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