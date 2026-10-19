# Third-party notices

**English** · [Русский](THIRD_PARTY_NOTICES.ru.md)

Cursor Brightness is distributed under **GPL-3.0-only**. The complete license is
in `LICENSE`. Modified source and build instructions are included in this
repository and in release source archives.

## Brightness engine and Quick Settings extension

Derived from **Per-monitor brightness and more in Quick Settings v3.0** by
**bardelyne**, published in the official Windhawk mod collection under GPL-3.0.

- Source: https://github.com/ramensoftware/windhawk-mods/blob/b00b5e3043a606e9864405a111acaf29d69dc170/mods/per-monitor-brightness.wh.cpp
- Upstream revision: `b00b5e3043a606e9864405a111acaf29d69dc170`
- Unmodified copy: `upstream/per-monitor-brightness.wh.cpp`
- Modifications: cursor selection, HID brightness keys, HDR SDR-content control,
  brightness-only probing, readback, and lifecycle/layout handling.
- The upstream extension acknowledges XAML-thread plumbing derived from m417z's
  Windows 11 Notification Center Styler, also under GPL-3.0.

## Bundled Windhawk

The complete distribution contains an unmodified minimal **Windhawk 1.7.3**
runtime, by Michael Maltsev / Ramen Software, licensed under GPL-3.0.
Its private daemon loads the Quick Settings extension into Windows ShellHost
and the native brightness popup helper into Explorer.
No compiler, Windhawk UI, system service, or upstream installer is shipped.

- Official project: https://github.com/ramensoftware/windhawk
- Release: https://github.com/ramensoftware/windhawk/releases/tag/v1.7.3
- Corresponding source: `third_party/windhawk/Windhawk-1.7.3-source.zip`
  (also included under `integration/windhawk/licenses/` in binary packages).
- Source download: https://github.com/ramensoftware/windhawk/archive/refs/tags/v1.7.3.zip
- Build instructions and component license notices are preserved in that archive.

Microsoft symbol/debugging components distributed with the official runtime
retain their original Microsoft signatures and notices. Mod runtime DLLs use
the LLVM/MinGW licenses included in `third_party/licenses/`.

## Windows interfaces

Native brightness popup synchronization uses Microsoft's public symbols for the
display-only `HWConfirmatorUI::BrightnessConfirmatorViewModel` interface in
`Windows.Internal.HardwareConfirmator.dll`. It does not ship Windows binaries
or create a replacement popup. ModernFlyouts was consulted during the earlier
popup investigation but none of its implementation remains in this release.

SDR white-level reads use Microsoft's documented `DISPLAYCONFIG_SDR_WHITE_LEVEL`.
The private Windows setter ABI was cross-checked against the public example at
https://github.com/ledoge/set_maxtml/blob/master/main_sdrwhite.c . The local
implementation was written for this project; it does not copy that program.

Windows system DLLs are supplied by the operating system and are not distributed.

## Statically linked compiler runtime (MinGW/LLVM builds)

LLVM libc++, libc++abi, and libunwind use Apache-2.0 WITH LLVM-exception; MinGW-w64
runtime components use their respective permissive licenses. Release packaging
includes the compiler runtime license files when using the local LLVM build.
Visual Studio release builds use Microsoft's static runtime under its toolchain
redistribution terms. No compiler or SDK is bundled with the application.
