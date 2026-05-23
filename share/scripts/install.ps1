# Appends the directory of this script to the PATH environment variable
#
# The executable must be placed in the same directory as this script

using namespace System.IO
using namespace System

$ErrorActionPreference = "Stop"

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Host "This script requires PowerShell 7+." -f Red
    Exit 1
}

function Set-PathEnvironmentVariable {
    param (
        [string]$Path,
        [ValidateSet("User", "Machine", "Process")]
        [string]$Scope
    )

    $Path = [Path]::TrimEndingDirectorySeparator($Path)

    $paths = @(
        [Environment]::GetEnvironmentVariable("PATH", $Scope) -split ";" |
        Where-Object { $_.Trim() -ne "" }
    )

    $exists = $paths.Where({
        [Path]::TrimEndingDirectorySeparator($_) -eq $Path
    }, "First").Count -ne 0

    if ($exists) {
        return $false
    }

    [Environment]::SetEnvironmentVariable(
        "PATH",
        (($paths + $Path) -join ";"),
        $Scope
    )

    return $true
}

$cmdName = "touch"
$cmdDir = $PSScriptRoot
$cmdPath = "$(Join-Path $cmdDir $cmdName).exe"

if (-Not (Test-Path $cmdPath)) {
    Write-Host `
        "Cannot find '$cmdName.exe'." `
        "It should be in the same directory as this script." -f Red
    Exit 1
}

$installed = $null -ne (
    Get-Command $cmdName `
        -Type Application `
        -ErrorAction Ignore
).Path

if (-not $installed) {
    # Add the command dir to the session PATH so that the command can be
    # instantly without having to restart the terminal
    Set-PathEnvironmentVariable -Path $cmdDir -Scope "Process" | Out-Null
}

if (Set-PathEnvironmentVariable -Path $cmdDir -Scope "User") {
    Write-Host "Directory '$cmdDir' has been added to PATH." -f Green
} else {
    Write-Host "Directory '$cmdDir' is already in PATH." -f Yellow
}