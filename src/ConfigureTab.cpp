#include "ConfigureTab.h"
#include "Config.h"
#include <shlobj.h>
#include <map>

namespace SparkBuild {

static const wchar_t* CFG_TAB_CLASS = L"SparkBuildCfgTab";
static const wchar_t* CFG_SCROLL_CLASS = L"SparkBuildCfgScroll";
static bool sCfgClassRegistered = false;

LRESULT CALLBACK ConfigureTab::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ConfigureTab* self = nullptr;
    if (msg == WM_CREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<ConfigureTab*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ConfigureTab*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == IDC_BTN_ALL_ON)     self->OnPresetAllOn();
            else if (id == IDC_BTN_ALL_OFF)    self->OnPresetAllOff();
            else if (id == IDC_BTN_DEFAULTS)   self->OnPresetDefaults();
            else if (id == IDC_BTN_MINIMAL)    self->OnPresetMinimal();
            else if (id == IDC_BTN_BROWSE_SRC) self->OnBrowseSource();
            else if (id == IDC_BTN_BROWSE_BLD) self->OnBrowseBuild();
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ConfigureTab::ScrollWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ConfigureTab* self = reinterpret_cast<ConfigureTab*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_VSCROLL: {
            SCROLLINFO si = {};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);

            int oldPos = si.nPos;
            switch (LOWORD(wParam)) {
                case SB_TOP:      si.nPos = si.nMin; break;
                case SB_BOTTOM:   si.nPos = si.nMax; break;
                case SB_LINEUP:   si.nPos -= 20; break;
                case SB_LINEDOWN: si.nPos += 20; break;
                case SB_PAGEUP:   si.nPos -= si.nPage; break;
                case SB_PAGEDOWN: si.nPos += si.nPage; break;
                case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
            }

            si.fMask = SIF_POS;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            GetScrollInfo(hwnd, SB_VERT, &si);

            if (si.nPos != oldPos) {
                ScrollWindow(hwnd, 0, oldPos - si.nPos, nullptr, nullptr);
                UpdateWindow(hwnd);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            SCROLLINFO si = {};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            int oldPos = si.nPos;
            si.nPos -= (delta / WHEEL_DELTA) * 40;
            si.fMask = SIF_POS;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            GetScrollInfo(hwnd, SB_VERT, &si);
            if (si.nPos != oldPos) {
                ScrollWindow(hwnd, 0, oldPos - si.nPos, nullptr, nullptr);
                UpdateWindow(hwnd);
            }
            return 0;
        }
    }
    return CallWindowProc(self->m_origScrollProc, hwnd, msg, wParam, lParam);
}

ConfigureTab::ConfigureTab() {}
ConfigureTab::~ConfigureTab() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

HWND ConfigureTab::Create(HWND hParent, HINSTANCE hInst, RECT area) {
    m_hInst = hInst;

    if (!sCfgClassRegistered) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = CFG_TAB_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&wc);
        sCfgClassRegistered = true;
    }

    m_hwnd = CreateWindowEx(0, CFG_TAB_CLASS, L"",
                            WS_CHILD | WS_CLIPCHILDREN,
                            area.left, area.top,
                            area.right - area.left, area.bottom - area.top,
                            hParent, nullptr, hInst, this);
    CreateControls();
    return m_hwnd;
}

