#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include "SparkBuild.h"

#pragma comment(linker, "/manifestdependency:\"type='win32' " \
    "name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Initialize COM (needed for Shell API / folder dialogs)
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Initialize common controls (tabs, list views, progress bars)
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS |
                ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    SparkBuild::SparkBuildApp app(hInstance);
    if (!app.Init()) {
        MessageBoxW(nullptr, L"Failed to initialize SparkBuild.", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    int result = app.Run();

    CoUninitialize();
    return result;
}
