#include "ProcessRunner.h"

namespace SparkBuild {

ProcessRunner::ProcessRunner() {}

ProcessRunner::~ProcessRunner() {
    Cancel();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool ProcessRunner::RunAsync(const std::string& command,
                              const std::string& workingDir,
                              OutputCallback onOutput,
                              CompletionCallback onComplete) {
    if (m_running.load()) return false;

    // Wait for any previous thread to finish
    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_cancelRequested.store(false);
    m_running.store(true);
    m_thread = std::thread(&ProcessRunner::AsyncThreadFunc, this,
                           command, workingDir, onOutput, onComplete);
    return true;
}

int ProcessRunner::RunSync(const std::string& command,
                            const std::string& workingDir,
                            std::string& output) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return -1;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = command;
    const char* dir = workingDir.empty() ? nullptr : workingDir.c_str();

    BOOL ok = CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr,
                              TRUE, CREATE_NO_WINDOW, nullptr, dir, &si, &pi);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        return -1;
    }

    output.clear();
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        output += buf;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    return static_cast<int>(exitCode);
}

void ProcessRunner::Cancel() {
    m_cancelRequested.store(true);
    std::lock_guard<std::mutex> lock(m_processMutex);
    if (m_hProcess) {
        TerminateProcess(m_hProcess, 1);
    }
}

void ProcessRunner::AsyncThreadFunc(std::string command,
                                     std::string workingDir,
                                     OutputCallback onOutput,
                                     CompletionCallback onComplete) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        m_running.store(false);
        if (onComplete) onComplete(-1, false);
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    const char* dir = workingDir.empty() ? nullptr : workingDir.c_str();

    // Use cmd /c to ensure shell commands work (e.g., cmake on PATH)
    std::string cmdLine = "cmd /c " + command;

    BOOL ok = CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr,
                              TRUE, CREATE_NO_WINDOW, nullptr, dir, &si, &pi);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        m_running.store(false);
        if (onComplete) onComplete(-1, false);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_processMutex);
        m_hProcess = pi.hProcess;
    }

    // Read output line by line
    std::string lineBuffer;
    ReadPipeOutput(hReadPipe, onOutput, lineBuffer);

    // Flush any remaining partial line
    if (!lineBuffer.empty() && onOutput) {
        onOutput(lineBuffer);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    m_exitCode = static_cast<int>(exitCode);

    {
        std::lock_guard<std::mutex> lock(m_processMutex);
        m_hProcess = nullptr;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    m_running.store(false);

    bool success = (exitCode == 0) && !m_cancelRequested.load();
    if (onComplete) onComplete(m_exitCode, success);
}

void ProcessRunner::ReadPipeOutput(HANDLE hPipe, OutputCallback& onOutput, std::string& lineBuffer) {
    char buf[1024];
    DWORD bytesRead;

    while (!m_cancelRequested.load()) {
        BOOL ok = ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr);
        if (!ok || bytesRead == 0) break;

        buf[bytesRead] = '\0';
        lineBuffer += buf;

        // Split into lines
        size_t pos;
        while ((pos = lineBuffer.find('\n')) != std::string::npos) {
            std::string line = lineBuffer.substr(0, pos);
            // Remove trailing \r
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (onOutput) {
                onOutput(line);
            }
            lineBuffer = lineBuffer.substr(pos + 1);
        }
    }
}

} // namespace SparkBuild
