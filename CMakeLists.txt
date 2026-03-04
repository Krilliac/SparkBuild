cmake_minimum_required(VERSION 3.16)
project(SparkBuild LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SparkBuild - Standalone CMake GUI Builder for SparkEngine
# Zero external dependencies - uses only Windows SDK APIs

add_executable(SparkBuild WIN32
    src/main.cpp
    src/SparkBuild.h
    src/SparkBuild.cpp
    src/Config.h
    src/Config.cpp
    src/ProcessRunner.h
    src/ProcessRunner.cpp
    src/Downloader.h
    src/Downloader.cpp
    src/EnvironmentTab.h
    src/EnvironmentTab.cpp
    src/ConfigureTab.h
    src/ConfigureTab.cpp
    src/BuildTab.h
    src/BuildTab.cpp
)

# Resource file (version info, icon, manifest)
if(WIN32)
    target_sources(SparkBuild PRIVATE resources/SparkBuild.rc)
endif()

target_include_directories(SparkBuild PRIVATE src)

# Link Windows system libraries (no external dependencies)
if(WIN32)
    target_link_libraries(SparkBuild PRIVATE
        comctl32      # Common Controls (tabs, list views, progress bars)
        winhttp       # HTTP downloads (WinHTTP API)
        shlwapi       # Shell lightweight utility functions
        shell32       # Shell API (folder dialogs, ZIP extraction)
        ole32         # COM runtime
        oleaut32      # OLE Automation (VARIANT, SysAllocString)
        uuid          # COM GUIDs (CLSID_Shell, IID_IShellDispatch)
        comdlg32      # Common dialogs
    )
endif()

# MSVC-specific settings
if(MSVC)
    # Use static CRT for zero-dependency .exe
    set_property(TARGET SparkBuild PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Suppress common warnings for Win32 API usage
    target_compile_definitions(SparkBuild PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        UNICODE
        _UNICODE
    )
endif()

# Output to a predictable location
set_target_properties(SparkBuild PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
