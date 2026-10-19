// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <map>
#include <string>
#include <vector>

namespace cursor_brightness::hdr {
struct Target { LUID adapter{}; UINT32 id = 0; };
inline bool Active(Target target) {
    // INFO_2 is present on this Windows build and distinguishes HDR from WCG.
    // Local ABI declaration also supports Windhawk's Windows 10 SDK baseline.
    struct ColorInfo2 {
        DISPLAYCONFIG_DEVICE_INFO_HEADER header;
        UINT32 flags, encoding, bits, mode;
    } info{};
    info.header = {static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(15), sizeof(info), target.adapter, target.id};
    if (DisplayConfigGetDeviceInfo(&info.header) == ERROR_SUCCESS) return info.mode == 2;
    // Deliberately no WCG/legacy fallback: an unrecognized mode is not HDR.
    return false;
}
inline bool ReadLevel(Target target, ULONG* level) {
    if (!Active(target)) return false;
    DISPLAYCONFIG_SDR_WHITE_LEVEL info{};
    info.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL, sizeof(info), target.adapter, target.id};
    if (DisplayConfigGetDeviceInfo(&info.header) != ERROR_SUCCESS) return false;
    *level = info.SDRWhiteLevel;
    return true;
}
inline int ReadPercent(Target target) {
    ULONG level = 0;
    return ReadLevel(target, &level) ? WhiteLevelToPercent(level) : -1;
}
inline bool SetLevel(Target target, ULONG level) {
    if (!Active(target)) return false;
    if (level < 1000 || level > 6000) return false;
    // Windows' private SDR-white setter; independently read back after writes.
    // ABI reference: ledoge/set_maxtml, main_sdrwhite.c. No HDR-state writes.
    struct SetWhiteLevel {
        DISPLAYCONFIG_DEVICE_INFO_HEADER header;
        ULONG level;
        BYTE finalValue;
    } info{};
    static_assert(sizeof(info) == 28);
    info.header = {static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(0xFFFFFFEEu), sizeof(info), target.adapter, target.id};
    info.level = level;
    info.finalValue = 1;
    return DisplayConfigSetDeviceInfo(&info.header) == ERROR_SUCCESS;
}
inline bool SetPercent(Target target, int percent) { return SetLevel(target, PercentToWhiteLevel(percent)); }
inline std::map<std::wstring, Target> Enumerate() {
    std::map<std::wstring, Target> result;
    for (int retry = 0; retry < 3; ++retry) {
        UINT32 pathCount = 0, modeCount = 0;
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) return result;
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        LONG status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
        if (status == ERROR_INSUFFICIENT_BUFFER) continue;
        if (status != ERROR_SUCCESS) return result;
        paths.resize(pathCount);
        for (auto const& path : paths) {
            Target target{path.targetInfo.adapterId, path.targetInfo.id};
            if (!Active(target)) continue;
            DISPLAYCONFIG_SOURCE_DEVICE_NAME name{};
            name.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(name), path.sourceInfo.adapterId, path.sourceInfo.id};
            if (DisplayConfigGetDeviceInfo(&name.header) == ERROR_SUCCESS) result.emplace(name.viewGdiDeviceName, target);
        }
        break;
    }
    return result;
}
} // namespace cursor_brightness::hdr
