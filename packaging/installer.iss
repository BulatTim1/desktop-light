#ifndef AppVersion
  #define AppVersion "1.2.1"
#endif
#ifndef BuildDir
  #define BuildDir "..\build\native\Release"
#endif
#ifndef OutputPath
  #define OutputPath "..\dist"
#endif
[Setup]
AppId={{E3933628-F896-4423-AF98-163632BE9071}
AppName=Cursor Brightness
AppVersion={#AppVersion}
AppPublisher=Cursor Brightness contributors
DefaultDirName={localappdata}\Programs\CursorBrightness
DefaultGroupName=Cursor Brightness
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.26100
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir={#OutputPath}
OutputBaseFilename=CursorBrightness-{#AppVersion}-windows-x64-setup
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\CursorBrightness.exe
Compression=lzma2
SolidCompression=yes
CloseApplications=no
RestartApplications=no

[Tasks]
Name: startup; Description: "Start Cursor Brightness when I sign in"; Flags: checkedonce
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#BuildDir}\CursorBrightness.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\integration\*"; DestDir: "{app}\integration"; Excludes: "windhawk\Engine\*"; Flags: recursesubdirs createallsubdirs ignoreversion
; Engine directories are versioned. Keep identical installed engine files when
; an earlier release's unloaded hooks left its DLL pinned in another process.
Source: "{#BuildDir}\integration\windhawk\Engine\*"; DestDir: "{app}\integration\windhawk\Engine"; Flags: recursesubdirs createallsubdirs onlyifdoesntexist
Source: "..\LICENSE"; DestDir: "{app}"
Source: "..\README.md"; DestDir: "{app}"
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"
Source: "..\third_party\licenses\*"; DestDir: "{app}\third_party\licenses"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Cursor Brightness"; Filename: "{app}\CursorBrightness.exe"
Name: "{autodesktop}\Cursor Brightness"; Filename: "{app}\CursorBrightness.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\CursorBrightness.exe"; Parameters: "--enable-startup"; Tasks: startup; Flags: runhidden waituntilterminated
Filename: "{app}\CursorBrightness.exe"; Parameters: "--disable-startup"; Check: not WizardIsTaskSelected('startup'); Flags: runhidden waituntilterminated
Filename: "{app}\CursorBrightness.exe"; Description: "Launch Cursor Brightness"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\CursorBrightness.exe"; Parameters: "--disable-startup"; Flags: runhidden waituntilterminated; RunOnceId: "DisableCursorBrightnessStartup"
Filename: "{app}\CursorBrightness.exe"; Parameters: "--exit"; Flags: runhidden waituntilterminated; RunOnceId: "ExitCursorBrightness"

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  if FileExists(ExpandConstant('{app}\CursorBrightness.exe')) then
  begin
    if not Exec(ExpandConstant('{app}\CursorBrightness.exe'), '--exit', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      Result := 'Close Cursor Brightness before upgrading.';
    if ResultCode <> 0 then
      Result := 'Cursor Brightness is still exiting. Close it and try the upgrade again.';
  end;
end;
