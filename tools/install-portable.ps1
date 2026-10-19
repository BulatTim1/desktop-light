param(
    [string]$WindhawkPath = (Join-Path $PSScriptRoot '..\.cache\windhawk'),
    [switch]$DiagnosticKeys,
    [switch]$ValidationLogging,
    [switch]$Start
)
$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WindhawkPath = (Resolve-Path -LiteralPath $WindhawkPath).Path
if (!(Select-String -LiteralPath "$WindhawkPath\windhawk.ini" -Pattern '^Portable=1$' -Quiet)) { throw 'This installer supports portable Windhawk only.' }
if (Get-Process windhawk -ErrorAction SilentlyContinue) { throw 'Exit Windhawk before installing or updating the local extension.' }
$appDataDirectory = Join-Path $WindhawkPath 'AppData'
$modsDirectory = Join-Path $appDataDirectory 'Engine\Mods'
$binaryDirectory = Join-Path $modsDirectory '64'
$sourceDirectory = Join-Path $appDataDirectory 'ModsSource'
New-Item -ItemType Directory -Path $binaryDirectory,$sourceDirectory -Force | Out-Null
Copy-Item -LiteralPath "$projectDirectory\build\cursor-brightness.dll", "$projectDirectory\build\libc++.whl", "$projectDirectory\build\libunwind.whl", "$projectDirectory\build\windhawk-mod-shim.dll" -Destination $binaryDirectory -Force
Copy-Item -LiteralPath "$projectDirectory\mods\cursor-brightness.wh.cpp" -Destination $sourceDirectory -Force
$diagnostic = if ($DiagnosticKeys) { 1 } else { 0 }
$logging = if ($DiagnosticKeys -or $ValidationLogging) { 1 } else { 0 }
$debugLogging = if ($ValidationLogging) { 1 } else { 0 }
@"
[Mod]
LibraryFileName=cursor-brightness.dll
Disabled=0
LoggingEnabled=$logging
DebugLoggingEnabled=$debugLogging
Include=ShellHost.exe
Architecture=x86-64
Version=1.0.0

[Settings]
enableBrightnessKeys=1
brightnessStep=5
diagnosticKeys=$diagnostic
"@ | Set-Content -LiteralPath "$modsDirectory\cursor-brightness.ini" -Encoding utf8
Write-Output "Installed in $WindhawkPath (no automatic startup configured)."
if ($Start) { Start-Process -FilePath "$WindhawkPath\windhawk.exe" -ArgumentList '-tray-only' -WindowStyle Hidden }
