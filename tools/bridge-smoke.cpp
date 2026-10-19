// SPDX-License-Identifier: GPL-3.0-only
// Explicit hardware integration test. Never run as part of ordinary CTest.
#include "../src/brightness-engine.hpp"
#include "../src/standalone-bridge.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view(argv[1]) != "--exercise") {
        std::cerr << "Run with --exercise while Cursor Brightness is running. Tests one display and restores its original brightness.\n";
        return 2;
    }
    brightness::Engine observer;
    observer.Start();
    const auto displays = observer.GetDisplays();
    if (displays.empty() || displays.front().percent < 0) return 3;
    const auto target = displays.front();
    struct Restore {
        std::wstring id; int percent;
        ~Restore() {
            cursor_brightness::bridge::SetPercent(id, percent);
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
        }
    } restore{target.stableId, target.percent};
    if (cursor_brightness::bridge::SetPercent(target.stableId, -1) ||
        cursor_brightness::bridge::SetPercent(L"disconnected-test-display", 50)) return 4;
    const int test = target.percent >= 5 ? target.percent - 5 : target.percent + 5;
    if (!cursor_brightness::bridge::SetPercent(target.stableId, test)) return 5;
    bool matched = false;
    for (int i = 0; i < 10 && !matched; ++i) {
        observer.RequestRefresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        for (const auto& display : observer.GetDisplays())
            if (display.stableId == target.stableId && display.percent == test) matched = true;
    }
    std::cout << "Invalid and disconnected targets rejected; slider IPC write "
              << (matched ? "matched" : "FAILED") << " independent hardware readback.\n";
    observer.Stop();
    return matched ? 0 : 6;
}
