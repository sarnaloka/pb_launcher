#!/usr/bin/env python3
"""Actualitza version.txt (compilació, mida, CRC32, novetats, nom) a partir d'Escriptori.app i src/base.c.
Les novetats es conserven de la línia 4 del fitxer existent; canvia-les a mà abans de publicar."""
import sys, zlib, re, os
root = sys.argv[1]
src = open(os.path.join(root, 'src/base.c'), encoding='utf8').read()
build = int(re.search(r'#define APP_BUILD (\d+)', src).group(1))
name = re.search(r'#define APP_VERNAME "([^"]+)"', src).group(1)
b = open(os.path.join(root, 'Escriptori.app'), 'rb').read()
p = os.path.join(root, 'version.txt')
old = open(p, encoding='utf8').read().split('\n') if os.path.exists(p) else []
notes = old[3] if len(old) > 3 else ''
open(p, 'w', encoding='utf8').write(f"{build}\n{len(b)}\n{zlib.crc32(b):08x}\n{notes}\n{name}\n")
print('compilació', build, 'versió', name, 'mida', len(b))
