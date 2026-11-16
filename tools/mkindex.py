#!/usr/bin/env python3
"""Genera packages/index.txt a partir de src/packages.txt i dels fitxers .pkg compilats.
Ús: mkindex.py <src/packages.txt> <carpeta de paquets>"""
import sys, zlib, os
meta, pkgdir = sys.argv[1], sys.argv[2]
out = []
for ln in open(meta, encoding='utf8'):
    ln = ln.strip()
    if not ln or ln.startswith('#'): continue
    pid, name, ver, desc = ln.split('|', 3)
    b = open(os.path.join(pkgdir, pid + '.pkg'), 'rb').read()
    out.append(f"{pid}|{name}|{ver}|{len(b)}|{zlib.crc32(b):08x}|{desc}")
open(os.path.join(pkgdir, 'index.txt'), 'w', encoding='utf8').write('\n'.join(out) + '\n')
print(len(out), 'paquets')
