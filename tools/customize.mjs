// SPDX-License-Identifier: GPL-3.0-only
// Deterministically produce a self-contained Windhawk source from the pinned
// upstream and reviewed local components. Fail if an upstream anchor changes.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = p => fs.readFileSync(path.join(root, p), 'utf8').replaceAll('\r\n', '\n');
let source = read('upstream/per-monitor-brightness.wh.cpp');
function replace(old, next) {
    if (!source.includes(old)) throw Error(`Missing anchor: ${old.slice(0,100)}`);
    source = source.replace(old, next);
}
function functionBody(signature, replacement) {
    const start = source.indexOf(signature);
    if (start < 0) throw Error(`Missing function: ${signature}`);
    // Function bodies below are bounded by explicit next-function anchors.
    return start;
}
replace('// @id              per-monitor-brightness', '// @id              cursor-brightness');
replace('// @name            Per-monitor brightness and more in Quick Settings', '// @name            Cursor brightness in Quick Settings');
replace('// @description     Brightness, contrast, volume, input and power controls for every connected monitor, plus sliders for all of them at once, in the Windows 11 Quick Settings panel', '// @description     Scrollable per-monitor brightness sliders and cursor-targeted hardware brightness keys');
replace('// @version         3.0', '// @version         1.0.0');
replace('-lruntimeobject\n', '-lruntimeobject -lhid\n');
const upstreamRevision = read('upstream/REVISION').trim();
replace('// Source code is published', `// Local adaptation: cursor selection and HID consumer brightness keys.\n// Upstream: ramensoftware/windhawk-mods @ ${upstreamRevision}\n// Source code is published`);
const readmeStart = source.indexOf('// ==WindhawkModReadme==');
const settingsEnd = source.indexOf('// ==/WindhawkModSettings==') + '// ==/WindhawkModSettings=='.length;
source = source.slice(0, readmeStart) + `// ==WindhawkModReadme==
/*
# Cursor brightness
Open Quick Settings (Win+A) to see every monitor's labelled brightness slider.
Two monitors fit in view; scroll vertically for additional monitors.
Fn+F1/F2 (Consumer Control screen-brightness events) follows the cursor on every
press and repeat, including while Quick Settings is closed. No ordinary F keys
are intercepted. DDC/CI must be enabled in the monitor's own menu.
HDR displays use Windows SDR-content brightness (labelled SDR brightness),
keeping HDR enabled. SDR monitors use their hardware brightness over DDC/CI.
Diagnostic mode logs brightness presses without making keyboard-driven writes.
Disable the mod to restore the original Quick Settings layout.
Based on bardelyne's GPL-3.0 per-monitor-brightness v3.0, revision ${upstreamRevision}.
*/
// ==/WindhawkModReadme==
// ==WindhawkModSettings==
/*
- enableBrightnessKeys: true
  $name: Enable hardware brightness keys
- brightnessStep: 5
  $name: Brightness step (percentage points)
  $description: From 1 to 100. Applies to keys and mouse wheel.
- diagnosticKeys: false
  $name: Diagnose brightness keys without changing brightness
  $description: Enable the mod's logging to see press and release events.
- standaloneBridge: false
  $name: Use the standalone app's brightness engine
  $description: Enabled by the bundled installer. Requires Cursor Brightness to be running.
*/
// ==/WindhawkModSettings==` + source.slice(settingsEnd);
replace('#include <atomic>', read('src/brightness-core.hpp').replace('#pragma once', '') + '\n' + read('src/consumer-input.hpp').replace('#pragma once', '') + '\n' + read('src/hdr-brightness.hpp').replace('#pragma once', '') + '\n' + read('src/standalone-bridge.hpp').replace('#pragma once', '') + '\n#include <atomic>');
replace('enum class Transport {', 'enum class Transport {\n    HdrSdr = 3, // Windows SDR-content brightness while HDR stays enabled.');
// Keep original numeric transport values despite the additional member.
replace('    None,', '    None = 0,');
replace('    RECT rect{};', '    RECT rect{};\n    cursor_brightness::hdr::Target hdrTarget{};');
replace('        std::map<std::wstring, WmiFacts> facts = CollectWmiFacts();', '        const auto hdrTargets = cursor_brightness::hdr::Enumerate();\n        std::map<std::wstring, WmiFacts> facts = CollectWmiFacts();');
replace('            // DDC/CI first:', `            if (auto hdr = hdrTargets.find(mi.szDevice); hdr != hdrTargets.end()) {
                d.hdrTarget = hdr->second;
                d.percent = cursor_brightness::hdr::ReadPercent(d.hdrTarget);
                d.transport = d.percent >= 0 ? Transport::HdrSdr : Transport::None;
                if (d.name.empty()) d.name = gdiName.empty() ? L"HDR display" : gdiName;
                found.push_back(std::move(d));
                continue; // HDR uses SDR white level, never ignored DDC luminance writes.
            }

            // DDC/CI first:`);
