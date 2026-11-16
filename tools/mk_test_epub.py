#!/usr/bin/env python3
"""Genera els EPUB d'exemple per provar el paquet «Lector» al simulador.
Ús: mk_test_epub.py <carpeta de sortida>
Crea exemple.epub (EPUB 3, índex nav, entrades comprimides) i exemple2.epub (EPUB 2, índex NCX,
entrades sense comprimir, un sol fitxer gran amb ancoratges). Els textos són originals."""
import sys, os, zipfile, random

FRASES = [
    "El vent de llevant arrossegava el perfum de sal fins als carrers estrets del poble.",
    "Na Rosa va obrir la finestra i va deixar que la llum del matí omplís la cuina.",
    "—No et facis il·lusions —va dir en Pere—, la barca no sortirà fins que s'aturi la tramuntana.",
    "A la plaça, els infants jugaven a pilota entre les cadires de la terrassa del cafè.",
    "Ningú no recordava quan s'havia encès per darrera vegada el far de la cala dels Esculls.",
    "Va ser aleshores que va sentir, ben lluny, el so greu d'una campana que no hauria d'haver sonat.",
    "«Potser tot plegat és un error», va pensar, i va guardar la carta a la butxaca de l'abric.",
    "El camí pujava entre oliveres i garrofers, i a cada revolt el mar semblava més gran i més quiet.",
    "Durant l'hora de la migdiada el poble sencer es quedava en silenci, com si respirés a poc a poc.",
    "L'àvia explicava que, en altres temps, les nits de tempesta s'hi encenien fogueres per guiar els pescadors.",
]
def paragraf(r, n):
    return " ".join(r.choice(FRASES) for _ in range(n))

def xhtml(title, body):
    return ('<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE html>\n'
            '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.w3.org/2007/ops" lang="ca">\n'
            f'<head><title>{title}</title><style>p {{ margin: 0 }}</style></head>\n<body>\n{body}\n</body></html>\n')

CONTAINER = ('<?xml version="1.0"?>\n<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">'
             '<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')

def epub3(path):
    r = random.Random(7)
    caps = []
    for i, t in enumerate(["La cala dels Esculls", "El far apagat", "Una carta sense remitent", "Tramuntana", "El darrer vaixell"], 1):
        cos = f'<h1 id="cap{i}">{t}</h1>\n'
        for p in range(14 if i != 3 else 6):
            cos += f'<p>{paragraf(r, r.randint(3, 7))}</p>\n'
            if i == 2 and p == 3:
                cos += '<blockquote><p>«Qui vigila el far, vigila també la gent que dorm al poble.»</p></blockquote>\n'
            if i == 4 and p == 2:
                cos += '<ul><li>Cordes i xarxes</li><li>Un fanal de petroli</li><li>Pa, oli i olives</li></ul>\n'
            if i == 5 and p == 1:
                cos += '<p>Entitats: caf&eacute; &amp; llet &mdash; &laquo;c&ograve;pia&raquo; &#8212; &#x2026;<br/>Segona línia després d\'un salt.</p>\n<hr/>\n'
        caps.append((f"cap{i}.xhtml", t, cos))
    portada = '<div style="text-align:center"><img src="cover.jpg" alt=""/></div>'
    nav = ('<nav epub:type="toc" id="toc"><h1>Continguts</h1><ol>' +
           "".join(f'<li><a href="{f}">{t}</a></li>' for f, t, _ in caps[:2]) +
           f'<li><a href="{caps[2][0]}">{caps[2][1]}</a><ol><li><a href="{caps[2][0]}#cap3">Obertura</a></li></ol></li>' +
           "".join(f'<li><a href="{f}">{t}</a></li>' for f, t, _ in caps[3:]) + '</ol></nav>')
    manifest = ('<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
                '<item id="portada" href="portada.xhtml" media-type="application/xhtml+xml"/>' +
                "".join(f'<item id="c{i}" href="{f}" media-type="application/xhtml+xml"/>' for i, (f, _, _) in enumerate(caps, 1)))
    spine = '<itemref idref="portada"/>' + "".join(f'<itemref idref="c{i}"/>' for i in range(1, len(caps) + 1))
    opf = ('<?xml version="1.0" encoding="utf-8"?>\n<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">'
           '<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">exemple-1</dc:identifier>'
           '<dc:title>La cala dels Esculls</dc:title><dc:creator>Autora d\'Exemple</dc:creator><dc:language>ca</dc:language></metadata>'
           f'<manifest>{manifest}</manifest><spine>{spine}</spine></package>')
    with zipfile.ZipFile(path, 'w') as z:
        z.writestr(zipfile.ZipInfo('mimetype'), 'application/epub+zip', compress_type=zipfile.ZIP_STORED)
        z.writestr('META-INF/container.xml', CONTAINER, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr('OEBPS/content.opf', opf, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr('OEBPS/nav.xhtml', xhtml('Continguts', nav), compress_type=zipfile.ZIP_DEFLATED)
        z.writestr('OEBPS/portada.xhtml', xhtml('Portada', portada), compress_type=zipfile.ZIP_DEFLATED)
        for f, t, cos in caps:
            z.writestr('OEBPS/' + f, xhtml(t, cos), compress_type=zipfile.ZIP_DEFLATED)

def epub2(path):
    r = random.Random(11)
    cos = ""
    ids = []
    for i, t in enumerate(["Primera part", "Segona part", "Tercera part"], 1):
        cos += f'<h2 id="p{i}">{t}</h2>\n'
        for p in range(40):
            cos += f'<p>{paragraf(r, r.randint(4, 9))}</p>\n'
        ids.append((f"p{i}", t))
    ncx = ('<?xml version="1.0" encoding="UTF-8"?>\n<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head/><docTitle><text>Memòries d\'un far</text></docTitle><navMap>' +
           "".join(f'<navPoint id="n{k}" playOrder="{k}"><navLabel><text>{t}</text></navLabel><content src="text/llibre.html#{i}"/></navPoint>' for k, (i, t) in enumerate(ids, 1)) +
           '</navMap></ncx>')
    opf = ('<?xml version="1.0"?>\n<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/">'
           '<dc:title>Memòries d\'un far &amp; altres històries</dc:title><dc:creator>Un Autor Qualsevol</dc:creator><dc:identifier id="id">exemple-2</dc:identifier></metadata>'
           '<manifest><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/><item id="llibre" href="text/llibre.html" media-type="application/xhtml+xml"/></manifest>'
           '<spine toc="ncx"><itemref idref="llibre"/></spine></package>')
    with zipfile.ZipFile(path, 'w') as z:
        z.writestr('META-INF/container.xml', CONTAINER.replace('OEBPS/content.opf', 'content.opf'), compress_type=zipfile.ZIP_STORED)
        z.writestr('content.opf', opf, compress_type=zipfile.ZIP_STORED)
        z.writestr('toc.ncx', ncx, compress_type=zipfile.ZIP_STORED)
        z.writestr('text/llibre.html', xhtml('Memòries', cos), compress_type=zipfile.ZIP_STORED)

if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    epub3(os.path.join(out, 'exemple.epub'))
    epub2(os.path.join(out, 'exemple2.epub'))
    print('fet')
