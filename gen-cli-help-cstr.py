# Generates a ready-to-use C string macro from the contents of cli-help.txt with
# properly escaped quotes and newlines  

from pathlib import Path
import sys

sys.stdout.reconfigure(encoding='utf-8')

file = "cli-help.txt"
cdef = "PROGRAM_USAGE_SUMMARY"

with Path(file).open(encoding="utf-8") as f:
    contents = f.read()

contents = contents.replace('"', '\\"')

result = []
newline_count = 0

for ch in contents:
    if ch == '\n':
        newline_count += 1
        continue
    if newline_count:
        result.append('\\n' * newline_count)
        result.append('\\\n')
        newline_count = 0
    result.append(ch)

print(f'#define {cdef} \\\n"{ "".join(result) }"')