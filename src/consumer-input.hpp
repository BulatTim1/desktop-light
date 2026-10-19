// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <functional>
#include <cstddef>
#include <map>
#include <set>
#include <vector>

namespace cursor_brightness {
// Receives only the Consumer Control collection, never ordinary typing.
// All methods run on the owning window's message thread. No hardware writes.
class ConsumerInput {
public:
    using Step = std::function<void(int)>;
    using Log = std::function<void(const wchar_t*)>;
    Step onStep;
    Log log;

    bool Register(HWND window) {
        // RegisterRawInputDevices replaces registrations within a process.
        // Do not steal a collection already owned by the Windows shell.
        UINT count = 0;
        if (GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) == UINT(-1)) return false;
        std::vector<RAWINPUTDEVICE> existing(count);
        if (count && GetRegisteredRawInputDevices(existing.data(), &count, sizeof(RAWINPUTDEVICE)) == UINT(-1)) return false;
        for (auto const& item : existing) {
            if (item.usUsagePage == 0x0c && (item.usUsage == 1 || item.usUsage == 0)) {
                Say(L"Consumer input is already registered by this process; keys left untouched");
                return false;
            }
        }
        RAWINPUTDEVICE device{0x0c, 1, RIDEV_INPUTSINK | RIDEV_DEVNOTIFY, window};
        registered_ = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
        Say(registered_ ? L"Consumer input registered" : L"Consumer input registration failed");
        if (registered_) Enumerate();
        return registered_;
    }
    void Stop() {
        if (registered_) {
            RAWINPUTDEVICE device{0x0c, 1, RIDEV_REMOVE, nullptr};
            RegisterRawInputDevices(&device, 1, sizeof(device));
        }
        registered_ = false;
        devices_.clear();
    }
    void Reset() { for (auto& [_, d] : devices_) d.pressed.clear(), d.repeat.Reset(); }
    void Remove(HANDLE device) { devices_.erase(device); }
    void Tick(std::uint64_t now) {
        for (auto& [_, d] : devices_) {
            if (d.repeat.Tick(now) && onStep) onStep(d.repeat.held);
        }
    }
    void Input(HRAWINPUT handle, std::uint64_t now) {
        UINT bytes = 0;
        if (GetRawInputData(handle, RID_INPUT, nullptr, &bytes, sizeof(RAWINPUTHEADER)) == UINT(-1) || bytes < sizeof(RAWINPUTHEADER)) return;
        // vector<max_align_t> gives RAWINPUT and its header natural alignment.
        std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        if (GetRawInputData(handle, RID_INPUT, storage.data(), &bytes, sizeof(RAWINPUTHEADER)) == UINT(-1)) return;
        const auto* raw = reinterpret_cast<const RAWINPUT*>(storage.data());
        if (raw->header.dwType != RIM_TYPEHID || bytes < offsetof(RAWINPUT, data.hid.bRawData)) return;
        const RAWHID& hid = raw->data.hid;
        const size_t available = bytes - offsetof(RAWINPUT, data.hid.bRawData);
        if (!hid.dwSizeHid || hid.dwCount > available / hid.dwSizeHid) return;
        Device& d = DeviceFor(raw->header.hDevice);
        if (!d.valid) return;
        for (DWORD index = 0; index < hid.dwCount; ++index) {
            auto* report = reinterpret_cast<PCHAR>(const_cast<BYTE*>(hid.bRawData + index * hid.dwSizeHid));
            BYTE reportId = d.numbered ? BYTE(report[0]) : 0;
            if (!d.brightnessReports.count(reportId)) continue;
            int direction = 0;
            bool up = false, down = false, decoded = false;
            ULONG length = HidP_MaxUsageListLength(HidP_Input, 0, d.Data());
            std::vector<USAGE_AND_PAGE> usages(length);
            if (length && HidP_GetUsagesEx(HidP_Input, 0, usages.data(), &length, d.Data(), report, hid.dwSizeHid) == HIDP_STATUS_SUCCESS) {
                decoded = true;
                for (ULONG n = 0; n < length; ++n) {
                    if (usages[n].UsagePage != 0x0c) continue;
                    up |= usages[n].Usage == 0x6f;
                    down |= usages[n].Usage == 0x70;
                }
            }
            // A few devices describe the controls as variable value fields.
            for (auto const& cap : d.values) {
                if (cap.ReportID != reportId || cap.UsagePage != 0x0c) continue;
                for (USAGE usage : {USAGE(0x6f), USAGE(0x70)}) {
                    bool includes = cap.IsRange ? usage >= cap.Range.UsageMin && usage <= cap.Range.UsageMax : cap.NotRange.Usage == usage;
                    if (!includes) continue;
                    ULONG value = 0;
                    if (HidP_GetUsageValue(HidP_Input, 0x0c, cap.LinkCollection, usage, &value, d.Data(), report, hid.dwSizeHid) == HIDP_STATUS_SUCCESS) {
                        decoded = true;
                        if (value) (usage == 0x6f ? up : down) = true;
                    }
                }
            }
            if (!decoded) continue; // Malformed/unrelated reports cannot release a key.
            d.pressed[reportId] = up == down ? 0 : up ? 1 : -1;
            bool anyUp = false, anyDown = false;
            for (auto const& [_, value] : d.pressed) anyUp |= value > 0, anyDown |= value < 0;
            direction = anyUp == anyDown ? 0 : anyUp ? 1 : -1;
            if (direction != d.repeat.held) Say(direction > 0 ? L"brightness-up press" : direction < 0 ? L"brightness-down press" : L"brightness release");
            if (d.repeat.Update(direction, now) && onStep) onStep(direction);
        }
    }
private:
    struct Device {
        std::vector<std::max_align_t> preparsed;
        std::vector<HIDP_VALUE_CAPS> values;
        std::set<BYTE> brightnessReports;
        std::map<BYTE, int> pressed;
        RepeatState repeat;
        bool valid = false, numbered = false;
        PHIDP_PREPARSED_DATA Data() { return reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsed.data()); }
    };
    std::map<HANDLE, Device> devices_;
    bool registered_ = false;
    void Say(const wchar_t* value) { if (log) log(value); }
    void Enumerate() {
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) == UINT(-1)) return;
        std::vector<RAWINPUTDEVICELIST> devices(count);
        if (count && GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST)) == UINT(-1)) return;
        for (auto const& device : devices) {
            if (device.dwType != RIM_TYPEHID) continue;
            RID_DEVICE_INFO info{}; info.cbSize = sizeof(info);
            UINT size = sizeof(info);
            if (GetRawInputDeviceInfoW(device.hDevice, RIDI_DEVICEINFO, &info, &size) == UINT(-1)) continue;
            if (info.hid.usUsagePage == 0x0c && info.hid.usUsage == 1) DeviceFor(device.hDevice);
        }
    }
    Device& DeviceFor(HANDLE handle) {
        auto [entry, fresh] = devices_.try_emplace(handle);
        Device& d = entry->second;
        if (!fresh) return d;
        UINT bytes = 0;
        if (GetRawInputDeviceInfoW(handle, RIDI_PREPARSEDDATA, nullptr, &bytes) == UINT(-1) || !bytes) return d;
        d.preparsed.resize((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
        if (GetRawInputDeviceInfoW(handle, RIDI_PREPARSEDDATA, d.preparsed.data(), &bytes) == UINT(-1)) return d;
        HIDP_CAPS caps{};
        if (HidP_GetCaps(d.Data(), &caps) != HIDP_STATUS_SUCCESS) return d;
        USHORT count = caps.NumberInputButtonCaps;
        std::vector<HIDP_BUTTON_CAPS> buttons(count);
        if (count && HidP_GetButtonCaps(HidP_Input, buttons.data(), &count, d.Data()) == HIDP_STATUS_SUCCESS) {
            for (auto const& cap : buttons) {
                d.numbered |= cap.ReportID != 0;
                if (cap.UsagePage != 0x0c) continue;
                bool brightness = cap.IsRange ? cap.Range.UsageMin <= 0x70 && cap.Range.UsageMax >= 0x6f : cap.NotRange.Usage == 0x6f || cap.NotRange.Usage == 0x70;
                if (brightness) d.brightnessReports.insert(cap.ReportID);
            }
        }
        count = caps.NumberInputValueCaps;
        d.values.resize(count);
        if (count && HidP_GetValueCaps(HidP_Input, d.values.data(), &count, d.Data()) != HIDP_STATUS_SUCCESS) d.values.clear();
        for (auto const& cap : d.values) {
            d.numbered |= cap.ReportID != 0;
            if (cap.UsagePage != 0x0c) continue;
            bool brightness = cap.IsRange ? cap.Range.UsageMin <= 0x70 && cap.Range.UsageMax >= 0x6f : cap.NotRange.Usage == 0x6f || cap.NotRange.Usage == 0x70;
            if (brightness) d.brightnessReports.insert(cap.ReportID);
        }
        d.valid = !d.brightnessReports.empty();
        Say(d.valid ? L"Consumer device exposes screen-brightness usages" : L"Consumer device has no screen-brightness usages");
        return d;
    }
};
} // namespace cursor_brightness
