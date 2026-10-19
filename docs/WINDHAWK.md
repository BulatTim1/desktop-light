# Cursor brightness for Windows 11

**English** · [Русский](WINDHAWK.ru.md)

This guide covers the **separate development / existing-Windhawk installation**.
Most users should use the complete installer described in the root README,
which includes its own private runtime and controls its lifecycle automatically.
The `.cache` layout and `control.ps1` startup task below are development tools.
Do not enable that separate startup task alongside the complete application.

For an existing Windhawk installation with the native app, set
`standaloneBridge=true` and `enableBrightnessKeys=false` in the mod settings.
The native app must be running. For the old mod-only installation, keep
`standaloneBridge=false` and enable its brightness keys instead.

The complete application also loads `mods/cursor-brightness-native-popup.wh.cpp`
into Explorer to synchronize the existing Windows brightness popup. That helper
requires the native app's shared state; the older mod-only installation does
not provide it. It changes only the popup's displayed value.

Per-monitor brightness sliders in **Quick Settings (Win+A)** and hardware brightness keys
that follow the mouse cursor. Tested on Windows 11 build **26100.9457**, with a
**Lofree Flow (Fn+F1/F2)**, **AOC Q27G2WG4**, and **Titan Army G2785S**.

- **Fn+F1** decreases and **Fn+F2** increases brightness on the display under
  the cursor. Each press changes it by 5 percentage points. Hold a key to repeat
  after 400 ms, then every 100 ms. Ordinary F1/F2 and other media keys are untouched.
- **Win+A** shows every monitor's labelled slider, with two monitor rows visible
  and vertical scrolling for additional displays. Each row keeps its own monitor
  identity while you drag. Brightness keys still follow the cursor.
- An **SDR monitor** uses actual hardware brightness via DDC/CI (VCP `0x10`).
- An **HDR monitor** uses Windows' **SDR-content brightness**, labelled
  **“SDR brightness”**. HDR stays enabled; this changes desktop/SDR white level,
  not the panel's HDR peak brightness or HDR games' own brightness settings.
- Startup and discovery only read existing brightness. No gamma ramps or dark
  overlays are used. A missing or unavailable selected monitor is never replaced
  by another target.

## Development installation

The portable Windhawk runtime is in `.cache\windhawk` inside this project.
It hosts the extension without installing a display driver or a system service.
Keep this directory and project path intact while login startup is enabled.

Stop or restart from PowerShell in this project:

```powershell
.\tools\control.ps1 -Action Stop
.\tools\control.ps1 -Action Start
```

Manage login startup:

```powershell
.\tools\control.ps1 -Action EnableStartup
.\tools\control.ps1 -Action DisableStartup
.\tools\control.ps1 -Action Status
```

Login startup uses a Task Scheduler task named **CursorBrightness**, restricted
to your interactive user session. It waits 15 seconds after login so the shell
can initialize, sets the runtime's working directory explicitly, and retries up
to three failed launches at one-minute intervals. It runs at normal user
privileges, including on battery, without requiring a password. Enabling it
migrates this project's older registry Run entry to the task. `DisableStartup`
and `Uninstall` remove both forms of this installation's startup registration.

`Stop` exits this portable Windhawk runtime and unloads its mods. If you install
other mods in the same runtime later, they will also stop. You can disable only
**Cursor brightness in Quick Settings** through Windhawk's Installed Mods page.

To uninstall this customization:

```powershell
.\tools\control.ps1 -Action Uninstall
```

This removes its login-startup entry, stops the portable runtime, and disables
the mod. Sources and portable files remain available for rebuilding. Hardware
brightness and SDR-content brightness stay at your latest chosen values.

## Build and install from source

Use Node.js and PowerShell. The setup script downloads the pinned official
Windhawk 1.7.3 offline installer and checks its Authenticode publisher signature.
The compiler is included in the portable package; Visual Studio is unnecessary.

