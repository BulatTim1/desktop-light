// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace cursor_brightness {
struct Bounds { long left, top, right, bottom; };
inline bool Contains(Bounds r, long x, long y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}
inline int Adjust(int current, int direction, int step) {
    return std::clamp(current + direction * std::clamp(step, 1, 100), 0, 100);
}
inline unsigned PercentToRaw(int percent, unsigned maximum) {
    return unsigned((std::uint64_t(std::clamp(percent, 0, 100)) * maximum + 50) / 100);
}
inline int RawToPercent(unsigned value, unsigned maximum) {
    return maximum ? int(std::min<std::uint64_t>(100, (std::uint64_t(value) * 100 + maximum / 2) / maximum)) : -1;
}
// Match Windows' SDR-content slider: 0..100 maps to 80..480 reference nits.
inline unsigned PercentToWhiteLevel(int percent) { return 1000 + 50 * std::clamp(percent, 0, 100); }
inline int WhiteLevelToPercent(unsigned value) { return int(std::clamp<long long>((static_cast<long long>(value) - 1000 + 25) / 50, 0, 100)); }
struct Candidate { std::wstring id; Bounds bounds; };
inline bool FreshNativePopup(int percent, bool enabled, std::uint64_t tick, std::uint64_t now) {
    return enabled && percent >= 0 && percent <= 100 && tick <= now && now - tick <= 2000;
}
struct PanelSelection {
    bool open = false, resolved = false;
    long x = 0, y = 0;
    std::wstring id;
    bool Open(long cursorX, long cursorY) {
        if (open) return false;
        open = true; resolved = false; id.clear();
        x = cursorX; y = cursorY;
        return true;
    }
    void Close() { open = false; }
    std::wstring Resolve(const std::vector<Candidate>& displays) {
        if (!resolved && !displays.empty()) {
            for (const auto& display : displays) {
                if (Contains(display.bounds, x, y)) { id = display.id; break; }
            }
            resolved = true;
        }
        return id;
    }
};
// Repeated identical HID reports do not reset the repeat deadline or trigger
// another immediate step. Release, direction changes and removal reset it.
struct RepeatState {
    int held = 0;
    std::uint64_t next = 0;
    bool Update(int direction, std::uint64_t now) {
        if (direction == held) return false;
        held = direction;
        next = direction ? now + 400 : 0;
        return direction != 0;
    }
    bool Tick(std::uint64_t now) {
        if (!held || now < next) return false;
        // Never replay missed repeats after a blocked message loop or resume.
        next = now + 100;
        return true;
    }
    void Reset() { held = 0; next = 0; }
};
} // namespace cursor_brightness
