// SPDX-License-Identifier: GPL-3.0-only
#include "../src/brightness-engine.hpp"
#include <cstdio>
int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    brightness::Engine engine;
    engine.Start();
    const auto displays = engine.GetDisplays();
    for (const auto& display : displays) {
        std::wprintf(L"%ls mode=%d brightness=%d%% bounds=%ld,%ld,%ld,%ld\n",
            display.name.c_str(), static_cast<int>(display.transport), display.percent,
            display.rect.left, display.rect.top, display.rect.right, display.rect.bottom);
    }
    engine.Stop();
    return displays.empty() ? 2 : 0;
}
