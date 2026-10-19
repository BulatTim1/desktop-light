// SPDX-License-Identifier: GPL-3.0-only
#include "../src/brightness-core.hpp"
#include "../src/consumer-input.hpp"
#include <cstdio>

cursor_brightness::ConsumerInput input;
unsigned presses = 0;
LRESULT CALLBACK ProbeWindow(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INPUT: input.Input(reinterpret_cast<HRAWINPUT>(lp), GetTickCount64()); break;
    case WM_INPUT_DEVICE_CHANGE: if (wp == GIDC_REMOVAL) input.Remove(reinterpret_cast<HANDLE>(lp)); break;
    case WM_TIMER: DestroyWindow(window); return 0;
    case WM_DESTROY: input.Stop(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
int main(int argc, char** argv) {
    unsigned seconds = argc > 1 ? unsigned(std::clamp(std::atoi(argv[1]), 5, 600)) : 60;
    input.log = [](const wchar_t* text) { std::wprintf(L"%llu %ls\n", GetTickCount64(), text); std::fflush(stdout); };
    input.onStep = [](int direction) { ++presses; std::printf("VERIFIED brightness %s (no hardware writes)\n", direction > 0 ? "up" : "down"); std::fflush(stdout); };
    WNDCLASSW wc{}; wc.lpfnWndProc = ProbeWindow; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"CursorBrightnessKeyProbe";
    if (!RegisterClassW(&wc)) return 2;
    HWND window = CreateWindowW(wc.lpszClassName, L"", 0, 0,0,0,0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!window || !input.Register(window)) return 3;
    std::printf("READY: press Fn+F1 and Fn+F2; no brightness will change. Listening %u seconds.\n", seconds); std::fflush(stdout);
    SetTimer(window, 1, seconds * 1000, nullptr);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) DispatchMessageW(&msg);
    std::printf("Captured %u brightness presses.\n", presses);
    return presses >= 2 ? 0 : 4;
}
