$path = "..\version.h"
$pattern = "^\d+\.\d+\.\d+\.\d+$"

$defMajor = "#define VER_MAJOR"
$defMinor = "#define VER_MINOR"
$defRevis = "#define VER_REVIS"
$defBuild = "#define VER_BUILD"

do {
    $newVer = Read-Host "Enter a version in the format #.#.#.#"
    if ($newVer -notmatch $pattern) {
        Write-Host "Input is not in the correct format!" -f Red
    }
} while ($newVer -notmatch $pattern)

$newVerArr = $newVer.Split(".")

(Get-Content $path) `
    -replace "^$defMajor \d+$", "$defMajor $($newVerArr[0])" `
    -replace "^$defMinor \d+$", "$defMinor $($newVerArr[1])" `
    -replace "^$defRevis \d+$", "$defRevis $($newVerArr[2])" `
    -replace "^$defBuild \d+$", "$defBuild $($newVerArr[3])" `
| Set-Content $path

Write-Host "Version bumped to $($newVerArr -join ".")" -f Green