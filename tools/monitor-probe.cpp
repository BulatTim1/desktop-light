// SPDX-License-Identifier: GPL-3.0-only
// Read-only DDC/CI check. No calls capable of setting monitor properties.
#include <windows.h>
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <cstdio>
#include <vector>
BOOL CALLBACK ReadMonitor(HMONITOR monitor, HDC, LPRECT rect, LPARAM) {
    DWORD count = 0;
    if (!GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count)) return TRUE;
    std::vector<PHYSICAL_MONITOR> physical(count);
    if (!GetPhysicalMonitorsFromHMONITOR(monitor, count, physical.data())) return TRUE;
    MONITORINFOEXW info{}; info.cbSize = sizeof(info); GetMonitorInfoW(monitor, &info);
    DISPLAY_DEVICEW device{}; device.cb = sizeof(device); EnumDisplayDevicesW(info.szDevice, 0, &device, EDD_GET_DEVICE_INTERFACE_NAME);
    for (auto const& p : physical) {
        MC_VCP_CODE_TYPE type{}; DWORD current = 0, maximum = 0;
        bool ok = GetVCPFeatureAndVCPFeatureReply(p.hPhysicalMonitor, 0x10, &type, &current, &maximum);
        std::wprintf(L"%ls bounds=%ld,%ld,%ld,%ld brightness=%lu/%lu readable=%d id=%ls\n", info.szDevice, rect->left, rect->top, rect->right, rect->bottom, current, maximum, ok, device.DeviceID);
    }
    DestroyPhysicalMonitors(count, physical.data());
    return TRUE;
}
int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    POINT cursor{}; GetCursorPos(&cursor);
    std::printf("Cursor=%ld,%ld\n", cursor.x, cursor.y);
    return EnumDisplayMonitors(nullptr, nullptr, ReadMonitor, 0) ? 0 : 1;
}
