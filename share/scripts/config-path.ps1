# Allows the user to add or remove the executable directory to/from the User PATH
# environment variable.
#
# The executable must be placed in the same directory as this script

using namespace System.IO
using namespace System

$ErrorActionPreference = "Stop"

if ($PSVersionTable.PSVersion -lt [Version]"5.1") {
    Write-Host "This script requires at least Windows PowerShell 5.1" `
        -ForegroundColor Red
    exit 1
}

function Test-IsAdmin {
    $user = [Security.Principal.WindowsIdentity]::GetCurrent()
    $role = [Security.Principal.WindowsBuiltinRole]::Administrator
    return (New-Object Security.Principal.WindowsPrincipal $user).IsInRole($role)
}

# Based on the implementation of [IO.Path]::TrimEndingDirectorySeparator()
# which is only available from .NET Core 3.0 and up (PS 7+)
#
# https://github.com/dotnet/runtime/blob/main/src/libraries/Common/src/System/IO/PathInternal.cs
function Remove-TrailingPathSeparator {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if ([string]::IsNullOrEmpty($Path)) {
        return $Path
    }

    $lastChar = $Path[-1]

    if (($lastChar -ne '\') -and ($lastChar -ne '/')) {
        return $Path
    }

    $root = [Path]::GetPathRoot($Path)

    if ($Path.Length -eq $root.Length) {
        return $Path
    }

    return $Path.Substring(0, $Path.Length - 1)
}

function Get-PATH {
    param(
        [Parameter(Mandatory)]
        [ValidateSet("User", "Machine", "Process")]
        [string]$Scope
    )

    return (
        [Environment]::GetEnvironmentVariable("PATH", $Scope) -split ";"  | 
        Where-Object { $_.Trim() -ne "" } # In case there is a trailing semicolon
    )
}

function Test-PathInPATH {
    param(
        [Parameter(Mandatory)]
        [string]$PathToTest,

        [Parameter(Mandatory)]
        [ValidateSet("User", "Machine", "Process")]
        [string]$Scope
    )

    $PathToTest = Remove-TrailingPathSeparator $PathToTest

    return (Get-PATH $Scope).Where({
        (Remove-TrailingPathSeparator $_) -eq $PathToTest
    }, "First").Count -ne 0
}

function Add-PathToPATH {
    param (
        [Parameter(Mandatory)]
        [string]$PathToAdd,

        [Parameter(Mandatory)]
        [ValidateSet("User", "Machine", "Process")]
        [string]$Scope
    )

    $PathToAdd = Remove-TrailingPathSeparator $PathToAdd
    $entries = Get-PATH $Scope

    $exists = $entries.Where({
        (Remove-TrailingPathSeparator $_) -eq $PathToAdd
    }, "First").Count -ne 0

    if ($exists) {
        return $false
    }

    [Environment]::SetEnvironmentVariable(
        "PATH",
        (($entries + $PathToAdd) -join ";"),
        $Scope
    )

    return $true
}

function Remove-PathFromPATH {
    param (
        [Parameter(Mandatory)]
        [string]$PathToRemove,

        [Parameter(Mandatory)]
        [ValidateSet("User", "Machine", "Process")]
        [string]$Scope
    )

    $PathToRemove = Remove-TrailingPathSeparator $PathToRemove
    $entries = Get-PATH $Scope

    $new = $entries.Where({
        (Remove-TrailingPathSeparator $_) -ne $PathToRemove
    })

    if ($new.Count -eq $entries.Count) {
        return $false
    }

    [Environment]::SetEnvironmentVariable(
        "PATH",
        ($new -join ";"),
        $Scope
    )

    return $true
}

function Show-Menu {
    param(
        [Parameter(Mandatory)]
        [string]$Title,

        [Parameter(Mandatory)]
        [array]$Options
    )

    $count = $Options.Count

    # Width of the Name column
    $nameWidth = (
        ($Options |
            ForEach-Object { $_.Name.Length } | Measure-Object -Maximum
        ).Maximum
    ) + 4

    # Total menu width
    $menuWidth = ($Options |
        ForEach-Object {
            if ([string]::IsNullOrWhiteSpace($_.Description)) {
                $_.Name.Length
            } else {
                ("{0,-$nameWidth} {1}" -f $_.Name, $_.Description).Length
            }
        } | Measure-Object -Maximum
    ).Maximum

    function Get-NextEnabledIndex {
        param(
            [int]$Start,
            [int]$Direction
        )

        $idx = $Start

        do {
            $idx = ($idx + $Direction + $count) % $count
        } until ($Options[$idx].Enabled)

        return $idx
    }

    $selected = 0

    # Start on first enabled item
    while (-not $Options[$selected].Enabled) {
        $selected++
    }

    do {
        Clear-Host

        Write-Host $Title
        Write-Host

        foreach ($idx in 0..($count - 1)) {
            $option = $Options[$idx]

            if ([string]::IsNullOrWhiteSpace($option.Description)) {
                $line = $option.Name
            } else {
                $line = "{0,-$nameWidth} {1}" -f `
                    $option.Name,
                    $option.Description
            }

            # Make all menu items have equal width
            $line = $line.PadRight($menuWidth)

            if (-not $option.Enabled) {
                Write-Host "  $line" `
                    -ForegroundColor DarkGray
            } elseif ($idx -eq $selected) {
                Write-Host "[ $line ]" `
                    -BackgroundColor Cyan `
                    -ForegroundColor Black
            } else {
                Write-Host "  $line"
            }
        }

        Write-Host

        # https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
        $key = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown").VirtualKeyCode

        switch ($key) {
            # VK_UP
            0x26 { $selected = Get-NextEnabledIndex $selected -1 }
            # VK_DOWN
            0x28 { $selected = Get-NextEnabledIndex $selected 1 }
            # VK_RETURN
            0x0D { return $Options[$selected] }
        }
    } while ($true)
}

$cmdName = "touch"
$cmdDir = $PSScriptRoot
$cmdPath = "$(Join-Path $cmdDir $cmdName).exe"

if (-not (Test-Path $cmdPath)) {
    Write-Host `
        "Cannot find '$cmdName.exe' - " `
        "It must be in the same directory as this script." -ForegroundColor Red
    exit 1
}

function Get-CommandPath {
    param(
        [Parameter(Mandatory)]
        [ValidateNotNullOrEmpty()]
        [string]$CommandName
    )

    return @(
        # Searches directories in $Env:PATH, which is basically 
        # Machine PATH + User PATH
        Get-Command $CommandName `
            -Type Application `
            -ErrorAction Ignore
    ).Path
}

function Show-ChoicePrompt {
    param(
        [string]$Title = "Confirmation",
        [string]$Prompt = "Choose an option",
        [string[]]$Options = @("&Yes", "&No", "&Cancel"),
        [string[]]$HelpMessages = @(
            "Proceed with the operation",
            "Do not proceed",
            "Abort and exit"
        ),
        [int]$Default = 0
    )

    $choices = for ($i = 0; $i -lt $Options.Count; $i++) {
        New-Object System.Management.Automation.Host.ChoiceDescription(
            $Options[$i],
            $HelpMessages[$i]
        )
    }

    $selection = $host.UI.PromptForChoice(
        $Title,
        $Prompt,
        $choices,
        $Default
    )

    return $selection
}


$reportedCmdPaths = @(Get-CommandPath $cmdName) # Returns full paths
$oldInstallPaths = $reportedCmdPaths | Where-Object {
    $_ -ne $cmdPath
}

if ($oldInstallPaths.Count -gt 0) {
    Write-Warning (
        "Existing PATH $(if ($oldInstallPaths.Count -gt 1) {"entries were"} else {"entry was"}) found for command '$cmdName':`n" +
        (($oldInstallPaths | ForEach-Object {
            "    $(Split-Path $_ -Parent)"
        }) -join "`n")
    )

    $oldInstallPromptResult = Show-ChoicePrompt `
        -Title "Do you want to remove $(if ($oldInstallPaths.Count -gt 1) {"these entries"} else {"this entry"}) from PATH?" `
        -Prompt "" `
        -Options @("&Yes", "&No", "&Cancel") `
        -HelpMessages @(
            "Remove",
            "Keep",
            "Abort and exit"
        ) `
        -Default 2

        switch ($oldInstallPromptResult) {
        0 <# Yes #> {
            foreach ($oldPath in $oldInstallPaths) {
                $oldInstallDir = Split-Path $oldPath -Parent

                $inUserPATH = Test-PathInPATH $oldInstallDir "User"
                $inMachinePATH = Test-PathInPATH $oldInstallDir "Machine"

                if ((-not $inUserPATH) -and (-not $inMachinePATH)) {
                    continue
                }

                if ($inUserPATH) {
                    Remove-PathFromPATH $oldInstallDir "User" | Out-Null
                } elseif ($inMachinePATH) {
                    if (-not (Test-IsAdmin)) {
                        Write-Host (
                            "Entry exists in the Machine PATH:`n" +
                            "    $oldInstallDir`n" +
                            "Try running this script again as administrator to remove it."
                        ) -ForegroundColor Red

                        exit 1
                    }

                    Remove-PathFromPATH $oldInstallDir "Machine" | Out-Null
                }

                Remove-PathFromPATH $oldInstallDir "Process" | Out-Null
            }
        }
        2 <# Cancel #> {
            exit 0
        }
    }
}

