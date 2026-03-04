#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>

namespace SparkBuild {

class ConfigManager;

class ConfigureTab {
public:
    ConfigureTab();
    ~ConfigureTab();

    HWND Create(HWND hParent, HINSTANCE hInst, RECT area);
    void Show(bool visible);

    // Sync UI ↔ config
    void LoadFromConfig();
    void SaveToConfig();

    void SetConfig(ConfigManager* cfg) { m_config = cfg; }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ScrollWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND GetHwnd() const { return m_hwnd; }

private:
    void CreateControls();
    void CreateOptionCheckboxes();
    void OnPresetAllOn();
    void OnPresetAllOff();
    void OnPresetDefaults();
    void OnPresetMinimal();
    void OnBrowseSource();
    void OnBrowseBuild();

    HWND m_hwnd = nullptr;
    HWND m_hScrollArea = nullptr;   // Scrollable child for option groups
    HINSTANCE m_hInst = nullptr;

    // Generator / build type controls
    HWND m_hCmbGenerator = nullptr;
    HWND m_hRadDebug = nullptr;
    HWND m_hRadRelease = nullptr;
    HWND m_hRadRelWithDebInfo = nullptr;
    HWND m_hEdtToolset = nullptr;
    HWND m_hEdtSourcePath = nullptr;
    HWND m_hEdtBuildPath = nullptr;
    HWND m_hEdtJobs = nullptr;

    // Preset buttons
    HWND m_hBtnAllOn = nullptr;
    HWND m_hBtnAllOff = nullptr;
    HWND m_hBtnDefaults = nullptr;
    HWND m_hBtnMinimal = nullptr;

    // Checkbox handles for each build option (index matches config.options)
    std::vector<HWND> m_optionChecks;

    ConfigManager* m_config = nullptr;
    WNDPROC m_origScrollProc = nullptr;
    int m_scrollHeight = 0;     // Total height of scrollable content

    static constexpr int IDC_CMB_GENERATOR  = 4001;
    static constexpr int IDC_RAD_DEBUG      = 4002;
    static constexpr int IDC_RAD_RELEASE    = 4003;
    static constexpr int IDC_RAD_RELDBG     = 4004;
    static constexpr int IDC_EDT_TOOLSET    = 4005;
    static constexpr int IDC_EDT_SOURCE     = 4006;
    static constexpr int IDC_EDT_BUILD      = 4007;
    static constexpr int IDC_EDT_JOBS       = 4008;
    static constexpr int IDC_BTN_ALL_ON     = 4010;
    static constexpr int IDC_BTN_ALL_OFF    = 4011;
    static constexpr int IDC_BTN_DEFAULTS   = 4012;
    static constexpr int IDC_BTN_MINIMAL    = 4013;
    static constexpr int IDC_BTN_BROWSE_SRC = 4014;
    static constexpr int IDC_BTN_BROWSE_BLD = 4015;
    static constexpr int IDC_OPT_BASE       = 4100; // Checkboxes start here
};

} // namespace SparkBuild
