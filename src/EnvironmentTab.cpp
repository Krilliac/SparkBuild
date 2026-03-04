#include "EnvironmentTab.h"
#include "ProcessRunner.h"
#include "Downloader.h"
#include "Config.h"
#include <shlobj.h>
#include <thread>
#include <sstream>
#include <filesystem>

namespace SparkBuild {

static const wchar_t* ENV_TAB_CLASS = L"SparkBuildEnvTab";
static bool sEnvClassRegistered = false;

// Custom message for posting UI updates from threads
#define WM_ENV_UPDATE (WM_USER + 100)
#define WM_ENV_LOG    (WM_USER + 101)

EnvironmentTab::EnvironmentTab() {
    // Initialize dependency items
    m_items.push_back({"Engine Repository", "SparkEngine source code", "", DependencyStatus::Unknown, true});
    m_items.push_back({"Git",               "Version control system",  "", DependencyStatus::Unknown, false});
    m_items.push_back({"CMake",             "Build system generator",  "", DependencyStatus::Unknown, true});
    m_items.push_back({"C++ Compiler",      "MSVC / GCC / Clang",      "", DependencyStatus::Unknown, false});
    m_items.push_back({"Git Submodules",    "Third-party dependencies","", DependencyStatus::Unknown, true});
}

EnvironmentTab::~EnvironmentTab() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

LRESULT CALLBACK EnvironmentTab::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    EnvironmentTab* self = nullptr;
    if (msg == WM_CREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = static_cast<EnvironmentTab*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<EnvironmentTab*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_CHECK_ALL)    self->OnCheckAll();
            else if (id == IDC_INSTALL_ALL) self->OnInstallAll();
            else if (id == IDC_INIT_SUB)    self->OnInitSubmodules();
            else if (id == IDC_CLONE_REPO)  self->OnCloneRepo();
            else if (id == IDC_DL_CMAKE)    self->OnDownloadCMake();
            return 0;
        }
        case WM_ENV_UPDATE:
            self->UpdateUI();
            return 0;
        case WM_ENV_LOG: {
            // lParam is a pointer to a heap-allocated string
            auto str = reinterpret_cast<std::string*>(lParam);
            if (str) {
                self->AppendLog(*str);
                delete str;
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND EnvironmentTab::Create(HWND hParent, HINSTANCE hInst, RECT area) {
    m_hInst = hInst;

    if (!sEnvClassRegistered) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = ENV_TAB_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&wc);
        sEnvClassRegistered = true;
    }

    m_hwnd = CreateWindowEx(0, ENV_TAB_CLASS, L"",
                            WS_CHILD | WS_CLIPCHILDREN,
                            area.left, area.top,
                            area.right - area.left, area.bottom - area.top,
                            hParent, nullptr, hInst, this);

    CreateControls();
    return m_hwnd;
}

