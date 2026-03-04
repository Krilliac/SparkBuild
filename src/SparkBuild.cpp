#include "SparkBuild.h"
#include "Config.h"
#include "ProcessRunner.h"
#include "EnvironmentTab.h"
#include "ConfigureTab.h"
#include "BuildTab.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace SparkBuild {

static const wchar_t* MAIN_WND_CLASS = L"SparkBuildMainWindow";

SparkBuildApp::SparkBuildApp(HINSTANCE hInst)
    : m_hInst(hInst)
    , m_config(std::make_unique<ConfigManager>())
    , m_processRunner(std::make_unique<ProcessRunner>())
{
}

SparkBuildApp::~SparkBuildApp() {
    // Save config on exit
    if (m_config) {
        m_config->Save(ConfigManager::GetDefaultIniPath());
    }
}

bool SparkBuildApp::Init() {
    // Load saved config
    m_config->Load(ConfigManager::GetDefaultIniPath());

    // Register main window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = m_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = MAIN_WND_CLASS;
    // Try to load icon from resources, fall back to default
    wc.hIcon = LoadIcon(m_hInst, MAKEINTRESOURCE(101));
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;

    if (!RegisterClassEx(&wc)) return false;

    // Calculate centered position
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - WINDOW_WIDTH) / 2;
    int y = (screenH - WINDOW_HEIGHT) / 2;

    m_hwnd = CreateWindowEx(
        0, MAIN_WND_CLASS, L"SparkBuild - SparkEngine Build Tool",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, m_hInst, this);

    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    return true;
}

int SparkBuildApp::Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(m_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK SparkBuildApp::MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SparkBuildApp* self = nullptr;
    if (msg == WM_CREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<SparkBuildApp*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->OnCreate(hwnd);
        return 0;
    }

    self = reinterpret_cast<SparkBuildApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            self->OnSize(w, h);
            return 0;
        }
        case WM_NOTIFY: {
            auto nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == IDC_TABCTRL && nmhdr->code == TCN_SELCHANGE) {
                self->OnTabChanged();
            }
            return 0;
        }
        case WM_CLOSE:
            self->OnClose();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void SparkBuildApp::OnCreate(HWND hwnd) {
    m_hwnd = hwnd;

    HFONT hTabFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    // Create tab control
    m_hTab = CreateWindowEx(0, WC_TABCONTROL, L"",
                            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FIXEDWIDTH,
                            0, 0, WINDOW_WIDTH, WINDOW_HEIGHT - 22,
                            hwnd, (HMENU)(UINT_PTR)IDC_TABCTRL, m_hInst, nullptr);
    SendMessage(m_hTab, WM_SETFONT, (WPARAM)hTabFont, TRUE);

    // Set tab width
    SendMessage(m_hTab, TCM_SETITEMSIZE, 0, MAKELPARAM(140, 28));

    // Add tabs
    TCITEM ti = {};
    ti.mask = TCIF_TEXT;

    ti.pszText = const_cast<wchar_t*>(L"  Environment  ");
    TabCtrl_InsertItem(m_hTab, 0, &ti);

    ti.pszText = const_cast<wchar_t*>(L"  Configure  ");
    TabCtrl_InsertItem(m_hTab, 1, &ti);

    ti.pszText = const_cast<wchar_t*>(L"  Build  ");
    TabCtrl_InsertItem(m_hTab, 2, &ti);

    // Create status bar
    m_hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, L"Ready",
                                   WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                   0, 0, 0, 0,
                                   hwnd, (HMENU)(UINT_PTR)IDC_STATUSBAR, m_hInst, nullptr);
    SendMessage(m_hStatusBar, WM_SETFONT, (WPARAM)hTabFont, TRUE);

    // Create tab pages
    RECT tabArea = GetTabDisplayArea();

    m_envTab = std::make_unique<EnvironmentTab>();
    m_envTab->SetConfig(m_config.get());
    m_envTab->SetProcessRunner(m_processRunner.get());
    m_envTab->SetStatusCallback([this](const std::string& s) { UpdateStatusBar(s); });
    m_envTab->Create(m_hTab, m_hInst, tabArea);

    m_cfgTab = std::make_unique<ConfigureTab>();
    m_cfgTab->SetConfig(m_config.get());
    m_cfgTab->Create(m_hTab, m_hInst, tabArea);
    m_cfgTab->LoadFromConfig();

    m_buildTab = std::make_unique<BuildTab>();
    m_buildTab->SetConfig(m_config.get());
    m_buildTab->SetProcessRunner(m_processRunner.get());
    m_buildTab->SetConfigureTab(m_cfgTab.get());
    m_buildTab->SetStatusCallback([this](const std::string& s) { UpdateStatusBar(s); });
    m_buildTab->Create(m_hTab, m_hInst, tabArea);

    // Show first tab
    m_envTab->Show(true);
    m_cfgTab->Show(false);
    m_buildTab->Show(false);
    m_currentTab = 0;
}

