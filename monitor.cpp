#define UNICODE
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include "config.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define WM_TRAY (WM_USER + 1)
#define ID_TRAY_RESTORE 1001
#define ID_TRAY_EXIT 1002
#define ID_HOTKEY_SCROLLLOCK 2001
#define IDC_CHECK_KEYBOARD 3001
#define IDC_CHECK_MOUSE 3002
#define IDC_COMBO_WINDOW 3003
#define IDC_BTN_RECORD 3005
#define IDC_LOG_EDIT 4001
#define IDC_FILTER_WIN 4002

typedef void (*HookFunc)();
typedef void (*SetConfigFunc)(bool, bool, const wchar_t*);

HINSTANCE hInst;
NOTIFYICONDATA nid = { 0 };
HMODULE hHookDLL = nullptr;
HookFunc InstallHook, UninstallHook;
SetConfigFunc SetRecordConfig;
bool isRecording = false;

HWND hLogEdit, hStatusLabel;
std::vector<std::wstring> rawLogs;
HICON hIconMain, hIconRec;

// 更新状态显示逻辑
void UpdateStatusUI(HWND hWnd) {
    if (isRecording) {
        SetWindowText(hStatusLabel, L"Status: ● RECORDING (Press ScrollLock to Stop)");
        nid.hIcon = hIconRec;
        lstrcpy(nid.szTip, L"Recorder: RECORDING...");
    } else {
        SetWindowText(hStatusLabel, L"Status: IDLE (Press ScrollLock to Start)");
        nid.hIcon = hIconMain;
        lstrcpy(nid.szTip, L"Recorder: Idle");
    }
    Shell_NotifyIcon(NIM_MODIFY, &nid); // 关键：更新托盘状态
}

void UpdateLogDisplay(HWND hWnd) {
    wchar_t filter[256];
    GetDlgItemText(hWnd, IDC_FILTER_WIN, filter, 256);
    std::wstring display;
    for (const auto& log : rawLogs) {
        if (wcslen(filter) == 0 || wcsstr(log.c_str(), filter)) {
            display += log + L"\r\n";
        }
    }
    SetWindowText(hLogEdit, display.c_str());
    SendMessage(hLogEdit, EM_SETSEL, -1, -1);
    SendMessage(hLogEdit, EM_SCROLLCARET, 0, 0);
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    wchar_t title[256];
    if (IsWindowVisible(hwnd) && GetWindowText(hwnd, title, 256) > 0) {
        SendMessage((HWND)lParam, CB_ADDSTRING, 0, (LPARAM)title);
    }
    return TRUE;
}

