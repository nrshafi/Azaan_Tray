#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TIMER_TICK 1001
#define ID_TRAY_EXIT 2001

static HWND g_hwnd = nullptr;
static NOTIFYICONDATAW g_nid = {};
static HFONT g_font = nullptr;
static HDC g_memDC = nullptr;
static HBITMAP g_memBmp = nullptr;
static HGDIOBJ g_oldBmp = nullptr;
static HBRUSH g_bgBrush = nullptr;
static int g_bufWidth = 0;
static int g_bufHeight = 0;
static wchar_t g_line1[64] = L"Asr in 24m";
static wchar_t g_line2[64] = L"16:42 \x2022 Dhaka";

static bool UpdateTimeText() {
    wchar_t prev[64];
    wcscpy_s(prev, g_line2);
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(g_line2, 64, L"%02d:%02d:%02d \x2022 Dhaka", st.wHour, st.wMinute, st.wSecond);
    return (wcscmp(prev, g_line2) != 0);
}

static void CleanupGdiCache() {
    if (g_memDC) {
        if (g_oldBmp) SelectObject(g_memDC, g_oldBmp);
        g_oldBmp = nullptr;
        DeleteDC(g_memDC);
        g_memDC = nullptr;
    }
    if (g_memBmp) {
        DeleteObject(g_memBmp);
        g_memBmp = nullptr;
    }
    if (g_bgBrush) {
        DeleteObject(g_bgBrush);
        g_bgBrush = nullptr;
    }
    g_bufWidth = 0;
    g_bufHeight = 0;
}

static void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    if (width <= 0 || height <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }

    // Reallocate backbuffer only if dimensions change or not yet initialized
    if (!g_memDC || width != g_bufWidth || height != g_bufHeight) {
        if (g_memDC) {
            SelectObject(g_memDC, g_oldBmp);
            DeleteObject(g_memBmp);
            DeleteDC(g_memDC);
        }
        g_memDC = CreateCompatibleDC(hdc);
        g_memBmp = CreateCompatibleBitmap(hdc, width, height);
        g_oldBmp = SelectObject(g_memDC, g_memBmp);
        g_bufWidth = width;
        g_bufHeight = height;
        if (!g_bgBrush) {
            g_bgBrush = CreateSolidBrush(RGB(31, 31, 31));
        }
    }

    // Dark background matching taskbar #1F1F1F
    FillRect(g_memDC, &rc, g_bgBrush);

    SetBkMode(g_memDC, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(g_memDC, g_font);

    // Primary line (White)
    SetTextColor(g_memDC, RGB(255, 255, 255));
    RECT r1 = { 4, 2, width - 4, height / 2 };
    DrawTextW(g_memDC, g_line1, -1, &r1, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

    // Secondary line (Subtle light gray #D1D5DB)
    SetTextColor(g_memDC, RGB(209, 213, 219));
    RECT r2 = { 4, height / 2, width - 4, height - 2 };
    DrawTextW(g_memDC, g_line2, -1, &r2, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

    BitBlt(hdc, 0, 0, width, height, g_memDC, 0, 0, SRCCOPY);

    SelectObject(g_memDC, oldFont);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(g_nid.szTip, L"Azaan Tray (Spike A - GDI)");
        Shell_NotifyIconW(NIM_ADD, &g_nid);

        SYSTEMTIME st;
        GetLocalTime(&st);
        UINT next_delay = 1000 - st.wMilliseconds;
        if (next_delay < 50) next_delay += 1000;
        SetTimer(hwnd, ID_TIMER_TICK, next_delay, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (wParam == ID_TIMER_TICK) {
            bool changed = UpdateTimeText();
            if (changed) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            SYSTEMTIME st;
            GetLocalTime(&st);
            UINT next_delay = 1000 - st.wMilliseconds;
            if (next_delay < 50) next_delay += 1000;
            SetTimer(hwnd, ID_TIMER_TICK, next_delay, nullptr);
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
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit Spike A");
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
        CleanupGdiCache();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        KillTimer(hwnd, ID_TIMER_TICK);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // 9pt Segoe UI Variable or Segoe UI
    g_font = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI Variable Small"
    );

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"AzaanTray_SpikeA_GDI";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Azaan Tray Spike A GDI",
        WS_POPUP | WS_VISIBLE,
        100, 100, 160, 42,
        nullptr, nullptr, hInst, nullptr
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
