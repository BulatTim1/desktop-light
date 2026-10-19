// SPDX-License-Identifier: GPL-3.0-only
#include "../src/brightness-core.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace cursor_brightness;
    assert(Contains({-1440,0,0,2560}, -1, 200));
    assert(!Contains({-1440,0,0,2560}, 0, 200));
    assert(Contains({0,0,2560,1440}, 0, 0));
    assert(!Contains({0,0,2560,1440}, 2560, 0));
    assert(Adjust(98, 1, 5) == 100 && Adjust(2,-1,5) == 0);
    assert(Adjust(50,-1,5) == 45 && Adjust(50,1,5) == 55);
    assert(PercentToRaw(56, 50) == 28 && RawToPercent(28,50) == 56);
    assert(PercentToRaw(100, 200) == 200 && PercentToRaw(-10, 100) == 0);
    assert(RawToPercent(200, 100) == 100 && RawToPercent(1, 0) == -1);
    assert(PercentToRaw(100, 0xffffffffu) == 0xffffffffu);
    assert(PercentToWhiteLevel(0) == 1000 && PercentToWhiteLevel(100) == 6000);
    assert(PercentToWhiteLevel(20) == 2000 && WhiteLevelToPercent(2000) == 20);
    assert(WhiteLevelToPercent(0) == 0 && WhiteLevelToPercent(10000) == 100);
    PanelSelection selection;
    const std::vector<Candidate> displays{{L"left", {-1440,0,0,2560}}, {L"right", {0,0,2560,1440}}};
    assert(selection.Open(-100, 300));
    assert(selection.Resolve({}).empty()); // Enumeration may finish after open.
    assert(selection.Resolve(displays) == L"left");
    assert(!selection.Open(500, 300)); // Moving onto a slider cannot retarget it.
    assert(selection.Resolve({displays[1]}) == L"left"); // Disconnect never redirects.
    selection.Close(); assert(selection.Open(500,300));
    assert(selection.Resolve(displays) == L"right");
    selection.Close(); selection.Open(5000,5000);
    assert(selection.Resolve(displays).empty()); // No nearest-monitor fallback.
    RepeatState repeat;
    assert(FreshNativePopup(50,true,1000,1000));
    assert(FreshNativePopup(100,true,1000,3000));
    assert(!FreshNativePopup(50,true,1000,3001));
    assert(!FreshNativePopup(50,false,1000,1000));
    assert(!FreshNativePopup(-1,true,1000,1000));
    assert(!FreshNativePopup(101,true,1000,1000));
    assert(!FreshNativePopup(50,true,1001,1000));
    assert(repeat.Update(-1, 1000));
    assert(!repeat.Update(-1, 1010));
    assert(!repeat.Tick(1399));
    assert(repeat.Tick(1400));
    assert(!repeat.Tick(1499));
    assert(repeat.Tick(1500));
    assert(!repeat.Update(0, 1510) && !repeat.Tick(10000));
    assert(repeat.Update(1, 10001));
    assert(repeat.Update(-1, 10002));
    assert(!repeat.Tick(10401) && repeat.Tick(10402));
    assert(repeat.Tick(99999) && !repeat.Tick(99999));
    repeat.Reset(); assert(!repeat.Tick(100000));
    std::cout << "Cursor selection/latching, disconnects, brightness normalization/limits, repeat timing, deduplication and release: passed\n";
}
