// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <algorithm>
#include <string>
#include <stdexcept>

namespace app {
inline std::filesystem::path DataDirectory() {
    PWSTR directory = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &directory))) throw std::runtime_error("Cannot locate local application data");
    std::filesystem::path result = std::filesystem::path(directory) / L"CursorBrightness";
    CoTaskMemFree(directory);
    std::filesystem::create_directories(result);
    return result;
}
struct Preferences {
    int step = 5;
    bool keys = true;
    bool integration = true;
    std::filesystem::path file;
    void Load() {
        file = DataDirectory() / L"settings.ini";
        step = std::clamp<int>(GetPrivateProfileIntW(L"Settings", L"BrightnessStep", 5, file.c_str()), 1, 100);
        keys = GetPrivateProfileIntW(L"Settings", L"BrightnessKeys", 1, file.c_str()) != 0;
        integration = GetPrivateProfileIntW(L"Settings", L"QuickSettings", 1, file.c_str()) != 0;
    }
    void Save() const {
        const auto value = std::to_wstring(step);
        if (!WritePrivateProfileStringW(L"Settings", L"BrightnessStep", value.c_str(), file.c_str()) ||
            !WritePrivateProfileStringW(L"Settings", L"BrightnessKeys", keys ? L"1" : L"0", file.c_str()) ||
            !WritePrivateProfileStringW(L"Settings", L"QuickSettings", integration ? L"1" : L"0", file.c_str())) throw std::runtime_error("Cannot save settings");
    }
};
} // namespace app
