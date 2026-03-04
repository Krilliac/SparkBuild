#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <functional>

namespace SparkBuild {

class ConfigManager;
class ProcessRunner;
class ConfigureTab;

enum class BuildState {
    Idle,
    Configuring,
    Building,
    Done,
    Error,
    Cancelled
};

class BuildTab {
public:
    BuildTab();
    ~BuildTab();

    HWND Create(HWND hParent, HINSTANCE hInst, RECT area);
    void Show(bool visible);

    void SetConfig(ConfigManager* cfg) { m_config = cfg; }
    void SetProcessRunner(ProcessRunner* pr) { m_processRunner = pr; }
    void SetConfigureTab(ConfigureTab* ct) { m_configureTab = ct; }
    void SetStatusCallback(std::function<void(const std::string&)> cb) { m_statusCallback = cb; }

    BuildState GetState() const { return m_state; }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND GetHwnd() const { return m_hwnd; }

private:
    void CreateControls();
    void OnGenerate();
    void OnBuild();
    void OnGenerateAndBuild();
    void OnCancel();
    void OnClean();
    void OnOpenBuildFolder();
    void OnOpenSolution();
    void OnRunEngine();

    void AppendLog(const std::string& msg);
    void SetState(BuildState state);
    void UpdateButtons();
    std::string FindSolutionFile();
    std::string FindEngineExe();

    HWND m_hwnd = nullptr;
    HWND m_hLog = nullptr;
    HWND m_hProgress = nullptr;
    HWND m_hStateLabel = nullptr;
    HWND m_hBtnGenerate = nullptr;
    HWND m_hBtnBuild = nullptr;
    HWND m_hBtnGenAndBuild = nullptr;
    HWND m_hBtnCancel = nullptr;
    HWND m_hBtnClean = nullptr;
    HWND m_hBtnOpenFolder = nullptr;
    HWND m_hBtnOpenSln = nullptr;
    HWND m_hBtnRun = nullptr;
    HINSTANCE m_hInst = nullptr;

    BuildState m_state = BuildState::Idle;
    ConfigManager* m_config = nullptr;
    ProcessRunner* m_processRunner = nullptr;
    ConfigureTab* m_configureTab = nullptr;
    std::function<void(const std::string&)> m_statusCallback;
    bool m_buildAfterGenerate = false;

    static constexpr int IDC_LOG            = 5001;
    static constexpr int IDC_PROGRESS       = 5002;
    static constexpr int IDC_STATE_LABEL    = 5003;
    static constexpr int IDC_BTN_GENERATE   = 5010;
    static constexpr int IDC_BTN_BUILD      = 5011;
    static constexpr int IDC_BTN_GEN_BUILD  = 5012;
    static constexpr int IDC_BTN_CANCEL     = 5013;
    static constexpr int IDC_BTN_CLEAN      = 5014;
    static constexpr int IDC_BTN_OPEN_DIR   = 5015;
    static constexpr int IDC_BTN_OPEN_SLN   = 5016;
    static constexpr int IDC_BTN_RUN        = 5017;
};

} // namespace SparkBuild
