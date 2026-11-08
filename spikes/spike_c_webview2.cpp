#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include <atomic>
#include "native/third_party/webview2/WebView2.h"

// Forward declarations
static void CreateController(HWND hwnd);
static void CleanupWebView2();
static void SuspendWebView2();
static void ResumeWebView2();
static void TrimWorkingSet();
static void PrewarmEnvironment(HWND hwnd);
static void EnsureUI(HWND hwnd);

// State tracking
static HWND g_hwnd = nullptr;
static ICoreWebView2Environment* g_env = nullptr;
static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2* g_webview = nullptr;
static ULONGLONG g_start_time = 0;
static EventRegistrationToken g_msg_token = {};
static EventRegistrationToken g_nav_token = {};
static bool g_prewarming = false;
static bool g_env_ready = false;
static bool g_is_suspended = false;
static bool g_ui_requested = false;
static bool g_tracking_hover = false;

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

class TrySuspendCompletedHandler : public ICoreWebView2TrySuspendCompletedHandler {
    std::atomic<ULONG> ref_{1};
public:
    virtual ~TrySuspendCompletedHandler() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2TrySuspendCompletedHandler) {
            *ppv = static_cast<ICoreWebView2TrySuspendCompletedHandler*>(this);
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
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, BOOL isSuccessful) override {
        wprintf(L"[Spike C] TrySuspend callback: hr=0x%08lX, isSuccessful=%d\n", errorCode, isSuccessful);
        fflush(stdout);
        return S_OK;
    }
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
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) override {
        BOOL success = FALSE;
        args->get_IsSuccess(&success);
        COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
        args->get_WebErrorStatus(&status);
        wprintf(L"[Spike C] Navigation completed: success=%d, status=%d\n", success, status);
        fflush(stdout);
        return S_OK;
    }
};

static void TrimWorkingSet() {
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    wprintf(L"[Spike C] Process working set trimmed via SetProcessWorkingSetSize.\n");
    fflush(stdout);
}

static void SuspendWebView2() {
    if (!g_webview || g_is_suspended) return;
    ICoreWebView2_3* wv3 = nullptr;
    if (SUCCEEDED(g_webview->QueryInterface(IID_ICoreWebView2_3, (void**)&wv3)) && wv3) {
        wprintf(L"[Spike C] Entering linger: calling ICoreWebView2_3::TrySuspend...\n");
        fflush(stdout);
        auto cb = new TrySuspendCompletedHandler();
        HRESULT hr = wv3->TrySuspend(cb);
        if (SUCCEEDED(hr)) {
            g_is_suspended = true;
        } else {
            wprintf(L"[Spike C] TrySuspend call returned: 0x%08lX\n", hr);
            fflush(stdout);
        }
        cb->Release();
        wv3->Release();
    }
}

static void ResumeWebView2() {
    if (!g_webview || !g_is_suspended) return;
    ICoreWebView2_3* wv3 = nullptr;
    if (SUCCEEDED(g_webview->QueryInterface(IID_ICoreWebView2_3, (void**)&wv3)) && wv3) {
        wprintf(L"[Spike C] Reopening UI: calling ICoreWebView2_3::Resume...\n");
        fflush(stdout);
        HRESULT hr = wv3->Resume();
        if (SUCCEEDED(hr)) {
            g_is_suspended = false;
        } else {
            wprintf(L"[Spike C] Resume call returned: 0x%08lX\n", hr);
            fflush(stdout);
        }
        wv3->Release();
    }
}

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
    g_env_ready = false;
    g_prewarming = false;
    g_is_suspended = false;
}

HRESULT STDMETHODCALLTYPE WebMessageReceivedHandler::Invoke(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) {
    LPWSTR json = nullptr;
    args->get_WebMessageAsJson(&json);
    ULONGLONG elapsed = GetTickCount64() - g_start_time;
    wprintf(L"[Spike C] Round-trip web message received in %llu ms: %ls\n", elapsed, json ? json : L"null");
    fflush(stdout);
    if (json) CoTaskMemFree(json);

    wprintf(L"[Spike C] Holding UI open for 2 seconds before simulating linger...\n");
    fflush(stdout);
    SetTimer(g_hwnd, 1003, 2000, nullptr);
    return S_OK;
}