void ConfigureTab::CreateControls() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;

    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT hBoldFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    int y = 5;
    int lbl_w = 100;
    int ctl_w = 200;
    int row_h = 26;
    int col2_x = 350;

    // --- Row 1: Generator + Build Type ---
    auto mkLabel = [&](const wchar_t* text, int x, int ly) -> HWND {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_RIGHT,
                              x, ly + 3, lbl_w, 18, m_hwnd, nullptr, m_hInst, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        return h;
    };

    mkLabel(L"Generator:", 5, y);
    m_hCmbGenerator = CreateWindow(L"COMBOBOX", L"",
                                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                                    lbl_w + 10, y, ctl_w, 200,
                                    m_hwnd, (HMENU)(UINT_PTR)IDC_CMB_GENERATOR, m_hInst, nullptr);
    SendMessage(m_hCmbGenerator, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hCmbGenerator, CB_ADDSTRING, 0, (LPARAM)L"Visual Studio 2022");
    SendMessage(m_hCmbGenerator, CB_ADDSTRING, 0, (LPARAM)L"Visual Studio 2026");
    SendMessage(m_hCmbGenerator, CB_ADDSTRING, 0, (LPARAM)L"Ninja");
    SendMessage(m_hCmbGenerator, CB_ADDSTRING, 0, (LPARAM)L"Unix Makefiles");
    SendMessage(m_hCmbGenerator, CB_SETCURSEL, 0, 0);

    mkLabel(L"Build Type:", col2_x, y);
    m_hRadDebug = CreateWindow(L"BUTTON", L"Debug",
                                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                                col2_x + lbl_w + 10, y, 70, row_h,
                                m_hwnd, (HMENU)(UINT_PTR)IDC_RAD_DEBUG, m_hInst, nullptr);
    m_hRadRelease = CreateWindow(L"BUTTON", L"Release",
                                  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                  col2_x + lbl_w + 85, y, 80, row_h,
                                  m_hwnd, (HMENU)(UINT_PTR)IDC_RAD_RELEASE, m_hInst, nullptr);
    m_hRadRelWithDebInfo = CreateWindow(L"BUTTON", L"RelWithDebInfo",
                                         WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                         col2_x + lbl_w + 170, y, 130, row_h,
                                         m_hwnd, (HMENU)(UINT_PTR)IDC_RAD_RELDBG, m_hInst, nullptr);
    SendMessage(m_hRadDebug, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hRadRelease, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hRadRelWithDebInfo, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hRadRelease, BM_SETCHECK, BST_CHECKED, 0);

    // --- Row 2: MSVC Toolset + Parallel Jobs ---
    y += row_h + 5;
    mkLabel(L"Toolset:", 5, y);
    m_hEdtToolset = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"v143",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                    lbl_w + 10, y, 80, 22,
                                    m_hwnd, (HMENU)(UINT_PTR)IDC_EDT_TOOLSET, m_hInst, nullptr);
    SendMessage(m_hEdtToolset, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hToolsetHint = CreateWindow(L"STATIC", L"(v143=VS2022, v144=VS2026)",
                                      WS_CHILD | WS_VISIBLE | SS_LEFT,
                                      lbl_w + 95, y + 3, 220, 18,
                                      m_hwnd, nullptr, m_hInst, nullptr);
    SendMessage(hToolsetHint, WM_SETFONT, (WPARAM)hFont, TRUE);

    mkLabel(L"Parallel Jobs:", col2_x, y);
    m_hEdtJobs = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
                                 col2_x + lbl_w + 10, y, 50, 22,
                                 m_hwnd, (HMENU)(UINT_PTR)IDC_EDT_JOBS, m_hInst, nullptr);
    SendMessage(m_hEdtJobs, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hJobsHint = CreateWindow(L"STATIC", L"(0 = auto)",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   col2_x + lbl_w + 65, y + 3, 80, 18,
                                   m_hwnd, nullptr, m_hInst, nullptr);
    SendMessage(hJobsHint, WM_SETFONT, (WPARAM)hFont, TRUE);

    // --- Row 3: Source Path ---
    y += row_h + 5;
    mkLabel(L"Source Path:", 5, y);
    m_hEdtSourcePath = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                       lbl_w + 10, y, w - lbl_w - 80, 22,
                                       m_hwnd, (HMENU)(UINT_PTR)IDC_EDT_SOURCE, m_hInst, nullptr);
    SendMessage(m_hEdtSourcePath, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hBtnSrc = CreateWindow(L"BUTTON", L"Browse",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 w - 65, y, 55, 24,
                                 m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_BROWSE_SRC, m_hInst, nullptr);
    SendMessage(hBtnSrc, WM_SETFONT, (WPARAM)hFont, TRUE);

    // --- Row 4: Build Path ---
    y += row_h + 5;
    mkLabel(L"Build Path:", 5, y);
    m_hEdtBuildPath = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"build",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                      lbl_w + 10, y, w - lbl_w - 80, 22,
                                      m_hwnd, (HMENU)(UINT_PTR)IDC_EDT_BUILD, m_hInst, nullptr);
    SendMessage(m_hEdtBuildPath, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hBtnBld = CreateWindow(L"BUTTON", L"Browse",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 w - 65, y, 55, 24,
                                 m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_BROWSE_BLD, m_hInst, nullptr);
    SendMessage(hBtnBld, WM_SETFONT, (WPARAM)hFont, TRUE);

    // --- Preset buttons ---
    y += row_h + 10;
    int btnW = 80, btnH = 28, gap = 8;

    HWND hPresetsLabel = CreateWindow(L"STATIC", L"  Presets:",
                                       WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       5, y + 4, 70, 20,
                                       m_hwnd, nullptr, m_hInst, nullptr);
    SendMessage(hPresetsLabel, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

    int bx = 80;
    m_hBtnAllOn = CreateWindow(L"BUTTON", L"All On", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                bx, y, btnW, btnH, m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_ALL_ON, m_hInst, nullptr);
    bx += btnW + gap;
    m_hBtnAllOff = CreateWindow(L"BUTTON", L"All Off", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 bx, y, btnW, btnH, m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_ALL_OFF, m_hInst, nullptr);
    bx += btnW + gap;
    m_hBtnDefaults = CreateWindow(L"BUTTON", L"Defaults", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   bx, y, btnW, btnH, m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_DEFAULTS, m_hInst, nullptr);
    bx += btnW + gap;
    m_hBtnMinimal = CreateWindow(L"BUTTON", L"Minimal", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  bx, y, btnW, btnH, m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_MINIMAL, m_hInst, nullptr);

    HWND btns[] = { m_hBtnAllOn, m_hBtnAllOff, m_hBtnDefaults, m_hBtnMinimal };
    for (auto b : btns) SendMessage(b, WM_SETFONT, (WPARAM)hFont, TRUE);

    // --- Scrollable area for build options ---
    y += btnH + 10;
    int scrollAreaHeight = h - y - 5;

    m_hScrollArea = CreateWindowEx(WS_EX_CLIENTEDGE, L"STATIC", L"",
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN,
                                    5, y, w - 10, scrollAreaHeight,
                                    m_hwnd, nullptr, m_hInst, nullptr);
    SetWindowLongPtr(m_hScrollArea, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_origScrollProc = (WNDPROC)SetWindowLongPtr(m_hScrollArea, GWLP_WNDPROC,
                                                  reinterpret_cast<LONG_PTR>(ScrollWndProc));

    CreateOptionCheckboxes();
}

void ConfigureTab::CreateOptionCheckboxes() {
    if (!m_config) return;

    HFONT hFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT hCatFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    RECT scrollRC;
    GetClientRect(m_hScrollArea, &scrollRC);
    int scrollW = scrollRC.right - scrollRC.left - 20; // account for scrollbar

    m_optionChecks.clear();

    // Group options by category
    std::map<OptionCategory, std::vector<int>> catMap;
    for (int i = 0; i < (int)m_config->config.options.size(); i++) {
        catMap[m_config->config.options[i].category].push_back(i);
    }

    int y = 5;
    int checkH = 22;
    int catH = 24;
    int colW = scrollW / 2;

    // Preallocate checkbox vector
    m_optionChecks.resize(m_config->config.options.size(), nullptr);

    OptionCategory catOrder[] = {
        OptionCategory::Core, OptionCategory::Graphics, OptionCategory::EditorTools,
        OptionCategory::Scripting, OptionCategory::Gameplay, OptionCategory::Experimental
    };

    for (auto cat : catOrder) {
        auto it = catMap.find(cat);
        if (it == catMap.end()) continue;

        // Category header
        int nameLen = MultiByteToWideChar(CP_UTF8, 0, CategoryDisplayName(cat), -1, nullptr, 0);
        std::wstring catName(nameLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, CategoryDisplayName(cat), -1, catName.data(), nameLen);

        HWND hCatLabel = CreateWindow(L"STATIC", catName.c_str(),
                                       WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       5, y, scrollW, catH,
                                       m_hScrollArea, nullptr, m_hInst, nullptr);
        SendMessage(hCatLabel, WM_SETFONT, (WPARAM)hCatFont, TRUE);
        y += catH;

        // Options in 2 columns
        const auto& indices = it->second;
        for (int j = 0; j < (int)indices.size(); j++) {
            int idx = indices[j];
            const auto& opt = m_config->config.options[idx];

            // Build display text: "Display Name  (CMAKE_VAR)"
            std::string text = opt.displayName;
            int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
            std::wstring wtext(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), wlen);

            int col = j % 2;
            int x = 15 + col * colW;
            if (col == 0 && j > 0) y += checkH;

            HWND hCheck = CreateWindow(L"BUTTON", wtext.c_str(),
                                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                        x, y, colW - 10, checkH,
                                        m_hScrollArea, (HMENU)(UINT_PTR)(IDC_OPT_BASE + idx), m_hInst, nullptr);
            SendMessage(hCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hCheck, BM_SETCHECK, opt.currentValue ? BST_CHECKED : BST_UNCHECKED, 0);

            m_optionChecks[idx] = hCheck;
        }
        // If odd number of items, advance y for last row
        y += checkH + 8;
    }

    m_scrollHeight = y + 10;

    // Set scroll range
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = m_scrollHeight;
    si.nPage = scrollRC.bottom - scrollRC.top;
    SetScrollInfo(m_hScrollArea, SB_VERT, &si, TRUE);
}

void ConfigureTab::Show(bool visible) {
    ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
}

void ConfigureTab::LoadFromConfig() {
    if (!m_config) return;

    // Generator
    int genIdx = 0;
    switch (m_config->config.generator) {
        case Generator::VS2022:        genIdx = 0; break;
        case Generator::VS2026:        genIdx = 1; break;
        case Generator::Ninja:         genIdx = 2; break;
        case Generator::UnixMakefiles: genIdx = 3; break;
    }
    SendMessage(m_hCmbGenerator, CB_SETCURSEL, genIdx, 0);

    // Build type
    SendMessage(m_hRadDebug, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(m_hRadRelease, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessage(m_hRadRelWithDebInfo, BM_SETCHECK, BST_UNCHECKED, 0);
    switch (m_config->config.buildType) {
        case BuildType::Debug:         SendMessage(m_hRadDebug, BM_SETCHECK, BST_CHECKED, 0); break;
        case BuildType::Release:       SendMessage(m_hRadRelease, BM_SETCHECK, BST_CHECKED, 0); break;
        case BuildType::RelWithDebInfo:SendMessage(m_hRadRelWithDebInfo, BM_SETCHECK, BST_CHECKED, 0); break;
    }

    // Toolset
    {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, m_config->config.msvcToolset.c_str(), -1, nullptr, 0);
        std::wstring wt(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, m_config->config.msvcToolset.c_str(), -1, wt.data(), wlen);
        SetWindowText(m_hEdtToolset, wt.c_str());
    }

    // Paths
    auto setEditText = [](HWND hEdit, const std::string& text) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        std::wstring wt(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wt.data(), wlen);
        SetWindowText(hEdit, wt.c_str());
    };
    setEditText(m_hEdtSourcePath, m_config->config.enginePath);
    setEditText(m_hEdtBuildPath, m_config->config.buildPath);

    // Jobs
    SetWindowText(m_hEdtJobs, std::to_wstring(m_config->config.parallelJobs).c_str());

    // Options
    for (int i = 0; i < (int)m_optionChecks.size() && i < (int)m_config->config.options.size(); i++) {
        if (m_optionChecks[i]) {
            SendMessage(m_optionChecks[i], BM_SETCHECK,
                        m_config->config.options[i].currentValue ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }
}

void ConfigureTab::SaveToConfig() {
    if (!m_config) return;

    // Generator
    int genIdx = (int)SendMessage(m_hCmbGenerator, CB_GETCURSEL, 0, 0);
    switch (genIdx) {
        case 0: m_config->config.generator = Generator::VS2022; break;
        case 1: m_config->config.generator = Generator::VS2026; break;
        case 2: m_config->config.generator = Generator::Ninja; break;
        case 3: m_config->config.generator = Generator::UnixMakefiles; break;
    }

    // Build type
    if (SendMessage(m_hRadDebug, BM_GETCHECK, 0, 0) == BST_CHECKED)
        m_config->config.buildType = BuildType::Debug;
    else if (SendMessage(m_hRadRelease, BM_GETCHECK, 0, 0) == BST_CHECKED)
        m_config->config.buildType = BuildType::Release;
    else
        m_config->config.buildType = BuildType::RelWithDebInfo;

    // Read text fields
    auto getEditText = [](HWND hEdit) -> std::string {
        int len = GetWindowTextLengthW(hEdit);
        std::wstring wt(len + 1, 0);
        GetWindowTextW(hEdit, wt.data(), len + 1);
        int mbLen = WideCharToMultiByte(CP_UTF8, 0, wt.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string s(mbLen, 0);
        WideCharToMultiByte(CP_UTF8, 0, wt.c_str(), -1, s.data(), mbLen, nullptr, nullptr);
        if (!s.empty() && s.back() == '\0') s.pop_back();
        return s;
    };

    m_config->config.msvcToolset = getEditText(m_hEdtToolset);
    m_config->config.enginePath = getEditText(m_hEdtSourcePath);
    m_config->config.buildPath = getEditText(m_hEdtBuildPath);
    m_config->config.parallelJobs = std::atoi(getEditText(m_hEdtJobs).c_str());

    // Options
    for (int i = 0; i < (int)m_optionChecks.size() && i < (int)m_config->config.options.size(); i++) {
        if (m_optionChecks[i]) {
            m_config->config.options[i].currentValue =
                (SendMessage(m_optionChecks[i], BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
    }
}

void ConfigureTab::OnPresetAllOn() {
    if (!m_config) return;
    m_config->ApplyPresetAllOn();
    for (int i = 0; i < (int)m_optionChecks.size(); i++) {
        if (m_optionChecks[i])
            SendMessage(m_optionChecks[i], BM_SETCHECK, BST_CHECKED, 0);
    }
}

void ConfigureTab::OnPresetAllOff() {
    if (!m_config) return;
    m_config->ApplyPresetAllOff();
    for (int i = 0; i < (int)m_optionChecks.size(); i++) {
        if (m_optionChecks[i])
            SendMessage(m_optionChecks[i], BM_SETCHECK, BST_UNCHECKED, 0);
    }
}

void ConfigureTab::OnPresetDefaults() {
    if (!m_config) return;
    m_config->ApplyPresetDefaults();
    LoadFromConfig();
}

void ConfigureTab::OnPresetMinimal() {
    if (!m_config) return;
    m_config->ApplyPresetMinimal();
    LoadFromConfig();
}

void ConfigureTab::OnBrowseSource() {
    BROWSEINFOW bi = {};
    bi.hwndOwner = m_hwnd;
    bi.lpszTitle = L"Select SparkEngine source directory:";
    bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;

    wchar_t path[MAX_PATH] = {};
    SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    SetWindowText(m_hEdtSourcePath, path);
}

void ConfigureTab::OnBrowseBuild() {
    BROWSEINFOW bi = {};
    bi.hwndOwner = m_hwnd;
    bi.lpszTitle = L"Select build output directory:";
    bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;

    wchar_t path[MAX_PATH] = {};
    SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    SetWindowText(m_hEdtBuildPath, path);
}

} // namespace SparkBuild
