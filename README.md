About
-----
Have you ever used the `touch` command on a GNU/Linux or macOS system? That is exactly what this utility does. I frequently use the command on Linux to quickly create files and find it rather annoying that I cannot do the same thing when I am back on Windows. As such, I decided to write a native Windows implementation.

This is not a clone or fork of the GNU Coreutils `touch`, and although the core functionality is the same, there are still some significant differences between the two.

The Command Line
----------------
### Basic Syntax
`touch [OPTION]... FILE...`

The `...` indicates that you may specify multiple options and multiple files. If options are used, they must appear before the file operands; otherwise, they may be interpreted as filenames.

### Options
Please refer to [cli-help.txt](cli-help.txt) for a detailed overview of the utility's command line interface.

#### Option Specification Quirks
* The order of options does not matter.
* Flag options can be combined together. For example: `touch -a -m -c` can be written as `touch -amc`.
* Options that take an argument like `-r` and `-t` must have other options separated by whitespace after their argument. For example: `touch -at 20110909T1230 -c`.

Difference With GNU/Linux
-------------------------
You may refer to the [man page](https://man7.org/linux/man-pages/man1/touch.1.html) of the GNU/Linux command for comparison, but in summary:
* Long option variants like `--no-create` for `-c` are not supported.
* Here, we use `-d` instead of `-h` on Linux to specify no-dereference for symbolic links.
* Option `--time=WORD` on Linux is not supported here. Use one or both of the `-a`, `-m` options instead.
* Option `-d` (`--date=STRING`) on Linux is not supported here.
* The format specifier of the `-t` option differs from the Linux one (`[[CC]YY]MMDDhhmm[.ss]`).
* The `-A` option does not exist on the Linux version of the command, but it does exist on macOS.
* The `-C` option does not exist on either the Linux or macOS version of the command.

Unicode Support
---------------
Support for Unicode (UTF-16, really) is provided via the `tchar.h` header and its macros, which automatically determine whether or not wide character types should be used, based on the *Character Set* setting in the Visual Studio project properties. Without Unicode support enabled, the program will not be able to to touch filenames like `مرحبا привет こんにちは` because the entrypoint itself will fail to properly receive Unicode command line arguments.

Installation
------------
Although there is no traditional installer, each [release](https://github.com/xv/touch-cmd-windows/releases) archive includes the `touch` executable along with a PowerShell script named `config-path.ps1`. This script lets you add or remove the executable's directory from the `PATH` environment variable, allowing `touch` to be invoked from any directory in the terminal.

Once added to PATH, the `touch` command becomes immediately available without requiring a terminal restart. If the directory is later removed from `PATH`, the command will only be accessible when run from the directory containing the executable.

> [!NOTE]
> If you get an error saying "*config-path.ps1 cannot be loaded because running scripts is disabled on this system.*" when you try running the script mentioned above, you will need to enable script execution via:
> ```PowerShell
> Set-ExecutionPolicy -Scope CurrentUser Bypass
> ```
> Once you are done, you may disable script execution again via:
> ```PowerShell
> Set-ExecutionPolicy -Scope CurrentUser Restricted
> ```

> [!NOTE]
> If script execution is enabled but you get an error saying "*The file config-path.ps1 is not digitally signed. You cannot run this script on the current system*", try unblocking the script directly via:
> ```PowerShell
> Unblock-File config-path.ps1
> ```

Build From Source
-----------------
This utility is written in C, using Visual Studio 2026 with MSVC v145 and Windows 11 SDK 26100 ([10.0.26100.0](https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads#windows-11--26100-versions)). The solution and project files are present in the `visualstudio\` directory. Simply run the IDE and build. Alternatively, there is also a `dev\build.ps1` script to compile the code if you only have a standalone Build Tools installation or don't feel like firing up the IDE.

Usage Examples
--------------
### Basic Usage
```shell
# Updates the "Created" timestamp of File1
# If File1 does not exist, it will be created
touch -C File1

# Updates the "Accessed" And "Modified" timestamps of File1, File2, File3
# If these files do not exist, they will NOT be created
touch -c File1 File2 File3

# Changes the "Accessed" and "Modified" timestamps of File to that of
# RefFile, and if File does not exist, it will be created with the
# referenced timestamps
touch -r RefFile File
```

### Timestamp Formatting: Calendar Dates
```shell
# Sets the timestamp to May 22, 2026 at 13:00 local time
touch -t 2026-05-22T13:00 File

# Same as above, but uses the basic format, allowing hyphens and colons
# to be omitted and the time precision to be reduced to hours only
touch -t 20260522T13 File

# Sets the timestamp to May 22, 2026 at 13:00 UTC+05:30
touch -t 2026-05-22T13:00+05:30 File

# Same as above but in basic format
touch -t 20260522T13+0530 File
```

### Timestamp Formatting: Ordinal Dates
```shell
# Sets the timestamp to the 142nd day of 2026 at 08:15 UTC
touch -t 2026-142T08:15Z File

# Same as above but in basic format
touch -t 2026142T0815Z File
```

### Timestamp Formatting: Week Dates
```shell
# Sets the timestamp to Friday of ISO week 21 in 2026 at 09:45 local time
touch -t 2026-W21-5T09:45 File

# Same as above but in basic format
touch -t 2026W215T0945 File
```

### Adjusting Current Timestamps
```shell
# Move the timestamps of File forward by 30s
touch -A 30 File

# Move the timestamps of File forward by 1m30s
touch -A 0130 File

# Move the timestamps of File backward by 12h
touch -A -120000 File

# Sets the timestamp to May 22, 2026 at 13:30:00 local time
touch -t 2026-05-22T13:30:40 -A -40 File
```

### Working With Directories
Just like the Unix version, this Windows utility does not create directories if they do not exit. If you want to use `touch` to create a file inside a directory that does not exist, you will need to create it first, then pipe in the path to `touch`. Here are some PowerShell examples to achieve that:
```powershell
# Creates Dir/Subdir/File1
md Dir/Subdir/ | % { touch $_/File1 }

# Same as the above but will cd into Dir/Subdir/ then touch
md Dir/Subdir/ | % { cd $_ } | touch File1

# Same as the above but with a shorter syntax
cd (md Dir/Subdir/) | touch File1
```

### File Operand Shell Expansion
Some Unix shells like Bash have a brace expansion feature that can be useful when combined with `touch`. For example, in a Bash shell, you could execute `touch File{1..3}`, which would create the files `File1`, `File2`, and `File3` if they do not exist, or update their timestamps if they exist. This is also possible on Windows with PowerShell, although the syntax is a little bit wordy. Here are some examples:
```powershell
# Creates the files or updates timestamps of File1, File2, File2
# Double quotes around filenames are a must!
touch (1..3 | % { "File$_" }) 

# A more complex version of the above that support string formatting
# Creates the files or update the timestamps of File001, File002, File002
touch (1..3 | % { "File{0:d3}" -f $_ })

# Creates the files or updates timestamps of FileA, FileB, FileC
touch ('A'[0]..'C'[0] | % { "File" + [char]$_ })

# Updates the timestamps of all files ending with .txt
touch -c (gi *.txt)

# Updates the timestamps of all files in Dir/
# It will NOT recurse subdirectories!
touch -c (gi Dir/*)
```

Verifying Timestamp Changes
---------------------------
If you need to check a file's timestamps after having called `touch`, I suggest you avoid checking through the Properties dialog of the file (right click &rarr; Properties). For whatever reason, opening the dialog will cause the *Accessed* timestamp to be sometimes updated to current time after the dialog is closed. Alternatively, check using PowerShell, through the `Get-Item` cmdlet, or its `gi` alias:
```PowerShell
(gi File).CreationTime
(gi File).LastAccessTime
(gi File).LastWriteTime

# You can also combine all three properties into a single output
# gp and fl are aliases for Get-ItemProperty and Format-List
gp File | fl CreationTime,LastAccessTime,LastWriteTime

# Same as above but shows time in UTC
gp File | fl CreationTimeUtc,LastAccessTimeUtc,LastWriteTimeUtc
```

License
-------
All code in this repository is available under the terms of the [MIT](LICENSE) license.
