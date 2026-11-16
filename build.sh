#!/bin/sh
# Compila Escriptori.app amb el SDK oficial de PocketBook (branca 6.5, plataforma B288):
#   git clone --depth 1 --branch 6.5 https://github.com/pocketbook/SDK_6.3.0 sdk
# Ús: SDK=/camí/a/sdk/SDK-B288 ./build.sh
set -e
: "${SDK:?defineix SDK=/camí/a/SDK-B288}"
export PATH="$SDK/usr/bin:$PATH" LD_LIBRARY_PATH="$SDK/usr/lib"
cd "$(dirname "$0")/src"
arm-obreey-linux-gnueabi-gcc -O2 -Wall -Wextra -fsigned-char -march=armv7-a -mfpu=neon -mfloat-abi=softfp \
  -DPLATFORM_FC -s -o ../Escriptori.app main.c amb.c data.S -linkview -lm
cd ..
python3 - <<'PY'
import zlib, re
b = open('Escriptori.app', 'rb').read()
v = int(re.search(r'#define APP_VERSION (\d+)', open('src/suite.c').read()).group(1))
old = open('version.txt').read().split('\n')
notes = old[3] if len(old) > 3 else ''
open('version.txt', 'w').write(f"{v}\n{len(b)}\n{zlib.crc32(b):08x}\n{notes}\n")
print('versió', v, 'mida', len(b))
PY
