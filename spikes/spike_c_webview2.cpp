#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include <atomic>
#include "native/third_party/webview2/WebView2.h"

// Dedicated lightweight COM handlers
class EnvironmentCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    std::atomic<ULONG> ref_{1};
    HWND hwnd_;
public:
    explicit EnvironmentCompletedHandler(HWND hwnd) : hwnd_(hwnd) {}
    virtual ~EnvironmentCompletedHandler() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = --ref_;
        if (c == 0) delete this;
        return c;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* createdEnvironment) override;
};

class ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    std::atomic<ULONG> ref_{1};
    HWND hwnd_;
public:
    explicit ControllerCompletedHandler(HWND hwnd) : hwnd_(hwnd) {}
    virtual ~ControllerCompletedHandler() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = --ref_;
        if (c == 0) delete this;
        return c;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* createdController) override;
};

class WebMessageReceivedHandler : public ICoreWebView2WebMessageReceivedEventHandler {
    std::atomic<ULONG> ref_{1};
public:
    virtual ~WebMessageReceivedHandler() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2WebMessageReceivedEventHandler) {
            *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = --ref_;
        if (c == 0) delete this;
        return c;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override;
};

class NavigationCompletedHandler : public ICoreWebView2NavigationCompletedEventHandler {
    std::atomic<ULONG> ref_{1};
public:
    virtual ~NavigationCompletedHandler() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2NavigationCompletedEventHandler) {
            *ppv = static_cast<ICoreWebView2NavigationCompletedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = --ref_;
        if (c == 0) delete this;
        return c;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
        BOOL success = FALSE;
        args->get_IsSuccess(&success);
        COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
        args->get_WebErrorStatus(&status);
        wprintf(L"[Spike C] Navigation completed: success=%d, status=%d\n", success, status);
        fflush(stdout);
        return S_OK;
    }
};

static HWND g_hwnd = nullptr;
static ICoreWebView2Environment* g_env = nullptr;
static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2* g_webview = nullptr;
static ULONGLONG g_start_time = 0;
static EventRegistrationToken g_msg_token = {};
static EventRegistrationToken g_nav_token = {};

