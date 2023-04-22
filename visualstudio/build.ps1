param (
    [string]$c = "release",
    [string]$p = "x86",
    [string]$v = "minimal"
)

$configure = ("debug", "release")
$platforms = ("x86", "x64")
$verbosity = ("quiet", "minimal", "normal", "detailed", "diagnostic")

if (!($configure -contains $c)) {
    Write-Host "-c must be one of [$configure]." -f Red
    Exit
}

if (!($platforms -contains $p)) {
    Write-Host "-p must be one of [$platforms]." -f Red
    Exit
}

if (!($verbosity -contains $v)) {
    Write-Host "-v must be one of [$verbosity]." -f Red
    Exit
}

$vsPath = &"${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest `
    -property installationPath

Import-Module (Get-ChildItem $vsPath `
    -Recurse -File `
    -Filter Microsoft.VisualStudio.DevShell.dll
).FullName

Enter-VsDevShell `
    -VsInstallPath $vsPath `
    -SkipAutomaticLocation `
    -DevCmdArguments '-arch=x64'

MSBuild -nologo -verbosity:$v -noWarn:C5105 `
    @(Get-ChildItem *.vcxproj) `
    /p:configuration=$c `
    /p:platform=$p