void EnvironmentTab::CreateControls() {
    int w = 0, h = 0;
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        w = rc.right;
        h = rc.bottom;
    }

    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    // Title label
    HWND hTitle = CreateWindow(L"STATIC", L"  Development Environment",
                               WS_CHILD | WS_VISIBLE | SS_LEFT,
                               0, 0, w, 28, m_hwnd, nullptr, m_hInst, nullptr);
    HFONT hTitleFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

    // ListView for dependencies
    m_hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
                             WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
                             10, 35, w - 20, 180,
                             m_hwnd, (HMENU)(UINT_PTR)IDC_LIST, m_hInst, nullptr);
    ListView_SetExtendedListViewStyle(m_hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    SendMessage(m_hList, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Add columns
    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt = LVCFMT_LEFT;

    col.pszText = const_cast<wchar_t*>(L"Component");
    col.cx = 160;
    ListView_InsertColumn(m_hList, 0, &col);

    col.pszText = const_cast<wchar_t*>(L"Status");
    col.cx = 100;
    ListView_InsertColumn(m_hList, 1, &col);

    col.pszText = const_cast<wchar_t*>(L"Version / Info");
    col.cx = 250;
    ListView_InsertColumn(m_hList, 2, &col);

    col.pszText = const_cast<wchar_t*>(L"Description");
    col.cx = w - 560;
    ListView_InsertColumn(m_hList, 3, &col);

    // Populate rows
    for (int i = 0; i < (int)m_items.size(); i++) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, m_items[i].name.c_str(), -1, nullptr, 0);
        std::wstring wname(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, m_items[i].name.c_str(), -1, wname.data(), wlen);

        LVITEM item = {};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.pszText = wname.data();
        ListView_InsertItem(m_hList, &item);

        ListView_SetItemText(m_hList, i, 1, const_cast<wchar_t*>(L"Not Checked"));

        int dlen = MultiByteToWideChar(CP_UTF8, 0, m_items[i].description.c_str(), -1, nullptr, 0);
        std::wstring wdesc(dlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, m_items[i].description.c_str(), -1, wdesc.data(), dlen);
        ListView_SetItemText(m_hList, i, 3, wdesc.data());
    }

    // Buttons row
    int btnY = 225;
    int btnW = 130;
    int btnH = 30;
    int btnGap = 10;

    m_hBtnCheckAll = CreateWindow(L"BUTTON", L"Check All",
                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   10, btnY, btnW, btnH,
                                   m_hwnd, (HMENU)(UINT_PTR)IDC_CHECK_ALL, m_hInst, nullptr);

    m_hBtnInstallAll = CreateWindow(L"BUTTON", L"Install All",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     10 + (btnW + btnGap), btnY, btnW, btnH,
                                     m_hwnd, (HMENU)(UINT_PTR)IDC_INSTALL_ALL, m_hInst, nullptr);

    m_hBtnCloneRepo = CreateWindow(L"BUTTON", L"Clone Engine",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    10 + 2 * (btnW + btnGap), btnY, btnW, btnH,
                                    m_hwnd, (HMENU)(UINT_PTR)IDC_CLONE_REPO, m_hInst, nullptr);

    m_hBtnDownloadCMake = CreateWindow(L"BUTTON", L"Download CMake",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        10 + 3 * (btnW + btnGap), btnY, btnW + 10, btnH,
                                        m_hwnd, (HMENU)(UINT_PTR)IDC_DL_CMAKE, m_hInst, nullptr);

    m_hBtnInitSub = CreateWindow(L"BUTTON", L"Init Submodules",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  10 + 4 * (btnW + btnGap) + 10, btnY, btnW + 10, btnH,
                                  m_hwnd, (HMENU)(UINT_PTR)IDC_INIT_SUB, m_hInst, nullptr);

    // Set fonts on buttons
    HWND btns[] = { m_hBtnCheckAll, m_hBtnInstallAll, m_hBtnCloneRepo, m_hBtnDownloadCMake, m_hBtnInitSub };
    for (auto b : btns) SendMessage(b, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Progress bar
    m_hProgress = CreateWindowEx(0, PROGRESS_CLASS, L"",
                                  WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                  10, btnY + btnH + 10, w - 20, 18,
                                  m_hwnd, (HMENU)(UINT_PTR)IDC_PROGRESS, m_hInst, nullptr);

    // Log output
    HWND hLogLabel = CreateWindow(L"STATIC", L"  Log Output:",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   10, btnY + btnH + 35, 100, 20,
                                   m_hwnd, nullptr, m_hInst, nullptr);
    SendMessage(hLogLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    int logTop = btnY + btnH + 55;
    m_hLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                            10, logTop, w - 20, h - logTop - 10,
                            m_hwnd, (HMENU)(UINT_PTR)IDC_LOG, m_hInst, nullptr);

    HFONT hMonoFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    SendMessage(m_hLog, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
}

void EnvironmentTab::Show(bool visible) {
    ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
}

void EnvironmentTab::SetItemStatus(int index, DependencyStatus status, const std::string& version) {
    if (index < 0 || index >= (int)m_items.size()) return;
    m_items[index].status = status;
    if (!version.empty()) m_items[index].version = version;
    PostMessage(m_hwnd, WM_ENV_UPDATE, 0, 0);
}

void EnvironmentTab::UpdateUI() {
    for (int i = 0; i < (int)m_items.size(); i++) {
        const wchar_t* statusText = L"Unknown";
        switch (m_items[i].status) {
            case DependencyStatus::Unknown:    statusText = L"Not Checked"; break;
            case DependencyStatus::Checking:   statusText = L"Checking..."; break;
            case DependencyStatus::Found:      statusText = L"Found"; break;
            case DependencyStatus::NotFound:   statusText = L"NOT FOUND"; break;
            case DependencyStatus::Installing: statusText = L"Installing..."; break;
        }
        ListView_SetItemText(m_hList, i, 1, const_cast<wchar_t*>(statusText));

        if (!m_items[i].version.empty()) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, m_items[i].version.c_str(), -1, nullptr, 0);
            std::wstring wver(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, m_items[i].version.c_str(), -1, wver.data(), wlen);
            ListView_SetItemText(m_hList, i, 2, wver.data());
        }
    }
}

