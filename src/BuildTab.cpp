#include "BuildTab.h"
#include "Config.h"
#include "ProcessRunner.h"
#include "ConfigureTab.h"
#include <filesystem>
#include <shellapi.h>
#include <thread>

namespace SparkBuild {

static const wchar_t* BUILD_TAB_CLASS = L"SparkBuildBuildTab";
static bool sBuildClassRegistered = false;

#define WM_BUILD_LOG    (WM_USER + 200)
#define WM_BUILD_STATE  (WM_USER + 201)
#define WM_BUILD_DONE   (WM_USER + 202)

LRESULT CALLBACK BuildTab::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BuildTab* self = nullptr;
    if (msg == WM_CREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<BuildTab*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<BuildTab*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case IDC_BTN_GENERATE:  self->OnGenerate(); break;
                case IDC_BTN_BUILD:     self->OnBuild(); break;
                case IDC_BTN_GEN_BUILD: self->OnGenerateAndBuild(); break;
                case IDC_BTN_CANCEL:    self->OnCancel(); break;
                case IDC_BTN_CLEAN:     self->OnClean(); break;
                case IDC_BTN_OPEN_DIR:  self->OnOpenBuildFolder(); break;
                case IDC_BTN_OPEN_SLN:  self->OnOpenSolution(); break;
                case IDC_BTN_RUN:       self->OnRunEngine(); break;
            }
            return 0;
        }
        case WM_BUILD_LOG: {
            auto* str = reinterpret_cast<std::string*>(lParam);
            if (str) {
                self->AppendLog(*str);
                delete str;
            }
            return 0;
        }
        case WM_BUILD_STATE: {
            self->SetState(static_cast<BuildState>(wParam));
            return 0;
        }
        case WM_BUILD_DONE: {
            bool success = (wParam != 0);
            if (success && self->m_buildAfterGenerate) {
                self->m_buildAfterGenerate = false;
                self->OnBuild();
            } else {
                self->SetState(success ? BuildState::Done : BuildState::Error);
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

BuildTab::BuildTab() {}
BuildTab::~BuildTab() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

HWND BuildTab::Create(HWND hParent, HINSTANCE hInst, RECT area) {
    m_hInst = hInst;

    if (!sBuildClassRegistered) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = BUILD_TAB_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&wc);
        sBuildClassRegistered = true;
    }

    m_hwnd = CreateWindowEx(0, BUILD_TAB_CLASS, L"",
                            WS_CHILD | WS_CLIPCHILDREN,
                            area.left, area.top,
                            area.right - area.left, area.bottom - area.top,
                            hParent, nullptr, hInst, this);
    CreateControls();
    return m_hwnd;
}

