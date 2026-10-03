#include "src/utils/updater.h"
#include <winhttp.h>
#include <shlobj.h>
#include <shellapi.h>
#include <thread>
#include <vector>
#include <algorithm>

namespace yanlv {

namespace {
const wchar_t* const VERSION_STRING = L"v1.0.0";
const char* const GITHUB_REPO_HOST = "api.github.com";
const wchar_t* const GITHUB_REPO_HOST_W = L"api.github.com";
const wchar_t* const GITHUB_RELEASES_API_PATH = L"/repos/Leonherben/yanlv-timer/releases/latest";

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], sizeNeeded);
    return wstr;
}
} // namespace

const wchar_t* Updater::GetCurrentVersion() {
    return VERSION_STRING;
}

bool Updater::CompareVersions(const std::string& currentVer, const std::string& latestVer, bool& outHasUpdate) {
    auto parseVersion = [](const std::string& str) -> std::vector<int> {
        std::vector<int> parts;
        size_t i = 0;
        while (i < str.size() && (str[i] == 'v' || str[i] == 'V' || str[i] == ' ')) {
            ++i;
        }
        int cur = 0;
        bool hasDigits = false;
        while (i < str.size()) {
            if (str[i] >= '0' && str[i] <= '9') {
                cur = cur * 10 + (str[i] - '0');
                hasDigits = true;
            } else if (str[i] == '.') {
                parts.push_back(cur);
                cur = 0;
                hasDigits = false;
            } else {
                break;
            }
            ++i;
        }
        if (hasDigits) {
            parts.push_back(cur);
        }
        return parts;
    };

    auto curParts = parseVersion(currentVer);
    auto latParts = parseVersion(latestVer);

    size_t maxLen = (std::max)(curParts.size(), latParts.size());
    curParts.resize(maxLen, 0);
    latParts.resize(maxLen, 0);

    for (size_t i = 0; i < maxLen; ++i) {
        if (latParts[i] > curParts[i]) {
            outHasUpdate = true;
            return true;
        }
        if (latParts[i] < curParts[i]) {
            outHasUpdate = false;
            return true;
        }
    }

    outHasUpdate = false;
    return true;
}

std::string Updater::ExtractJsonString(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) return "";

    size_t valStart = json.find('"', colonPos + 1);
    if (valStart == std::string::npos) return "";

    std::string result;
    bool inEscape = false;
    for (size_t i = valStart + 1; i < json.size(); ++i) {
        char c = json[i];
        if (inEscape) {
            if (c == '"') result += '"';
            else if (c == '\\') result += '\\';
            else if (c == 'n') result += '\n';
            else if (c == 'r') result += '\r';
            else if (c == 't') result += '\t';
            else { result += '\\'; result += c; }
            inEscape = false;
        } else if (c == '\\') {
            inEscape = true;
        } else if (c == '"') {
            break;
        } else {
            result += c;
        }
    }
    return result;
}

