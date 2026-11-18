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
# Doom: el codi original (1997) es compila a part, sense avisos i amb -fcommon/-fno-strict-aliasing
DOOMOBJ=$(mktemp -d)
doom_objs() {
  for s in apps/doom/*.c apps/doom/pbdoom/*.c apps/doom/pbdoom/frames/*.c apps/doom/sdldoom-1.10/*.c; do
    o="$DOOMOBJ/$(echo "$s" | tr '/' '_').o"
    $CC $F -w -fcommon -fno-strict-aliasing -fPIC -fvisibility=hidden -c "$s" -o "$o"
    echo "$o"
  done
}
# Lector: el pont amb el CREngine (C++), les seves dades i les biblioteques de build/cre-arm (tools/cre/build_deps.sh).
# libstdc++ s'hi enllaça estàticament i totes les biblioteques queden amagades dins del paquet (el lector ja té el seu FreeType).
CRE="$ROOT/build/cre-arm"
[ -f "$CRE/lib/libcrengine.a" ] || SDK="$SDK" "$ROOT/tools/cre/build_deps.sh" arm
CREINC="-I$CRE/include -I$CRE/include/freetype2 -I$CRE/include/harfbuzz -I${CRE_SRC:-$HOME/cre-src}/crengine/crengine/include"
lector_pkg() {
  arm-obreey-linux-gnueabi-g++ -O2 -fsigned-char -march=armv7-a -mfpu=neon -mfloat-abi=softfp -fPIC -fvisibility=hidden -std=gnu++14 -w $CREINC \
    -c apps/lector/cre.cpp -o "$DOOMOBJ/cre.o"
  $CC $F -c apps/lector/data.S -Wa,-I"$CRE/data" -o "$DOOMOBJ/lecdata.o"
  $CC $F -fPIC -fvisibility=hidden -c apps/lector.c -o "$DOOMOBJ/lector.o"
  arm-obreey-linux-gnueabi-g++ $F -s -shared -fPIC -fvisibility=hidden -static-libstdc++ -static-libgcc -o "$1" "$DOOMOBJ/lector.o" \
    "$DOOMOBJ/cre.o" "$DOOMOBJ/lecdata.o" -L"$CRE/lib" -Wl,--exclude-libs,ALL \
    -lcrengine -lharfbuzz -lfreetype -lpng16 -ljpeg -lunibreak -lutf8proc -lxxhash -lz -linkview -lm
}
for f in apps/*.c; do
  id=$(basename "$f" .c); extra=""
  [ "$id" = busamb ] && extra="apps/busamb/amb.c apps/busamb/data.S -Wa,-Iapps/busamb"
  [ "$id" = doom ] && extra="$(doom_objs)"
  if [ "$id" = lector ]; then lector_pkg "$ROOT/packages/lector.pkg"; continue; fi
  $CC $F -s -shared -fPIC -fvisibility=hidden -o "$ROOT/packages/$id.pkg" "$f" $extra -linkview -lm
done
rm -rf "$DOOMOBJ"
unset LD_LIBRARY_PATH
python3 "$ROOT/tools/mkindex.py" "$ROOT/src/packages.txt" "$ROOT/packages"
python3 "$ROOT/tools/mkversion.py" "$ROOT"
