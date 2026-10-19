param([string]$BuildDirectory=(Join-Path $PSScriptRoot '..\build'))
$ErrorActionPreference='Stop'
$projectDirectory=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildDirectory=(Resolve-Path -LiteralPath $BuildDirectory).Path
$executable=Join-Path $BuildDirectory 'CursorBrightness.exe'
if(!(Test-Path -LiteralPath $executable)){throw 'Build CursorBrightness.exe before packaging.'}
$cmake=Get-Content -LiteralPath "$projectDirectory\CMakeLists.txt" -Raw
$version=[regex]::Match($cmake,'project\(CursorBrightness VERSION ([0-9.]+)').Groups[1].Value
if(!$version){throw 'Cannot read application version.'}
$distDirectory=Join-Path $projectDirectory 'dist'
New-Item -ItemType Directory -Path $distDirectory -Force | Out-Null
$stageRoot=Join-Path $projectDirectory ('build\package-'+[guid]::NewGuid().ToString('N'))
$stageDirectory=Join-Path $stageRoot "CursorBrightness-$version-windows-x64"
New-Item -ItemType Directory -Path $stageDirectory -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination $stageDirectory -Force
foreach($name in @('LICENSE','README.md','README.ru.md','CONTRIBUTING.md','CONTRIBUTING.ru.md','THIRD_PARTY_NOTICES.md','THIRD_PARTY_NOTICES.ru.md')){Copy-Item -LiteralPath "$projectDirectory\$name" -Destination $stageDirectory -Force}
Copy-Item -LiteralPath "$projectDirectory\docs","$projectDirectory\assets" -Destination $stageDirectory -Recurse -Force
New-Item -ItemType Directory -Path "$stageDirectory\tests" -Force | Out-Null
Copy-Item -Path "$projectDirectory\tests\*.md" -Destination "$stageDirectory\tests" -Force
Copy-Item -LiteralPath "$projectDirectory\third_party" -Destination $stageDirectory -Recurse -Force
if(!(Test-Path -LiteralPath "$BuildDirectory\integration\windhawk\windhawk.exe")){throw 'Run tools/prepare-integration.ps1 with the build integration directory before packaging.'}
Copy-Item -LiteralPath "$BuildDirectory\integration" -Destination $stageDirectory -Recurse -Force
$portableZip=Join-Path $distDirectory "CursorBrightness-$version-windows-x64.zip"
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Write-Zip([string]$directory,[string]$archive) {
    if(Test-Path -LiteralPath $archive){Remove-Item -LiteralPath $archive}
    [IO.Compression.ZipFile]::CreateFromDirectory($directory,$archive,[IO.Compression.CompressionLevel]::Optimal,$false)
}
Write-Zip $stageDirectory $portableZip
# Explicit allowlist excludes caches, local configuration, logs and Git data.
$sourceDirectories=@('app','src','assets','packaging','tools','tests','mods','upstream','docs','.github','third_party')
$sourceFiles=@('CMakeLists.txt','README.md','README.ru.md','LICENSE','THIRD_PARTY_NOTICES.md','THIRD_PARTY_NOTICES.ru.md','CONTRIBUTING.md','CONTRIBUTING.ru.md','.gitignore','.gitattributes')
$sourceStage=Join-Path $stageRoot "CursorBrightness-$version-source"
New-Item -ItemType Directory -Path $sourceStage -Force | Out-Null
foreach($directory in $sourceDirectories){
    if(Test-Path -LiteralPath "$projectDirectory\$directory") { Copy-Item -LiteralPath "$projectDirectory\$directory" -Destination $sourceStage -Recurse -Force }
}
foreach($file in $sourceFiles){
    if(Test-Path -LiteralPath "$projectDirectory\$file"){Copy-Item -LiteralPath "$projectDirectory\$file" -Destination $sourceStage -Force}
}
$sourceZip=Join-Path $distDirectory "CursorBrightness-$version-source.zip"
Write-Zip $sourceStage $sourceZip
Get-ChildItem -LiteralPath $distDirectory -File | Where-Object { $_.Extension -in '.exe','.zip' } | Get-FileHash -Algorithm SHA256 | ForEach-Object { "$($_.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($_.Path))" } | Set-Content -LiteralPath "$distDirectory\SHA256SUMS.txt" -Encoding ascii
Write-Output $portableZip
Write-Output $sourceZip
