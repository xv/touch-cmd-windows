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
   Exit
}

$cmdName = "touch"
$cmdPath = "$(Join-Path $PSScriptRoot $cmdName).exe"

if (-Not (Test-Path $cmdPath)) {
    Write-Host `
        "Cannot find '$cmdName.exe'." `
        "It should be in the same directory as this script." -f Red
    Exit
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
    if ($env:PATH -notlike "*$PSScriptRoot*") {
        if (-Not $env:PATH.EndsWith(";")) {
            $env:PATH += ";"
        }

        $env:PATH += $PSScriptRoot
    }
}

$regEnvPath = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"
$regPathVal = (Get-ItemProperty $regEnvPath).Path

if (($regPathVal -like "*$PSScriptRoot*") -and $installed) {
    Write-Host "Directory '$PSScriptRoot' already exists in PATH." -f Green
    Exit
} else {
    if (-Not $regPathVal.EndsWith(";")) {
        $regPathVal += ";"
    }

    $cmdEnvPath = -join ($regPathVal, $PSScriptRoot)

    if ($cmdEnvPath.Length -ne ($regPathVal.Length + $PSScriptRoot.Length)) {
        Write-Host "Could not join command path with environment PATH." -f Red
        Exit
    }

    # Add the script/command dir to the permanent PATH in registry
    Set-ItemProperty `
        -Path $regEnvPath `
        -Name Path `
        -Value $cmdEnvPath
}

Write-Host "Directory '$PSScriptRoot' has been added to PATH." -f Green