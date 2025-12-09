# uninstall.ps1
#
# Removes the directory of this script from PATH if it exists. A registry key
# backup file will also be created just in case something goes terribly wrong
# (hope not)

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
$cmdDir = "$(Join-Path $PSScriptRoot "\")"

# Note: assumes this path is not the first entry in PATH, hence the ';'
# It's very unlikely that it could be the first entry, but not impossible either
$lookup = ";$cmdDir"

$installed = $null -ne (
    Get-Command $cmdName `
        -Type Application `
        -ErrorAction Ignore
).Path

if ($installed -and ($env:PATH -like "*$lookup*")) {
    $env:PATH = $env:PATH.Replace($lookup, $null)
}

$regEnvPath = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"
$regPathVal = (Get-ItemProperty $regEnvPath).Path

if (-Not ($regPathVal -like "*$lookup*")) {
    Write-Host "Could not find '$cmdDir' in PATH." -f Red
    Exit 1
}

# Backup the registry key
Invoke-Command  {
    reg export `
        $regEnvPath.Replace(":", $null) `
        "$(Join-Path $cmdDir "$(Split-Path -Path $regEnvPath -Leaf).bak.reg")" `
        /y
} | out-null

$regPathValNew = $regPathVal.Replace($lookup, $null)

Set-ItemProperty `
    -Path $regEnvPath `
    -Name Path `
    -Value $regPathValNew

Write-Host "Directory '$cmdDir' has been removed from PATH." -f Green