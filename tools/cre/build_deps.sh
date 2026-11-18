#!/bin/bash
# Compila el CREngine (el motor de pàgines del KOReader i del Kobalt) i les seves biblioteques com a biblioteques
# estàtiques per al paquet Lector.
#   tools/cre/build_deps.sh arm   → per al lector (cal SDK=<camí>/SDK-B288)
#   tools/cre/build_deps.sh sim   → per al simulador (x86 de 32 bits, amb comprovadors de memòria)
# El codi de tercers es baixa (versions fixades) a $CRE_SRC (per defecte ~/cre-src), fora del repositori; el resultat
# va a build/cre-<destí>/ (lib/ i include/). Tot es compila amb -fvisibility=hidden: el lector ja carrega el seu propi
# FreeType i les biblioteques del paquet no hi han de topar.
set -e
T=${1:?arm o sim}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SRC=${CRE_SRC:-$HOME/cre-src}
OUT="$ROOT/build/cre-$T"
J=$(nproc 2>/dev/null || echo 4)

fetch() {   # nom url etiqueta-o-commit
  [ -d "$SRC/$1/.git" ] && return 0
  mkdir -p "$SRC/$1"; git -C "$SRC/$1" init -q; git -C "$SRC/$1" remote add origin "$2"
  git -C "$SRC/$1" fetch -q --depth 1 origin "$3"; git -C "$SRC/$1" -c advice.detachedHead=false checkout -q FETCH_HEAD
}
fetch zlib          https://github.com/madler/zlib                 v1.3.1
fetch libpng        https://github.com/pnggroup/libpng             v1.6.43
fetch libjpeg-turbo https://github.com/libjpeg-turbo/libjpeg-turbo 3.0.4
fetch freetype      https://github.com/freetype/freetype           VER-2-13-3
fetch harfbuzz      https://github.com/harfbuzz/harfbuzz           8.5.0
fetch libunibreak   https://github.com/adah1972/libunibreak        libunibreak_6_1
fetch utf8proc      https://github.com/JuliaStrings/utf8proc       v2.9.0
fetch xxhash        https://github.com/Cyan4973/xxHash             v0.8.3
fetch crengine      https://github.com/koreader/crengine           4ce1aed17e9d7852e047e0b9b0bf6a637486fd97   # la del Kobalt
# Pegats de l'Escriptori al CREngine (tools/cre/patches), un sol cop
if [ ! -f "$SRC/crengine/.escriptori-patched" ]; then
  for p in "$ROOT"/tools/cre/patches/*.patch; do git -C "$SRC/crengine" apply "$p"; done
  touch "$SRC/crengine/.escriptori-patched"
fi

if [ "$T" = arm ]; then
  : "${SDK:?cal SDK=<camí>/SDK-B288}"
  # el compilador del SDK necessita les biblioteques de $SDK/usr/lib, però la resta d'eines no: embolcalls
  mkdir -p "$OUT/bin"
  for t in gcc g++ ar; do
    printf '#!/bin/sh\nLD_LIBRARY_PATH="%s/usr/lib" exec "%s/usr/bin/arm-obreey-linux-gnueabi-%s" "$@"\n' "$SDK" "$SDK" "$t" > "$OUT/bin/$t"
    chmod +x "$OUT/bin/$t"
  done
  CC="$OUT/bin/gcc"; CXX="$OUT/bin/g++"; AR="$OUT/bin/ar"
  FL="-O2 -fPIC -fvisibility=hidden -fsigned-char -march=armv7-a -mfpu=neon -mfloat-abi=softfp"
  CM="-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm"
else
  CC=gcc; CXX=g++; AR=ar
  FL="-m32 -O1 -g -fPIC -fvisibility=hidden -fsanitize=address"
  CM=""
fi
mkdir -p "$OUT/lib" "$OUT/include" "$OUT/obj"
P="$OUT"
cmk() {   # directori-font directori-build opcions...
  s=$1; b=$2; shift 2
  cmake -S "$s" -B "$b" -G Ninja $CM -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX -DCMAKE_AR=$(command -v $AR) \
    -DCMAKE_C_FLAGS="$FL" -DCMAKE_CXX_FLAGS="$FL" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX="$P" -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_PREFIX_PATH="$P" -DCMAKE_FIND_ROOT_PATH="$P" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@" > "$b.log" 2>&1 || { cat "$b.log"; exit 1; }
  cmake --build "$b" -j "$J" >> "$b.log" 2>&1 || { tail -40 "$b.log"; exit 1; }
  cmake --install "$b" >> "$b.log" 2>&1
}
B="$OUT/obj"
[ -f "$P/lib/libz.a" ]          || cmk "$SRC/zlib" "$B/zlib" -DZLIB_BUILD_EXAMPLES=OFF
rm -f "$P"/lib/libz.so*
[ -f "$P/lib/libpng16.a" ]      || cmk "$SRC/libpng" "$B/libpng" -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_ARM_NEON=off -DZLIB_ROOT="$P"
[ -f "$P/lib/libjpeg.a" ]       || cmk "$SRC/libjpeg-turbo" "$B/jpeg" -DENABLE_SHARED=OFF -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF
[ -f "$P/lib/libfreetype.a" ]   || cmk "$SRC/freetype" "$B/freetype" -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON \
                                     -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DZLIB_ROOT="$P"
# HarfBuzz, libunibreak i utf8proc: un sol fitxer o pocs, es compilen directament
if [ ! -f "$P/lib/libharfbuzz.a" ]; then
  $CXX $FL -std=c++11 -fno-exceptions -fno-rtti -DHB_NO_MT -DHAVE_FREETYPE -I"$P/include/freetype2" -c "$SRC/harfbuzz/src/harfbuzz.cc" -o "$B/harfbuzz.o"
  $AR rcs "$P/lib/libharfbuzz.a" "$B/harfbuzz.o"
fi
mkdir -p "$P/include/harfbuzz"; cp "$SRC"/harfbuzz/src/hb*.h "$P/include/harfbuzz/"
if [ ! -f "$P/lib/libunibreak.a" ]; then
  o=""; for f in "$SRC"/libunibreak/src/*.c; do n=$(basename "$f" .c); case $n in filter_dup|tests*|*test*) continue;; esac
    $CC $FL -c "$f" -o "$B/ub_$n.o"; o="$o $B/ub_$n.o"; done
  $AR rcs "$P/lib/libunibreak.a" $o
fi
cp "$SRC"/libunibreak/src/*.h "$P/include/"
if [ ! -f "$P/lib/libutf8proc.a" ]; then
  $CC $FL -DUTF8PROC_STATIC -c "$SRC/utf8proc/utf8proc.c" -o "$B/utf8proc.o"; $AR rcs "$P/lib/libutf8proc.a" "$B/utf8proc.o"
fi
cp "$SRC/utf8proc/utf8proc.h" "$P/include/"
if [ ! -f "$P/lib/libxxhash.a" ]; then
  $CC $FL -c "$SRC/xxhash/xxhash.c" -o "$B/xxhash.o"; $AR rcs "$P/lib/libxxhash.a" "$B/xxhash.o"
fi
cp "$SRC/xxhash/xxhash.h" "$P/include/"
# CREngine: només el que cal per a EPUB (també FB2, HTML, TXT, RTF, DOCX i ODT, que venen amb el mateix codi)
C="$SRC/crengine"
sed -e 's/@USE_ZLIB@/1/;s/@USE_ZSTD@/0/;s/@USE_CHMLIB@/0/;s/@USE_ANTIWORD@/0/;s/@USE_MD4C@/0/;s/@USE_GIF@/1/' \
    -e 's/@USE_LIBJPEG@/1/;s/@USE_LIBPNG@/1/;s/@USE_LIBWEBP@/0/;s/@USE_LUNASVG@/0/;s/@USE_NANOSVG@/0/;s/@USE_STB_IMAGE@/0/' \
    -e 's/@USE_MATHML@/1/;s/@USE_SRELL@/0/;s/@USE_OWN_MEM_MAN@/1/;s/@USE_FONTCONFIG@/0/;s/@USE_FREETYPE@/1/;s/@USE_FRIBIDI@/0/' \
    -e 's/@USE_HARFBUZZ@/1/;s/@USE_LIBUNIBREAK@/1/;s/@USE_UTF8PROC@/1/;s/#cmakedefine DISABLE_CLOEXEC.*//;s/#cmakedefine HAVE_OFF64_T *1/#define HAVE_OFF64_T 1/' \
    "$C/crengine/include/crsetup.h.cmake" > "$P/include/crsetup.h"
