#!/bin/sh
# nasm -> COFF64, линковка mingw-w64 ld: родной, если есть в PATH, иначе в WSL
set -e
cd "$(dirname "$0")"
mkdir -p build
(cd src && nasm -f win64 main.asm -o ../build/main.obj -l ../build/main.lst)
LDARGS="build/main.obj -o build/stuzha.exe --subsystem windows -e start -Map build/stuzha.map -lkernel32 -luser32 -lgdi32 -ldwmapi -lwinmm"
if command -v x86_64-w64-mingw32-ld >/dev/null 2>&1; then
    x86_64-w64-mingw32-ld $LDARGS
else
    root=$(pwd -W 2>/dev/null || pwd)
    wsl -e sh -c "cd \"\$(wslpath '$root')\" && x86_64-w64-mingw32-ld $LDARGS"
fi
ls -la build/stuzha.exe
