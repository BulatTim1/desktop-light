# Contributing

Build with the native CMake instructions in README.md and run CTest before
submitting a pull request. Describe the observed/resulting behavior, Windows
build, and validation performed.

The SDK-only engine is generated from the pinned Windhawk adaptation. For engine
changes, update `tools/customize.mjs` or its `src` components, then run:

```powershell
node tools/customize.mjs
node tools/generate-engine.mjs
node tools/generate-popup-mod.mjs
```

Do not edit a generated engine/mod independently. No Node.js generation step is
needed to build the checked-in standalone app. Native code belongs in `app/`;
shared keyboard, HDR and selection components belong in `src/`.
The native Windows popup helper is generated from `src/native-popup-mod.inc`
and the shared popup-state and core headers.

Run hardware checks for affected paths: SDR/HDR, repeated keys, cursor movement,
disconnect/reconnect, resume, startup and exit. Do not change unrelated monitor
properties or startup brightness. Automated write tests must restore the original
value unless the user is deliberately choosing a new level.

Keep upstream GPL and runtime notices. Never commit `build/`, `dist/`, `.cache/`,
device serials, personal paths, settings or local diagnostic output.
