param([string]$WindhawkPath = (Join-Path $PSScriptRoot '..\.cache\windhawk'))
$ErrorActionPreference = 'Stop'
$projectDirectory = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$WindhawkPath = [IO.Path]::GetFullPath($WindhawkPath)
$cacheDirectory = Join-Path $projectDirectory '.cache'
New-Item -ItemType Directory -Path $cacheDirectory -Force | Out-Null
$package = Join-Path $cacheDirectory 'windhawk_setup_offline.exe'
if (!(Test-Path -LiteralPath $package)) {
    Invoke-WebRequest -Uri 'https://github.com/ramensoftware/windhawk/releases/download/v1.7.3/windhawk_setup_offline.exe' -OutFile $package
}
$signature = Get-AuthenticodeSignature -LiteralPath $package
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Michael Maltsev') { throw 'Official installer signature verification failed.' }
if (Test-Path -LiteralPath "$WindhawkPath\windhawk.exe") { Write-Output "Portable Windhawk already exists at $WindhawkPath"; return }
$process = Start-Process -FilePath $package -ArgumentList @('/S', '/PORTABLE', "/D=$WindhawkPath") -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath "$WindhawkPath\Compiler\bin\clang++.exe")) { throw 'Portable Windhawk setup failed.' }
Write-Output "Portable runtime prepared in $WindhawkPath. No startup entry or system service created."
