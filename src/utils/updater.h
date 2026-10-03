#pragma once

#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace yanlv {

struct UpdateInfo {
    bool hasUpdate = false;
    std::wstring currentVersion;
    std::wstring latestVersion;
    std::wstring releaseTitle;
    std::wstring releaseNotes;
    std::wstring releaseUrl;
    std::wstring downloadUrl;
    std::wstring errorMessage;
};

class Updater {
public:
    static const wchar_t* GetCurrentVersion();
    static bool CompareVersions(const std::string& currentVer, const std::string& latestVer, bool& outHasUpdate);
    static std::string ExtractJsonString(const std::string& json, const std::string& key);

    // 检查更新 (异步后台请求，弹窗提示，不卡顿 UI)
    static void CheckForUpdatesAsync(HWND parentHwnd, bool silentIfLatest = false);

    // 同步执行网络检查更新
    static UpdateInfo CheckForUpdatesSync();

    // 打开数据存储目录 (备份数据)
    static void OpenDataDirectory(HWND parentHwnd = nullptr);
};

} // namespace yanlv