class CustomEnvironmentOptions : public ICoreWebView2EnvironmentOptions {
    std::atomic<ULONG> ref_{1};
    const wchar_t* args_;
public:
    explicit CustomEnvironmentOptions(const wchar_t* args) : args_(args) {}
    virtual ~CustomEnvironmentOptions() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2EnvironmentOptions) {
            *ppv = static_cast<ICoreWebView2EnvironmentOptions*>(this);
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
    HRESULT STDMETHODCALLTYPE get_AdditionalBrowserArguments(LPWSTR* value) override {
        if (!value) return E_POINTER;
        size_t len = wcslen(args_);
        *value = static_cast<LPWSTR>(CoTaskMemAlloc((len + 1) * sizeof(wchar_t)));
        if (!*value) return E_OUTOFMEMORY;
        wcscpy_s(*value, len + 1, args_);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_AdditionalBrowserArguments(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE get_Language(LPWSTR* value) override {
        if (value) *value = nullptr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_Language(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE get_TargetCompatibleBrowserVersion(LPWSTR* value) override {
        if (!value) return E_POINTER;
        const wchar_t* ver = L"131.0.2903.40";
        size_t len = wcslen(ver);
        *value = static_cast<LPWSTR>(CoTaskMemAlloc((len + 1) * sizeof(wchar_t)));
        if (!*value) return E_OUTOFMEMORY;
        wcscpy_s(*value, len + 1, ver);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_TargetCompatibleBrowserVersion(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE get_AllowSingleSignOnUsingOSPrimaryAccount(BOOL* allow) override {
        if (allow) *allow = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_AllowSingleSignOnUsingOSPrimaryAccount(BOOL) override { return S_OK; }
};

HRESULT STDMETHODCALLTYPE ControllerCompletedHandler::Invoke(HRESULT errorCode, ICoreWebView2Controller* createdController) {
    if (FAILED(errorCode) || !createdController) {
        wprintf(L"[Spike C] Failed to create controller: 0x%08lX\n", errorCode);
        fflush(stdout);
        return errorCode;
    }
    g_controller = createdController;
    g_controller->AddRef();
    g_controller->get_CoreWebView2(&g_webview);

    // Feature detect ICoreWebView2Controller2 to set dark background color immediately
    ICoreWebView2Controller2* ctl2 = nullptr;
    if (SUCCEEDED(g_controller->QueryInterface(IID_ICoreWebView2Controller2, (void**)&ctl2)) && ctl2) {
        COREWEBVIEW2_COLOR bg{ 255, 31, 31, 31 }; // #1F1F1F dark taskbar color
        ctl2->put_DefaultBackgroundColor(bg);
        ctl2->Release();
    }

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

static void CreateController(HWND hwnd) {
    if (!g_env || g_controller) return;
    wprintf(L"[Spike C] Creating controller from pre-warmed/ready environment...\n");
    fflush(stdout);
    auto ctl_cb = new ControllerCompletedHandler(hwnd);
    g_env->CreateCoreWebView2Controller(hwnd, ctl_cb);
    ctl_cb->Release();
}

HRESULT STDMETHODCALLTYPE EnvironmentCompletedHandler::Invoke(HRESULT errorCode, ICoreWebView2Environment* createdEnvironment) {
    g_prewarming = false;
    if (FAILED(errorCode) || !createdEnvironment) {
        wprintf(L"[Spike C] Failed to create environment: 0x%08lX\n", errorCode);
        fflush(stdout);
        return errorCode;
    }
    g_env = createdEnvironment;
    g_env->AddRef();
    g_env_ready = true;
    wprintf(L"[Spike C] Environment created in %llu ms.\n", GetTickCount64() - g_start_time);
    fflush(stdout);

    if (g_ui_requested) {
        CreateController(hwnd_);
    }
    return S_OK;
}

static void PrewarmEnvironment(HWND hwnd) {
    if (g_env || g_prewarming) return;
    g_prewarming = true;
    g_start_time = GetTickCount64();

    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    wchar_t* last_slash = wcsrchr(exe_path, L'\\');
    if (last_slash) *(last_slash + 1) = L'\0';
    wchar_t user_data[MAX_PATH];
    swprintf_s(user_data, MAX_PATH, L"%lsdata\\WebView2", exe_path);

    wprintf(L"[Spike C] Speculatively pre-warming WebView2 environment at: %ls\n", user_data);
    fflush(stdout);

    static const wchar_t* BROWSER_ARGS =
        L"--disable-features=Translate,OptimizationHints,MediaRouter,InterestFeedContentSuggestions,CalculateNativeWinOcclusion "
        L"--renderer-process-limit=1 "
        L"--no-first-run "
        L"--no-default-browser-check "
        L"--disable-background-networking "
        L"--disable-component-update "
        L"--disable-sync "
        L"--disable-domain-reliability "
        L"--disk-cache-size=10485760";

    auto options = new CustomEnvironmentOptions(BROWSER_ARGS);
    auto env_cb = new EnvironmentCompletedHandler(hwnd);
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, user_data, options, env_cb);
    if (FAILED(hr)) {
        wprintf(L"[Spike C] CreateCoreWebView2EnvironmentWithOptions returned: 0x%08lX\n", hr);
        fflush(stdout);
        g_prewarming = false;
    }
    options->Release();
    env_cb->Release();
}

static void EnsureUI(HWND hwnd) {
    g_ui_requested = true;
    if (g_controller) {
        if (g_is_suspended) {
            ResumeWebView2();
        }
        g_controller->put_IsVisible(TRUE);
        ShowWindow(hwnd, SW_SHOW);
    } else if (g_env_ready) {
        CreateController(hwnd);
    } else if (!g_prewarming) {
        PrewarmEnvironment(hwnd);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        EnsureUI(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        if (!g_tracking_hover) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_HOVER | TME_LEAVE;
            tme.hwndTrack = hwnd;
            tme.dwHoverTime = 50; // 50ms hover
            TrackMouseEvent(&tme);
            g_tracking_hover = true;
            PrewarmEnvironment(hwnd);
        }
        return 0;
    case WM_MOUSEHOVER:
        PrewarmEnvironment(hwnd);
        return 0;
    case WM_MOUSELEAVE:
        g_tracking_hover = false;
        return 0;
    case WM_LBUTTONUP:
        EnsureUI(hwnd);
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
            wprintf(L"[Spike C] Simulating user closing UI: entering linger and suspending WebView2...\n");
            fflush(stdout);
            if (g_controller) {
                g_controller->put_IsVisible(FALSE);
            }
            SuspendWebView2();
            wprintf(L"[Spike C] Linger running (1.5 seconds)... will test resume...\n");
            fflush(stdout);
            SetTimer(hwnd, 1004, 1500, nullptr);
        } else if (wParam == 1004) {
            KillTimer(hwnd, 1004);
            wprintf(L"[Spike C] Simulating user reopening UI during linger: calling Resume...\n");
            fflush(stdout);
            ResumeWebView2();
            if (g_controller) {
                g_controller->put_IsVisible(TRUE);
            }
            wprintf(L"[Spike C] UI resumed successfully. Holding for 1.5 seconds before linger expiration...\n");
            fflush(stdout);
            SetTimer(hwnd, 1005, 1500, nullptr);
        } else if (wParam == 1005) {
            KillTimer(hwnd, 1005);
            wprintf(L"[Spike C] Linger expired: Closing WebView2 controller...\n");
            fflush(stdout);
            CleanupWebView2();
            TrimWorkingSet();
            wprintf(L"[Spike C] Controller closed and working set trimmed. Window will exit in 1.5 seconds.\n");
            fflush(stdout);
            SetTimer(hwnd, 1002, 1500, nullptr);
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
