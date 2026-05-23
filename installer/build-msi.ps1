[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$BuildDir = '',
    [string]$OutputDir = '',
    [string]$WixBin = 'C:\Program Files (x86)\WiX Toolset v3.11\bin'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$installerDir = Split-Path -Parent $PSCommandPath
$projectDir = Split-Path -Parent $installerDir

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $projectDir 'build-msi-x64'
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $projectDir 'out'
}

$candle = Join-Path $WixBin 'candle.exe'
$light = Join-Path $WixBin 'light.exe'

if (-not (Test-Path -LiteralPath $candle)) {
    throw "candle.exe was not found at '$candle'. Install WiX Toolset v3.11 or pass -WixBin."
}

if (-not (Test-Path -LiteralPath $light)) {
    throw "light.exe was not found at '$light'. Install WiX Toolset v3.11 or pass -WixBin."
}

$cmakeLists = Get-Content -LiteralPath (Join-Path $projectDir 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch 'project\s*\(\s*PrtSc\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Could not read the PrtSc version from CMakeLists.txt.'
}

$productVersion = $Matches[1]

Write-Host 'Stopping running PrtSc processes...'
Get-Process PrtSc -ErrorAction SilentlyContinue | Stop-Process -Force

Write-Host "Configuring x64 build in '$BuildDir'..."
& cmake -S $projectDir -B $BuildDir -A x64
if ($LASTEXITCODE -ne 0) {
    throw "cmake configure failed with exit code $LASTEXITCODE."
}

Write-Host "Building PrtSc $Configuration x64..."
& cmake --build $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed with exit code $LASTEXITCODE."
}

$sourceDir = Join-Path $BuildDir $Configuration
$exePath = Join-Path $sourceDir 'PrtSc.exe'
$ocrPath = Join-Path $sourceDir 'PrtScOcr.dll'

if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Build output is missing '$exePath'."
}

if (-not (Test-Path -LiteralPath $ocrPath)) {
    throw "Build output is missing '$ocrPath'."
}

$msiDir = Join-Path $OutputDir 'msi'
$objDir = Join-Path $msiDir 'obj'
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

$wxsPath = Join-Path $installerDir 'PrtSc.wxs'
$wixObj = Join-Path $objDir 'PrtSc.wixobj'
$msiPath = Join-Path $msiDir "PrtSc-$productVersion-x64.msi"

Write-Host "Compiling WiX source..."
& $candle `
    -nologo `
    -arch x64 `
    "-dProjectDir=$projectDir" `
    "-dSourceDir=$sourceDir" `
    "-dProductVersion=$productVersion" `
    -out $wixObj `
    $wxsPath
if ($LASTEXITCODE -ne 0) {
    throw "WiX candle failed with exit code $LASTEXITCODE."
}

Write-Host "Linking MSI..."
& $light `
    -nologo `
    -ext WixUIExtension `
    -cultures:en-us `
    -sice:ICE91 `
    -out $msiPath `
    $wixObj
if ($LASTEXITCODE -ne 0) {
    throw "WiX light failed with exit code $LASTEXITCODE."
}

Write-Host "MSI package created: $msiPath"