void BuildTab::CreateControls() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;

    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT hBoldFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT hMonoFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

    int y = 5;
    int btnW = 125;
    int btnH = 32;
    int gap = 8;

    // Title
    HWND hTitle = CreateWindow(L"STATIC", L"  Build Actions",
                               WS_CHILD | WS_VISIBLE | SS_LEFT,
                               0, y, 200, 24, m_hwnd, nullptr, m_hInst, nullptr);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hBoldFont, TRUE);
    y += 28;

    // Action buttons row
    int bx = 10;
    m_hBtnGenerate = CreateWindow(L"BUTTON", L"Generate Project",
                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   bx, y, btnW + 10, btnH,
                                   m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_GENERATE, m_hInst, nullptr);
    bx += btnW + 10 + gap;

    m_hBtnBuild = CreateWindow(L"BUTTON", L"Build Project",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                bx, y, btnW, btnH,
                                m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_BUILD, m_hInst, nullptr);
    bx += btnW + gap;

    m_hBtnGenAndBuild = CreateWindow(L"BUTTON", L"Generate && Build",
                                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      bx, y, btnW + 15, btnH,
                                      m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_GEN_BUILD, m_hInst, nullptr);
    bx += btnW + 15 + gap;

    m_hBtnCancel = CreateWindow(L"BUTTON", L"Cancel",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                                 bx, y, 80, btnH,
                                 m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_CANCEL, m_hInst, nullptr);
    bx += 80 + gap;

    m_hBtnClean = CreateWindow(L"BUTTON", L"Clean",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                bx, y, 70, btnH,
                                m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_CLEAN, m_hInst, nullptr);

    HWND actionBtns[] = { m_hBtnGenerate, m_hBtnBuild, m_hBtnGenAndBuild, m_hBtnCancel, m_hBtnClean };
    for (auto b : actionBtns) SendMessage(b, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Progress bar & state label
    y += btnH + 10;
    m_hStateLabel = CreateWindow(L"STATIC", L"  Ready",
                                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  10, y, 200, 20,
                                  m_hwnd, (HMENU)(UINT_PTR)IDC_STATE_LABEL, m_hInst, nullptr);
    SendMessage(m_hStateLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hProgress = CreateWindowEx(0, PROGRESS_CLASS, L"",
                                  WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                  10, y + 22, w - 20, 16,
                                  m_hwnd, (HMENU)(UINT_PTR)IDC_PROGRESS, m_hInst, nullptr);

    // Post-build action buttons
    y += 48;
    int pbx = 10;
    m_hBtnOpenFolder = CreateWindow(L"BUTTON", L"Open Build Folder",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     pbx, y, btnW + 15, 28,
                                     m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_OPEN_DIR, m_hInst, nullptr);
    pbx += btnW + 15 + gap;

    m_hBtnOpenSln = CreateWindow(L"BUTTON", L"Open Solution",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  pbx, y, btnW, 28,
                                  m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_OPEN_SLN, m_hInst, nullptr);
    pbx += btnW + gap;

    m_hBtnRun = CreateWindow(L"BUTTON", L"Run Engine",
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              pbx, y, btnW - 20, 28,
                              m_hwnd, (HMENU)(UINT_PTR)IDC_BTN_RUN, m_hInst, nullptr);

    HWND postBtns[] = { m_hBtnOpenFolder, m_hBtnOpenSln, m_hBtnRun };
    for (auto b : postBtns) SendMessage(b, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Build log
    y += 36;
    HWND hLogLabel = CreateWindow(L"STATIC", L"  Build Output:",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   10, y, 200, 20,
                                   m_hwnd, nullptr, m_hInst, nullptr);
    SendMessage(hLogLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += 22;

    m_hLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                            10, y, w - 20, h - y - 10,
                            m_hwnd, (HMENU)(UINT_PTR)IDC_LOG, m_hInst, nullptr);
    SendMessage(m_hLog, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

    // Set edit control text limit to 2MB
    SendMessage(m_hLog, EM_SETLIMITTEXT, 2 * 1024 * 1024, 0);
}

void BuildTab::Show(bool visible) {
    ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
}

void BuildTab::AppendLog(const std::string& msg) {
    if (!m_hLog) return;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, nullptr, 0);
    std::wstring wmsg(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, wmsg.data(), wlen);

    int len = GetWindowTextLength(m_hLog);
    SendMessage(m_hLog, EM_SETSEL, len, len);
    SendMessage(m_hLog, EM_REPLACESEL, FALSE, (LPARAM)wmsg.c_str());
    SendMessage(m_hLog, EM_SETSEL, len, len);
    SendMessage(m_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    SendMessage(m_hLog, EM_SCROLLCARET, 0, 0);
}

void BuildTab::SetState(BuildState state) {
    m_state = state;

    const wchar_t* text = L"  Ready";
    switch (state) {
        case BuildState::Idle:        text = L"  Ready"; break;
        case BuildState::Configuring: text = L"  Configuring (CMake)..."; break;
        case BuildState::Building:    text = L"  Building..."; break;
        case BuildState::Done:        text = L"  Build Complete!"; break;
        case BuildState::Error:       text = L"  Build Failed!"; break;
        case BuildState::Cancelled:   text = L"  Cancelled"; break;
    }
    SetWindowText(m_hStateLabel, text);

    // Progress bar
    if (state == BuildState::Configuring || state == BuildState::Building) {
        // Marquee mode
        SetWindowLong(m_hProgress, GWL_STYLE,
                      GetWindowLong(m_hProgress, GWL_STYLE) | PBS_MARQUEE);
        SendMessage(m_hProgress, PBM_SETMARQUEE, TRUE, 30);
    } else {
        SetWindowLong(m_hProgress, GWL_STYLE,
                      GetWindowLong(m_hProgress, GWL_STYLE) & ~PBS_MARQUEE);
        SendMessage(m_hProgress, PBM_SETMARQUEE, FALSE, 0);
        if (state == BuildState::Done) {
            SendMessage(m_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessage(m_hProgress, PBM_SETPOS, 100, 0);
        } else if (state == BuildState::Error || state == BuildState::Cancelled) {
            SendMessage(m_hProgress, PBM_SETPOS, 0, 0);
        }
    }

    UpdateButtons();
}

void BuildTab::UpdateButtons() {
    bool busy = (m_state == BuildState::Configuring || m_state == BuildState::Building);
    EnableWindow(m_hBtnGenerate, !busy);
    EnableWindow(m_hBtnBuild, !busy);
    EnableWindow(m_hBtnGenAndBuild, !busy);
    EnableWindow(m_hBtnCancel, busy);
    EnableWindow(m_hBtnClean, !busy);
}

void BuildTab::OnGenerate() {
    if (!m_config || !m_processRunner) return;

    // Save current UI state to config
    if (m_configureTab) m_configureTab->SaveToConfig();

    m_buildAfterGenerate = false;

    // Clear log
    SetWindowText(m_hLog, L"");

    std::string cmd = m_config->BuildCMakeConfigureCommand();
    AppendLog(">>> " + cmd);
    AppendLog("");

    SetState(BuildState::Configuring);

    HWND hwnd = m_hwnd;
    m_processRunner->RunAsync(cmd, m_config->config.enginePath,
        [hwnd](const std::string& line) {
            auto* msg = new std::string(line);
            PostMessage(hwnd, WM_BUILD_LOG, 0, (LPARAM)msg);
        },
        [hwnd](int exitCode, bool success) {
            auto* msg = new std::string(success ?
                "\n=== Configuration complete ===" :
                "\n=== Configuration FAILED (exit code " + std::to_string(exitCode) + ") ===");
            PostMessage(hwnd, WM_BUILD_LOG, 0, (LPARAM)msg);
            PostMessage(hwnd, WM_BUILD_DONE, success ? 1 : 0, 0);
        });
}

void BuildTab::OnBuild() {
    if (!m_config || !m_processRunner) return;

    if (m_configureTab) m_configureTab->SaveToConfig();

    std::string cmd = m_config->BuildCMakeBuildCommand();
    AppendLog(">>> " + cmd);
    AppendLog("");

    SetState(BuildState::Building);

    HWND hwnd = m_hwnd;
    m_processRunner->RunAsync(cmd, m_config->config.enginePath,
        [hwnd](const std::string& line) {
            auto* msg = new std::string(line);
            PostMessage(hwnd, WM_BUILD_LOG, 0, (LPARAM)msg);
        },
        [hwnd](int exitCode, bool success) {
            auto* msg = new std::string(success ?
                "\n=== Build complete ===" :
                "\n=== Build FAILED (exit code " + std::to_string(exitCode) + ") ===");
            PostMessage(hwnd, WM_BUILD_LOG, 0, (LPARAM)msg);
            PostMessage(hwnd, WM_BUILD_DONE, success ? 1 : 0, 0);
        });
}

void BuildTab::OnGenerateAndBuild() {
    m_buildAfterGenerate = true;
    OnGenerate();
}

void BuildTab::OnCancel() {
    if (m_processRunner) {
        m_processRunner->Cancel();
        m_buildAfterGenerate = false;
        SetState(BuildState::Cancelled);
        AppendLog("\n=== Cancelled by user ===");
    }
}

void BuildTab::OnClean() {
    if (!m_config) return;

    std::string buildDir = m_config->config.buildPath;
    if (buildDir.empty()) buildDir = "build";

    // Make absolute if relative
    if (!std::filesystem::path(buildDir).is_absolute() && !m_config->config.enginePath.empty()) {
        buildDir = m_config->config.enginePath + "\\" + buildDir;
    }

    if (!std::filesystem::exists(buildDir)) {
        AppendLog("Build directory does not exist: " + buildDir);
        return;
    }

    int result = MessageBoxW(m_hwnd,
        L"This will delete the entire build directory. Continue?",
        L"Clean Build", MB_YESNO | MB_ICONWARNING);

    if (result == IDYES) {
        AppendLog("Cleaning build directory: " + buildDir);
        std::error_code ec;
        std::filesystem::remove_all(buildDir, ec);
        if (ec) {
            AppendLog("Error cleaning: " + ec.message());
        } else {
            AppendLog("Build directory cleaned.");
        }
    }
}

void BuildTab::OnOpenBuildFolder() {
    if (!m_config) return;
    std::string buildDir = m_config->config.buildPath;
    if (buildDir.empty()) buildDir = "build";

    if (!std::filesystem::path(buildDir).is_absolute() && !m_config->config.enginePath.empty()) {
        buildDir = m_config->config.enginePath + "\\" + buildDir;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, buildDir.c_str(), -1, nullptr, 0);
    std::wstring wdir(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, buildDir.c_str(), -1, wdir.data(), wlen);
    ShellExecuteW(m_hwnd, L"explore", wdir.c_str(), nullptr, nullptr, SW_SHOW);
}

void BuildTab::OnOpenSolution() {
    std::string slnPath = FindSolutionFile();
    if (slnPath.empty()) {
        MessageBoxW(m_hwnd, L"No .sln file found in build directory.", L"Open Solution", MB_OK | MB_ICONINFORMATION);
        return;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, slnPath.c_str(), -1, nullptr, 0);
    std::wstring wsln(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, slnPath.c_str(), -1, wsln.data(), wlen);
    ShellExecuteW(m_hwnd, L"open", wsln.c_str(), nullptr, nullptr, SW_SHOW);
}

void BuildTab::OnRunEngine() {
    std::string exePath = FindEngineExe();
    if (exePath.empty()) {
        MessageBoxW(m_hwnd, L"Engine executable not found. Build the project first.", L"Run Engine", MB_OK | MB_ICONINFORMATION);
        return;
    }

    AppendLog("Launching: " + exePath);
    int wlen = MultiByteToWideChar(CP_UTF8, 0, exePath.c_str(), -1, nullptr, 0);
    std::wstring wexe(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, exePath.c_str(), -1, wexe.data(), wlen);

    // Get the directory containing the exe
    std::filesystem::path p(exePath);
    std::wstring wdir = p.parent_path().wstring();

    ShellExecuteW(m_hwnd, L"open", wexe.c_str(), nullptr, wdir.c_str(), SW_SHOW);
}

std::string BuildTab::FindSolutionFile() {
    if (!m_config) return "";
    std::string buildDir = m_config->config.buildPath;
    if (buildDir.empty()) buildDir = "build";
    if (!std::filesystem::path(buildDir).is_absolute() && !m_config->config.enginePath.empty()) {
        buildDir = m_config->config.enginePath + "\\" + buildDir;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(buildDir)) {
            if (entry.path().extension() == ".sln") {
                return entry.path().string();
            }
        }
    } catch (...) {}

    return "";
}

std::string BuildTab::FindEngineExe() {
    if (!m_config) return "";
    std::string buildDir = m_config->config.buildPath;
    if (buildDir.empty()) buildDir = "build";
    if (!std::filesystem::path(buildDir).is_absolute() && !m_config->config.enginePath.empty()) {
        buildDir = m_config->config.enginePath + "\\" + buildDir;
    }

    // Look in bin/ subdirectory
    std::string binDir = buildDir + "\\bin";
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(binDir)) {
            if (entry.path().extension() == ".exe") {
                std::string name = entry.path().stem().string();
                // Look for the engine executable (not editor, not tests)
                if (name.find("Spark") != std::string::npos && name.find("Test") == std::string::npos) {
                    return entry.path().string();
                }
            }
        }
    } catch (...) {}

    return "";
}

} // namespace SparkBuild