void EnvironmentTab::AppendLog(const std::string& msg) {
    if (!m_hLog) return;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, nullptr, 0);
    std::wstring wmsg(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, wmsg.data(), wlen);

    int len = GetWindowTextLength(m_hLog);
    SendMessage(m_hLog, EM_SETSEL, len, len);
    SendMessage(m_hLog, EM_REPLACESEL, FALSE, (LPARAM)(wmsg.c_str()));
    SendMessage(m_hLog, EM_SETSEL, len, len);
    SendMessage(m_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");

    // Auto-scroll to bottom
    SendMessage(m_hLog, EM_SCROLLCARET, 0, 0);
}

void EnvironmentTab::CheckAll() {
    for (auto& item : m_items) item.status = DependencyStatus::Checking;
    UpdateUI();

    std::thread([this]() {
        CheckEngineRepo();
        CheckGit();
        CheckCMake();
        CheckCompilers();
        CheckSubmodules();

        auto* msg = new std::string("Environment check complete.");
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    }).detach();
}

void EnvironmentTab::CheckGit() {
    SetItemStatus(1, DependencyStatus::Checking);
    ProcessRunner pr;
    std::string output;
    int rc = pr.RunSync("git --version", "", output);

    if (rc == 0 && output.find("git version") != std::string::npos) {
        // Extract version
        auto pos = output.find("git version ");
        std::string ver = (pos != std::string::npos) ? output.substr(pos + 12) : output;
        // Trim whitespace
        while (!ver.empty() && (ver.back() == '\r' || ver.back() == '\n' || ver.back() == ' '))
            ver.pop_back();
        SetItemStatus(1, DependencyStatus::Found, ver);
        auto* msg = new std::string("Git found: " + ver);
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    } else {
        SetItemStatus(1, DependencyStatus::NotFound);
        auto* msg = new std::string("Git not found. Please install Git from https://git-scm.com");
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    }
}

void EnvironmentTab::CheckCMake() {
    SetItemStatus(2, DependencyStatus::Checking);

    // First check config path, then PATH
    std::string cmakeCmd = "cmake";
    if (m_config && !m_config->config.cmakePath.empty()) {
        cmakeCmd = "\"" + m_config->config.cmakePath + "\"";
    }

    ProcessRunner pr;
    std::string output;
    int rc = pr.RunSync(cmakeCmd + " --version", "", output);

    if (rc == 0 && output.find("cmake version") != std::string::npos) {
        auto pos = output.find("cmake version ");
        std::string ver = "";
        if (pos != std::string::npos) {
            ver = output.substr(pos + 14);
            auto nl = ver.find_first_of("\r\n");
            if (nl != std::string::npos) ver = ver.substr(0, nl);
        }
        SetItemStatus(2, DependencyStatus::Found, ver);
        auto* msg = new std::string("CMake found: " + ver);
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    } else {
        SetItemStatus(2, DependencyStatus::NotFound);
        auto* msg = new std::string("CMake not found. Use 'Download CMake' button to install.");
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    }
}

void EnvironmentTab::CheckCompilers() {
    SetItemStatus(3, DependencyStatus::Checking);

    // Try to find Visual Studio using vswhere
    ProcessRunner pr;
    std::string output;
    std::string vswhere = "\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\"";
    vswhere += " -latest -property installationVersion";

    int rc = pr.RunSync(vswhere, "", output);
    if (rc == 0 && !output.empty()) {
        // Trim
        while (!output.empty() && (output.back() == '\r' || output.back() == '\n'))
            output.pop_back();

        std::string ver = "MSVC (VS " + output + ")";
        SetItemStatus(3, DependencyStatus::Found, ver);
        auto* msg = new std::string("Compiler found: " + ver);
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        return;
    }

    // Try cl.exe
    rc = pr.RunSync("cl", "", output);
    if (output.find("Microsoft") != std::string::npos) {
        SetItemStatus(3, DependencyStatus::Found, "MSVC (cl.exe on PATH)");
        auto* msg = new std::string("Compiler found: MSVC (cl.exe)");
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        return;
    }

    // Try g++
    rc = pr.RunSync("g++ --version", "", output);
    if (rc == 0 && output.find("g++") != std::string::npos) {
        auto pos = output.find('\n');
        std::string firstLine = (pos != std::string::npos) ? output.substr(0, pos) : output;
        SetItemStatus(3, DependencyStatus::Found, firstLine);
        auto* msg = new std::string("Compiler found: " + firstLine);
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        return;
    }

    // Try clang++
    rc = pr.RunSync("clang++ --version", "", output);
    if (rc == 0 && output.find("clang") != std::string::npos) {
        auto pos = output.find('\n');
        std::string firstLine = (pos != std::string::npos) ? output.substr(0, pos) : output;
        SetItemStatus(3, DependencyStatus::Found, firstLine);
        auto* msg = new std::string("Compiler found: " + firstLine);
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        return;
    }

    SetItemStatus(3, DependencyStatus::NotFound);
    auto* msg = new std::string("No C++ compiler found. Install Visual Studio, GCC, or Clang.");
    PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
}

