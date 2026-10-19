param([string]$Compiler = (Join-Path $PSScriptRoot '..\.cache\windhawk\Compiler\bin\clang++.exe'))
$ErrorActionPreference='Stop'
$projectDirectory=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Compiler=(Resolve-Path -LiteralPath $Compiler).Path
Push-Location -LiteralPath $projectDirectory
try {
    & node tools/generate-resources.mjs
    if($LASTEXITCODE){throw 'Resource generation failed'}
    $common=@('-std=c++20','-O2','-static','-DUNICODE','-D_UNICODE','-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-DNTDDI_VERSION=0x0A000008','-target','x86_64-w64-mingw32')
    & $Compiler @common '-municode' '-mwindows' app/main.cpp build/app.res '-o' build/CursorBrightness.exe '-ldxva2' '-lole32' '-loleaut32' '-lwbemuuid' '-luuid' '-lcomctl32' '-lshell32' '-lgdi32' '-ldwmapi' '-lhid' '-lwtsapi32' '-ltaskschd' '-ladvapi32'
    if($LASTEXITCODE){throw 'Standalone app compilation failed'}
    & $Compiler @common '-UNDEBUG' tests/brightness-core.cpp '-o' build/standalone-core-tests.exe
    if($LASTEXITCODE){throw 'Standalone tests compilation failed'}
    & .\build\standalone-core-tests.exe
    if($LASTEXITCODE){throw 'Standalone tests failed'}
    & $Compiler @common '-UNDEBUG' '-DNOMINMAX' tests/native-popup-state.cpp '-o' build/native-popup-state-tests.exe
    if($LASTEXITCODE){throw 'Native popup tests compilation failed'}
    & .\build\native-popup-state-tests.exe
    if($LASTEXITCODE){throw 'Native popup state tests failed'}
    Write-Output 'Built self-contained build\CursorBrightness.exe (Windows system DLLs only).'
} finally { Pop-Location }
