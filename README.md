# Desktop Light · Cursor Brightness

<img src="assets/icon.svg" width="64" height="64" alt="Cursor Brightness sun icon">

A small standalone Windows app with brightness keys that follow **the display
under your cursor**, per-monitor Windows Quick Settings (**Win+A**) controls,
and a system-tray slider.

[Build status](https://github.com/BulatTim1/desktop-light/actions/workflows/build.yml)
· [Releases](https://github.com/BulatTim1/desktop-light/releases)
· [Report an issue](https://github.com/BulatTim1/desktop-light/issues)

Desktop Light is the repository for the **Cursor Brightness** Windows app.

**One installer includes the app and a private Windhawk runtime for Win+A.**
No separate Windhawk installation, .NET installation, administrator account, or
display driver is required. The app sends no telemetry. The shell helper may
download matching Windows symbols when first used or after a Windows update.

## Features

- Hardware brightness keys follow the cursor, including **Lofree Flow Fn+F1/F2**.
- **SDR monitors:** real hardware brightness through DDC/CI.
- **HDR monitors:** Windows SDR-content brightness while HDR stays enabled.
- Built-in laptop display brightness through WMI when available.
- Tray window with display selection, slider, percentage and current control mode.
- A labelled Win+A slider for every monitor: two visible, with scrolling for more.
- Adjustable keyboard steps (5 percentage points by default), held-key repetition,
  and synchronization with the existing Windows brightness popup.
- Optional login startup, background/coalesced writes, and reconnect/resume handling.

Ordinary F1/F2 and unrelated media keys are not remapped. The keyboard must send
standard Consumer Control screen-brightness events; firmware-only Fn actions
that never reach Windows cannot be detected.

## Download and use

Download a package from the [Releases page](https://github.com/BulatTim1/desktop-light/releases)
when a release is available. Build artifacts are also available from successful
[Windows build runs](https://github.com/BulatTim1/desktop-light/actions/workflows/build.yml)
while signed into GitHub:

- `CursorBrightness-<version>-windows-x64-setup.exe`: per-user installer with
  Start menu and uninstall entries.
- `CursorBrightness-<version>-windows-x64.zip`: portable app; extract to a
  permanent folder and double-click `CursorBrightness.exe`.
- `CursorBrightness-<version>-source.zip`: matching source and build files.

Requirements: **Windows 11 24H2 or newer, x64**. DDC/CI must be enabled in the
monitor’s menu and supported by the cable/dock. Some picture modes lock hardware
brightness. Release binaries are currently unsigned.

Move the cursor onto a display and press brightness-up/down. Click the tray sun
icon to open the brightness window. Its initial monitor follows the cursor and
stays selected while you adjust the slider; the display list allows manual
selection. Right-click the tray icon or choose **Options** for keyboard controls,
step size, display refresh, **Start with Windows**, and **Quit**.

Win+A and the tray slider use the same background write queue as the keys.
Open Win+A to adjust any monitor directly. Monitors appear left-to-right by
desktop position; the brightness list shows two rows and scrolls for more.
When more than two are connected, the mouse wheel scrolls the list, including
over its sliders. Drag a slider to change that monitor's brightness.
Closing the app window keeps it in the tray. Startup only reads existing
brightness. No saved brightness level, gamma ramp or dark overlay is applied.
Settings live in `%LOCALAPPDATA%\CursorBrightness\settings.ini`. Portable users
should enable startup after choosing a permanent location; enable it again after
moving the app. Login startup is delayed 15 seconds and uses a normal-user task.

## HDR

The **SDR content brightness · HDR stays enabled** mode adjusts desktop/SDR white
level, not the panel’s HDR peak output or an HDR game’s own brightness settings.
With HDR off, hardware DDC/CI is used where supported. The 0–100 SDR-content scale
maps to 80–480 reference nits. Windows’ private SDR-white setter is verified on
build 26100; unsupported calls fail without disabling HDR.

The native Windows keyboard popup can report a different hardware brightness
while the app controls HDR SDR-content brightness. A bundled helper synchronizes
the display-only brightness property of the existing Windows popup with the
selected monitor's percentage. The original Windows design, position, timeout
and volume popup remain in use; no custom popup is drawn or hidden. Stale app
data expires after two seconds, restoring Windows' value. Missing private shell
symbols leave the native popup unchanged. Keyboard step size remains independently
configurable independently of Windows' own brightness increments.

## Windows Quick Settings integration

The complete installer and portable package include a minimal private
**Windhawk 1.7.3** runtime and the compiled GPL extension. It starts with the app,
adds the monitor brightness list to **Win+A**, and exits when you choose **Quit**. The native app
handles brightness keys; the bundled extension forwards slider writes to it.
The private engine excludes all processes except `ShellHost.exe` (Win+A) and
`explorer.exe` (native brightness popup). Upgrades exit
the app/helper themselves and never ask Restart Manager to close other apps.
You can disable this helper using **Options → Windows Quick Settings (Win+A)**.
If shell hooks cannot be resolved, stock Quick Settings remains usable and the
native tray controls still work. Shell customization is verified on Windows
build 26100; future Windows changes may require an extension update.

Windhawk permits one runtime per session. If you already use Windhawk, either
quit that installation to use the bundled helper, or turn off the bundled helper
and install `mods/cursor-brightness.wh.cpp` in your existing Windhawk. For that
second configuration, leave **standaloneBridge** enabled and
**enableBrightnessKeys** disabled in the mod settings. Also install
`mods/cursor-brightness-native-popup.wh.cpp` to synchronize the native popup.
See [the add-on guide](docs/WINDHAWK.md). Other Windhawk installations are not
reconfigured or removed. Integration startup errors are recorded in
`%LOCALAPPDATA%\CursorBrightness\integration-error.txt`.

For the earlier development installation, first run
`tools/control.ps1 -Action DisableStartup`, then `-Action Stop`, before launching
the complete app. To uninstall the new app, use **Settings → Apps → Installed
apps → Cursor Brightness → Uninstall**. Portable users should uncheck **Start
with Windows**, choose **Quit**, then delete the extracted folder. Preferences
remain in `%LOCALAPPDATA%\CursorBrightness` and can be deleted separately.

## Build

Clone the repository:

```powershell
git clone https://github.com/BulatTim1/desktop-light.git
cd desktop-light
```

Use Visual Studio 2022 or newer with **Desktop development with C++**, a Windows
SDK, and CMake:

```powershell
cmake -S . -B build/native -A x64
cmake --build build/native --config Release --parallel
ctest --test-dir build/native -C Release --output-on-failure
```

Output: `build/native/Release/CursorBrightness.exe`. Runtime libraries are linked
statically. Building the checked-in source needs neither Node.js nor Windhawk.
For LLVM/MinGW, use `tools/build-standalone.ps1 -Compiler <path-to-clang++.exe>`.
The locally cached compiler is usable for this native-only build.

For a complete distributable, install Node.js and run the integration build. It
downloads the signed official Windhawk offline installer into an ignored build
cache, extracts its compiler, and prepares a minimal runtime with clean settings:

```powershell
.\tools\prepare-integration.ps1 -Destination build/native/Release/integration
.\tools\package.ps1 -BuildDirectory build/native/Release
```

With [Inno Setup 6](https://jrsoftware.org/isinfo.php), run
`tools/build-installer.ps1 -BuildDirectory build/native/Release -Iscc <path-to-ISCC.exe>`.
Alternatively, `tools/setup-inno.ps1` prepares the signed official compiler in
the ignored build cache; then omit `-Iscc` when building the installer.
Matching Windhawk source and third-party licenses accompany the release.

## Diagnostics and tests

```powershell
Start-Process .\CursorBrightness.exe -ArgumentList '--diagnose "diagnostics.json"' -Wait
```

This read-only command lists monitor names, control modes and current brightness.
It does not change brightness or capture ordinary keyboard typing. Core tests
cover cursor selection/latching, disconnects, normalization, HDR mapping, bounds,
duplicate input, repeats and release. See [the test checklist](tests/VALIDATION.md)
for manual hardware scenarios.

## Publish and contribute

See [PUBLISHING.md](docs/PUBLISHING.md) for repository/release commands and
[CONTRIBUTING.md](CONTRIBUTING.md) for development notes. The GitHub workflow
builds and tests the app, creates portable/source archives and an installer, and
publishes releases for version tags. Caches, user settings and logs are excluded.

## License

**GPL-3.0-only**; see [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The engine and Win+A
integration derive from bardelyne’s GPL-3.0
[per-monitor-brightness v3.0](https://github.com/ramensoftware/windhawk-mods/blob/b00b5e3043a606e9864405a111acaf29d69dc170/mods/per-monitor-brightness.wh.cpp).
The untouched upstream source and revision are preserved in `upstream/`.
