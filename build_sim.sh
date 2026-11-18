#!/bin/sh
# Compila el sistema base i els paquets per al simulador d'ordinador (amb comprovadors de memòria).
set -e
cd "$(dirname "$0")/src"
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
for f in apps/*.c; do
  id=$(basename "$f" .c); extra=""
  [ "$id" = busamb ] && extra="apps/busamb/amb.c apps/busamb/data.S -Wa,-Iapps/busamb"
  [ "$id" = doom ] && extra="$(doom_objs)"
  gcc $F -shared -fPIC -fvisibility=hidden -o ../sim/net/repo/packages/$id.pkg "$f" $extra -lm
done
rm -rf "$DOOMOBJ"
