#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

namespace SparkBuild {

// Callback types for process events
using OutputCallback = std::function<void(const std::string& line)>;
using CompletionCallback = std::function<void(int exitCode, bool success)>;

class ProcessRunner {
public:
    ProcessRunner();
    ~ProcessRunner();

    // Run a command asynchronously. Output is delivered via callbacks.
    // workingDir: working directory for the process (empty = current dir)
    // Returns true if the process was started successfully.
    bool RunAsync(const std::string& command,
                  const std::string& workingDir,
                  OutputCallback onOutput,
                  CompletionCallback onComplete);

    // Run a command synchronously, capturing all output.
    // Returns the exit code.
    int RunSync(const std::string& command,
                const std::string& workingDir,
                std::string& output);

    // Cancel a running async process
    void Cancel();

    // Check if a process is currently running
    bool IsRunning() const { return m_running.load(); }

    // Get the exit code of the last completed process
    int GetExitCode() const { return m_exitCode; }

private:
    void AsyncThreadFunc(std::string command,
                         std::string workingDir,
                         OutputCallback onOutput,
                         CompletionCallback onComplete);

    // Read all available output from a pipe handle, calling onOutput per line
    void ReadPipeOutput(HANDLE hPipe, OutputCallback& onOutput, std::string& lineBuffer);

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancelRequested{false};
    HANDLE m_hProcess = nullptr;
    std::mutex m_processMutex;
    int m_exitCode = 0;
};

} // namespace SparkBuild
