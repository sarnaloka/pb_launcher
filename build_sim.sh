#!/bin/sh
# Compila el sistema base i els paquets per al simulador d'ordinador (amb comprovadors de memòria).
set -e
cd "$(dirname "$0")/src"
F="-O1 -g -DESC_SIM -pthread -Wall -Wextra -Wno-format-truncation -Wno-misleading-indentation -fsanitize=address,undefined -Ishim -I."
gcc $F -rdynamic -o ../sim/uitest base.c shim/shim.c -lm -ldl
mkdir -p ../sim/net/repo/packages
for f in apps/*.c; do
  id=$(basename "$f" .c); extra=""
  [ "$id" = busamb ] && extra="apps/busamb/amb.c apps/busamb/data.S -Wa,-Iapps/busamb"
  gcc $F -shared -fPIC -fvisibility=hidden -o ../sim/net/repo/packages/$id.pkg "$f" $extra -lm
done
