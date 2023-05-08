What Is This
------------
Have you ever used the `touch` command on a GNU/Linux or a macOS system? Well, this is exactly what this utility does. I frequently use this command on Linux to quickly create files and I find it rather annoying that I can't do the same thing when I am back on Windows, and so I decided to write a native Windows version of it to satisfy my needs. This is not a clone or a fork of the GNU Coreutils `touch`, and although the core functionality is the same, there are some subtle differences between the two. These differences are discussed later in the document.

The Command Line
----------------
### Basic Syntax
`touch [options] file [...]`

The `[...]` part means you can specify more than one file and as many files as you wish. Additionally, if any options are specified, they must be provided before file operands or they may be interpreted as filenames.

### Options
| Item          | Description |
|---------------|:------------|
| `-A` *offset* | Adjust the timestamps of a file by an offset specified by the *offset* argument, which must be in the format `[-]HH[mm][ss]`, where `HH` is a 2-digit numerical representation of hours between 0 to 99; `mm` is a 2-digit numerical representation of minutes between 0 and 59; and `ss` is a 2-digit numerical representation of seconds between 0 and 59. A negative offset will move time backward. If a file does not exist, it will be created with adjusted timestamps, unless the `-c` option is specified.
| `-a`          | Change only the *last access* timestamp. The *last modified* timestamp will not be changed unless `-m` is specified. |
| `-c`          | Do not create a new file if the file specified does not exist. The program will not display a diagnostic or error message and the exit value will not be affected. |
| `-d`          | Do not dereference symbolic links. If this option is specified, the timestamp of the symbolic link itself will be changed rather than the file it refers to. |
| `-m`          | Change only the *last modified* timestamp. The *last access* timestamp will not be changed unless `-a` is specified. |
| `-r` *file*   | Use the timestamp of the file specified by the *file* argument instead of the current time of day. This option cannot be combined with `-t`. |
| `-t` *stamp*  | Use the timestamp specified by the *stamp* argument, which must be in the format `yyyyMMddHHmm[ss][Z]`, where `yyyy` is a 4-digit numerical representation of the year (e.g., 1991); `MM` is a 2-digit numerical representation of the month (e.g., 05); `dd` is a 2-digit numerical representation of the day (e.g., 17); and `ss` is an optional 2-digit numerical representation of the seconds (e.g., 50). The timestamp is in local time by default; however, you may optionally append `Z` (case-sensitive) at the end to convert it to UTC time. This option cannot be combined with `-r`. |
| `-h`          | Display help information and exit. |
| `-v`          | Display version information and exit. |

#### Option Specification Quirks
* The order of options does not matter.
* Flag options can be combined together. For example: `touch -a -m -c` can be written as `touch -amc`.
* If neither `-a` nor `-m` are specified, both options will be set internally.
* Options that take an argument like `-r` and `-t` must have other options separated by whitespace after their argument. For example: `touch -at 201109091200 -c`.