UpdateInfo Updater::CheckForUpdatesSync() {
    UpdateInfo info;
    info.currentVersion = GetCurrentVersion();

    HINTERNET hSession = WinHttpOpen(
        L"YanlvTimer-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) {
        info.errorMessage = L"无法初始化网络会话组件。";
        return info;
    }

    WinHttpSetTimeouts(hSession, 5000, 5000, 5000, 5000);

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        GITHUB_REPO_HOST_W,
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        info.errorMessage = L"无法连接至 GitHub 服务器。";
        return info;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        GITHUB_RELEASES_API_PATH,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        info.errorMessage = L"创建网络请求失败。";
        return info;
    }

    LPCWSTR headers = L"User-Agent: YanlvTimer-Updater\r\nAccept: application/vnd.github+json\r\n";
    BOOL sent = WinHttpSendRequest(
        hRequest,
        headers,
        -1,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );

    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        info.errorMessage = L"网络请求超时或未收到响应。";
        return info;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    if (statusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        info.errorMessage = L"服务器响应状态异常 (HTTP " + std::to_wstring(statusCode) + L")";
        return info;
    }

    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            responseBody.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::string tagStr = ExtractJsonString(responseBody, "tag_name");
    std::string nameStr = ExtractJsonString(responseBody, "name");
    std::string urlStr = ExtractJsonString(responseBody, "html_url");
    std::string bodyStr = ExtractJsonString(responseBody, "body");
    std::string dlStr = ExtractJsonString(responseBody, "browser_download_url");

    if (tagStr.empty()) {
        info.errorMessage = L"解析版本数据失败。";
        return info;
    }

    info.latestVersion = Utf8ToWide(tagStr);
    info.releaseTitle = Utf8ToWide(nameStr.empty() ? tagStr : nameStr);
    info.releaseNotes = Utf8ToWide(bodyStr);
    info.releaseUrl = Utf8ToWide(urlStr.empty() ? "https://github.com/Leonherben/yanlv-timer/releases" : urlStr);
    info.downloadUrl = Utf8ToWide(dlStr);

    std::string curVerUtf8 = "v1.0.0";
    bool hasNew = false;
    CompareVersions(curVerUtf8, tagStr, hasNew);
    info.hasUpdate = hasNew;

    return info;
}

void Updater::CheckForUpdatesAsync(HWND parentHwnd, bool silentIfLatest) {
    std::thread([parentHwnd, silentIfLatest]() {
        UpdateInfo info = CheckForUpdatesSync();

        if (!info.errorMessage.empty()) {
            if (!silentIfLatest) {
                int res = MessageBoxW(
                    parentHwnd,
                    (L"检查更新失败：\n" + info.errorMessage + L"\n\n是否直接前往 GitHub 仓库主页查看最新动态？").c_str(),
                    L"检查更新 - 言律时钟",
                    MB_YESNO | MB_ICONWARNING | MB_TOPMOST
                );
                if (res == IDYES) {
                    ShellExecuteW(nullptr, L"open", L"https://github.com/Leonherben/yanlv-timer/releases", nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return;
        }

        if (info.hasUpdate) {
            std::wstring msg = L"🎉 发现新版本：";
            msg += (info.releaseTitle.empty() ? info.latestVersion : info.releaseTitle);
            msg += L"\n当前版本：" + info.currentVersion;
            msg += L"\n最新版本：" + info.latestVersion;
            if (!info.releaseNotes.empty()) {
                std::wstring previewNotes = info.releaseNotes;
                if (previewNotes.size() > 300) {
                    previewNotes = previewNotes.substr(0, 300) + L"...";
                }
                msg += L"\n\n更新说明：\n" + previewNotes;
            }
            msg += L"\n\n========================================\n";
            msg += L"★ 数据安全保证：\n";
            msg += L"您的所有学习记录、专注历史与各项个性化设置均独立保存在系统用户目录 (%APPDATA%\\YanlvTimer) 中，更新后数据 100% 完整保留，绝不丢失！\n";
            msg += L"========================================\n\n";
            msg += L"是否立即前往 GitHub 下载最新版？";

            int res = MessageBoxW(
                parentHwnd,
                msg.c_str(),
                L"发现新版本 - 言律时钟",
                MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST
            );

            if (res == IDYES) {
                ShellExecuteW(nullptr, L"open", info.releaseUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        } else {
            if (!silentIfLatest) {
                std::wstring msg = L"您当前使用的是最新版本 (";
                msg += info.currentVersion;
                msg += L")！\n无需更新。\n\n★ 数据安全保证：\n所有学习记录与个性化设置均独立保存在系统用户目录中，长期安全保留。";
                MessageBoxW(
                    parentHwnd,
                    msg.c_str(),
                    L"检查更新 - 言律时钟",
                    MB_OK | MB_ICONINFORMATION | MB_TOPMOST
                );
            }
        }
    }).detach();
}

void Updater::OpenDataDirectory(HWND /*parentHwnd*/) {
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        std::wstring dir = std::wstring(appDataPath) + L"\\YanlvTimer";
        CreateDirectoryW(dir.c_str(), nullptr);
        ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

} // namespace yanlv
