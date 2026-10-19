// SPDX-License-Identifier: GPL-3.0-only
#include "../src/brightness-core.hpp"
#include "../src/hdr-brightness.hpp"
#include <cstdio>
#include <cstring>
int main(int argc, char** argv) {
    bool exercise = argc > 1 && std::strcmp(argv[1], "--exercise") == 0;
    auto targets = cursor_brightness::hdr::Enumerate();
    if (targets.empty()) { std::puts("No active HDR targets"); return 2; }
    for (auto const& [name, target] : targets) {
        ULONG original = 0;
        bool readable = cursor_brightness::hdr::ReadLevel(target, &original);
        std::wprintf(L"%ls HDR active=%d white-level=%lu slider=%d%%\n", name.c_str(), cursor_brightness::hdr::Active(target), original, cursor_brightness::hdr::ReadPercent(target));
        if (exercise && readable && original >= 1000 && original <= 6000) {
            struct Restore { cursor_brightness::hdr::Target target; ULONG level; ~Restore(){cursor_brightness::hdr::SetLevel(target,level);} } restore{target,original};
            int before = cursor_brightness::hdr::ReadPercent(target);
            int requested = before >= 50 ? before - 5 : before + 5;
            bool ok = cursor_brightness::hdr::SetPercent(target, requested);
            Sleep(250);
            int after = cursor_brightness::hdr::ReadPercent(target);
            std::printf("  SDR TEST write=%d%% ok=%d readback=%d%% HDR-still-active=%d; restoring raw=%lu\n",requested,ok,after,cursor_brightness::hdr::Active(target),original);
            if (!ok || requested != after) return 3;
        }
    }
    return 0;
}
