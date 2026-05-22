# Removes the directory of this script from the PATH environment variable

using namespace System.IO
using namespace System

function Remove-PathEnvironmentVariable {
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

    $new = $paths.Where({
        [Path]::TrimEndingDirectorySeparator($_) -ne $Path
    })

    if ($new.Count -eq $paths.Count) {
        return $false
    }

    [Environment]::SetEnvironmentVariable(
        "PATH",
        ($new -join ";"),
        $Scope
    )

    return $true
}

$cmdName = "touch"
$cmdDir = $PSScriptRoot

$installed = $null -ne (
    Get-Command $cmdName `
        -Type Application `
        -ErrorAction Ignore
).Path

if ($installed) {
    Remove-PathEnvironmentVariable -Path $cmdDir -Scope "Process" | Out-Null
}

if (Remove-PathEnvironmentVariable -Path $cmdDir -Scope "User") {
    Write-Host "Directory '$cmdDir' has been removed from PATH." -f Green
} else {
    Write-Host "Directory '$cmdDir' in not in PATH." -f Red
    Exit 1
}