$installed = @($reportedCmdPaths).Where({
    $_ -eq $cmdPath
}, "First").Count -ne 0

while ($true) {
    $menuResult = Show-Menu `
        -Title "Select an option:" `
        -Options @(
            @{
                Name = "Install"
                Description = "Add current directory to User PATH"
                Enabled = -not $installed
            },
            @{
                Name = "Uninstall"
                Description = "Remove current directory from User PATH"
                Enabled = $installed
            },
            @{
                Name = "Exit"
                Description = "Quit without making changes"
                Enabled = $true
            }
        )

    switch ($menuResult.Name) {
        "Install" {
            if (-not $installed) {
                # Add the command dir to the session PATH so that the command can be
                # instantly without having to restart the terminal
                Add-PathToPATH $cmdDir "Process" | Out-Null
            }

            if (Add-PathToPATH $cmdDir "User") {
                $installed = $true
                Write-Host "Installed to User PATH successfully." `
                    -ForegroundColor Green
            } else {
                Write-Host "Directory '$cmdDir' is already in PATH." `
                    -ForegroundColor Yellow
            }

            Write-Host "Press any key to continue..."
            [Console]::ReadKey($true) | Out-Null
        }
        "Uninstall" {
            if ($installed) {
                $inMachinePATH = Test-PathInPATH $cmdDir "Machine"
                $inUserPATH = Test-PathInPATH $cmdDir "User"

                if ($inMachinePATH) {
                    if (-not (Test-IsAdmin)) {
                        Write-Host (
                            "Directory is installed the Machine PATH:`n" +
                            "    $cmdDir`n" +
                            "Try running this script again as administrator to remove it."
                        ) -ForegroundColor Red

                        exit 1
                    }

                    if (Remove-PathFromPATH $cmdDir "Machine") {
                        $inMachinePATH = $false

                        Write-Host "Uninstalled from Machine PATH successfully." `
                            -ForegroundColor Green
                    }
                }

                if ($inUserPATH -and (Remove-PathFromPATH $cmdDir "User")) {
                    $inUserPATH = $false

                    Write-Host "Uninstalled from User PATH successfully." `
                        -ForegroundColor Green
                }

                Remove-PathFromPATH $cmdDir "Process" | Out-Null
                $installed = $inMachinePATH -or $inUserPATH
            }

            Write-Host "Press any key to continue..."
            [Console]::ReadKey($true) | Out-Null
        }
        "Exit" {
            exit 0
        }
    }
}