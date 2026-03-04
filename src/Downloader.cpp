#include "Downloader.h"
#include <winhttp.h>
#include <ole2.h>
#include <oleauto.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shldisp.h>
#include <fstream>
#include <filesystem>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")

namespace SparkBuild {

// Parse a URL into host, path, and whether it's HTTPS
static bool ParseUrl(const std::string& url, std::wstring& host, std::wstring& path, bool& isHttps) {
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);

    wchar_t hostBuf[256] = {};
    wchar_t pathBuf[2048] = {};
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = 2048;

    // Convert URL to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring wurl(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wurl.data(), wlen);

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
        return false;
    }

    host = hostBuf;
    path = pathBuf;
    isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

bool Downloader::DownloadFile(const std::string& url,
                               const std::string& outputPath,
                               DownloadProgressCallback progress) {
    std::wstring host, path;
    bool isHttps = true;
    if (!ParseUrl(url, host, path, isHttps)) {
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"SparkBuild/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    INTERNET_PORT port = isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Follow redirects automatically (up to 5)
    DWORD maxRedirects = 5;
    for (DWORD attempt = 0; attempt <= maxRedirects; ++attempt) {
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            break;
        }
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            break;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode >= 300 && statusCode < 400) {
            // Handle redirect
            wchar_t redirectUrl[2048] = {};
            DWORD redirectSize = sizeof(redirectUrl);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION,
                                    WINHTTP_HEADER_NAME_BY_INDEX, redirectUrl, &redirectSize, WINHTTP_NO_HEADER_INDEX)) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);

                URL_COMPONENTS uc = {};
                uc.dwStructSize = sizeof(uc);
                wchar_t hostBuf[256] = {};
                wchar_t pathBuf[2048] = {};
                uc.lpszHostName = hostBuf;
                uc.dwHostNameLength = 256;
                uc.lpszUrlPath = pathBuf;
                uc.dwUrlPathLength = 2048;

                if (!WinHttpCrackUrl(redirectUrl, 0, 0, &uc)) break;

                host = hostBuf;
                path = pathBuf;
                isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
                port = isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

                hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
                if (!hConnect) break;

                flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
                hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                               nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
                if (!hRequest) break;
                continue;
            }
            break;
        }

        if (statusCode != 200) break;

        // Get content length if available
        size_t totalBytes = 0;
        wchar_t contentLength[32] = {};
        DWORD clSize = sizeof(contentLength);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
                                WINHTTP_HEADER_NAME_BY_INDEX, contentLength, &clSize, WINHTTP_NO_HEADER_INDEX)) {
            totalBytes = static_cast<size_t>(_wtoi64(contentLength));
        }

        // Create output directory if needed
        std::filesystem::path outPath(outputPath);
        if (outPath.has_parent_path()) {
            std::filesystem::create_directories(outPath.parent_path());
        }

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile.is_open()) break;

        size_t bytesDownloaded = 0;
        DWORD bytesAvail = 0;
        bool downloadOk = true;

        while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0) {
            std::vector<char> buf(bytesAvail);
            DWORD bytesRead = 0;
            if (!WinHttpReadData(hRequest, buf.data(), bytesAvail, &bytesRead)) {
                downloadOk = false;
                break;
            }
            outFile.write(buf.data(), bytesRead);
            bytesDownloaded += bytesRead;

            if (progress) {
                progress(bytesDownloaded, totalBytes);
            }
        }

        outFile.close();

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return downloadOk;
    }

    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
}

bool Downloader::ExtractZip(const std::string& zipPath, const std::string& destDir) {
    // Use the Shell API to extract ZIP files — no external dependencies needed
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Convert paths to wide strings
    int zipWLen = MultiByteToWideChar(CP_UTF8, 0, zipPath.c_str(), -1, nullptr, 0);
    std::wstring zipW(zipWLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, zipPath.c_str(), -1, zipW.data(), zipWLen);

    int destWLen = MultiByteToWideChar(CP_UTF8, 0, destDir.c_str(), -1, nullptr, 0);
    std::wstring destW(destWLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, destDir.c_str(), -1, destW.data(), destWLen);

    // Create destination directory
    std::filesystem::create_directories(destDir);

    // Use IShellDispatch to extract
    IShellDispatch* pShell = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellDispatch, (void**)&pShell);
    if (FAILED(hr) || !pShell) return false;

    VARIANT vZip, vDest;
    VariantInit(&vZip);
    VariantInit(&vDest);

    vZip.vt = VT_BSTR;
    vZip.bstrVal = SysAllocString(zipW.c_str());
    vDest.vt = VT_BSTR;
    vDest.bstrVal = SysAllocString(destW.c_str());

    Folder* pZipFolder = nullptr;
    hr = pShell->NameSpace(vZip, &pZipFolder);
    if (FAILED(hr) || !pZipFolder) {
        VariantClear(&vZip);
        VariantClear(&vDest);
        pShell->Release();
        return false;
    }

    Folder* pDestFolder = nullptr;
    hr = pShell->NameSpace(vDest, &pDestFolder);
    if (FAILED(hr) || !pDestFolder) {
        pZipFolder->Release();
        VariantClear(&vZip);
        VariantClear(&vDest);
        pShell->Release();
        return false;
    }

    FolderItems* pItems = nullptr;
    pZipFolder->Items(&pItems);
    if (!pItems) {
        pDestFolder->Release();
        pZipFolder->Release();
        VariantClear(&vZip);
        VariantClear(&vDest);
        pShell->Release();
        return false;
    }

    VARIANT vItems;
    VariantInit(&vItems);
    vItems.vt = VT_DISPATCH;
    vItems.pdispVal = pItems;

    // FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT = 0x0614
    VARIANT vOptions;
    VariantInit(&vOptions);
    vOptions.vt = VT_I4;
    vOptions.lVal = 0x0614;

    hr = pDestFolder->CopyHere(vItems, vOptions);

    pItems->Release();
    pDestFolder->Release();
    pZipFolder->Release();
    VariantClear(&vZip);
    VariantClear(&vDest);
    pShell->Release();

    return SUCCEEDED(hr);
}

bool Downloader::DownloadAndExtract(const std::string& url,
                                     const std::string& destDir,
                                     DownloadProgressCallback progress) {
    std::string tempPath = GetTempDir() + "\\sparkbuild_download.zip";

    if (!DownloadFile(url, tempPath, progress)) {
        return false;
    }

    bool ok = ExtractZip(tempPath, destDir);

    // Clean up temp file
    DeleteFileA(tempPath.c_str());

    return ok;
}

std::string Downloader::GetTempDir() {
    char buf[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, buf);
    // Remove trailing backslash
    std::string path(buf);
    if (!path.empty() && path.back() == '\\') {
        path.pop_back();
    }
    return path;
}

} // namespace SparkBuild
