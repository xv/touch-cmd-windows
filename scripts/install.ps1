# install.ps1
#
# Appends the directory of this script to PATH. The executable should be
# placed in the directory of this script so that it can be called from
# anywhere in the terminal

function Test-IsAdmin {
    $user = [Security.Principal.WindowsIdentity]::GetCurrent()
    $role = [Security.Principal.WindowsBuiltinRole]::Administrator
    return (New-Object Security.Principal.WindowsPrincipal $user).IsInRole($role)
}

if (-Not (Test-IsAdmin)) {
   Write-Host "Admin privileges are required to run the script!" ` -f Red
   Exit 1
}

$cmdName = "touch"
$cmdDir = "$(Join-Path $PSScriptRoot "\")"
$cmdPath = "$(Join-Path $cmdDir $cmdName).exe"

if (-Not (Test-Path $cmdPath)) {
    Write-Host `
        "Cannot find '$cmdName.exe'." `
        "It should be in the same directory as this script." -f Red
    Exit 1
}

# Check if installed in the temp/session PATH
$installed = $null -ne (
    Get-Command $cmdName `
        -Type Application `
        -ErrorAction Ignore
).Path

if (-Not $installed) {
    # Add the script/command dir to the temp. session PATH so that the command
    # can be used instantly without having to restart the terminal
    if ($env:PATH -notlike "*$cmdDir*") {
        if (-Not $env:PATH.EndsWith(";")) {
            $env:PATH += ";"
        }

        $env:PATH += $cmdDir
    }
}

$regEnvPath = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"
$regPathVal = (Get-ItemProperty $regEnvPath).Path

if (($regPathVal -like "*$cmdDir*") -and $installed) {
    Write-Host "Directory '$cmdDir' already exists in PATH." -f Green
    Exit 0
} else {
    if (-Not $regPathVal.EndsWith(";")) {
        $regPathVal += ";"
    }

    $cmdEnvPath = -join ($regPathVal, $cmdDir)

    if ($cmdEnvPath.Length -ne ($regPathVal.Length + $cmdDir.Length)) {
        Write-Host "Could not join command path with environment PATH." -f Red
        Exit 1
    }

    # Add the script/command dir to the permanent PATH in registry
    Set-ItemProperty `
        -Path $regEnvPath `
        -Name Path `
        -Value $cmdEnvPath
}

Write-Host "Directory '$cmdDir' has been added to PATH." -f Green