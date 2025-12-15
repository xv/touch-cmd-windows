# General-purpose MSBuild build script
#
# The script works with either a full Visual Studio IDE installation that 
# includes the MSBuild component or with a standalone Build Tools installation
#
# vswhere.exe requires Visual Studio/Build Tools 2017 and later
 
param (
    [string]$c = "release",
    [string]$p = "x64",
    [string]$v = "minimal"
)

$configure = ("debug", "release")
$platforms = ("x86", "x64", "arm64")
$verbosity = ("quiet", "minimal", "normal", "detailed", "diagnostic")

if (!($configure -contains $c)) {
    Write-Host "-c must be one of [$($configure -join ', ')]." -f Red
    Exit 1
}

if (!($platforms -contains $p)) {
    Write-Host "-p must be one of [$($platforms -join ', ')]." -f Red
    Exit 1
}

if (!($verbosity -contains $v)) {
    Write-Host "-v must be one of [$($verbosity -join ', ')]." -f Red
    Exit 1
}

$vsLocatorPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (!(Test-Path -Path $vsLocatorPath)) {
    Write-Host "Could not find '$vsLocatorPath'." -f Red
    Exit 1
}

$vsProductIds = "BuildTools", "Community", "Professional", "Enterprise" |  ForEach-Object { 
    "Microsoft.VisualStudio.Product.$_" 
}

$vsInstallPath = $null

foreach ($productId in $vsProductIds) {
    $path = & $vsLocatorPath `
        -latest `
        -products $productId `
        -requires "Microsoft.Component.MSBuild" `
        -property "installationPath"

    if ($path) {
        $vsInstallPath = $path
        break
    }
}

if (!$vsInstallPath) {
    Write-Host "No suitable MSBuild installation was found." -f Red
    Exit 1
}

Import-Module (Get-ChildItem $vsInstallPath `
    -Recurse -File `
    -Filter Microsoft.VisualStudio.DevShell.dll
).FullName -ErrorAction Stop

Write-Host "Entering Visual Studio Developer Shell..." -f Blue

Enter-VsDevShell `
    -VsInstallPath $vsInstallPath `
    -SkipAutomaticLocation `
    -DevCmdArguments "-arch=x64 -no_logo"

Write-Host "Creating a $c build for $p..." -f Blue

MSBuild -nologo -verbosity:$v -noWarn:C5105 `
    @(Get-ChildItem *.sln)[0] `
    "/p:configuration=$c" `
    "/p:platform=$p" `
    "/t:clean;restore;build"
