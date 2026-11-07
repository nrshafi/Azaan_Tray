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
static wchar_t g_line1[64] = L"Asr in 24m";
static wchar_t g_line2[64] = L"16:42 \x2022 Dhaka";

static void UpdateTimeText() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(g_line2, 64, L"%02d:%02d:%02d \x2022 Dhaka", st.wHour, st.wMinute, st.wSecond);
}

static void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    // Double-buffered GDI
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // Dark background matching taskbar #1F1F1F
    HBRUSH bgBrush = CreateSolidBrush(RGB(31, 31, 31));
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(memDC, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(memDC, g_font);

    // Primary line (White)
    SetTextColor(memDC, RGB(255, 255, 255));
    RECT r1 = { 4, 2, width - 4, height / 2 };
    DrawTextW(memDC, g_line1, -1, &r1, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

    // Secondary line (Subtle light gray #D1D5DB)
    SetTextColor(memDC, RGB(209, 213, 219));
    RECT r2 = { 4, height / 2, width - 4, height - 2 };
    DrawTextW(memDC, g_line2, -1, &r2, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

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
