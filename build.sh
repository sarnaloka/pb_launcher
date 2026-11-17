#!/bin/sh
# Compila l'Escriptori (sistema base) i tots els paquets per a PocketBook amb el SDK oficial
# (branca 6.5, plataforma B288):  git clone --depth 1 --branch 6.5 https://github.com/pocketbook/SDK_6.3.0 sdk
# Ús: SDK=/camí/a/sdk/SDK-B288 ./build.sh
set -e
: "${SDK:?defineix SDK=/camí/a/SDK-B288}"
export PATH="$SDK/usr/bin:$PATH" LD_LIBRARY_PATH="$SDK/usr/lib"
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT/src"
CC=arm-obreey-linux-gnueabi-gcc
F="-O2 -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -fsigned-char -march=armv7-a -mfpu=neon -mfloat-abi=softfp -DPLATFORM_FC -I."
$CC $F -s -o "$ROOT/Escriptori.app" base.c -linkview -lm -ldl -lpthread
mkdir -p "$ROOT/packages"
for f in apps/*.c; do
  id=$(basename "$f" .c); extra=""
  [ "$id" = busamb ] && extra="apps/busamb/amb.c apps/busamb/data.S -Wa,-Iapps/busamb"
  $CC $F -s -shared -fPIC -fvisibility=hidden -o "$ROOT/packages/$id.pkg" "$f" $extra -linkview -lm
done
unset LD_LIBRARY_PATH
python3 "$ROOT/tools/mkindex.py" "$ROOT/src/packages.txt" "$ROOT/packages"
python3 "$ROOT/tools/mkversion.py" "$ROOT"
