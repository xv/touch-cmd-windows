$path = "..\src\version.h"
$pattern = "^\d+\.\d+\.\d+\.\d+$"

$defMajor = "#define VER_MAJOR"
$defMinor = "#define VER_MINOR"
$defRevis = "#define VER_REVIS"
$defBuild = "#define VER_BUILD"

function Get-CurrentVersion {
    $content = Get-Content $path
    $values = @()

    foreach ($def in @($defMajor, $defMinor, $defRevis, $defBuild)) {
        $match = $content | Select-String -Pattern "^$def (\d+)"
        if ($match) {
            $values += $match.Matches.Groups[1].Value
        }
    }
    
    return ($values -join ".")
}

$curVer = Get-CurrentVersion

Write-Host "Current version is $curVer"

do {
    $newVer = Read-Host "Enter a new version in the format #.#.#.#"
    if ($newVer -notmatch $pattern) {
        Write-Host "Input is not in the correct format!" -f Red
    }
} while ($newVer -notmatch $pattern)


if (([version]$newVer).CompareTo(([version]$curVer)) -lt 0) {
    Write-Host "WARNING: Entered version is lower than current version!" -f Yellow
    
    while ($true) {
        $confirm = Read-Host "Do you still want to proceed? [y/N]"

        if ($confirm -eq "") { $confirm = "n" }
        if ($confirm.ToLower() -eq "y") { break }
        if ($confirm.ToLower() -eq "n") { Exit 0 }
        
        Write-Host "Please respond with either Y or N." -f Yellow
    }
}

$newVerArr = $newVer.Split(".")

(Get-Content $path) `
    -replace "^$defMajor \d+$", "$defMajor $($newVerArr[0])" `
    -replace "^$defMinor \d+$", "$defMinor $($newVerArr[1])" `
    -replace "^$defRevis \d+$", "$defRevis $($newVerArr[2])" `
    -replace "^$defBuild \d+$", "$defBuild $($newVerArr[3])" `
| Set-Content $path

Write-Host "Version bumped to $($newVerArr -join ".")" -f Green