replace('            std::vector<int> inputs;\n        };', '            std::vector<int> inputs;\n            cursor_brightness::hdr::Target hdrTarget;\n        };');
replace('d.volumeMax > 0, d.inputs});', 'd.volumeMax > 0, d.inputs, d.hdrTarget});');
replace('                case Transport::Wmi: {', '                case Transport::HdrSdr:\n                    percent = cursor_brightness::hdr::ReadPercent(target.hdrTarget);\n                    break;\n                case Transport::Wmi: {');
replace('        std::wstring wmiPath;\n        {', '        std::wstring wmiPath;\n        cursor_brightness::hdr::Target hdrTarget{};\n        {');
replace('                    wmiPath = d.wmiPath;', '                    wmiPath = d.wmiPath;\n                    hdrTarget = d.hdrTarget;');
replace('            case Transport::Wmi:\n                ok =', '            case Transport::HdrSdr:\n                ok = cursor_brightness::hdr::SetPercent(hdrTarget, percent);\n                break;\n            case Transport::Wmi:\n                ok =');
// No capability-string, contrast, power, input, or speaker-volume probing.
replace('                ProbeExtras(d);', '                // Brightness-only fork: do not probe unrelated VCP codes.');
replace('static_cast<int>((current * 100 + maximum / 2) / maximum)', 'cursor_brightness::RawToPercent(current, maximum)');
replace('DWORD raw = static_cast<DWORD>(\n                    (static_cast<DWORD>(percent) * vcpMax + 50) / 100);', 'DWORD raw = cursor_brightness::PercentToRaw(percent, vcpMax);');
replace('    bool LoadCapabilities() {', '    bool LoadCapabilities() {\n        return false; // No input selector; avoid capabilities-string requests.');
// Reject stale or unavailable targets before queuing a write.
replace('            if (rebaseFollow) {', `            Display* target = FindLocked(stableId);
            if (!target || target->transport == Transport::None || target->percent < 0) return;
            if (rebaseFollow) {`);
replace('    // Contrast, coalesced exactly like brightness.', `    bool AdjustAtPoint(POINT point, int direction, int step) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& d : displays_) {
                if (!cursor_brightness::Contains({d.rect.left, d.rect.top, d.rect.right, d.rect.bottom}, point.x, point.y)) continue;
                if (d.transport == Transport::None || d.percent < 0) return false;
                int effectiveStep = step;
                if (d.transport == Transport::DdcCi && d.vcpMax > 0) effectiveStep = std::max(step, static_cast<int>(std::ceil(100.0 / d.vcpMax)));
                const int value = cursor_brightness::Adjust(d.percent, direction, effectiveStep);
                if (value == d.percent) return true;
                d.percent = value;
                pending_[d.stableId] = value;
                lastRequest_[d.stableId] = std::chrono::steady_clock::now();
                changed = true;
                break;
            }
        }
        if (changed) work_.notify_all();
        return changed;
    }

    // Contrast, coalesced exactly like brightness.`);
// Hardware readback after the coalesced queue drains; never at startup.
replace('            pending_[stableId] = std::clamp(percent, 0, 100);', '            refresh_ = true;\n            pending_[stableId] = std::clamp(percent, 0, 100);');
replace('                pending_[d.stableId] = value;', '                pending_[d.stableId] = value;\n                refresh_ = true;');
// Do not let a slow readback overwrite a more recent pending key/slider request.
replace('                        d.percent = update.second;', '                        if (!pending_.contains(update.first)) d.percent = update.second;');
// A failed write is reflected by hardware readback and marks the target stale.
replace('        return transport;\n    }\n\n    std::thread worker_;', `        if (!ok && transport != Transport::None) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (Display* d = FindLocked(stableId)) { d->percent = -1; d->transport = Transport::None; }
            ddcAnswered_.erase(stableId);
            rescan_ = true;
        }
        return transport;
    }

    std::thread worker_;`);
