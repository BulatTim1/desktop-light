// SPDX-License-Identifier: GPL-3.0-only
#include <windows.h>
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <cstdio>
#include <cstring>
#include <vector>
bool exercise = false;
BOOL CALLBACK Diagnose(HMONITOR monitor, HDC, LPRECT, LPARAM) {
    MONITORINFOEXW info{}; info.cbSize = sizeof(info); GetMonitorInfoW(monitor, &info);
    DWORD count = 0;
    if (!GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count)) return TRUE;
    std::vector<PHYSICAL_MONITOR> physical(count);
    if (!GetPhysicalMonitorsFromHMONITOR(monitor, count, physical.data())) return TRUE;
    for (auto const& p : physical) {
        MC_VCP_CODE_TYPE type{}; DWORD original = 0, maximum = 0;
        bool readable = GetVCPFeatureAndVCPFeatureReply(p.hPhysicalMonitor,0x10,&type,&original,&maximum);
        std::wprintf(L"%ls primary=%d brightness=%lu/%lu readable=%d\n", info.szDevice, !!(info.dwFlags & MONITORINFOF_PRIMARY), original, maximum, readable);
        for (BYTE code : {BYTE(0x6b), BYTE(0x14), BYTE(0xdc)}) {
            DWORD value = 0, limit = 0;
            bool ok = GetVCPFeatureAndVCPFeatureReply(p.hPhysicalMonitor,code,&type,&value,&limit);
            std::printf("  read VCP=%02x supported=%d value=%lu max=%lu\n", code,ok,value,limit);
        }
        if (exercise && (info.dwFlags & MONITORINFOF_PRIMARY) && readable && maximum) {
            // Restore even when readback fails. Only luminance 0x10 is written.
            struct Restore { HANDLE handle; DWORD value; ~Restore(){ SetVCPFeature(handle,0x10,value); } } restore{p.hPhysicalMonitor,original};
            bool ok = SetVCPFeature(p.hPhysicalMonitor,0x10,maximum/2);
            std::printf("  TEST write 50%% ok=%d; waiting for hardware\n",ok); std::fflush(stdout);
            Sleep(1500);
            DWORD value = 0, limit = 0;
            ok = GetVCPFeatureAndVCPFeatureReply(p.hPhysicalMonitor,0x10,&type,&value,&limit);
            std::printf("  TEST readback supported=%d value=%lu/%lu\n",ok,value,limit);
            std::printf("  Restoring %lu/%lu\n",original,maximum);
        }
    }
    DestroyPhysicalMonitors(count,physical.data()); return TRUE;
}
int main(int argc, char** argv) {
    exercise = argc > 1 && std::strcmp(argv[1],"--exercise-primary") == 0;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    UINT32 pathCount=0, modeCount=0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pathCount,&modeCount) == ERROR_SUCCESS) {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount); std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pathCount,paths.data(),&modeCount,modes.data(),nullptr) == ERROR_SUCCESS) {
            paths.resize(pathCount);
            for (auto const& path : paths) {
                DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO color{};
                color.header={DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO,sizeof(color),path.targetInfo.adapterId,path.targetInfo.id};
                LONG result=DisplayConfigGetDeviceInfo(&color.header);
                DISPLAYCONFIG_SOURCE_DEVICE_NAME name{};
                name.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof(name),path.sourceInfo.adapterId,path.sourceInfo.id};
                DisplayConfigGetDeviceInfo(&name.header);
                std::wprintf(L"%ls advanced-color query=%ld supported=%d enabled=%d bits=%lu\n",name.viewGdiDeviceName,result,color.advancedColorSupported,color.advancedColorEnabled,color.bitsPerColorChannel);
            }
        }
    }
    return EnumDisplayMonitors(nullptr,nullptr,Diagnose,0)?0:1;
}
