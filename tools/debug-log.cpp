// SPDX-License-Identifier: GPL-3.0-only
// Local DebugView-compatible reader, filtering all non-Windhawk output out.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(int argc, char** argv) {
    DWORD seconds = argc > 1 ? DWORD(std::atoi(argv[1])) : 180;
    HANDLE ready = CreateEventW(nullptr, FALSE, FALSE, L"DBWIN_BUFFER_READY");
    HANDLE data = CreateEventW(nullptr, FALSE, FALSE, L"DBWIN_DATA_READY");
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, L"DBWIN_BUFFER");
    if (!ready || !data || !mapping || GetLastError() == ERROR_ALREADY_EXISTS) return 2;
    const auto* buffer = static_cast<const char*>(MapViewOfFile(mapping, FILE_MAP_READ, 0,0,4096));
    if (!buffer) return 3;
    ULONGLONG end = GetTickCount64() + seconds * 1000ull;
    std::puts("Windhawk log capture ready"); std::fflush(stdout);
    while (GetTickCount64() < end) {
        SetEvent(ready);
        if (WaitForSingleObject(data, 1000) != WAIT_OBJECT_0) continue;
        char message[4093]{};
        std::memcpy(message, buffer + sizeof(DWORD), 4092);
        if (std::strstr(message, "[WH]")) {
            std::printf("%lu %s", *reinterpret_cast<const DWORD*>(buffer), message);
            std::fflush(stdout);
        }
    }
    UnmapViewOfFile(buffer); CloseHandle(mapping); CloseHandle(ready); CloseHandle(data);
}