if [ ! -f "$P/lib/libcrengine.a" ]; then
  o=""
  for n in qimagescale/qimagescale src/cp_stats src/crtxtenc src/docxfmt src/drawutil src/epubfmt src/fb3fmt src/hist src/hyphman \
           src/lstridmap src/lvdocview src/lvdrawbuf src/lvfnt src/lvfntman src/lvimg src/lvmemman src/lvopc src/lvpagesplitter \
           src/lvrend src/lvstream src/lvstring src/lvstsheet src/lvstyles src/lvtextfm src/lvtinydom src/lvxml src/mathml \
           src/odtfmt src/odxutil src/pdbfmt src/props src/renderutil src/rtfimp src/textlang src/txtselector src/wordfmt; do
    ob="$B/cre_$(echo $n | tr '/' '_').o"
    $CXX $FL -std=gnu++14 -w -I"$P/include" -I"$P/include/freetype2" -I"$P/include/harfbuzz" -I"$C/crengine/include" \
      -c "$C/crengine/$n.cpp" -o "$ob" &
    o="$o $ob"
    while [ "$(jobs -r | wc -l)" -ge "$J" ]; do sleep 0.2; done
  done
  wait
  for x in $o; do [ -f "$x" ] || { echo "falta $x"; exit 1; }; done
  $AR rcs "$P/lib/libcrengine.a" $o
fi
# Dades que el paquet porta a dins (data.S): el CSS per defecte i els diccionaris de partició de mots de les llengües
# més properes (l'alemany, d'1 MB, no hi és)
mkdir -p "$P/data/hyph"
cp "$C/cr3gui/data/epub.css" "$P/data/"
for h in Catalan Spanish English_US English_GB French Italian Portuguese Occitan Basque Galician; do cp "$C/cr3gui/data/hyph/$h.pattern" "$P/data/hyph/"; done
echo "CREngine ($T) a $P"
