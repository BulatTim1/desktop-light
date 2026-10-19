param(
    [string]$BuildDirectory=(Join-Path $PSScriptRoot '..\build'),
    [string]$Iscc=(Join-Path $PSScriptRoot '..\.cache\inno\ISCC.exe')
)
$ErrorActionPreference='Stop'
$projectDirectory=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildDirectory=(Resolve-Path -LiteralPath $BuildDirectory).Path
$Iscc=(Resolve-Path -LiteralPath $Iscc).Path
$cmake=Get-Content -LiteralPath "$projectDirectory\CMakeLists.txt" -Raw
$version=[regex]::Match($cmake,'project\(CursorBrightness VERSION ([0-9.]+)').Groups[1].Value
if(!$version){throw 'Cannot read application version.'}
$distDirectory=Join-Path $projectDirectory 'dist'
New-Item -ItemType Directory -Path $distDirectory -Force | Out-Null
& $Iscc "/DAppVersion=$version" "/DBuildDir=$BuildDirectory" "/DOutputPath=$distDirectory" "$projectDirectory\packaging\installer.iss"
if($LASTEXITCODE){throw 'Installer compilation failed'}
Get-ChildItem -LiteralPath $distDirectory -File | Where-Object { $_.Extension -in '.exe','.zip' } | Get-FileHash -Algorithm SHA256 | ForEach-Object { "$($_.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($_.Path))" } | Set-Content -LiteralPath "$distDirectory\SHA256SUMS.txt" -Encoding ascii