```powershell
.\tools\setup-portable.ps1
.\tools\control.ps1 -Action Stop  # if already running
.\tools\build.ps1
.\tools\install-portable.ps1 -Start
```

For a different **portable** Windhawk directory, pass `-WindhawkPath` to these
PowerShell scripts. The installer never creates a startup entry. Enable startup
only after checking the panel and keys on your Windows build.

The standalone Windhawk source is `mods\cursor-brightness.wh.cpp`. It can also be
used with Windhawk's **Create a new mod** editor: replace the sample source with
the full file, compile it, and enable it. Disable the upstream per-monitor
brightness mod if installed, so that two extensions do not modify the same panel.

Build inputs are `upstream\per-monitor-brightness.wh.cpp`, `upstream\REVISION`,
the components under `src`, and `tools\customize.mjs`. Generation fails when an
expected upstream anchor is absent. Rebuild with `tools\build.ps1`; do not edit
the generated source independently.

## Settings and diagnostics

Windhawk exposes **Enable hardware brightness keys**, **Brightness step**, and
**Diagnose brightness keys without changing brightness**. Defaults are enabled,
5 points, and diagnostic mode off. Settings changes reload the extension and
clear held-key state.

With the portable runtime stopped, `install-portable.ps1 -DiagnosticKeys -Start`
installs a diagnostic configuration. Enable per-mod logging in Windhawk to see
brightness presses and releases. Slider changes remain active in diagnostic mode.
Consumer reports are decoded from the device's HID descriptor; ordinary keyboard
typing is never registered or logged.

Built diagnostic programs:

```powershell
.\build\brightness-core-tests.exe
.\build\key-probe.exe 60          # read-only: press Fn+F1/F2
.\build\monitor-probe.exe         # read-only DDC/CI values and monitor bounds
.\build\hdr-probe.exe             # read-only HDR state and SDR white level
.\build\hdr-probe.exe --exercise  # temporary 5-point change, restores exact value
```

`monitor-diagnostics.exe --exercise-primary` temporarily tests primary-monitor
hardware brightness at 50% and restores the original value. It demonstrated that
the G2785S acknowledges DDC writes but ignores them while HDR is enabled.
`debug-log.exe 120` captures only Windhawk debug messages for 120 seconds; run it
before starting Windhawk and do not run a second debug-output reader concurrently.

## Compatibility and limitations

The Quick Settings extension targets `ShellHost.exe` and validates required shell
symbols and grid structure before injecting. Missing hooks leave the system
panel usable. Shell updates can require a new compatibility check. DDC/CI must be
enabled in the monitor's menu and supported by the cable/dock.

HDR is detected separately from wide-color-gamut mode. SDR white level is read
using Microsoft's documented `DISPLAYCONFIG_SDR_WHITE_LEVEL`; the setter uses
the Windows private `0xFFFFFFEE` device-info operation, verified on this build.
Its 0–100 scale maps to 80–480 reference nits. Unsupported calls report failure
and do not disable HDR or fall back to changing another display.

The complete Settings > System > Display app is not modified. Display reconnect,
layout changes, and resume request a background rescan. Current display identities
are rebuilt after reconnect; no previous brightness levels are applied on restart.

## Attribution and license

GPL-3.0-only; see `LICENSE`. Adapted from bardelyne's
[per-monitor-brightness v3.0](https://github.com/ramensoftware/windhawk-mods/blob/b00b5e3043a606e9864405a111acaf29d69dc170/mods/per-monitor-brightness.wh.cpp),
revision `b00b5e3043a606e9864405a111acaf29d69dc170`. The untouched upstream source
and revision are retained locally. Changes add cursor-target latching, HID
brightness keys, HDR SDR-content control, readback, and layout restoration.

References:
[Windows SDR white level](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level),
[SDR setter ABI example](https://github.com/ledoge/set_maxtml/blob/master/main_sdrwhite.c),
[Windhawk development](https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod).
