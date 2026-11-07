#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <algorithm>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TIMER_TICK 1001
#define ID_TRAY_EXIT 2001

static HWND g_hwnd = nullptr;
static NOTIFYICONDATAW g_nid = {};
static HFONT g_font = nullptr;
static UINT g_wm_taskbar_created = 0;
static wchar_t g_line1[64] = L"Asr in 24m";
static wchar_t g_line2[64] = L"16:42 \x2022 Dhaka";

static void UpdateTimeText() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(g_line2, 64, L"%02d:%02d:%02d \x2022 Dhaka", st.wHour, st.wMinute, st.wSecond);
}

// Find placement in taskbar
static void DockToTaskbar(HWND hwndWidget) {
    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!hTray) return;

    HWND hNotify = FindWindowExW(hTray, nullptr, L"TrayNotifyWnd", nullptr);
    if (!hNotify) return;

    RECT rcTray{};
    GetWindowRect(hTray, &rcTray);

    RECT rcNotify{};
    GetWindowRect(hNotify, &rcNotify);

    // Default target right boundary is TrayNotifyWnd's left edge
    int targetRight = rcNotify.left;

    // Scan for other docked widgets (like TrafficMonitor)
    HWND hChild = nullptr;
    while ((hChild = FindWindowExW(hTray, hChild, nullptr, nullptr)) != nullptr) {
        if (hChild == hwndWidget || hChild == hNotify) continue;
        if (!IsWindowVisible(hChild)) continue;

        wchar_t className[64]{};
        GetClassNameW(hChild, className, 64);
        wchar_t windowText[64]{};
        GetWindowTextW(hChild, windowText, 64);

        if (wcsstr(windowText, L"TrafficMonitor") || wcsstr(className, L"AwqatSalaat")) {
            RECT rcChild{};
            GetWindowRect(hChild, &rcChild);
            if (rcChild.left < targetRight && rcChild.left > rcTray.left + 200) {
                targetRight = std::min(targetRight, (int)rcChild.left);
            }
        }
    }

    int widgetWidth = 130;
    int widgetHeight = rcTray.bottom - rcTray.top;
    if (widgetHeight > 48) widgetHeight = 48; // clamp on large bars

    // Position relative to hTray coordinate space
    POINT pt = { targetRight - widgetWidth - 4, rcTray.top + (rcTray.bottom - rcTray.top - widgetHeight) / 2 };
    ScreenToClient(hTray, &pt);

    SetParent(hwndWidget, hTray);
    SetWindowPos(
        hwndWidget,
        HWND_TOP,
        pt.x, pt.y,
        widgetWidth, widgetHeight,
        SWP_SHOWWINDOW | SWP_NOACTIVATE
    );
}

static void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // Transparent or solid dark matching taskbar
    HBRUSH bgBrush = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDC, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(memDC, g_font);

    SetTextColor(memDC, RGB(255, 255, 255));
    RECT r1 = { 4, 3, width - 4, height / 2 };
    DrawTextW(memDC, g_line1, -1, &r1, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

    SetTextColor(memDC, RGB(209, 213, 219));
    RECT r2 = { 4, height / 2, width - 4, height - 3 };
    DrawTextW(memDC, g_line2, -1, &r2, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == g_wm_taskbar_created && g_wm_taskbar_created != 0) {
        DockToTaskbar(hwnd);
        return 0;
    }

    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(g_nid.szTip, L"Azaan Tray (Spike B - Taskbar Docked)");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        DockToTaskbar(hwnd);
        SetTimer(hwnd, ID_TIMER_TICK, 1000, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (wParam == ID_TIMER_TICK) {
            UpdateTimeText();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_PAINT:
        OnPaint(hwnd);
        return 0;
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit Spike B");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        KillTimer(hwnd, ID_TIMER_TICK);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    g_wm_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    g_font = CreateFontW(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI Variable Small"
    );

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"AzaanTray_SpikeB";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Azaan Tray Spike B",
        WS_CHILD | WS_CLIPSIBLINGS,
        0, 0, 130, 40,
        GetDesktopWindow(), nullptr, hInst, nullptr
    );

    if (!hwnd) return 1;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_font) DeleteObject(g_font);
    return (int)msg.wParam;
}
