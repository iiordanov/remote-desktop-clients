#!/bin/bash
OUTPUT="$1"
touch "$OUTPUT"
PREFIX="$2"
LIB="$3"

walk_dlls() {
    ${arch}-w64-mingw32-objdump -p "$1" | grep '\.dll$' | sed 's/.* \([^ ]*\.dll\).*/\1/' | while read base_dll; do
        DLL=`grep -m 1 "$base_dll" "$DLLS"`
        if [ -z "$DLL" -o ! -f "$DLL" ]; then
            echo "$base_dll ($DLL) not found"
        elif ! grep -q "$DLL" "$OUTPUT"; then
            echo "$DLL" >> "$OUTPUT"
            walk_dlls "$DLL"
        fi
    done
}

DLLS=`mktemp`
trap 'rm -f $DLLS' EXIT
if file "$LIB" | grep -q x86-64; then
    arch=x86_64
else
    arch=i686
fi
find `${arch}-w64-mingw32-gcc -print-sysroot` $PREFIX -name "*.dll" -type f 2>/dev/null | sort -u > "$DLLS"
walk_dlls $LIB

