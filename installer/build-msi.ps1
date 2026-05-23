[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$BuildDir = '',
    [string]$OutputDir = '',
    [string]$WixBin = 'C:\Program Files\WiX Toolset v7.0\bin'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$installerDir = Split-Path -Parent $PSCommandPath
$projectDir = Split-Path -Parent $installerDir

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $projectDir 'build'
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $projectDir 'out'
}

$wix = Join-Path $WixBin 'wix.exe'
if (-not (Test-Path -LiteralPath $wix)) {
    throw "wix.exe was not found at '$wix'. Install WiX Toolset v7 or pass -WixBin."
}

$cmakeLists = Get-Content -LiteralPath (Join-Path $projectDir 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch 'project\s*\(\s*PrtSc\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Could not read the PrtSc version from CMakeLists.txt.'
}

$productVersion = $Matches[1]

Write-Host 'Stopping running PrtSc processes...'
Get-Process PrtSc -ErrorAction SilentlyContinue | Stop-Process -Force

if (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt')) {
    Write-Host "Using existing CMake build directory '$BuildDir'..."
} else {
    Write-Host "Configuring x64 build in '$BuildDir'..."
    & cmake -S $projectDir -B $BuildDir -A x64
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed with exit code $LASTEXITCODE."
    }
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
$msiPath = Join-Path $msiDir "PrtSc-$productVersion-x64.msi"
$pdbPath = Join-Path $msiDir "PrtSc-$productVersion-x64.wixpdb"

Write-Host 'Building MSI with WiX v7...'
& $wix build `
    -arch x64 `
    -ext WixToolset.UI.wixext `
    -culture en-us `
    -d "ProjectDir=$projectDir" `
    -d "SourceDir=$sourceDir" `
    -d "ProductVersion=$productVersion" `
    -intermediatefolder $objDir `
    -pdb $pdbPath `
    -out $msiPath `
    $wxsPath
if ($LASTEXITCODE -ne 0) {
    throw "WiX build failed with exit code $LASTEXITCODE."
}

Write-Host "MSI package created: $msiPath"