replace('    std::vector<std::wstring> builtFor;', '    std::vector<std::wstring> builtFor;\n    unsigned targetGeneration = 0;');
// Keep the shell elements we mutate alive until their state is restored. On
// 26100 the temporary projection peers behind weak refs can expire immediately.
replace('winrt::weak_ref<wux::FrameworkElement> cardBorder;', 'wux::FrameworkElement cardBorder{nullptr};');
replace('winrt::weak_ref<wux::FrameworkElement> element;\n        int originalRow', 'wux::FrameworkElement element{nullptr};\n        int originalRow');
source = source.replaceAll('injection.cardBorder.get()', 'injection.cardBorder');
replace('injection.cardBorder = winrt::make_weak(fe);', 'injection.cardBorder = fe;');
replace('injection.movedChildren.push_back({winrt::make_weak(fe), row});', 'injection.movedChildren.push_back({fe, row});');
replace('moved.element.get()', 'moved.element');
// Recover the known 3-row native layout after an older test instance whose
// weak references expired. The card ends before FooterGrid on this layout.
replace('    // Before building the panel: the icons need', `    for (uint32_t i = 0; i < grid.Children().Size(); ++i) {
        auto child = grid.Children().GetAt(i).try_as<wux::FrameworkElement>();
        if (child && child.Name() == L"FooterGrid" && rowCount == 3) {
            const int footerRow = std::min(wuxc::Grid::GetRow(child), static_cast<int>(rowCount) - 1);
            if (wuxc::Grid::GetRow(child) != footerRow) wuxc::Grid::SetRow(child, footerRow);
            for (uint32_t j = 0; j < grid.Children().Size(); ++j) {
                auto card = grid.Children().GetAt(j).try_as<wuxc::Border>();
                if (card && footerRow == 2 && wuxc::Grid::GetRow(card) == 0 && wuxc::Grid::GetRowSpan(card) > footerRow) {
                    wuxc::Grid::SetRowSpan(card, footerRow);
                    Wh_Log(L"Recovered card span left by an older instance");
                }
            }
        }
    }
    LatchCursorTarget();

    // Before building the panel: the icons need`);
const panelStart = functionBody('void PopulateSliderPanel(');
const panelEnd = source.indexOf('wuxc::StackPanel BuildSliderPanel(', panelStart);
if (panelEnd < 0) throw Error('Missing BuildSliderPanel boundary');
source = source.slice(0, panelStart) + read('src/cursor-ui.inc') + '\n\n' + source.slice(panelEnd);
replace('currentIds.push_back(d.stableId);', 'currentIds.push_back(PanelDisplaySignature(d));');
replace('if (injection.builtFor != currentIds)', `std::vector<std::wstring> availableIds, boundIds;
        for (const auto& d : displays) if (d.transport != brightness::Transport::None && d.percent >= 0) availableIds.push_back(d.stableId);
        for (const auto& binding : injection.bindings) boundIds.push_back(binding.id);
        if (injection.builtFor != currentIds || availableIds != boundIds)`);
replace('    static void CALLBACK WinEventProc(', '    cursor_brightness::ConsumerInput input_;\n\n    static void CALLBACK WinEventProc(');
replace('if (event != EVENT_OBJECT_SHOW || idObject != OBJID_WINDOW ||', 'if ((event != EVENT_OBJECT_SHOW && event != EVENT_OBJECT_HIDE) || idObject != OBJID_WINDOW ||');
replace('        // A burst of show events must not become', `        if (event == EVENT_OBJECT_HIDE) { CloseCursorTarget(); return; }
        LatchCursorTarget();

        // A burst of show events must not become`);
replace('            case WM_DISPLAYCHANGE:', `            case WM_NCCREATE: {
                auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
                break;
            }
            case WM_INPUT: {
                auto* self = reinterpret_cast<ShellEventWatcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (self) self->input_.Input(reinterpret_cast<HRAWINPUT>(lParam), GetTickCount64());
                break;
            }
            case WM_INPUT_DEVICE_CHANGE: {
                auto* self = reinterpret_cast<ShellEventWatcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (self && wParam == GIDC_REMOVAL) self->input_.Remove(reinterpret_cast<HANDLE>(lParam));
                break;
            }
            case WM_TIMER: {
                auto* self = reinterpret_cast<ShellEventWatcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (self) self->input_.Tick(GetTickCount64());
                break;
            }
            case WM_POWERBROADCAST:
                if (auto* self = reinterpret_cast<ShellEventWatcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA))) self->input_.Reset();
                if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
                    if (g_engine) g_engine->RequestRescan();
                }
                break;
            case WM_DISPLAYCHANGE:`);
