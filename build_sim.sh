#!/bin/sh
# Compila el sistema base i els paquets per al simulador d'ordinador (amb comprovadors de memòria).
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT/src"
# En 32 bits, com el lector (el codi del Doom no funciona en 64 bits). Cal gcc-multilib.
F="-m32 -O1 -g -DESC_SIM -pthread -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -fsanitize=address,undefined -Ishim -I."
gcc $F -rdynamic -o ../sim/uitest base.c shim/shim.c -lm -ldl
mkdir -p ../sim/net/repo/packages
# Doom: el codi original (1997) es compila a part, sense avisos i només amb el comprovador d'adreces
DOOMOBJ=$(mktemp -d)
DF="-m32 -O1 -g -DESC_SIM -fsanitize=address -Ishim -I. -w -fcommon -fno-strict-aliasing"
doom_objs() {
  for s in apps/doom/*.c apps/doom/pbdoom/*.c apps/doom/pbdoom/frames/*.c apps/doom/sdldoom-1.10/*.c; do
    o="$DOOMOBJ/$(echo "$s" | tr '/' '_').o"
    gcc $DF -fPIC -fvisibility=hidden -c "$s" -o "$o"
    echo "$o"
  done
}
# Lector: el pont amb el CREngine (C++), les seves dades i les biblioteques de build/cre-sim (tools/cre/build_deps.sh)
CRE="$ROOT/build/cre-sim"
[ -f "$CRE/lib/libcrengine.a" ] || "$ROOT/tools/cre/build_deps.sh" sim
CREINC="-I$CRE/include -I$CRE/include/freetype2 -I$CRE/include/harfbuzz -I${CRE_SRC:-$HOME/cre-src}/crengine/crengine/include"
lector_pkg() {
  g++ -m32 -O1 -g -fsanitize=address -fPIC -fvisibility=hidden -std=gnu++14 -w $CREINC -c apps/lector/cre.cpp -o "$DOOMOBJ/cre.o"
  gcc $F -c apps/lector/data.S -Wa,-I"$CRE/data" -o "$DOOMOBJ/lecdata.o"
  gcc $F -fPIC -fvisibility=hidden -c apps/lector.c -o "$DOOMOBJ/lector.o"
  g++ $F -shared -fPIC -fvisibility=hidden -o "$1" "$DOOMOBJ/lector.o" "$DOOMOBJ/cre.o" "$DOOMOBJ/lecdata.o" -L"$CRE/lib" \
    -Wl,--exclude-libs,ALL -lcrengine -lharfbuzz -lfreetype -lpng16 -ljpeg -lunibreak -lutf8proc -lxxhash -lz -lm
}
for f in apps/*.c; do
  id=$(basename "$f" .c); extra=""
  [ "$id" = busamb ] && extra="apps/busamb/amb.c apps/busamb/data.S -Wa,-Iapps/busamb"
  [ "$id" = doom ] && extra="$(doom_objs)"
  if [ "$id" = lector ]; then lector_pkg ../sim/net/repo/packages/lector.pkg; continue; fi
  gcc $F -shared -fPIC -fvisibility=hidden -o ../sim/net/repo/packages/$id.pkg "$f" $extra -lm
done
rm -rf "$DOOMOBJ"
