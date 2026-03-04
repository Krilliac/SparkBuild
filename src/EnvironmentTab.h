#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <functional>

namespace SparkBuild {

class ProcessRunner;
class ConfigManager;

enum class DependencyStatus {
    Unknown,
    Checking,
    Found,
    NotFound,
    Installing
};

struct DependencyItem {
    std::string name;
    std::string description;
    std::string version;        // Detected version string
    DependencyStatus status = DependencyStatus::Unknown;
    bool canAutoInstall;        // Whether we can auto-install this
};

class EnvironmentTab {
public:
    EnvironmentTab();
    ~EnvironmentTab();

    // Create the tab page as a child of the tab control area
    HWND Create(HWND hParent, HINSTANCE hInst, RECT area);

    // Show/hide the tab
    void Show(bool visible);

    // Run all dependency checks
    void CheckAll();

    // Set the config manager and process runner references
    void SetConfig(ConfigManager* cfg) { m_config = cfg; }
    void SetProcessRunner(ProcessRunner* pr) { m_processRunner = pr; }

    // Callback for posting UI updates from background threads
    void SetStatusCallback(std::function<void(const std::string&)> cb) { m_statusCallback = cb; }

    // Window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND GetHwnd() const { return m_hwnd; }

private:
    void CreateControls();
    void UpdateUI();
    void CheckGit();
    void CheckCMake();
    void CheckCompilers();
    void CheckSubmodules();
    void CheckEngineRepo();

    void OnCheckAll();
    void OnInstallAll();
    void OnInitSubmodules();
    void OnCloneRepo();
    void OnDownloadCMake();

    void SetItemStatus(int index, DependencyStatus status, const std::string& version = "");
    void AppendLog(const std::string& msg);

    HWND m_hwnd = nullptr;
    HWND m_hList = nullptr;         // ListView for dependency items
    HWND m_hLog = nullptr;          // Edit control for log output
    HWND m_hBtnCheckAll = nullptr;
    HWND m_hBtnInstallAll = nullptr;
    HWND m_hBtnInitSub = nullptr;
    HWND m_hBtnCloneRepo = nullptr;
    HWND m_hBtnDownloadCMake = nullptr;
    HWND m_hProgress = nullptr;     // Progress bar
    HINSTANCE m_hInst = nullptr;

    std::vector<DependencyItem> m_items;
    ConfigManager* m_config = nullptr;
    ProcessRunner* m_processRunner = nullptr;
    std::function<void(const std::string&)> m_statusCallback;

    static constexpr int IDC_LIST       = 3001;
    static constexpr int IDC_LOG        = 3002;
    static constexpr int IDC_CHECK_ALL  = 3003;
    static constexpr int IDC_INSTALL_ALL= 3004;
    static constexpr int IDC_INIT_SUB   = 3005;
    static constexpr int IDC_CLONE_REPO = 3006;
    static constexpr int IDC_DL_CMAKE   = 3007;
    static constexpr int IDC_PROGRESS   = 3008;
};

} // namespace SparkBuild