replace('        switch (msg) {\n            case WM_NCCREATE:', `        static const UINT externalChange = RegisterWindowMessageW(L"CursorBrightness.ExternalBrightnessChanged.v1");
        if (msg == externalChange) {
            if (g_engine) g_engine->RequestRefresh();
            return 0;
        }
        switch (msg) {
            case WM_NCCREATE:`);
replace('0, nullptr, nullptr, wc.hInstance, nullptr);', '0, nullptr, nullptr, wc.hInstance, this);');
replace('        // Must be installed and removed on the thread that pumps messages.', `        input_.log = [](const wchar_t* text) { Wh_Log(L"HID: %ls", text); };
        input_.onStep = [](int direction) {
            if (!g_enableBrightnessKeys.load() || g_diagnosticKeys.load() || !g_engine || !g_engineStarted.load()) return;
            POINT point{};
            if (GetCursorPos(&point) && !g_engine->AdjustAtPoint(point, direction, g_brightnessStep.load())) Wh_Log(L"Brightness unavailable under cursor");
        };
        if (HWND window = hwnd_.load()) {
            if (g_enableBrightnessKeys.load()) input_.Register(window);
            SetTimer(window, 0xCB01, 25, nullptr);
        }

        // Must be installed and removed on the thread that pumps messages.`);
replace('SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW,', 'SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE,');
// Both startup-abort and ordinary exit must unregister Raw Input before unload.
replace('            if (hook_) {\n                UnhookWinEvent(hook_);', '            input_.Stop();\n            if (hook_) {\n                UnhookWinEvent(hook_);');
replace('        if (hook_) {\n            UnhookWinEvent(hook_);', '        input_.Stop();\n        if (HWND window = hwnd_.exchange(nullptr)) DestroyWindow(window);\n        if (hook_) {\n            UnhookWinEvent(hook_);');
replace('    int ret = ControlCenterView_OnGotFocus_Original(pThis, args);', '    LatchCursorTarget();\n    int ret = ControlCenterView_OnGotFocus_Original(pThis, args);');
// Settings are deliberately restricted to the requested capability.
const settingsStart = functionBody('ModSettings ReadSettings()');
const settingsFinish = source.indexOf('\nvoid ApplySettings(', settingsStart);
source = source.slice(0, settingsStart) + `ModSettings ReadSettings() {
    ModSettings out;
    out.hideStockBrightness = true;
    out.hideUnsupported = false;
    out.followMode = brightness::FollowMode::Off;
    out.showPerMonitor = true;
    out.showMasterBrightness = false;
    out.showContrast = out.showMasterContrast = out.showPowerButton = false;
    out.showVolume = out.showInputSwitcher = false;
    out.scrollStep = std::clamp(Wh_GetIntSetting(L"brightnessStep"), 1, 100);
    g_enableBrightnessKeys.store(Wh_GetIntSetting(L"enableBrightnessKeys") != 0);
    g_diagnosticKeys.store(Wh_GetIntSetting(L"diagnosticKeys") != 0);
    g_standaloneBridge.store(Wh_GetIntSetting(L"standaloneBridge") != 0);
    g_brightnessStep.store(out.scrollStep);
    return out;
}
` + source.slice(settingsFinish);
const changedStart = functionBody('BOOL Wh_ModSettingsChanged(');
const changedEnd = source.indexOf('// Removing the injected UI', changedStart);
source = source.slice(0, changedStart) + `BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    *bReload = TRUE; // Re-register input and clear held keys on settings changes.
    return TRUE;
}

` + source.slice(changedEnd);
fs.mkdirSync(path.join(root, 'mods'), {recursive:true});
fs.writeFileSync(path.join(root, 'mods/cursor-brightness.wh.cpp'), source);
console.log(`Generated cursor-brightness.wh.cpp from ${upstreamRevision}`);