void InitTray(HWND hWnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA); // 必须准确设置大小
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO; // 增加 NIF_INFO 以支持气泡提示
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = hIconMain;
    lstrcpy(nid.szTip, L"Recorder: Idle");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        Config::hMainWnd = hWnd;
        InitCommonControls();
        RegisterHotKey(hWnd, ID_HOTKEY_SCROLLLOCK, 0, VK_SCROLL);
        
        // 加载资源图标
        hIconMain = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIN_ICON));
        hIconRec = LoadIcon(hInst, MAKEINTRESOURCE(IDI_REC_ICON));
        InitTray(hWnd);

        // UI 布局
        CreateWindow(L"button", L"Record Keyboard", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 20, 20, 140, 25, hWnd, (HMENU)IDC_CHECK_KEYBOARD, hInst, NULL);
        CreateWindow(L"button", L"Record Mouse", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 170, 20, 130, 25, hWnd, (HMENU)IDC_CHECK_MOUSE, hInst, NULL);
        CreateWindow(L"static", L"Select Window to Monitor:", WS_VISIBLE | WS_CHILD, 20, 55, 280, 20, hWnd, NULL, hInst, NULL);
        CreateWindow(L"combobox", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL, 20, 75, 280, 200, hWnd, (HMENU)IDC_COMBO_WINDOW, hInst, NULL);
        hStatusLabel = CreateWindow(L"static", L"Status: IDLE", WS_VISIBLE | WS_CHILD, 20, 110, 280, 25, hWnd, NULL, hInst, NULL);
        CreateWindow(L"button", L"Start / Stop", WS_VISIBLE | WS_CHILD, 20, 140, 280, 40, hWnd, (HMENU)IDC_BTN_RECORD, hInst, NULL);

        CreateWindow(L"static", L"Live Logs (Copyable):", WS_VISIBLE | WS_CHILD, 320, 20, 200, 20, hWnd, NULL, hInst, NULL);
        CreateWindow(L"edit", L"", WS_VISIBLE | WS_CHILD | WS_BORDER, 520, 18, 260, 22, hWnd, (HMENU)IDC_FILTER_WIN, hInst, NULL);
        hLogEdit = CreateWindow(L"edit", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 320, 50, 460, 130, hWnd, (HMENU)IDC_LOG_EDIT, hInst, NULL);

        CheckDlgButton(hWnd, IDC_CHECK_KEYBOARD, BST_CHECKED); CheckDlgButton(hWnd, IDC_CHECK_MOUSE, BST_CHECKED);
        SendMessage(GetDlgItem(hWnd, IDC_COMBO_WINDOW), CB_ADDSTRING, 0, (LPARAM)L"--- Global Mode ---");
        EnumWindows(EnumWindowsProc, (LPARAM)GetDlgItem(hWnd, IDC_COMBO_WINDOW));
        SendMessage(GetDlgItem(hWnd, IDC_COMBO_WINDOW), CB_SETCURSEL, 0, 0);
        break;

    case WM_COPYDATA: {
        PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;
        wchar_t wbuf[512]; MultiByteToWideChar(CP_UTF8, 0, (char*)pcds->lpData, -1, wbuf, 512);
        rawLogs.push_back(wbuf);
        if (rawLogs.size() > 500) rawLogs.erase(rawLogs.begin());
        UpdateLogDisplay(hWnd);
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_FILTER_WIN && HIWORD(wParam) == EN_CHANGE) UpdateLogDisplay(hWnd);
        if (LOWORD(wParam) == IDC_BTN_RECORD) SendMessage(hWnd, WM_HOTKEY, ID_HOTKEY_SCROLLLOCK, 0);
        if (LOWORD(wParam) == ID_TRAY_EXIT) DestroyWindow(hWnd);
        if (LOWORD(wParam) == ID_TRAY_RESTORE) { ShowWindow(hWnd, SW_SHOW); SetForegroundWindow(hWnd); }
        break;

    case WM_HOTKEY:
        if (wParam == ID_HOTKEY_SCROLLLOCK) {
            isRecording = !isRecording;
            if (isRecording) {
                wchar_t sel[256]; GetDlgItemText(hWnd, IDC_COMBO_WINDOW, sel, 256);
                SetRecordConfig(IsDlgButtonChecked(hWnd, IDC_CHECK_KEYBOARD), IsDlgButtonChecked(hWnd, IDC_CHECK_MOUSE), wcscmp(sel, L"--- Global Mode ---")==0 ? L"" : sel);
                InstallHook();
                ShowWindow(hWnd, SW_HIDE);
                // 弹出气泡提示告知用户
                nid.uFlags |= NIF_INFO;
                lstrcpy(nid.szInfoTitle, L"Recorder Active");
                lstrcpy(nid.szInfo, L"Recording has started. Window hidden to tray.");
                nid.dwInfoFlags = NIIF_INFO;
            } else {
                UninstallHook();
                ShowWindow(hWnd, SW_SHOW);
                SetForegroundWindow(hWnd);
                nid.uFlags &= ~NIF_INFO;
            }
            UpdateStatusUI(hWnd); // 同步更新 UI 和图标状态
        }
        break;

    case WM_TRAY:
        if (lParam == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt); HMENU hm = CreatePopupMenu();
            AppendMenu(hm, MF_STRING, ID_TRAY_RESTORE, L"Show Main Window");
            AppendMenu(hm, MF_STRING, ID_TRAY_EXIT, L"Exit Completely");
            SetForegroundWindow(hWnd); TrackPopupMenu(hm, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hm);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hWnd, SW_SHOW); SetForegroundWindow(hWnd);
        }
        break;

    case WM_CLOSE: ShowWindow(hWnd, SW_HIDE); return 0;
    case WM_DESTROY: 
        Shell_NotifyIcon(NIM_DELETE, &nid); // 程序退出前必须删除图标
        PostQuitMessage(0); 
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE h, HINSTANCE, LPSTR, int n) {
    hInst = h; WNDCLASS wc = { 0 }; wc.lpfnWndProc = WndProc; wc.hInstance = h;
    wc.lpszClassName = L"InputRecorderV3"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(h, MAKEINTRESOURCE(IDI_MAIN_ICON)); 
    RegisterClass(&wc);
    HWND hWnd = CreateWindow(L"InputRecorderV3", L"Pro Input Recorder v3.1", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, 820, 240, NULL, NULL, h, NULL);
    hHookDLL = LoadLibrary(L"myhook.dll");
    InstallHook = (HookFunc)GetProcAddress(hHookDLL, "InstallHook");
    UninstallHook = (HookFunc)GetProcAddress(hHookDLL, "UninstallHook");
    SetRecordConfig = (SetConfigFunc)GetProcAddress(hHookDLL, "SetRecordConfig");
    ShowWindow(hWnd, n);
    MSG m; while (GetMessage(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
    return 0;
}