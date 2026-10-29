#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${CC:?Set CC to the ARM compiler (use gcc for a native build)}"
: "${DROPBEAR_SOURCE:?Path to extracted Dropbear 2026.94 source, with libtomcrypt built}"
mkdir -p build
python3 tools/embed_panel.py
"$CC" -Os -static -s -ffunction-sections -fdata-sections -Wl,--gc-sections -Wall -Wextra -Wno-misleading-indentation -DLTC_MD5 -Ivendor -Isrc -I"$DROPBEAR_SOURCE" -I"$DROPBEAR_SOURCE/src" -I"$DROPBEAR_SOURCE/libtomcrypt/src/headers" -I"$DROPBEAR_SOURCE/libtommath" src/localmod-web.c vendor/cJSON.c "$DROPBEAR_SOURCE/libtomcrypt/src/hashes/md5.c" "$DROPBEAR_SOURCE/libtomcrypt/libtomcrypt.a" -lm -o build/localmod-web.arm
