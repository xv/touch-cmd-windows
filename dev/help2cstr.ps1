# Generates a ready-to-use C string macro from the contents of cli-help.txt with
# properly escaped quotes and newlines  

$file = "..\cli-help.txt"
$cdef = "PROGRAM_USAGE_SUMMARY"

$contents = Get-Content $file -Raw -Encoding UTF8

$contents = $contents.Replace("`r`n", "`n")
$contents = $contents.Replace("`"", "\`"")
$contents = [Regex]::Replace(
    $contents,
    "`n+", {
        param($match)
        ("\n" * $match.Value.Length) + "\" + "`n"
    }
)

Write-Output `
    "#define $cdef \" `
    "`"$contents`""