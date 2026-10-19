param([string]$Destination=(Join-Path $PSScriptRoot '..\build\integration'))
$ErrorActionPreference='Stop'
$projectDirectory=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Destination=[IO.Path]::GetFullPath($Destination)
$runtime=Join-Path $projectDirectory '.cache\windhawk'
if(!(Test-Path -LiteralPath "$runtime\windhawk.exe")){ & "$PSScriptRoot\setup-portable.ps1" }
& "$PSScriptRoot\build.ps1"
if($LASTEXITCODE){throw 'Quick Settings extension build failed'}
$target=Join-Path $Destination 'windhawk'
$bin=Join-Path $target 'AppData\Engine\Mods\64'
New-Item -ItemType Directory -Path $target,$bin,"$target\AppData\Engine\ModsWritable","$target\AppData\Engine\Symbols","$target\AppData\ModsSource" -Force | Out-Null
Copy-Item -LiteralPath "$runtime\windhawk.exe","$runtime\windhawk-x64-helper.exe" -Destination $target -Force
New-Item -ItemType Directory -Path "$target\Engine" -Force | Out-Null
Copy-Item -Path "$runtime\Engine\*" -Destination "$target\Engine" -Recurse -Force
Copy-Item -LiteralPath "$projectDirectory\build\cursor-brightness.dll","$projectDirectory\build\cursor-brightness-native-popup.dll","$projectDirectory\build\libc++.whl","$projectDirectory\build\libunwind.whl","$projectDirectory\build\windhawk-mod-shim.dll" -Destination $bin -Force
Copy-Item -LiteralPath "$projectDirectory\mods\cursor-brightness.wh.cpp","$projectDirectory\mods\cursor-brightness-native-popup.wh.cpp" -Destination "$target\AppData\ModsSource" -Force
@'
[Storage]
Portable=1
CompilerPath=Compiler
EnginePath=Engine\1.7.3
UIPath=UI
AppDataPath=AppData
'@ | Set-Content -LiteralPath "$target\windhawk.ini" -Encoding ascii
@'
[Settings]
Language=en
LoggingVerbosity=0
HideTrayIcon=1
DisableToolkitHotkey=1
DontAutoShowToolkit=1
DisableUpdateCheck=1
SafeMode=0
'@ | Set-Content -LiteralPath "$target\AppData\settings.ini" -Encoding ascii
@'
[Settings]
LoggingVerbosity=0
Include=ShellHost.exe|explorer.exe
Exclude=*
ThreadAttachExempt=
InjectIntoCriticalProcesses=0
InjectIntoIncompatiblePrograms=0
InjectIntoGames=0
'@ | Set-Content -LiteralPath "$target\AppData\Engine\settings.ini" -Encoding ascii
@'
[Mod]
LibraryFileName=cursor-brightness.dll
Disabled=0
LoggingEnabled=0
DebugLoggingEnabled=0
Include=ShellHost.exe
Architecture=x86-64
Version=1.0.0
[Settings]
enableBrightnessKeys=0
brightnessStep=5
diagnosticKeys=0
standaloneBridge=1
'@ | Set-Content -LiteralPath "$target\AppData\Engine\Mods\cursor-brightness.ini" -Encoding ascii
@'
[Mod]
LibraryFileName=cursor-brightness-native-popup.dll
Disabled=0
LoggingEnabled=0
DebugLoggingEnabled=0
Include=explorer.exe
Architecture=x86-64
Version=1.2.1
'@ | Set-Content -LiteralPath "$target\AppData\Engine\Mods\cursor-brightness-native-popup.ini" -Encoding ascii
New-Item -ItemType Directory -Path "$target\licenses" -Force | Out-Null
Copy-Item -LiteralPath "$projectDirectory\LICENSE" -Destination "$target\licenses\Windhawk-GPL-3.0.txt" -Force
$sourceDirectory=Join-Path $projectDirectory 'third_party\windhawk'
New-Item -ItemType Directory -Path $sourceDirectory -Force | Out-Null
$sourceArchive=Join-Path $sourceDirectory 'Windhawk-1.7.3-source.zip'
if(!(Test-Path -LiteralPath $sourceArchive)) {
    Invoke-WebRequest -Uri 'https://github.com/ramensoftware/windhawk/archive/refs/tags/v1.7.3.zip' -OutFile $sourceArchive
}
if((Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256).Hash -ne '7DCCE5A390081B8EB241A4661F306E8B3E56B85F26DEF5AAC4B36385783D855F') {
    throw 'Pinned Windhawk source archive checksum does not match.'
}
Copy-Item -LiteralPath $sourceArchive -Destination "$target\licenses" -Force
Write-Output "Prepared private Quick Settings runtime: $target"