static void CleanupWebView2() {
    if (g_webview && g_msg_token.value) {
        g_webview->remove_WebMessageReceived(g_msg_token);
        g_msg_token.value = 0;
    }
    if (g_webview && g_nav_token.value) {
        g_webview->remove_NavigationCompleted(g_nav_token);
        g_nav_token.value = 0;
    }
    if (g_controller) {
        g_controller->Close();
        g_controller->Release();
        g_controller = nullptr;
    }
    if (g_webview) {
        g_webview->Release();
        g_webview = nullptr;
    }
    if (g_env) {
        g_env->Release();
        g_env = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE WebMessageReceivedHandler::Invoke(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) {
    LPWSTR json = nullptr;
    args->get_WebMessageAsJson(&json);
    ULONGLONG elapsed = GetTickCount64() - g_start_time;
    wprintf(L"[Spike C] Round-trip web message received in %llu ms: %ls\n", elapsed, json ? json : L"null");
    fflush(stdout);
    if (json) CoTaskMemFree(json);

    wprintf(L"[Spike C] Holding UI open for 4 seconds for perf measurement...\n");
    fflush(stdout);
    SetTimer(g_hwnd, 1003, 4000, nullptr);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE ControllerCompletedHandler::Invoke(HRESULT errorCode, ICoreWebView2Controller* createdController) {
    if (FAILED(errorCode) || !createdController) {
        wprintf(L"[Spike C] Failed to create controller: 0x%08lX\n", errorCode);
        fflush(stdout);
        return errorCode;
    }
    g_controller = createdController;
    g_controller->AddRef();
    g_controller->get_CoreWebView2(&g_webview);

    RECT rc;
    GetClientRect(hwnd_, &rc);
    g_controller->put_Bounds(rc);
    g_controller->put_IsVisible(TRUE);

    // Resolve www directory relative to executable directory
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    wchar_t* last_slash = wcsrchr(exe_path, L'\\');
    if (last_slash) *(last_slash + 1) = L'\0';
    wchar_t www_path[MAX_PATH];
    swprintf_s(www_path, MAX_PATH, L"%lswww", exe_path);

    wprintf(L"[Spike C] Mapping virtual host to: %ls\n", www_path);
    fflush(stdout);

    ICoreWebView2_3* wv3 = nullptr;
    if (SUCCEEDED(g_webview->QueryInterface(IID_ICoreWebView2_3, (void**)&wv3)) && wv3) {
        wv3->SetVirtualHostNameToFolderMapping(
            L"app.azaantray.invalid",
            www_path,
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
        );
        wv3->Release();
    }

    auto msg_cb = new WebMessageReceivedHandler();
    g_webview->add_WebMessageReceived(msg_cb, &g_msg_token);
    msg_cb->Release();

    auto nav_cb = new NavigationCompletedHandler();
    g_webview->add_NavigationCompleted(nav_cb, &g_nav_token);
    nav_cb->Release();

    wprintf(L"[Spike C] Navigating to https://app.azaantray.invalid/index.html\n");
    fflush(stdout);
    g_webview->Navigate(L"https://app.azaantray.invalid/index.html");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE EnvironmentCompletedHandler::Invoke(HRESULT errorCode, ICoreWebView2Environment* createdEnvironment) {
    if (FAILED(errorCode) || !createdEnvironment) {
        wprintf(L"[Spike C] Failed to create environment: 0x%08lX\n", errorCode);
        fflush(stdout);
        return errorCode;
    }
    g_env = createdEnvironment;
    g_env->AddRef();
    wprintf(L"[Spike C] Environment created in %llu ms. Creating controller...\n", GetTickCount64() - g_start_time);
    fflush(stdout);

    auto ctl_cb = new ControllerCompletedHandler(hwnd_);
    g_env->CreateCoreWebView2Controller(hwnd_, ctl_cb);
    ctl_cb->Release();
    return S_OK;
}

static void InitWebView2(HWND hwnd) {
    g_start_time = GetTickCount64();

    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    wchar_t* last_slash = wcsrchr(exe_path, L'\\');
    if (last_slash) *(last_slash + 1) = L'\0';
    wchar_t user_data[MAX_PATH];
    swprintf_s(user_data, MAX_PATH, L"%lsdata\\WebView2", exe_path);

    wprintf(L"[Spike C] Initializing WebView2 environment at: %ls\n", user_data);
    fflush(stdout);

    auto env_cb = new EnvironmentCompletedHandler(hwnd);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, user_data, nullptr, env_cb);
    if (FAILED(hr)) {
        wprintf(L"[Spike C] CreateCoreWebView2EnvironmentWithOptions returned: 0x%08lX\n", hr);
        fflush(stdout);
    }
    env_cb->Release();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitWebView2(hwnd);
        return 0;
    case WM_SIZE:
        if (g_controller) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            g_controller->put_Bounds(rc);
        }
        return 0;
    case WM_TIMER:
        if (wParam == 1003) {
            KillTimer(hwnd, 1003);
            wprintf(L"[Spike C] Closing WebView2 controller...\n");
            fflush(stdout);
            CleanupWebView2();
            wprintf(L"[Spike C] Controller closed. Window will exit in 2 seconds.\n");
            fflush(stdout);
            SetTimer(hwnd, 1002, 2000, nullptr);
        } else if (wParam == 1002) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        CleanupWebView2();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int main() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"AzaanTray_SpikeC";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Azaan Tray Spike C",
        WS_POPUP | WS_VISIBLE,
        200, 200, 360, 480,
        nullptr, nullptr, hInst, nullptr
    );

    if (!g_hwnd) return 1;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return 0;
}
