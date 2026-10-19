// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstring>

namespace cursor_brightness::bridge {
inline constexpr wchar_t kWindowClass[] = L"CursorBrightness.Standalone.Window";
inline constexpr ULONG_PTR kSetPercent = 0x43425231;
inline bool SetPercent(const std::wstring& id, int percent) {
    HWND window = FindWindowW(kWindowClass, nullptr);
    if (!window || id.size() > 2048) return false;
    std::vector<BYTE> payload(sizeof(int) + (id.size() + 1) * sizeof(wchar_t));
    std::memcpy(payload.data(), &percent, sizeof(percent));
    std::memcpy(payload.data() + sizeof(int), id.c_str(), (id.size() + 1) * sizeof(wchar_t));
    COPYDATASTRUCT data{kSetPercent, static_cast<DWORD>(payload.size()), payload.data()};
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data),
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &result) && result;
}
} // namespace cursor_brightness::bridge
