/* Escriptori — paquet «Lector»: biblioteca i lector de llibres EPUB. La composició i les pàgines les fa el CREngine
   (el motor del KOReader i del Kobalt, GPL v2 o posterior; vegeu lector/LLEGEIX.md), i per tant aquest paquet també és GPL. */
#define ESC_MODULE
#include "../mod.h"
#include <ctype.h>
#include "lector/cre.h"

enum { V_LIB, V_BOOK, V_MENU, V_TOC };
enum { A_OPEN = 100, A_SCAN, A_TAPPREV, A_TAPNEXT, A_MENU, A_TOC, A_TOCGO, A_FSIZE, A_FFAM, A_FSPC, A_FMAR, A_GOTO, A_CLOSE, A_READ, A_FJUST, A_FHYPH };

/* ═════════════ Descompressió (deflate, basada en l'algorisme de «puff») ═════════════ */
typedef struct { const unsigned char *in; size_t inlen, incnt; unsigned bitbuf; int bitcnt, eof; unsigned char *out; size_t outlen, outcnt; } Inf;
typedef struct { short count[16]; short symbol[288]; } Huff;
static int inf_bits(Inf *s, int need) {
  unsigned long val = s->bitbuf;
  while (s->bitcnt < need) {
    if (s->incnt >= s->inlen) { s->eof = 1; return 0; }
    val |= (unsigned long)s->in[s->incnt++] << s->bitcnt;
    s->bitcnt += 8;
  }
  s->bitbuf = (unsigned)(val >> need);
  s->bitcnt -= need;
  return (int)(val & ((1UL << need) - 1));
}
static int inf_decode(Inf *s, const Huff *h) {
  int len, code = 0, first = 0, count, index = 0;
  for (len = 1; len <= 15; len++) {
    code |= inf_bits(s, 1);
    if (s->eof) return -1;
    count = h->count[len];
    if (code - count < first) return h->symbol[index + (code - first)];
    index += count; first += count; first <<= 1; code <<= 1;
  }
  return -1;
}
static int inf_construct(Huff *h, const short *length, int n) {
  int symbol, len, left; short offs[16];
  for (len = 0; len <= 15; len++) h->count[len] = 0;
  for (symbol = 0; symbol < n; symbol++) h->count[length[symbol]]++;
  if (h->count[0] == n) return 0;
  left = 1;
  for (len = 1; len <= 15; len++) { left <<= 1; left -= h->count[len]; if (left < 0) return left; }
  offs[1] = 0;
  for (len = 1; len < 15; len++) offs[len + 1] = (short)(offs[len] + h->count[len]);
  for (symbol = 0; symbol < n; symbol++) if (length[symbol] != 0) h->symbol[offs[length[symbol]]++] = (short)symbol;
  return left;
}
static int inf_codes(Inf *s, const Huff *lc, const Huff *dc) {
  static const short lens[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
  static const short lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
  static const short dists[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
  static const short dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
  int symbol, len; size_t dist;
  do {
    symbol = inf_decode(s, lc);
    if (symbol < 0) return -1;
    if (symbol < 256) {
      if (s->outcnt >= s->outlen) return 1;
      s->out[s->outcnt++] = (unsigned char)symbol;
    } else if (symbol > 256) {
      symbol -= 257;
      if (symbol >= 29) return -10;
      len = lens[symbol] + inf_bits(s, lext[symbol]);
      symbol = inf_decode(s, dc);
      if (symbol < 0 || symbol >= 30) return -10;
      dist = (size_t)(dists[symbol] + inf_bits(s, dext[symbol]));
      if (s->eof) return -1;
      if (dist > s->outcnt) return -11;
      if (s->outcnt + (size_t)len > s->outlen) return 1;
      while (len--) { s->out[s->outcnt] = s->out[s->outcnt - dist]; s->outcnt++; }
    }
  } while (symbol != 256);
  return 0;
}
static int inf_fixed(Inf *s) {
  static Huff lc, dc; static int ready;
  if (!ready) {
    short l[288]; int i;
    for (i = 0; i < 144; i++) l[i] = 8;
    for (; i < 256; i++) l[i] = 9;
    for (; i < 280; i++) l[i] = 7;
    for (; i < 288; i++) l[i] = 8;
    inf_construct(&lc, l, 288);
    for (i = 0; i < 30; i++) l[i] = 5;
    inf_construct(&dc, l, 30);
    ready = 1;
  }
  return inf_codes(s, &lc, &dc);
}
static int inf_dynamic(Inf *s) {
  static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
  int nlen, ndist, ncode, index, err; short lengths[320]; Huff lc, dc;
  nlen = inf_bits(s, 5) + 257; ndist = inf_bits(s, 5) + 1; ncode = inf_bits(s, 4) + 4;
  if (s->eof || nlen > 286 || ndist > 30) return -3;
  for (index = 0; index < ncode; index++) lengths[order[index]] = (short)inf_bits(s, 3);
  for (; index < 19; index++) lengths[order[index]] = 0;
  if (s->eof || inf_construct(&lc, lengths, 19) != 0) return -4;
  index = 0;
  while (index < nlen + ndist) {
    int symbol = inf_decode(s, &lc);
    if (symbol < 0) return -1;
    if (symbol < 16) lengths[index++] = (short)symbol;
    else {
      int len = 0;
      if (symbol == 16) { if (index == 0) return -5; len = lengths[index - 1]; symbol = 3 + inf_bits(s, 2); }
      else if (symbol == 17) symbol = 3 + inf_bits(s, 3);
      else symbol = 11 + inf_bits(s, 7);
      if (s->eof || index + symbol > nlen + ndist) return -6;
      while (symbol--) lengths[index++] = (short)len;
    }
  }
  if (lengths[256] == 0) return -9;
  err = inf_construct(&lc, lengths, nlen);
  if (err && (err < 0 || nlen != lc.count[0] + lc.count[1])) return -7;
  err = inf_construct(&dc, lengths + nlen, ndist);
  if (err && (err < 0 || ndist != dc.count[0] + dc.count[1])) return -8;
  return inf_codes(s, &lc, &dc);
}
/* 0 = correcte; *outn rep la mida real */
static int inflate_raw(const unsigned char *in, size_t inlen, unsigned char *out, size_t outlen, size_t *outn) {
  Inf s; int last, type, err = 0;
  memset(&s, 0, sizeof(s));
  s.in = in; s.inlen = inlen; s.out = out; s.outlen = outlen;
  do {
    last = inf_bits(&s, 1); type = inf_bits(&s, 2);
    if (s.eof) { err = -1; break; }
    if (type == 0) {
      unsigned len, nlen;
      s.bitbuf = 0; s.bitcnt = 0;
      if (s.incnt + 4 > s.inlen) { err = 2; break; }
      len = s.in[s.incnt] | ((unsigned)s.in[s.incnt + 1] << 8); nlen = s.in[s.incnt + 2] | ((unsigned)s.in[s.incnt + 3] << 8);
      s.incnt += 4;
      if ((len ^ 0xffffu) != nlen) { err = -2; break; }
      if (s.incnt + len > s.inlen) { err = 2; break; }
      if (s.outcnt + len > s.outlen) { err = 1; break; }
      memcpy(s.out + s.outcnt, s.in + s.incnt, len); s.incnt += len; s.outcnt += len;
    } else if (type == 1) err = inf_fixed(&s);
    else if (type == 2) err = inf_dynamic(&s);
    else err = -1;
    if (err) break;
  } while (!last);
  *outn = s.outcnt;
  return err;
}

/* ═════════════ ZIP ═════════════ */
typedef struct { char *name; unsigned method; unsigned long csize, usize, lho; } ZEnt;
typedef struct { FILE *f; ZEnt *e; int n; long size; } Zip;
static unsigned u16(const unsigned char *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }
static unsigned long u32(const unsigned char *p) { return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24); }
static void zip_close(Zip *z) {
  int i;
  if (z->e) for (i = 0; i < z->n; i++) free(z->e[i].name);
  free(z->e);
  if (z->f) fclose(z->f);
  memset(z, 0, sizeof(*z));
}
static int zip_open(Zip *z, const char *path) {
  unsigned char tail[65600], *cd = NULL; long tn, i, cdoff, cdsize; int n, k; size_t pos;
  memset(z, 0, sizeof(*z));
  z->f = fopen(path, "rb");
  if (!z->f) return -1;
  fseek(z->f, 0, SEEK_END); z->size = ftell(z->f);
  if (z->size < 22) goto bad;
  tn = z->size < (long)sizeof(tail) ? z->size : (long)sizeof(tail);
  fseek(z->f, z->size - tn, SEEK_SET);
  if (fread(tail, 1, (size_t)tn, z->f) != (size_t)tn) goto bad;
  for (i = tn - 22; i >= 0; i--) if (tail[i] == 'P' && tail[i + 1] == 'K' && tail[i + 2] == 5 && tail[i + 3] == 6) break;
  if (i < 0) goto bad;
  n = (int)u16(tail + i + 10); cdsize = (long)u32(tail + i + 12); cdoff = (long)u32(tail + i + 16);
  if (n <= 0 || n > 20000 || cdsize <= 0 || cdsize > 8 * 1024 * 1024 || cdoff < 0 || cdoff + cdsize > z->size) goto bad;
  cd = (unsigned char *)malloc((size_t)cdsize);
  z->e = (ZEnt *)calloc((size_t)n, sizeof(ZEnt));
  if (!cd || !z->e) goto bad;
  fseek(z->f, cdoff, SEEK_SET);
  if (fread(cd, 1, (size_t)cdsize, z->f) != (size_t)cdsize) goto bad;
  pos = 0;
  for (k = 0; k < n; k++) {
    unsigned nl, el, cl; ZEnt *e = &z->e[z->n];
    if (pos + 46 > (size_t)cdsize || memcmp(cd + pos, "PK\1\2", 4)) break;
    nl = u16(cd + pos + 28); el = u16(cd + pos + 30); cl = u16(cd + pos + 32);
    if (pos + 46 + nl + el + cl > (size_t)cdsize) break;
    e->method = u16(cd + pos + 10); e->csize = u32(cd + pos + 20); e->usize = u32(cd + pos + 24); e->lho = u32(cd + pos + 42);
    e->name = (char *)malloc(nl + 1);
    if (!e->name) break;
    memcpy(e->name, cd + pos + 46, nl); e->name[nl] = 0;
    if (e->csize != 0xffffffffUL && e->usize != 0xffffffffUL && e->lho < (unsigned long)z->size) z->n++; else free(e->name);
    pos += 46 + nl + el + cl;
  }
  free(cd);
  if (z->n == 0) { zip_close(z); return -1; }
  return 0;
bad:
  free(cd); zip_close(z);
  return -1;
}
static int zip_find(const Zip *z, const char *name) {
  int i;
  for (i = 0; i < z->n; i++) if (!strcmp(z->e[i].name, name)) return i;
  for (i = 0; i < z->n; i++) if (!strcasecmp(z->e[i].name, name)) return i;
  return -1;
}
#define ZMAX (6 * 1024 * 1024)
static char *zip_read(Zip *z, int idx, size_t *len) {
  const ZEnt *e; unsigned char h[30], *cb = NULL; char *out = NULL; size_t n = 0;
  if (idx < 0 || idx >= z->n) return NULL;
  e = &z->e[idx];
  if (e->usize > ZMAX || e->csize > (unsigned long)z->size || (e->method != 0 && e->method != 8)) return NULL;
  if (fseek(z->f, (long)e->lho, SEEK_SET) != 0 || fread(h, 1, 30, z->f) != 30 || memcmp(h, "PK\3\4", 4)) return NULL;
  if (fseek(z->f, (long)(e->lho + 30 + u16(h + 26) + u16(h + 28)), SEEK_SET) != 0) return NULL;
  cb = (unsigned char *)malloc(e->csize + 1);
  out = (char *)malloc(e->usize + 1);
  if (!cb || !out || fread(cb, 1, e->csize, z->f) != e->csize) { free(cb); free(out); return NULL; }
  if (e->method == 0) { n = e->csize < e->usize ? e->csize : e->usize; memcpy(out, cb, n); }
  else if (inflate_raw(cb, e->csize, (unsigned char *)out, e->usize, &n) != 0) { free(cb); free(out); return NULL; }
  free(cb);
  out[n] = 0;
  if (len) *len = n;
  return out;
}

/* ═════════════ XML / HTML: ajudes ═════════════ */
typedef struct { char *p; size_t n, cap; } Buf;
#define BUFMAX (6 * 1024 * 1024)
static int buf_put(Buf *b, const char *s, size_t k) {
  if (b->n + k + 1 > b->cap) {
    size_t c = b->cap ? b->cap : 4096; char *np;
    if (b->n + k > BUFMAX) return -1;
    while (c < b->n + k + 1) c *= 2;
    np = (char *)realloc(b->p, c);
    if (!np) return -1;
    b->p = np; b->cap = c;
  }
  memcpy(b->p + b->n, s, k); b->n += k; b->p[b->n] = 0;
  return 0;
}
static int ieq(const char *a, const char *b, size_t n) { size_t i; for (i = 0; i < n; i++) if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0; return 1; }
/* següent etiqueta: retorna el '<' i deixa a *te el '>'. Els comentaris i els CDATA es retornen com a «etiquetes»
   (lt[1] == '!') perquè el text d'abans no els inclogui; un '<' que no obre res (p. ex. «a < b») és text. */
static const char *tag_next(const char *p, const char *end, const char **te) {
  while (p < end) {
    const char *lt = (const char *)memchr(p, '<', (size_t)(end - p)), *q; char qc = 0;
    if (!lt) return NULL;
    if (end - lt >= 4 && !memcmp(lt, "<!--", 4)) { q = strstr(lt + 4, "-->"); if (!q || q >= end) return NULL; *te = q + 2; return lt; }
    if (end - lt >= 9 && !memcmp(lt, "<![CDATA[", 9)) { q = strstr(lt + 9, "]]>"); if (!q || q >= end) return NULL; *te = q + 2; return lt; }
    if (lt + 1 >= end || !(isalpha((unsigned char)lt[1]) || lt[1] == '/' || lt[1] == '!' || lt[1] == '?')) { p = lt + 1; continue; }
    q = lt + 1;
    while (q < end && (*q != '>' || qc)) { if (qc) { if (*q == qc) qc = 0; } else if (*q == '"' || *q == '\'') qc = *q; q++; }
    if (q >= end) { q = (const char *)memchr(lt, '>', (size_t)(end - lt)); if (!q) return NULL; }
    *te = q;
    return lt;
  }
  return NULL;
}
/* nom local de l'etiqueta, en minúscules, sense prefix; retorna 1 si és de tancament */
static int tag_name(const char *lt, const char *te, char *out, int max) {
  const char *p = lt + 1; int closing = 0, n = 0;
  if (p < te && *p == '/') { closing = 1; p++; }
  while (p < te && !isspace((unsigned char)*p) && *p != '/' && *p != '>') {
    if (*p == ':') n = 0; else if (n < max - 1) out[n++] = (char)tolower((unsigned char)*p);
    p++;
  }
  out[n] = 0;
  return closing;
}
static int attr_get(const char *lt, const char *te, const char *name, char *out, int max) {
  const char *p = lt + 1; size_t nl = strlen(name);
  while (p < te && !isspace((unsigned char)*p)) p++;
  while (p < te) {
    const char *ns, *ne, *vs, *ve; char qc;
    while (p < te && (isspace((unsigned char)*p) || *p == '/')) p++;
    ns = p;
    while (p < te && *p != '=' && !isspace((unsigned char)*p) && *p != '/') p++;
    ne = p;
    if (ne == ns) { p++; continue; }
    while (p < te && isspace((unsigned char)*p)) p++;
    if (p >= te || *p != '=') continue;
    p++;
    while (p < te && isspace((unsigned char)*p)) p++;
    if (p < te && (*p == '"' || *p == '\'')) { qc = *p++; vs = p; while (p < te && *p != qc) p++; ve = p; if (p < te) p++; }
    else { vs = p; while (p < te && !isspace((unsigned char)*p)) p++; ve = p; }
    if ((size_t)(ne - ns) == nl && ieq(ns, name, nl)) {
      size_t k = (size_t)(ve - vs);
      if (k > (size_t)max - 1) k = (size_t)max - 1;
      memcpy(out, vs, k); out[k] = 0;
      return 1;
    }
  }
  out[0] = 0;
  return 0;
}
static void u8put(Buf *b, unsigned c) {
  char t[4]; int n = 0;
  if (c < 0x80) t[n++] = (char)c;
  else if (c < 0x800) { t[n++] = (char)(0xC0 | (c >> 6)); t[n++] = (char)(0x80 | (c & 63)); }
  else if (c < 0x10000) { t[n++] = (char)(0xE0 | (c >> 12)); t[n++] = (char)(0x80 | ((c >> 6) & 63)); t[n++] = (char)(0x80 | (c & 63)); }
  else { t[n++] = (char)(0xF0 | (c >> 18)); t[n++] = (char)(0x80 | ((c >> 12) & 63)); t[n++] = (char)(0x80 | ((c >> 6) & 63)); t[n++] = (char)(0x80 | (c & 63)); }
  buf_put(b, t, (size_t)n);
}
/* entitat a s ('&'); retorna els bytes consumits (0 si no és una entitat) i el caràcter a *cp */
static int ent_decode(const char *s, const char *end, unsigned *cp) {
  static const struct { const char *n; unsigned c; } NM[] = {
    { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' }, { "nbsp", ' ' }, { "hellip", 0x2026 }, { "mdash", 0x2014 },
    { "ndash", 0x2013 }, { "lsquo", 0x2018 }, { "rsquo", 0x2019 }, { "ldquo", 0x201C }, { "rdquo", 0x201D }, { "laquo", 0xAB }, { "raquo", 0xBB },
    { "middot", 0xB7 }, { "bull", 0x2022 }, { "copy", 0xA9 }, { "deg", 0xB0 }, { "euro", 0x20AC }, { "shy", 0 }, { "agrave", 0xE0 }, { "aacute", 0xE1 },
    { "egrave", 0xE8 }, { "eacute", 0xE9 }, { "iacute", 0xED }, { "iuml", 0xEF }, { "ograve", 0xF2 }, { "oacute", 0xF3 }, { "uacute", 0xFA },
    { "uuml", 0xFC }, { "ccedil", 0xE7 }, { "ntilde", 0xF1 }, { "Agrave", 0xC0 }, { "Aacute", 0xC1 }, { "Egrave", 0xC8 }, { "Eacute", 0xC9 },
    { "Iacute", 0xCD }, { "Ograve", 0xD2 }, { "Oacute", 0xD3 }, { "Uacute", 0xDA }, { "Ccedil", 0xC7 } };
  const char *sc; size_t i, n;
  if (s[0] != '&') return 0;
  sc = (const char *)memchr(s, ';', (size_t)(end - s > 12 ? 12 : end - s));
  if (!sc) return 0;
  if (s[1] == '#') {
    unsigned long v = (s[2] == 'x' || s[2] == 'X') ? strtoul(s + 3, NULL, 16) : strtoul(s + 2, NULL, 10);
    if (v > 0x10FFFF) return 0;
    *cp = (unsigned)v;
    return (int)(sc - s) + 1;
  }
  n = (size_t)(sc - s - 1);
  for (i = 0; i < sizeof(NM) / sizeof(NM[0]); i++) if (strlen(NM[i].n) == n && !memcmp(NM[i].n, s + 1, n)) { *cp = NM[i].c; return (int)(sc - s) + 1; }
  return 0;
}
/* text sense etiquetes ni entitats, amb els espais compactats */
static void plain_text(const char *s, const char *e, char *out, int max) {
  Buf b; int sp = 1; memset(&b, 0, sizeof(b));
  while (s < e) {
    if (*s == '<') { const char *te, *lt = tag_next(s, e, &te); if (!lt) break; s = te + 1; if (!sp) { buf_put(&b, " ", 1); sp = 1; } continue; }
    if (*s == '&') { unsigned c; int k = ent_decode(s, e, &c); if (k) { if (c == ' ' || c == '\n' || c == '\t' || c == 0xA0) { if (!sp) { buf_put(&b, " ", 1); sp = 1; } } else if (c >= 32) { u8put(&b, c); sp = 0; } s += k; continue; } }
    if (isspace((unsigned char)*s)) { if (!sp) { buf_put(&b, " ", 1); sp = 1; } s++; continue; }
    if ((unsigned char)*s < 32) { s++; continue; }
    buf_put(&b, s, 1); sp = 0; s++;
  }
  if (b.p) {
    size_t k;
    while (b.n > 0 && b.p[b.n - 1] == ' ') b.p[--b.n] = 0;
    snprintf(out, (size_t)max, "%s", b.p);
    k = strlen(out);
    if (b.n >= (size_t)max) {            /* retallat: no deixis mig caràcter UTF-8 */
      while (k > 0 && ((unsigned char)out[k - 1] & 0xC0) == 0x80) k--;
      if (k > 0 && (unsigned char)out[k - 1] >= 0xC0) k--;
      out[k] = 0;
    }
  } else out[0] = 0;
  free(b.p);
}
static void pct_decode(char *s) {
  char *w = s, *r = s;
  while (*r) {
    if (*r == '%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) { char h[3]; h[0] = r[1]; h[1] = r[2]; h[2] = 0; *w++ = (char)strtol(h, NULL, 16); r += 3; }
    else *w++ = *r++;
  }
  *w = 0;
}
static void norm_path(char *p) {
  char *seg[64], tmp[700]; int ns = 0, i; char *s = p, *q; size_t o = 0;
  for (;;) {
    q = strchr(s, '/');
    if (q) *q = 0;
    if (!strcmp(s, "..")) { if (ns > 0) ns--; }
    else if (s[0] && strcmp(s, ".")) { if (ns < 64) seg[ns++] = s; }
    if (!q) break;
    s = q + 1;
  }
  for (i = 0; i < ns; i++) {
    size_t k = strlen(seg[i]);
    if (o + k + 2 >= sizeof(tmp)) break;
    if (i) tmp[o++] = '/';
    memcpy(tmp + o, seg[i], k); o += k;
  }
  tmp[o] = 0;
  strcpy(p, tmp);
}
/* resol href respecte a la carpeta base (amb «/» final); treu el fragment (si frag != NULL el desa) */
static void path_join(const char *base, const char *href, char *out, int max, char *frag, int fmax) {
  char t[700]; char *h;
  snprintf(t, sizeof(t), "%s", href);
  h = strchr(t, '#');
  if (h) { if (frag) snprintf(frag, (size_t)fmax, "%s", h + 1); *h = 0; } else if (frag) frag[0] = 0;
  h = strchr(t, '?'); if (h) *h = 0;
  pct_decode(t);
  if (t[0] == '/') snprintf(out, (size_t)max, "%s", t + 1);
  else snprintf(out, (size_t)max, "%s%s", base, t);
  norm_path(out);
  if (frag) pct_decode(frag);
}

/* ═════════════ Llibre obert ═════════════ */
typedef struct { char *path; unsigned long usize; double start; } Chap;
typedef struct { char *title; int chap, lvl; char frag[48]; } Toc;
typedef struct {
  char path[300], title[120], author[80]; long size, mtime; int nch; long total;
  int chap; long off; int pm; time_t last;
  char xp[400];                 /* posició al motor CREngine (XPointer) */
} Book;
static Zip Z; static int zopen;
static Chap *chs; static int nch; static double totalBytes;
static Toc *toc; static int ntoc;
static char opfDir[300];

static void bk_close(void) {
  int i;
  for (i = 0; i < nch; i++) free(chs[i].path);
  free(chs); chs = NULL; nch = 0;
  for (i = 0; i < ntoc; i++) free(toc[i].title);
  free(toc); toc = NULL; ntoc = 0;
  if (zopen) { zip_close(&Z); zopen = 0; }
}
typedef struct { char *id, *path; char type[48]; int nav; } Item;
static void items_free(Item *it, int n) { int i; for (i = 0; i < n; i++) { free(it[i].id); free(it[i].path); } free(it); }
static int item_find(const Item *it, int n, const char *id) { int i; for (i = 0; i < n; i++) if (!strcmp(it[i].id, id)) return i; return -1; }
static void toc_add(const char *label, int chap, int lvl, const char *frag) {
  Toc *nt;
  if (ntoc >= 2000 || !label[0]) return;
  nt = (Toc *)realloc(toc, sizeof(Toc) * (size_t)(ntoc + 1));
  if (!nt) return;
  toc = nt;
  toc[ntoc].title = strdup(label); toc[ntoc].chap = chap; toc[ntoc].lvl = lvl > 4 ? 4 : lvl; snprintf(toc[ntoc].frag, sizeof(toc[ntoc].frag), "%s", frag);
  if (toc[ntoc].title) ntoc++;
}
static int chap_by_path(const char *path) { int i; for (i = 0; i < nch; i++) if (!strcmp(chs[i].path, path)) return i; return -1; }
static void toc_from_nav(const char *doc, size_t n, const char *base) {
  const char *p = doc, *end = doc + n, *lt, *te; char nm[24], href[500], frag[48], full[700], label[160]; int depth = 0, innav = 0, seen = 0;
  while ((lt = tag_next(p, end, &te)) != NULL) {
    int cl = tag_name(lt, te, nm, sizeof(nm));
    p = te + 1;
    if (!strcmp(nm, "nav")) {
      if (cl) { if (innav) break; }
      else if (!seen) { char ty[80]; attr_get(lt, te, "type", ty, sizeof(ty)); if (strstr(ty, "toc") || !ty[0]) { innav = 1; seen = 1; depth = 0; } }
      continue;
    }
    if (!innav) continue;
    if (!strcmp(nm, "ol") || !strcmp(nm, "ul")) { depth += cl ? -1 : 1; if (depth < 0) depth = 0; continue; }
    if (!cl && !strcmp(nm, "a")) {
      const char *tb = te + 1, *t2, *l2, *q = tb; int ci;
      attr_get(lt, te, "href", href, sizeof(href));
      while ((l2 = tag_next(q, end, &t2)) != NULL) { char n2[24]; int c2 = tag_name(l2, t2, n2, sizeof(n2)); q = t2 + 1; if (c2 && !strcmp(n2, "a")) break; }
      if (!l2) break;
      plain_text(tb, l2, label, sizeof(label));
      if (href[0]) { path_join(base, href, full, sizeof(full), frag, sizeof(frag)); ci = chap_by_path(full); if (ci >= 0) toc_add(label, ci, depth > 0 ? depth - 1 : 0, frag); }
      p = q;
    }
  }
}
static void toc_from_ncx(const char *doc, size_t n, const char *base) {
  const char *p = doc, *end = doc + n, *lt, *te; char nm[24], src[500], frag[48], full[700], label[160]; int depth = 0, inlabel = 0; label[0] = 0;
  while ((lt = tag_next(p, end, &te)) != NULL) {
    int cl = tag_name(lt, te, nm, sizeof(nm));
    p = te + 1;
    if (!strcmp(nm, "navpoint")) { depth += cl ? -1 : 1; if (depth < 0) depth = 0; if (!cl) label[0] = 0; }
    else if (!strcmp(nm, "navlabel")) inlabel = !cl;
    else if (!strcmp(nm, "text") && !cl && inlabel && !label[0]) { const char *l2, *t2; l2 = tag_next(p, end, &t2); if (l2) plain_text(p, l2, label, sizeof(label)); }
    else if (!strcmp(nm, "content") && !cl) {
      if (attr_get(lt, te, "src", src, sizeof(src))) { int ci; path_join(base, src, full, sizeof(full), frag, sizeof(frag)); ci = chap_by_path(full); if (ci >= 0 && label[0]) toc_add(label, ci, depth > 0 ? depth - 1 : 0, frag); }
      label[0] = 0;
    }
  }
}
/* obre el fitxer, llegeix l'OPF (metadades, espinada, continguts). Si b != NULL en completa títol i autor. */
static int bk_load(Book *b, int withToc) {
  size_t n; int i, ci; char *cont, *opf = NULL; char path[700], base[300], nm[24], a1[300], a2[120], navPath[700] = "", ncxPath[700] = "", tocId[80] = "";
  const char *p, *end, *lt, *te; Item *items = NULL; int nitems = 0, itemCap = 0, ok = -1, inmeta = 0, gotT = 0, gotA = 0;
  bk_close();
  if (zip_open(&Z, b->path) != 0) return -1;
  zopen = 1;
  i = zip_find(&Z, "META-INF/container.xml"); path[0] = 0;
  cont = i >= 0 ? zip_read(&Z, i, &n) : NULL;
  if (cont) {
    p = cont; end = cont + n;
    while ((lt = tag_next(p, end, &te)) != NULL) { tag_name(lt, te, nm, sizeof(nm)); p = te + 1; if (!strcmp(nm, "rootfile") && attr_get(lt, te, "full-path", a1, sizeof(a1))) { snprintf(path, sizeof(path), "%s", a1); break; } }
    free(cont);
  }
  if (!path[0]) for (i = 0; i < Z.n; i++) { size_t k = strlen(Z.e[i].name); if (k > 4 && !strcasecmp(Z.e[i].name + k - 4, ".opf")) { snprintf(path, sizeof(path), "%s", Z.e[i].name); break; } }
  if (!path[0]) goto done;
  pct_decode(path); norm_path(path);
  { char *sl = strrchr(path, '/'); if (sl) { size_t k = (size_t)(sl - path) + 1; if (k >= sizeof(opfDir)) k = sizeof(opfDir) - 1; memcpy(opfDir, path, k); opfDir[k] = 0; } else opfDir[0] = 0; }
  snprintf(base, sizeof(base), "%s", opfDir);
  i = zip_find(&Z, path);
  opf = i >= 0 ? zip_read(&Z, i, &n) : NULL;
  if (!opf) goto done;
  p = opf; end = opf + n;
  while ((lt = tag_next(p, end, &te)) != NULL) {
    int cl = tag_name(lt, te, nm, sizeof(nm));
    p = te + 1;
    if (!strcmp(nm, "metadata")) inmeta = !cl;
    else if (inmeta && !cl && !strcmp(nm, "title") && !gotT) { const char *l2, *t2; l2 = tag_next(p, end, &t2); if (l2) { plain_text(p, l2, b->title, sizeof(b->title)); gotT = b->title[0] != 0; } }
    else if (inmeta && !cl && !strcmp(nm, "creator") && !gotA) { const char *l2, *t2; l2 = tag_next(p, end, &t2); if (l2) { plain_text(p, l2, b->author, sizeof(b->author)); gotA = b->author[0] != 0; } }
    else if (!cl && !strcmp(nm, "item")) {
      char id[120], href[400], props[120];
      if (nitems >= 4000) continue;
      if (!attr_get(lt, te, "id", id, sizeof(id)) || !attr_get(lt, te, "href", href, sizeof(href))) continue;
      if (nitems >= itemCap) { int c = itemCap ? itemCap * 2 : 64; Item *ni = (Item *)realloc(items, sizeof(Item) * (size_t)c); if (!ni) continue; items = ni; itemCap = c; }
      items[nitems].id = strdup(id);
      path_join(base, href, a1, sizeof(a1), NULL, 0);
      items[nitems].path = strdup(a1);
      attr_get(lt, te, "media-type", items[nitems].type, sizeof(items[nitems].type));
      attr_get(lt, te, "properties", props, sizeof(props));
      items[nitems].nav = strstr(props, "nav") != NULL;
      if (items[nitems].id && items[nitems].path) {
        if (items[nitems].nav && !navPath[0]) snprintf(navPath, sizeof(navPath), "%s", a1);
        if (!strcmp(items[nitems].type, "application/x-dtbncx+xml") && !ncxPath[0]) snprintf(ncxPath, sizeof(ncxPath), "%s", a1);
        nitems++;
      }
    }
    else if (!cl && !strcmp(nm, "spine")) attr_get(lt, te, "toc", tocId, sizeof(tocId));
  }
  /* espinada */
  p = opf;
  while ((lt = tag_next(p, end, &te)) != NULL) {
    int cl = tag_name(lt, te, nm, sizeof(nm));
    p = te + 1;
    if (!cl && !strcmp(nm, "itemref") && attr_get(lt, te, "idref", a2, sizeof(a2))) {
      int k = item_find(items, nitems, a2), zi; Chap *nc;
      if (k < 0) continue;
      zi = zip_find(&Z, items[k].path);
      if (zi < 0 || nch >= 3000) continue;
      nc = (Chap *)realloc(chs, sizeof(Chap) * (size_t)(nch + 1));
      if (!nc) continue;
      chs = nc; chs[nch].path = strdup(items[k].path); chs[nch].usize = Z.e[zi].usize; chs[nch].start = 0;
      if (chs[nch].path) nch++;
    }
  }
  if (nch == 0) goto done;
  totalBytes = 0;
  for (ci = 0; ci < nch; ci++) { chs[ci].start = totalBytes; totalBytes += (double)(chs[ci].usize ? chs[ci].usize : 1); }
  b->nch = nch; b->total = (long)(totalBytes < 2e9 ? totalBytes : 2e9);
  ok = 0;
  if (withToc) {
    char *doc = NULL; size_t dn = 0; int zi;
    if (navPath[0]) { zi = zip_find(&Z, navPath); if (zi >= 0 && (doc = zip_read(&Z, zi, &dn)) != NULL) { char nb[700]; char *sl; snprintf(nb, sizeof(nb), "%s", navPath); sl = strrchr(nb, '/'); if (sl) sl[1] = 0; else nb[0] = 0; toc_from_nav(doc, dn, nb); free(doc); } }
    if (!ntoc) {
      if (tocId[0]) { int k = item_find(items, nitems, tocId); if (k >= 0) snprintf(ncxPath, sizeof(ncxPath), "%s", items[k].path); }
      if (ncxPath[0]) { zi = zip_find(&Z, ncxPath); if (zi >= 0 && (doc = zip_read(&Z, zi, &dn)) != NULL) { char nb[700]; char *sl; snprintf(nb, sizeof(nb), "%s", ncxPath); sl = strrchr(nb, '/'); if (sl) sl[1] = 0; else nb[0] = 0; toc_from_ncx(doc, dn, nb); free(doc); } }
    }
    if (!ntoc) for (ci = 0; ci < nch; ci++) { char t[40]; snprintf(t, sizeof(t), "Secció %d", ci + 1); toc_add(t, ci, 0, ""); }
  }
done:
  free(opf); items_free(items, nitems);
  if (ok != 0) bk_close();
  return ok;
}

/* ═════════════ Motor de pàgines: CREngine ═════════════
   La composició, la partició de mots i la paginació les fa el CREngine (lector/cre.cpp), el motor del KOReader i del
   Kobalt. Els diccionaris i el CSS van dins del paquet (lector/data.S) i s'escriuen a la memòria la primera vegada. */
#define LDIR FLASHDIR "/system/escriptori/lector"
extern const unsigned char lec_css[], lec_css_end[];
#define HYPHS X(Catalan) X(Spanish) X(English_US) X(English_GB) X(French) X(Italian) X(Portuguese) X(Occitan) X(Basque) X(Galician)
#define X(n) extern const unsigned char lec_h_##n[], lec_h_##n##_end[];
HYPHS
#undef X
static int creOk = -1;             /* -1 sense provar, 0 a punt, 1 no ha funcionat */
static void put_file(const char *path, const unsigned char *a, const unsigned char *b) {
  struct stat st; FILE *f; long n = (long)(b - a);
  if (stat(path, &st) == 0 && st.st_size == n) return;
  f = fopen(path, "wb");
  if (!f) return;
  fwrite(a, 1, (size_t)n, f); fclose(f);
}
static int cre_setup(void) {
  static const char *const FONTS[] = {
#ifdef ESC_SIM
    "/usr/share/fonts/truetype/liberation", "/usr/share/fonts/truetype/dejavu",
#else
    "/ebrmain/fonts",
#endif
    FLASHDIR "/system/fonts", NULL };
  if (creOk >= 0) return creOk;
  mkdir(FLASHDIR "/system/escriptori", 0777); mkdir(LDIR, 0777); mkdir(LDIR "/hyph", 0777); mkdir(LDIR "/cache", 0777);
  put_file(LDIR "/epub.css", lec_css, lec_css_end);
#define X(n) put_file(LDIR "/hyph/" #n ".pattern", lec_h_##n, lec_h_##n##_end);
  HYPHS
#undef X
  creOk = cre_init(LDIR, FONTS, LDIR "/cache") == 0 ? 0 : 1;
  return creOk;
}

/* ═════════════ Configuració de lectura ═════════════ */
static const int FSZ[8] = { 30, 34, 38, 42, 46, 52, 58, 66 };
static const int SPC[3] = { 100, 120, 145 };
typedef struct { int size, fam, spc, mar, just, hyph; } Cfg;
static Cfg cfg = { 3, 1, 1, 1, 1, 1 };
static Store cfgst = { 0, 0, 0, CFG "escriptori_lector_cfg.txt" };
static void cfg_load(void) {
  int a, b, c, d, e = 1, f = 1, n;
  cfgst.loaded = 0; st_load(&cfgst);
  if (cfgst.n > 0 && (n = sscanf(cfgst.it[0], "%d %d %d %d %d %d", &a, &b, &c, &d, &e, &f)) >= 4 && a >= 0 && a < 8 && b >= 0 && b < 2 && c >= 0 && c < 3 && d >= 0 && d < 3) {
    cfg.size = a; cfg.fam = b; cfg.spc = c; cfg.mar = d; cfg.just = n >= 5 ? !!e : 1; cfg.hyph = n >= 6 ? !!f : 1;
  }
}
static void cfg_save(void) {
  char t[60];
  snprintf(t, sizeof(t), "%d %d %d %d %d %d", cfg.size, cfg.fam, cfg.spc, cfg.mar, cfg.just, cfg.hyph);
  st_load(&cfgst);
  st_clear(&cfgst); st_insert(&cfgst, 0, t); st_save(&cfgst);
}
static int margin_px(void) { return U * (cfg.mar == 0 ? 2 : cfg.mar == 1 ? 4 : 6) / 4 + U / 4; }
static int page_by(void) { return SH - 3 * U; }                  /* on comença la barra inferior */
static void cur_layout(CreLayout *l) {
  memset(l, 0, sizeof(*l));
  l->w = SW; l->h = page_by() - SB;
  l->font_px = FSZ[cfg.size]; l->serif = cfg.fam; l->interline = SPC[cfg.spc];
  l->margin_x = margin_px(); l->margin_top = U; l->margin_bottom = U / 3;
  l->justify = cfg.just; l->hyphenate = cfg.hyph; l->embedded_styles = 1;
}

/* ═════════════ Biblioteca: llibres i progrés ═════════════ */
#define MAXBK 300
static Book *bk; static int nbk;
static Store libc = { 0, 0, 0, CFG "escriptori_lector_lib.txt" };
static Store prog = { 0, 0, 0, CFG "escriptori_lector.txt" };
static int rb = -1;            /* llibre obert */
static int turns;
static int split_tab(char *s, char **f, int max) {
  int k = 0; char *p = s;
  while (k < max) { f[k++] = p; p = strchr(p, '\t'); if (!p) break; *p++ = 0; }
  return k;
}
/* progrés: camí, capítol, desplaçament, per mil, última lectura i (des del motor nou) la posició XPointer */
static void prog_apply(Book *b) {
  int i, k; char *f[7], t[1400];
  b->chap = 0; b->off = 0; b->pm = 0; b->last = 0; b->xp[0] = 0;
  for (i = 0; i < prog.n; i++) {
    snprintf(t, sizeof(t), "%s", prog.it[i]);
    if ((k = split_tab(t, f, 7)) >= 5 && !strcmp(f[0], b->path)) {
      b->chap = atoi(f[1]); b->off = atol(f[2]); b->pm = atoi(f[3]); b->last = (time_t)atol(f[4]);
      if (k >= 6) snprintf(b->xp, sizeof(b->xp), "%s", f[5]);
      return;
    }
  }
}
static void prog_save(const Book *b) {
  int i, k = -1; char t[1400], pre[340];
  snprintf(t, sizeof(t), "%s\t%d\t%ld\t%d\t%ld\t%s", b->path, b->chap, b->off, b->pm, (long)b->last, b->xp);
  snprintf(pre, sizeof(pre), "%s\t", b->path);
  for (i = 0; i < prog.n; i++) if (!strncmp(prog.it[i], pre, strlen(pre))) { k = i; break; }
  if (k >= 0) st_del(&prog, k);
  st_insert(&prog, 0, t);
  st_save(&prog);
}
static int cmp_book(const void *a, const void *b) { const Book *x = (const Book *)a, *y = (const Book *)b; int c = strcasecmp(x->title, y->title); return c ? c : strcmp(x->path, y->path); }
typedef struct { char path[300]; long size, mtime; } Found;
static Found *found; static int nfound;
static void scan_dir(const char *dir, int depth) {
  DIR *d = opendir(dir); struct dirent *e;
  if (!d) return;
  while ((e = readdir(d)) != NULL && nfound < MAXBK) {
    char full[300]; struct stat st; size_t k;
    if (e->d_name[0] == '.') continue;
    if (snprintf(full, sizeof(full), "%s/%s", dir, e->d_name) >= (int)sizeof(full)) continue;
    if (stat(full, &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) { if (depth < 4 && !(depth == 0 && !strcmp(e->d_name, "system"))) scan_dir(full, depth + 1); continue; }
    k = strlen(e->d_name);
    if (k > 5 && !strcasecmp(e->d_name + k - 5, ".epub")) { snprintf(found[nfound].path, sizeof(found[nfound].path), "%s", full); found[nfound].size = (long)st.st_size; found[nfound].mtime = (long)st.st_mtime; nfound++; }
  }
  closedir(d);
}
static void book_close(void);
static void lib_scan(void) {
  int i, j; char t[900], *f[8]; Book *nb; int nn = 0;
  book_close();
  st_load(&libc); st_load(&prog);
  found = (Found *)calloc(MAXBK, sizeof(Found)); nfound = 0;
  nb = (Book *)calloc(MAXBK, sizeof(Book));
  if (!found || !nb) { free(found); free(nb); found = NULL; return; }
  scan_dir(FLASHDIR, 0);
  ShowHourglass();
  for (i = 0; i < nfound; i++) {
    Book *b = &nb[nn]; int cached = 0;
    memset(b, 0, sizeof(*b));
    snprintf(b->path, sizeof(b->path), "%s", found[i].path); b->size = found[i].size; b->mtime = found[i].mtime;
    for (j = 0; j < libc.n; j++) {
      snprintf(t, sizeof(t), "%s", libc.it[j]);
      if (split_tab(t, f, 7) == 7 && !strcmp(f[0], b->path) && atol(f[1]) == b->size && atol(f[2]) == b->mtime) {
        b->nch = atoi(f[3]); b->total = atol(f[4]); snprintf(b->title, sizeof(b->title), "%s", f[5]); snprintf(b->author, sizeof(b->author), "%s", f[6]); cached = 1; break;
      }
    }
    if (!cached) {
      if (bk_load(b, 0) != 0) { const char *nm = strrchr(b->path, '/'); snprintf(b->title, sizeof(b->title), "%s", nm ? nm + 1 : b->path); b->author[0] = 0; }
      else if (!b->title[0]) { const char *nm = strrchr(b->path, '/'); snprintf(b->title, sizeof(b->title), "%s", nm ? nm + 1 : b->path); }
      bk_close();
    }
    prog_apply(b);
    nn++;
  }
  HideHourglass();
  qsort(nb, (size_t)nn, sizeof(Book), cmp_book);
  free(bk); bk = nb; nbk = nn;
  st_clear(&libc);
  for (i = 0; i < nbk; i++) { snprintf(t, sizeof(t), "%s\t%ld\t%ld\t%d\t%ld\t%s\t%s", bk[i].path, bk[i].size, bk[i].mtime, bk[i].nch, bk[i].total, bk[i].title, bk[i].author); st_insert(&libc, libc.n, t); }
  st_save(&libc);
  free(found); found = NULL;
}

/* ═════════════ Lectura: llibre obert, posició, pàgines ═════════════ */
static ibitmap *pageBmp;
static int cur_pm(void) { return cre_is_open() ? cre_percent() / 10 : 0; }
static void save_pos(void) {
  Book *b;
  if (rb < 0 || rb >= nbk || !cre_is_open()) return;
  b = &bk[rb];
  cre_bookmark(b->xp, sizeof(b->xp));
  b->pm = cur_pm(); b->chap = 0; b->off = cre_page(); b->last = time(NULL);
  prog_save(b);
}
static void book_close(void) {
  if (cre_is_open()) { save_pos(); cre_close(); }
  rb = -1;
}
static int book_open(int i) {
  Book *b; CreLayout l;
  if (i < 0 || i >= nbk) return -1;
  if (cre_setup() != 0) return -2;
  book_close();
  b = &bk[i];
  ShowHourglass();
  cur_layout(&l); cre_layout(&l);
  if (cre_open(b->path) != 0) { HideHourglass(); return -1; }
  if ((!b->xp[0] || cre_goto_bookmark(b->xp) != 0) && b->pm > 0) cre_goto_percent(b->pm * 10);   /* progrés del motor antic */
  HideHourglass();
  rb = i;
  return 0;
}

/* ═════════════ Vistes ═════════════ */
static void progress_bar(int x, int y, int w, int h, int pm) {
  FillArea(x, y, w, h, LGRAY);
  if (pm > 0) FillArea(x, y, (int)((long)w * (pm > 1000 ? 1000 : pm) / 1000), h, BLACK);
}
static void pm_text(char *out, int sz, const Book *b) {
  if (b->last == 0 && b->pm == 0) snprintf(out, (size_t)sz, "Nou");
  else if (b->pm >= 985) snprintf(out, (size_t)sz, "Acabat");
  else snprintf(out, (size_t)sz, "%d %%", b->pm / 10);
}
static void card(int x, int y, int w, int h, int idx) {
  const Book *b = &bk[idx]; char t[40]; int sp = U / 3;
  box(x, y, w, h);
  FillArea(x, y, sp, h, BLACK);
  SetFont(fBold, BLACK);
  DrawTextRect(x + sp + U / 3, y + U / 3, w - sp - U / 3 - U / 3, h - 3 * U - U / 3, b->title, ALIGN_LEFT | VALIGN_TOP | DOTS);
  SetFont(fSmall, BLACK);
  DrawTextRect(x + sp + U / 3, y + h - 3 * U + U / 4, w - sp - 2 * U / 3, U, b->author, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  progress_bar(x + sp + U / 3, y + h - U - U / 3, w - sp - U / 3 - U / 3 - 4 * U, U / 4, b->pm);
  pm_text(t, sizeof(t), b);
  DrawTextRect(x + w - U / 3 - 4 * U, y + h - U - U / 3 - U / 2, 4 * U, U + U / 4, t, ALIGN_RIGHT | VALIGN_MIDDLE);
  add_hit(x, y, w, h, A_OPEN, idx);
}
static int last_read(void) { int i, best = -1; for (i = 0; i < nbk; i++) if (bk[i].last > 0 && (best < 0 || bk[i].last > bk[best].last)) best = i; return best; }
static void v_lib(View *v) {
  int y = TOP, bh = 2 * U, gap = U / 3, cw = (SW - U - gap) / 2, chh = 6 * U + U / 2, lr = last_read(), cap0, cap1, i, from, per, row;
  int bannerH = 6 * U, bottom = BOTTOM - bh - gap; char sub[60];
  if (!bk) lib_scan();
  snprintf(sub, sizeof(sub), nbk == 1 ? "%d llibre" : "%d llibres", nbk);
  header("Llibres", sub);
  button(U / 2, BOTTOM - bh, SW - U, bh, "Actualitza la biblioteca", 0, A_SCAN, 0);
  if (nbk == 0) {
    SetFont(fText, BLACK);
    DrawTextRect(U / 2, y, SW - U, 8 * U, "No hi ha cap llibre EPUB.\nCopia'n per USB a qualsevol carpeta del lector i prem «Actualitza la biblioteca».", ALIGN_LEFT | VALIGN_TOP);
    return;
  }
  cap1 = ((bottom - y + gap) / (chh + gap)) * 2; if (cap1 < 2) cap1 = 2;
  cap0 = lr >= 0 ? ((bottom - y - bannerH - gap + gap) / (chh + gap)) * 2 : cap1; if (cap0 < 2) cap0 = 2;
  pages = 1; if (nbk > cap0) pages += (nbk - cap0 + cap1 - 1) / cap1;
  if (v->page >= pages) v->page = pages - 1;
  if (v->page == 0) {
    from = 0; per = cap0;
    if (lr >= 0) {
      const Book *b = &bk[lr]; char t[40];
      box(U / 2, y, SW - U, bannerH); box(U / 2 + 1, y + 1, SW - U - 2, bannerH - 2);
      SetFont(fSmall, BLACK); DrawTextRect(U, y + U / 3, SW - 2 * U, U, "CONTINUA LLEGINT", ALIGN_LEFT | VALIGN_MIDDLE);
      SetFont(fBig, BLACK); DrawTextRect(U, y + U + U / 3, SW - 2 * U, 2 * U + U / 2, b->title, ALIGN_LEFT | VALIGN_TOP | DOTS);
      SetFont(fSmall, BLACK); DrawTextRect(U, y + 3 * U + U / 2, SW - 2 * U, U, b->author, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
      progress_bar(U, y + bannerH - U - U / 2, SW - 2 * U - 5 * U, U / 3, b->pm);
      pm_text(t, sizeof(t), b);
      SetFont(fBold, BLACK); DrawTextRect(SW - U - 5 * U, y + bannerH - 2 * U, 5 * U, U + U / 2, t, ALIGN_RIGHT | VALIGN_MIDDLE);
      add_hit(U / 2, y, SW - U, bannerH, A_OPEN, lr);
      y += bannerH + gap;
    }
  } else { from = cap0 + (v->page - 1) * cap1; per = cap1; }
  for (i = from, row = 0; i < nbk && i < from + per; i++, row++) {
    int cx = U / 2 + (row % 2) * (cw + gap), cy = y + (row / 2) * (chh + gap);
    card(cx, cy, cw, chh, i);
  }
}
/* La pàgina la pinta el CREngine en grisos; es dibuixa d'un sol cop (en tema fosc l'Escriptori la inverteix). */
static void draw_page(void) {
  CreLayout l; int w, h;
  cur_layout(&l); cre_layout(&l);
  w = l.w; h = l.h;
  if (!pageBmp || pageBmp->width != w || pageBmp->height != h) {
    free(pageBmp);
    pageBmp = (ibitmap *)malloc(sizeof(ibitmap) + (size_t)w * (size_t)h);
    if (!pageBmp) return;
    pageBmp->width = (unsigned short)w; pageBmp->height = (unsigned short)h; pageBmp->depth = 8; pageBmp->scanline = (unsigned short)w;
  }
  if (cre_render(pageBmp->data, w, h) == 0) DrawBitmap(0, SB, pageBmp);
}
static void v_book(View *v) {
  int by = page_by(), mar = margin_px(), pm; char info[160], title[200] = "";
  (void)v;
  if (!cre_is_open() || rb < 0) { header("Lector", "No hi ha cap llibre obert."); return; }
  draw_page();
  /* zones de tacte: tercer esquerre = enrere, la resta = endavant; la barra inferior obre el menú */
  add_hit(0, SB, SW * 3 / 10, by - SB, A_TAPPREV, 0);
  add_hit(SW * 3 / 10, SB, SW - SW * 3 / 10, by - SB, A_TAPNEXT, 0);
  pm = cre_percent();
  progress_bar(mar, by + U / 4, SW - 2 * mar, 4, pm / 10);
  if (cre_chapter_title(title, sizeof(title)) != 0 || !title[0]) snprintf(title, sizeof(title), "%s", bk[rb].title);
  snprintf(info, sizeof(info), "pàg. %d de %d  ·  %d,%d %%", cre_page() + 1, cre_pages(), pm / 100, pm / 10 % 10);
  SetFont(fSmall, BLACK);
  DrawTextRect(mar, by + U / 2, SW - 2 * mar - 9 * U, 2 * U, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  DrawTextRect(SW - mar - 9 * U, by + U / 2, 9 * U, 2 * U, info, ALIGN_RIGHT | VALIGN_MIDDLE);
  add_hit(0, by, SW, 3 * U, A_MENU, 0);
}
static void v_menu(View *v) {
  int y = TOP, bh = 2 * U, gap = U / 3, w3 = (SW - U - 2 * gap) / 3, w2 = (SW - U - gap) / 2, x0 = U / 2; char t[60];
  (void)v;
  if (rb < 0 || !cre_is_open()) { header("Lector", "No hi ha cap llibre obert."); return; }
  header(bk[rb].title, bk[rb].author);
  button(x0, y, SW - U, bh, "Torna a la lectura", 1, A_READ, 0); y += bh + 2 * gap;
  SetFont(fSmall, BLACK); DrawTextRect(x0, y, SW - U, U, "Mida de la lletra", ALIGN_LEFT | VALIGN_MIDDLE); y += U;
  button(x0, y, 4 * U, bh, "A-", 0, A_FSIZE, -1);
  box(x0 + 4 * U + gap, y, SW - U - 8 * U - 2 * gap, bh);
  snprintf(t, sizeof(t), "Mida %d de 8", cfg.size + 1); SetFont(fBold, BLACK);
  DrawTextRect(x0 + 4 * U + gap, y, SW - U - 8 * U - 2 * gap, bh, t, ALIGN_CENTER | VALIGN_MIDDLE);
  button(SW - x0 - 4 * U, y, 4 * U, bh, "A+", 0, A_FSIZE, 1); y += bh + gap;
  SetFont(fSmall, BLACK); DrawTextRect(x0, y, SW - U, U, "Tipus de lletra", ALIGN_LEFT | VALIGN_MIDDLE); y += U;
  button(x0, y, w2, bh, "Sense serifa", cfg.fam == 0, A_FFAM, 0); button(x0 + w2 + gap, y, w2, bh, "Amb serifa", cfg.fam == 1, A_FFAM, 1); y += bh + gap;
  SetFont(fSmall, BLACK); DrawTextRect(x0, y, SW - U, U, "Interlineat", ALIGN_LEFT | VALIGN_MIDDLE); y += U;
  button(x0, y, w3, bh, "Compacte", cfg.spc == 0, A_FSPC, 0); button(x0 + w3 + gap, y, w3, bh, "Normal", cfg.spc == 1, A_FSPC, 1);
  button(x0 + 2 * (w3 + gap), y, w3, bh, "Ampli", cfg.spc == 2, A_FSPC, 2); y += bh + gap;
  SetFont(fSmall, BLACK); DrawTextRect(x0, y, SW - U, U, "Marges", ALIGN_LEFT | VALIGN_MIDDLE); y += U;
  button(x0, y, w3, bh, "Estrets", cfg.mar == 0, A_FMAR, 0); button(x0 + w3 + gap, y, w3, bh, "Normals", cfg.mar == 1, A_FMAR, 1);
  button(x0 + 2 * (w3 + gap), y, w3, bh, "Amples", cfg.mar == 2, A_FMAR, 2); y += bh + gap;
  SetFont(fSmall, BLACK); DrawTextRect(x0, y, SW - U, U, "Composició", ALIGN_LEFT | VALIGN_MIDDLE); y += U;
  button(x0, y, w2, bh, "Justificat", cfg.just, A_FJUST, 0); button(x0 + w2 + gap, y, w2, bh, "Partició de mots", cfg.hyph, A_FHYPH, 0); y += bh + 2 * gap;
  button(x0, y, SW - U, bh, "Continguts", 0, A_TOC, 0); y += bh + gap;
  button(x0, y, SW - U, bh, "Anar a un percentatge", 0, A_GOTO, 0); y += bh + gap;
  button(x0, y, SW - U, bh, "Tanca el llibre", 0, A_CLOSE, 0); y += bh + gap;
  if (y + 2 * U < BOTTOM + NAVH) {
    snprintf(t, sizeof(t), "Lletra: %s · motor CREngine", cre_font_name(cfg.fam));
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x0, y + gap, SW - U, U + U / 2, t, ALIGN_LEFT | VALIGN_TOP | DOTS);
  }
}
static void v_toc(View *v) {
  int y = TOP, rh = 2 * U + U / 4, per, i, cur = -1, n = cre_toc_count(), p = cre_page(); char sub[40], t[300];
  if (rb < 0 || !cre_is_open()) { header("Continguts", ""); return; }
  for (i = 0; i < n; i++) { int lv, pg; if (cre_toc_get(i, t, sizeof(t), &lv, &pg) == 0 && pg <= p) cur = i; }
  snprintf(sub, sizeof(sub), n == 1 ? "%d entrada" : "%d entrades", n);
  header("Continguts", sub);
  if (n == 0) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 4 * U, "Aquest llibre no té índex.", ALIGN_LEFT | VALIGN_TOP); return; }
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = (n + per - 1) / per;
  if (v->a == 0) { v->a = 1; if (cur >= 0) v->page = cur / per; }
  if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    int lv = 0, pg = 0, ind; char pn[16];
    cre_toc_get(i, t, sizeof(t), &lv, &pg);
    ind = (lv > 4 ? 4 : lv) * U;
    snprintf(pn, sizeof(pn), "%d", pg + 1);
    SetFont(i == cur ? fBold : fText, BLACK);
    DrawTextRect(U / 2 + ind, y, SW - U - ind - 3 * U, rh - U / 4, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(SW - U / 2 - 3 * U, y, 3 * U, rh - U / 4, pn, ALIGN_RIGHT | VALIGN_MIDDLE);
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LGRAY);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_TOCGO, i);
    y += rh;
  }
}
static void mod_icon(int x, int y, int s) { ic_path(IC_BOOK, x, y, s, 1.8, ICON_FG, ICON_BG); }   /* icona del prototip (icons.h) */
static void mod_draw(View *v) {
  switch (v->type) {
    case V_LIB: v_lib(v); break;
    case V_BOOK: v_book(v); break;
    case V_MENU: v_menu(v); break;
    case V_TOC: v_toc(v); break;
  }
}
static void goto_fn(const char *t) {
  double pct = parse_num(t);
  if (!cre_is_open()) return;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  cre_goto_percent((int)(pct * 100));
  save_pos();
  POP();
}
static void turn(int dir) {
  if (!cre_is_open()) return;
  if (!(dir > 0 ? cre_next() : cre_prev())) return;
  save_pos();
  draw(++turns % 6 == 0);        /* cada sis pàgines, refresc complet per netejar la tinta */
}
static void relayout_draw(void) { cfg_save(); draw(0); }
static void mod_action(View *v, int a, int arg) {
  (void)v;
  switch (a) {
    case A_OPEN: {
      int r = book_open(arg);
      if (r == -2) { say("No s'han trobat fonts per compondre el llibre."); draw(1); }
      else if (r != 0) { say("No s'ha pogut obrir aquest llibre."); draw(1); }
      else push(V_BOOK, arg, 0, 0, NULL);
      break; }
    case A_SCAN: lib_scan(); v->page = 0; draw(1); break;
    case A_TAPPREV: turn(-1); break;
    case A_TAPNEXT: turn(1); break;
    case A_MENU: save_pos(); push(V_MENU, 0, 0, 0, NULL); break;
    case A_TOC: push(V_TOC, 0, 0, 0, NULL); break;
    case A_TOCGO: {
      char t[8]; int lv, pg;
      if (cre_toc_get(arg, t, sizeof(t), &lv, &pg) == 0) { cre_goto_page(pg); save_pos(); POP(); POP(); draw(1); }
      break; }
    case A_FSIZE: cfg.size += arg; if (cfg.size < 0) cfg.size = 0; if (cfg.size > 7) cfg.size = 7; relayout_draw(); break;
    case A_FFAM: cfg.fam = arg; relayout_draw(); break;
    case A_FSPC: cfg.spc = arg; relayout_draw(); break;
    case A_FMAR: cfg.mar = arg; relayout_draw(); break;
    case A_FJUST: cfg.just = !cfg.just; relayout_draw(); break;
    case A_FHYPH: cfg.hyph = !cfg.hyph; relayout_draw(); break;
    case A_GOTO: ask("Anar al percentatge del llibre (0-100)", "", goto_fn); break;
    case A_READ: POP(); draw(1); break;
    case A_CLOSE: book_close(); POP(); POP(); draw(1); break;
  }
}
static int mod_back(View *v) { if (v->type == V_BOOK) save_pos(); return 0; }
static void mod_open(View *v) { (void)v; cfg_load(); lib_scan(); }
static Module M = { ESC_API_MIN, "lector", "Lector", 0, mod_icon, mod_draw, mod_action, mod_back, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
/* Es pot obrir en un panell de la pantalla dividida (el dibuix fa servir SB, SH, TOP i BOTTOM). */
__attribute__((visibility("default"))) const int escriptori_split = 1;
