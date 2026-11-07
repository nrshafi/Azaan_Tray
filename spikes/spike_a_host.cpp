#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <stdio.h>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TIMER_TICK 1001
#define ID_TRAY_EXIT 2001

template <typename T>
class ComPtr {
    T* ptr_ = nullptr;
public:
    ComPtr() = default;
    ~ComPtr() { Reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* Get() const { return ptr_; }
    T** GetAddressOf() { Reset(); return &ptr_; }
    T* operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    void Reset() {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }
};

static HWND g_hwnd = nullptr;
static NOTIFYICONDATAW g_nid = {};
static ComPtr<ID2D1Factory> g_d2d_factory;
static ComPtr<IDWriteFactory> g_dwrite_factory;
static ComPtr<ID2D1HwndRenderTarget> g_render_target;
static ComPtr<IDWriteTextFormat> g_text_format;
static ComPtr<ID2D1SolidColorBrush> g_brush;
static wchar_t g_display_text[128] = L"Asr in 24m\n16:42 \x2022 Dhaka";

static void UpdateTimeText() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(g_display_text, 128, L"Asr in 24m\n%02d:%02d:%02d \x2022 Dhaka", st.wHour, st.wMinute, st.wSecond);
}

static HRESULT CreateDeviceResources(HWND hwnd) {
    if (g_render_target) return S_OK;

    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = g_d2d_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, size),
        g_render_target.GetAddressOf()
    );
    if (FAILED(hr)) return hr;

    hr = g_render_target->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White),
        g_brush.GetAddressOf()
    );
    return hr;
}

static void OnPaint(HWND hwnd) {
    if (FAILED(CreateDeviceResources(hwnd))) return;

    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);

    g_render_target->BeginDraw();
    g_render_target->Clear(D2D1::ColorF(0.12f, 0.12f, 0.12f, 1.0f));

    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_RECT_F layout_rect = D2D1::RectF(
        4.0f, 2.0f,
        static_cast<float>(rc.right - rc.left - 4),
        static_cast<float>(rc.bottom - rc.top - 2)
    );

    g_render_target->DrawText(
        g_display_text,
        static_cast<UINT32>(wcslen(g_display_text)),
        g_text_format.Get(),
        &layout_rect,
        g_brush.Get()
    );

    HRESULT hr = g_render_target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        g_render_target.Reset();
        g_brush.Reset();
    }

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
        wcscpy_s(g_nid.szTip, L"Azaan Tray (Spike A)");
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
    case WM_SIZE:
        if (g_render_target) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            g_render_target->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
        }
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
        g_render_target.Reset();
        g_brush.Reset();
        g_text_format.Reset();
        g_dwrite_factory.Reset();
        g_d2d_factory.Reset();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, g_d2d_factory.GetAddressOf());
    if (FAILED(hr)) return 1;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(g_dwrite_factory.GetAddressOf())
    );
    if (FAILED(hr)) return 1;

    hr = g_dwrite_factory->CreateTextFormat(
        L"Segoe UI Variable Small",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f,
        L"en-US",
        g_text_format.GetAddressOf()
    );
    if (FAILED(hr)) {
        // Fallback to standard Segoe UI
        g_dwrite_factory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f,
            L"en-US",
            g_text_format.GetAddressOf()
        );
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"AzaanTray_SpikeA";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    // Create a 160x40 widget-like window
    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Azaan Tray Spike A",
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

    return (int)msg.wParam;
}