Difference With GNU/Linux
-------------------------
You may refer to the [man page](https://man7.org/linux/man-pages/man1/touch.1.html) of the command for comparison, but in summary:
* Long option variants like `--no-create` for `-c` are not supported.
* Here, we use `-d` instead of `-h` on Linux to specify no-dereference for symbolic links.
* Option `--time=WORD` on Linux is not supported here. Use one or both of the `-a`, `-m` options instead.
* Option `-d` (`--date=STRING`) on Linux is not supported here.
* The format specifier of the `-t` option slightly differs from the Linux one (`[[CC]YY]MMDDhhmm[.ss]`).
* The `-A` option does not exist on the Linux version of the command, but it does exist on macOS.

File Operand Shell Expansion
----------------------------
Some Unix shells like Bash have a brace expansion feature that can be useful when combined with `touch`. For example, in a Bash shell, you could execute `touch File{1..3}`, which would create the files `File1`, `File2`, and `File3` if they do not exist, or update their timestamps if they exist. This is also possible on Windows with PowerShell, although the syntax is a little bit wordy. Here are some examples:
```powershell
# Creates the files or updates timestamps of File1, File2, File2
# Note: double quotes around filenames are a must!
touch (1..3 | % { "File$_" }) 

# A more complex version of the above that support string formatting
# Creates the files or update the timestamps of File001, File002, File002
touch (1..3 | % { "File{0:d3}" -f $_ })

# Creates the files or updates timestamps of FileA, FileB, FileC
touch ('A'[0]..'C'[0] | % { "File" + [char]$_ })
```

Unicode Support
---------------
Support for Unicode (UTF-16, really) is provided via the `tchar.h` header and its macros, which automatically determine whether or not wide character types should be used, based on the *Character Set* setting in the Visual Studio project properties. Without Unicode support enabled, the program will not be able to to touch filenames like `مرحبا привет こんにちは` because the entrypoint itself will fail to properly receive Unicode command line arguments.

Installation
------------
While there's no traditional installer, if you [download a release](https://github.com/xv/touch-cmd-windows/releases), the .zip archive will contain the executable `touch` utility itself and two PowerShell scripts: `install.ps1` and `uninstall.ps1`. The purpose of these scripts is to add or remove the directory of the executable to the PATH environment variable so that `touch` can be called from any directory location in the terminal like any other commands.

To add `touch` to the PATH environment variable, extract the archive into a directory &mdash;with a name of your choice&mdash; anywhere you want (I personally prefer `%USERPROFILE%`), then run `.\install` from an elevated PowerShell terminal. The `touch` command will become instantly available to use without requiring a restart of the terminal.

To remove `touch` from the PATH environment variable, run `.\uninstall` from an elevated PowerShell terminal. The `touch` command will no longer be accessible unless you are in the same directory as the executable.

### Security Note
If you get an error saying "*&lt;script&gt;.ps1 cannot be loaded because running scripts is disabled on this system.*" when you try executing one of the scripts mentioned above, you will need to enable the execution of scripts via:
```PowerShell
Set-ExecutionPolicy -Scope CurrentUser Bypass
```
Once you are done, you may disable script executaion again via:
```PowerShell
Set-ExecutionPolicy -Scope CurrentUser Restricted
```

Build From Source
-----------------
This utility is written in C, using Visual Studio 2022 with MSVC v143 and Windows 10 SDK v2004 ([10.0.19041.0](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/#windows-10)). The solution and project files are present in the `/visualstudio/` directory. Simply run the IDE and build. Alternatively, there also a `.\build` PowerShell script to compile the code if you don't feel like firing up the IDE.

Usage Examples
--------------
```powershell
# Updates the Accessed And Modified timestamps of File1, File2, File3
# If these files do not exist, they will not be created
touch -amc File1 File2 File3

# Changes the Modified timestamp of File to 2011-07-08 14:30:00 UTC
touch -mt 201107081430Z File

# Changes the Accessed and Modified timestamps of File to that of
# RefFile, and if File does not exist, it will not be created
touch -cr RefFile File

# Changes the Modified timestamp of File to 2020-10-24 11:00:00 then
# increments it by an 1 hr and 30 mins to become 2020-10-24 12:30:00
touch -mA 0130 -t 202010241100 File

# Decrements the Accessed and Modified timestamps of File by 10 mins
touch -A -0010 File
```

### Working With Directories
Just like the Unix version, this Windows utility does not create directories if they do not exit. If you want to use `touch` to create a file inside a directory that does not exist, you will need to create it first, then pipe in the path to `touch`. Here are some PowerShell examples to achieve that:
```PowerShell
# Creates Dir/Subdir/File1
md Dir/Subdir/ | % { touch $_/File1 }

# Same as the above but will cd into Dir/Subdir/ then touch
md Dir/Subdir/ | % { cd $_ } | touch File1

# Same as the above but with a shorter syntax
cd (md Dir/Subdir/) | touch File1
```

Verifying Timestamp Changes
---------------------------
If you need to check a file's timestamps after having called `touch`, I suggest you avoid checking through the Properties dialog of the file (right click &rarr; Properties). For whatever reason, opening the dialog will cause the *Accessed* timestamp to be sometimes updated to current time after the dialog is closed. Alternatively, check using PowerShell, through the `Get-Item` cmdlet, or its `gi` alias:
```PowerShell
(gi File).CreationTime
(gi File).LastAccessTime
(gi File).LastWriteTime

# You can also combine all three properties into a single output via:
gp File | fl CreationTime,LastAccessTime,LastWriteTime
```

License
-------
All code in this repository is distributed under the terms of [MIT](LICENSE) license.
