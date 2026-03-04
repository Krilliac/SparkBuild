#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <memory>
#include <string>

namespace SparkBuild {

class ConfigManager;
class ProcessRunner;
class EnvironmentTab;
class ConfigureTab;
class BuildTab;

class SparkBuildApp {
public:
    SparkBuildApp(HINSTANCE hInst);
    ~SparkBuildApp();

    // Initialize and show the main window. Returns false on failure.
    bool Init();

    // Run the message loop. Returns the exit code.
    int Run();

    static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void OnCreate(HWND hwnd);
    void OnSize(int width, int height);
    void OnTabChanged();
    void OnClose();
    void UpdateStatusBar(const std::string& text);

    // Calculate the display area below the tab control header
    RECT GetTabDisplayArea() const;

    HINSTANCE m_hInst;
    HWND m_hwnd = nullptr;          // Main window
    HWND m_hTab = nullptr;          // Tab control
    HWND m_hStatusBar = nullptr;    // Status bar

    // Tab pages
    std::unique_ptr<EnvironmentTab> m_envTab;
    std::unique_ptr<ConfigureTab> m_cfgTab;
    std::unique_ptr<BuildTab> m_buildTab;

    // Shared modules
    std::unique_ptr<ConfigManager> m_config;
    std::unique_ptr<ProcessRunner> m_processRunner;

    int m_currentTab = 0;

    static constexpr int IDC_TABCTRL   = 2001;
    static constexpr int IDC_STATUSBAR = 2002;
    static constexpr int WINDOW_WIDTH  = 950;
    static constexpr int WINDOW_HEIGHT = 680;
};

} // namespace SparkBuild