void EnvironmentTab::CheckSubmodules() {
    SetItemStatus(4, DependencyStatus::Checking);

    if (!m_config || m_config->config.enginePath.empty()) {
        SetItemStatus(4, DependencyStatus::NotFound, "Engine path not set");
        return;
    }

    ProcessRunner pr;
    std::string output;
    int rc = pr.RunSync("git submodule status", m_config->config.enginePath, output);

    if (rc != 0) {
        SetItemStatus(4, DependencyStatus::NotFound, "Not a git repository");
        return;
    }

    // Parse submodule status
    int total = 0, initialized = 0, missing = 0;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        total++;
        if (line[0] == '-') {
            missing++;
        } else {
            initialized++;
        }
    }

    if (total == 0) {
        SetItemStatus(4, DependencyStatus::Found, "No submodules");
    } else if (missing == 0) {
        SetItemStatus(4, DependencyStatus::Found,
                      std::to_string(initialized) + "/" + std::to_string(total) + " initialized");
        auto* msg = new std::string("All " + std::to_string(total) + " submodules initialized.");
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    } else {
        SetItemStatus(4, DependencyStatus::NotFound,
                      std::to_string(missing) + "/" + std::to_string(total) + " missing");
        auto* msg = new std::string(std::to_string(missing) + " of " + std::to_string(total) +
                                    " submodules not initialized. Click 'Init Submodules'.");
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    }
}

void EnvironmentTab::CheckEngineRepo() {
    SetItemStatus(0, DependencyStatus::Checking);

    std::string enginePath;
    if (m_config && !m_config->config.enginePath.empty()) {
        enginePath = m_config->config.enginePath;
    } else {
        // Try to detect: look for CMakeLists.txt in parent directories
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path p(exePath);

        // Walk up looking for SparkEngine's CMakeLists.txt
        for (auto dir = p.parent_path(); dir != dir.root_path(); dir = dir.parent_path()) {
            if (std::filesystem::exists(dir / "CMakeLists.txt") &&
                std::filesystem::exists(dir / "Spark Engine")) {
                enginePath = dir.string();
                break;
            }
        }
    }

    if (!enginePath.empty() && std::filesystem::exists(enginePath + "/CMakeLists.txt")) {
        SetItemStatus(0, DependencyStatus::Found, enginePath);
        if (m_config) m_config->config.enginePath = enginePath;
        auto* msg = new std::string("Engine found at: " + enginePath);
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    } else {
        SetItemStatus(0, DependencyStatus::NotFound, "Not found");
        auto* msg = new std::string("Engine repository not found. Use 'Clone Engine' to download.");
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    }
}

void EnvironmentTab::OnCheckAll() {
    AppendLog("Starting environment check...");
    SendMessage(m_hProgress, PBM_SETMARQUEE, TRUE, 30);
    SetWindowLong(m_hProgress, GWL_STYLE,
                  GetWindowLong(m_hProgress, GWL_STYLE) | PBS_MARQUEE);
    SendMessage(m_hProgress, PBM_SETMARQUEE, TRUE, 30);
    CheckAll();
}

void EnvironmentTab::OnInstallAll() {
    AppendLog("Installing all missing dependencies...");

    std::thread([this]() {
        // Check what's missing and install
        for (int i = 0; i < (int)m_items.size(); i++) {
            if (m_items[i].status == DependencyStatus::NotFound && m_items[i].canAutoInstall) {
                switch (i) {
                    case 0: OnCloneRepo(); break;
                    case 2: OnDownloadCMake(); break;
                    case 4: OnInitSubmodules(); break;
                }
            }
        }
        auto* msg = new std::string("Install all complete.");
        PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
    }).detach();
}