void SparkBuildApp::OnSize(int width, int height) {
    // Resize tab control
    if (m_hTab) {
        RECT statusRC;
        GetWindowRect(m_hStatusBar, &statusRC);
        int statusH = statusRC.bottom - statusRC.top;
        MoveWindow(m_hTab, 0, 0, width, height - statusH, TRUE);
    }

    // Resize status bar
    if (m_hStatusBar) {
        SendMessage(m_hStatusBar, WM_SIZE, 0, 0);
    }

    // Resize tab pages
    RECT tabArea = GetTabDisplayArea();
    if (m_envTab && m_envTab->GetHwnd()) {
        MoveWindow(m_envTab->GetHwnd(), tabArea.left, tabArea.top,
                   tabArea.right - tabArea.left, tabArea.bottom - tabArea.top, TRUE);
    }
    if (m_cfgTab && m_cfgTab->GetHwnd()) {
        MoveWindow(m_cfgTab->GetHwnd(), tabArea.left, tabArea.top,
                   tabArea.right - tabArea.left, tabArea.bottom - tabArea.top, TRUE);
    }
    if (m_buildTab && m_buildTab->GetHwnd()) {
        MoveWindow(m_buildTab->GetHwnd(), tabArea.left, tabArea.top,
                   tabArea.right - tabArea.left, tabArea.bottom - tabArea.top, TRUE);
    }
}

void SparkBuildApp::OnTabChanged() {
    int sel = TabCtrl_GetCurSel(m_hTab);
    if (sel == m_currentTab) return;

    // When leaving Configure tab, save to config
    if (m_currentTab == 1 && m_cfgTab) {
        m_cfgTab->SaveToConfig();
    }

    m_currentTab = sel;

    if (m_envTab)   m_envTab->Show(sel == 0);
    if (m_cfgTab)   m_cfgTab->Show(sel == 1);
    if (m_buildTab) m_buildTab->Show(sel == 2);

    // When entering Configure tab, refresh from config
    if (sel == 1 && m_cfgTab) {
        m_cfgTab->LoadFromConfig();
    }
}

void SparkBuildApp::OnClose() {
    // Save config
    if (m_cfgTab) m_cfgTab->SaveToConfig();
    if (m_config) m_config->Save(ConfigManager::GetDefaultIniPath());

    // Check if a build is running
    if (m_processRunner && m_processRunner->IsRunning()) {
        int result = MessageBoxW(m_hwnd,
            L"A process is still running. Cancel it and exit?",
            L"SparkBuild", MB_YESNO | MB_ICONQUESTION);
        if (result == IDNO) return;
        m_processRunner->Cancel();
    }

    DestroyWindow(m_hwnd);
}

RECT SparkBuildApp::GetTabDisplayArea() const {
    RECT rc;
    GetClientRect(m_hTab, &rc);
    TabCtrl_AdjustRect(m_hTab, FALSE, &rc);
    // Add a small margin
    rc.left += 2;
    rc.top += 2;
    rc.right -= 2;
    rc.bottom -= 2;
    return rc;
}

void SparkBuildApp::UpdateStatusBar(const std::string& text) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wtext(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), wlen);
    SendMessage(m_hStatusBar, SB_SETTEXT, 0, (LPARAM)wtext.c_str());
}

} // namespace SparkBuild
