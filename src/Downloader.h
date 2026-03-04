#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <functional>

namespace SparkBuild {

// Progress callback: (bytesDownloaded, totalBytes) — totalBytes may be 0 if unknown
using DownloadProgressCallback = std::function<void(size_t bytesDownloaded, size_t totalBytes)>;

class Downloader {
public:
    // Download a file from a URL to a local path.
    // Returns true on success.
    static bool DownloadFile(const std::string& url,
                             const std::string& outputPath,
                             DownloadProgressCallback progress = nullptr);

    // Extract a ZIP file to a directory using the Windows Shell API.
    // Returns true on success.
    static bool ExtractZip(const std::string& zipPath,
                           const std::string& destDir);

    // Download and extract a ZIP in one step.
    // The ZIP is downloaded to a temp file, extracted, then deleted.
    static bool DownloadAndExtract(const std::string& url,
                                   const std::string& destDir,
                                   DownloadProgressCallback progress = nullptr);

    // Get the system temp directory
    static std::string GetTempDir();
};

} // namespace SparkBuild