void EnvironmentTab::OnInitSubmodules() {
    if (!m_config || m_config->config.enginePath.empty()) {
        AppendLog("Error: Engine path not set. Cannot initialize submodules.");
        return;
    }

    AppendLog("Initializing git submodules (this may take a while)...");
    SetItemStatus(4, DependencyStatus::Installing);

    std::thread([this]() {
        ProcessRunner pr;
        std::string output;
        int rc = pr.RunSync("git submodule update --init --recursive",
                            m_config->config.enginePath, output);

        if (rc == 0) {
            SetItemStatus(4, DependencyStatus::Found, "All initialized");
            auto* msg = new std::string("Submodules initialized successfully.");
            PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        } else {
            SetItemStatus(4, DependencyStatus::NotFound, "Init failed");
            auto* msg = new std::string("Submodule init failed:\n" + output);
            PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        }
    }).detach();
}

void EnvironmentTab::OnCloneRepo() {
    // Ask user for destination via folder browse dialog
    BROWSEINFOW bi = {};
    bi.hwndOwner = m_hwnd;
    bi.lpszTitle = L"Select folder to clone SparkEngine into:";
    bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;

    wchar_t pathW[MAX_PATH] = {};
    SHGetPathFromIDListW(pidl, pathW);
    CoTaskMemFree(pidl);

    int len = WideCharToMultiByte(CP_UTF8, 0, pathW, -1, nullptr, 0, nullptr, nullptr);
    std::string destPath(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, pathW, -1, destPath.data(), len, nullptr, nullptr);
    // Remove null terminator from string
    if (!destPath.empty() && destPath.back() == '\0') destPath.pop_back();

    AppendLog("Cloning SparkEngine to: " + destPath + "\\SparkEngine ...");
    SetItemStatus(0, DependencyStatus::Installing);

    std::thread([this, destPath]() {
        ProcessRunner pr;
        std::string output;
        std::string cmd = "git clone --recursive https://github.com/Krilliac/SparkEngine.git";
        int rc = pr.RunSync(cmd, destPath, output);

        if (rc == 0) {
            std::string enginePath = destPath + "\\SparkEngine";
            SetItemStatus(0, DependencyStatus::Found, enginePath);
            if (m_config) m_config->config.enginePath = enginePath;
            auto* msg = new std::string("Engine cloned successfully to: " + enginePath);
            PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        } else {
            SetItemStatus(0, DependencyStatus::NotFound, "Clone failed");
            auto* msg = new std::string("Clone failed:\n" + output);
            PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        }
    }).detach();
}

void EnvironmentTab::OnDownloadCMake() {
    AppendLog("Downloading CMake...");
    SetItemStatus(2, DependencyStatus::Installing);

    std::thread([this]() {
        // Download CMake Windows x64 zip
        std::string url = "https://github.com/Kitware/CMake/releases/download/v3.31.5/cmake-3.31.5-windows-x86_64.zip";
        std::string destDir = ".";

        if (m_config && !m_config->config.enginePath.empty()) {
            destDir = m_config->config.enginePath + "\\Tools";
        }

        bool ok = Downloader::DownloadAndExtract(url, destDir, [this](size_t downloaded, size_t total) {
            if (total > 0) {
                int pct = static_cast<int>((downloaded * 100) / total);
                SendMessage(m_hProgress, PBM_SETPOS, pct, 0);
            }
        });

        if (ok) {
            // Find the cmake executable in the extracted folder
            std::string cmakePath = destDir + "\\cmake-3.31.5-windows-x86_64\\bin\\cmake.exe";
            if (m_config) m_config->config.cmakePath = cmakePath;
            SetItemStatus(2, DependencyStatus::Found, "3.31.5 (local)");
            auto* msg = new std::string("CMake downloaded to: " + cmakePath);
            PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        } else {
            SetItemStatus(2, DependencyStatus::NotFound, "Download failed");
            auto* msg = new std::string("CMake download failed. Install manually from https://cmake.org");
            PostMessage(m_hwnd, WM_ENV_LOG, 0, (LPARAM)msg);
        }
    }).detach();
}

} // namespace SparkBuild
