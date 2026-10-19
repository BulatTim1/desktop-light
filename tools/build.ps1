param([string]$WindhawkPath = (Join-Path $PSScriptRoot '..\.cache\windhawk'))
$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WindhawkPath = (Resolve-Path -LiteralPath $WindhawkPath).Path
$compilerDirectory = Join-Path $WindhawkPath 'Compiler'
$clang = Join-Path $compilerDirectory 'bin\clang++.exe'
$storage = Get-Content -LiteralPath (Join-Path $WindhawkPath 'windhawk.ini')
$engineRelative = ($storage | Where-Object { $_ -match '^EnginePath=' }) -replace '^EnginePath=', ''
if (!$engineRelative) { throw 'EnginePath missing from windhawk.ini' }
$engineLibrary = Join-Path $WindhawkPath "$engineRelative\64\windhawk.lib"
$outputDirectory = Join-Path $projectDirectory 'build'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
Push-Location -LiteralPath $projectDirectory
try {
    & node (Join-Path $PSScriptRoot 'customize.mjs')
    if ($LASTEXITCODE) { throw 'Source generation failed' }
    & node (Join-Path $PSScriptRoot 'generate-popup-mod.mjs')
    if ($LASTEXITCODE) { throw 'Native popup source generation failed' }
    $common = @('-std=c++23', '-O2', '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008', '-D__USE_MINGW_ANSI_STDIO=0', '-target', 'x86_64-w64-mingw32')
    & $clang @common '-shared' '-DWH_MOD' '-DWH_MOD_ID=L"cursor-brightness"' '-DWH_MOD_VERSION=L"1.0.0"' $engineLibrary '-include' 'windhawk_api.h' 'mods/cursor-brightness.wh.cpp' '-Wl,--export-all-symbols' '-o' "$outputDirectory\cursor-brightness.dll" '-ldxva2' '-lole32' '-loleaut32' '-lwbemuuid' '-luuid' '-lruntimeobject' '-lhid'
    if ($LASTEXITCODE) { throw 'Extension compilation failed' }
    & $clang @common '-shared' '-DWH_MOD' '-DWH_MOD_ID=L"cursor-brightness-native-popup"' '-DWH_MOD_VERSION=L"1.2.1"' $engineLibrary '-include' 'windhawk_api.h' 'mods/cursor-brightness-native-popup.wh.cpp' '-Wl,--export-all-symbols' '-o' "$outputDirectory\cursor-brightness-native-popup.dll" '-lruntimeobject' '-luuid'
    if ($LASTEXITCODE) { throw 'Native popup extension compilation failed' }
    & $clang @common 'tools/key-probe.cpp' '-o' "$outputDirectory\key-probe.exe" '-lhid' '-luser32'
    if ($LASTEXITCODE) { throw 'Key probe compilation failed' }
    & $clang @common 'tests/brightness-core.cpp' '-o' "$outputDirectory\brightness-core-tests.exe"
    if ($LASTEXITCODE) { throw 'Tests compilation failed' }
    & $clang @common 'tools/monitor-probe.cpp' '-o' "$outputDirectory\monitor-probe.exe" '-ldxva2'
    if ($LASTEXITCODE) { throw 'Monitor probe compilation failed' }
    & $clang @common 'tools/hdr-probe.cpp' '-o' "$outputDirectory\hdr-probe.exe"
    if ($LASTEXITCODE) { throw 'HDR probe compilation failed' }
    & $clang @common 'tools/monitor-diagnostics.cpp' '-o' "$outputDirectory\monitor-diagnostics.exe" '-ldxva2'
    if ($LASTEXITCODE) { throw 'Monitor diagnostics compilation failed' }
    if (!(Get-Process debug-log -ErrorAction SilentlyContinue)) {
        & $clang @common 'tools/debug-log.cpp' '-o' "$outputDirectory\debug-log.exe"
        if ($LASTEXITCODE) { throw 'Log reader compilation failed' }
    }
    $runtime = Join-Path $compilerDirectory 'x86_64-w64-mingw32\bin'
    Copy-Item -LiteralPath "$runtime\libc++.dll" -Destination "$outputDirectory\libc++.whl" -Force
    Copy-Item -LiteralPath "$runtime\libunwind.dll" -Destination "$outputDirectory\libunwind.whl" -Force
    Copy-Item -LiteralPath "$runtime\windhawk-mod-shim.dll" -Destination $outputDirectory -Force
    & "$outputDirectory\brightness-core-tests.exe"
    if ($LASTEXITCODE) { throw 'Core behavior tests failed' }
    Write-Output "Built extension and diagnostic probe in $outputDirectory"
} finally { Pop-Location }
