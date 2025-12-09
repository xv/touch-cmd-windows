# General-purpose MSBuild build script
# Requires the presence of a Visual Studio installation along with the MSBuild component
# Uses vswhere.exe (included in Visual Studio 2017 and later) to find the VS installation
 
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

$vsPath = &$vsLocatorPath `
    -latest `
    -requires Microsoft.Component.MSBuild `
    -property installationPath `

Import-Module (Get-ChildItem $vsPath `
    -Recurse -File `
    -Filter Microsoft.VisualStudio.DevShell.dll
).FullName -ErrorAction Stop

Write-Host "Entering Visual Studio Developer Shell..." -f Blue

Enter-VsDevShell `
    -VsInstallPath $vsPath `
    -SkipAutomaticLocation `
    -DevCmdArguments "-arch=x64 -no_logo"

Write-Host "Creating a $c build for $p..." -f Blue

MSBuild -nologo -verbosity:$v -noWarn:C5105 `
    @(Get-ChildItem *.vcxproj) `
    "/p:configuration=$c" `
    "/p:platform=$p" `
    "/t:clean;restore;build"
