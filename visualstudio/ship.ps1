param (
    [string]$p = "x64"
)

$platforms = ("x86", "x64")

if (!($platforms -contains $p)) {
    Write-Host "-p must be one of [$($platforms -join ", ")]." -f Red
    Exit
}

$exeDirName = if ($p -eq "x86") { "Win32" } else { $p }
$exePath = "release\$exeDirName\touch.exe"

if (-Not (Test-Path $exePath)) {
    Write-Host "Could not find '$exePath'." -f Red
    Exit
}

function Get-SoftwareVersion {
    $versionFile = "..\src\version.h"

    $defMajor = "#define VER_MAJOR"
    $defMinor = "#define VER_MINOR"
    $defPatch = "#define VER_PATCH"

    $major = $null
    $minor = $null
    $patch = $null

    Get-Content $versionFile | ForEach-Object {
        switch -Regex ($_) {
            "^$defMajor\s+(\d+)$" { $major = $Matches[1] }
            "^$defMinor\s+(\d+)$" { $minor = $Matches[1] }
            "^$defPatch\s+(\d+)$" { $patch = $Matches[1] }
        }
    }

    return "$major.$minor.$patch"
}

$ver = Get-SoftwareVersion

$filesToZip = @(
    $exePath,
    "..\scripts\install.ps1",
    "..\scripts\uninstall.ps1",
    "..\LICENSE"
)

$zipOutPath = "release\touch-$ver-$p.zip"

Compress-Archive `
    -Path $filesToZip `
    -DestinationPath $zipOutPath `
    -Force

$zipSHA256 = (
    Get-FileHash `
        -Algorithm SHA256 `
        $zipOutPath
).Hash

Write-Host "Created archive: $zipOutPath" -f Green
Write-Host "SHA256 $zipSHA256"