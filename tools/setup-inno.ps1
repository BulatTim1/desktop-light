param([string]$Destination=(Join-Path $PSScriptRoot '..\.cache\inno'))
$ErrorActionPreference='Stop'
$Destination=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath "$Destination\ISCC.exe"){Write-Output "Inno Setup compiler is available at $Destination"; return}
$cacheDirectory=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\.cache'))
New-Item -ItemType Directory -Path $cacheDirectory -Force | Out-Null
$package=Join-Path $cacheDirectory 'innosetup-6.7.3.exe'
if(!(Test-Path -LiteralPath $package)) {
    Invoke-WebRequest -Uri 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-6.7.3.exe' -OutFile $package
}
$signature=Get-AuthenticodeSignature -LiteralPath $package
if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Pyrsys B\.V\.') {
    throw 'Official Inno Setup installer signature verification failed.'
}
$process=Start-Process -FilePath $package -ArgumentList @('/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="'+$Destination+'"')) -PassThru -Wait -WindowStyle Hidden
if($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath "$Destination\ISCC.exe")){throw 'Inno Setup compiler installation failed.'}
Write-Output "Inno Setup compiler prepared in $Destination"
