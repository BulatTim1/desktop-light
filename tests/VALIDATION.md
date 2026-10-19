# Validation and hardware checklist

## Verified locally

Windows 11 build 26100.9457, x64; local LLVM/MinGW static application build.
The CMake/MSVC GitHub workflow is supplied but has not run in this local session.

- Shared behavior tests passed: negative display coordinates and edges, cursor
  selection, panel latching, deferred discovery, disconnect without retargeting,
  normalization, limits, HDR mapping, duplicate reports, release and 400/100 ms
  held-key timing. Release builds retain test assertions.
- The standalone executable imports Windows system DLLs only.
- The complete private Windhawk runtime launched without its UI/compiler/service
  and loaded the extension into ShellHost. Matching ControlCenter symbols
  downloaded successfully. The separate earlier installation was stopped.
- Native panel activation after background startup, layout and accessible
  controls were inspected. AOC Q27G2WG4 changed from 100% to 95% via the minus
  button; independent DDC/CI readback confirmed it. Original 100% was restored.
- The IPC used by Win+A changed brightness by five points and agreed with
  independent hardware readback. Out-of-range and disconnected IDs were rejected.
  Original brightness was restored. This checks the write path, not live flyout
  interaction in the final standalone package.
- After forcibly ending only the test app, its restart adopted the existing
  private helper. IPC brightness control still passed and restored the original.
- Exiting only the installed private helper left the app running; its periodic
  health check launched a replacement helper automatically.
- A fresh extraction of the portable ZIP ran its read-only diagnostic command
  successfully. Archives include hidden build/workflow files and exclude runtime
  logs, downloaded symbol caches and process state.
- The installer deployed both components without administrator privileges to an
  isolated test folder. The installed app registered a normal-user interactive
  login task with a 15-second delay, no runtime limit and three failure retries.
  Launching that task started the app and its helper; the hardware test passed.
- Uninstall removed the test application, private helper and startup task, and
  left no test processes running. Actual reboot/logon-trigger execution remains
  a manual check.
- An in-place reinstall over the running app exited it and its helper cleanly,
  replaced files successfully, and retained the installed login task. Its
  installed executable hash matched the final build. The old project-specific
  development startup task was removed after the new installation passed.

Earlier mod-only testing also verified actual Lofree Flow Fn+F1/F2 HID reports,
one labelled Win+A slider, and cursor targeting on AOC Q27G2WG4 and Titan Army
G2785S. The HDR backend changed SDR-content brightness, read it back and retained
HDR; the user confirmed operation. The user subsequently tested standalone 1.1
and reported its features working, with one remaining issue: the separate native
Windows brightness keyboard popup disagreed with the app's HDR SDR percentage.

## Versions 1.2 and 1.2.1

- Quick Settings now lists every connected monitor in desktop order. Its
  ScrollViewer has a two-row maximum height; additional monitors scroll.
  Each binding retains a stable monitor ID. Control-mode and availability changes
  rebuild the affected list; ordinary value updates preserve scroll position.
  The user confirmed that both monitor sliders work independently.
- Version 1.2.1 preserves the original Windows brightness popup. The custom
  popup and native-popup suppression were removed at the user's request.
  A separate Explorer mod synchronizes only the screen-brightness view-model
  with the app's current monitor percentage; it calls no hardware, power-policy
  or audio setters. The user confirmed that the Windows popup matches on both
  monitors.
- The popup channel tests passed for missing mappings, percentage endpoints,
  disabled/unavailable targets and publisher destruction. Shared logic tests
  also cover stale and future timestamps. Updates expire after two seconds if
  the application stops publishing.
- Public symbols resolved the native popup getter and setter on this build.
  Windows folds screen and keyboard-backlight getters to one address; a runtime
  class check restricts the hook to the screen-brightness view-model.
- Both displays were connected during the upgrade: G2785S in HDR with SDR white
  level 3500 (50%) and Q27G2WG4 in SDR at hardware 100%. Those values and HDR state
  remained unchanged by startup. ShellHost resolved its hooks and enumerated both.
- A legacy engine DLL pinned in Chrome exposed an installer Restart Manager
  conflict. The installer now exits only its own app, keeps existing files in
  versioned engine directories, and disables automatic application closure. The
  private runtime's engine filter now excludes all processes except ShellHost
  and Explorer; each mod targets its own host.
  The corrected in-place upgrade succeeded while Chrome stayed running.

A physical three-monitor scrolling test remains to be recorded. The supplied
manual checklist also includes reboot, sleep/resume and shell restart tests.

## Check before calling a public release stable

1. Connect both displays. With Quick Settings closed, move the cursor onto each
   in turn: Fn+F1 should decrease only that display by 5; Fn+F2 should increase it.
   Try rapid taps, a long hold, moving between displays while held, and 0/100 limits.
2. Open Win+A. Confirm a labelled slider for every monitor, with two rows visible
   and additional monitors reachable by scrolling. Adjust each independently.
   Check that keys, Win+A and the tray window agree after hardware readback.
3. On HDR, check the "SDR brightness" label and desktop dimming while HDR stays
   enabled. With HDR disabled, check hardware brightness through DDC/CI.
4. Check that the original Windows brightness popup's bar agrees with the Win+A
   percentage, without an extra popup. Try ordinary F1/F2
   and volume/playback keys, including a volume key immediately after brightness:
   the normal volume popup must remain available.
5. Disconnect/reconnect a display and rearrange displays (including negative
   coordinates). A missing target must show unavailable, never control another.
6. Sleep/resume, then check both input and sliders. Restart the Windows shell
   when convenient and confirm Win+A returns. Toggle the Quick Settings option
   off/on and confirm stock layout is restored while it is off.
7. Reboot with "Start with Windows" enabled. After signing in, wait about
   15–30 seconds, then test keys and Win+A before manually launching the app.
8. Try installing over an earlier version, quitting/relaunching, disabling
   startup, and uninstalling. Settings are deliberately preserved.

No luminance meter measurements have been made. Docks, adapters, other monitors,
and Windows builds may behave differently. UI hooks and the SDR-white setter
use private Windows interfaces; unsupported operations must fail safely.

`tools/bridge-smoke.cpp` is an explicit hardware write test, excluded from CTest.
It requires `--exercise`, a running app and a controllable display; it restores
the original brightness on ordinary success/failure. Do not interrupt its process
mid-write. Diagnostic output belongs in ignored `build/`, not in source releases.
