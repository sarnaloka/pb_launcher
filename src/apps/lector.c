/* Escriptori — paquet «Lector»: biblioteca i lector de llibres EPUB. La composició i les pàgines les fa el CREngine
   (el motor del KOReader i del Kobalt, GPL v2 o posterior; vegeu lector/LLEGEIX.md), i per tant aquest paquet també és GPL. */
#define ESC_MODULE
#include "../mod.h"
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include "lector/cre.h"

enum { V_LIB, V_BOOK, V_MENU, V_FONTS, V_PRESETS, V_SEARCH, V_MARKS, V_SET };   /* V_MENU: v->a = plafó (0 cap, 1 Navega, 2 Text, 3 Disposició, 4 Capítols) */
enum { A_OPEN = 100, A_SCAN };                /* la resta, A_LK_..., són a la part del menú de lectura */

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

/* ═════════════ Configuració de lectura ═════════════
   Els noms i els valors segueixen el prototip «2.4 · Lector» (pestanyes Text i Disposició). Les mides són en píxels
   de la pantalla (300 ppi): la mida de lletra es mostra en punts. */
typedef struct {
  int size;            /* px */
  int lh;              /* interlineat en % */
  int align;           /* 0 esquerra, 1 justificat, 2 del llibre */
  int hyph;            /* partició de mots */
  int fam;             /* 0 sense serifa, 1 amb serifa, 2 fonts del llibre */
  int mside, mtb;      /* marges: laterals i de dalt i baix */
  int hf;              /* capçalera i peu: 0 net, 1 capítol i pàgina, 2 complet */
  int refresh;         /* refresc total: 0 sempre, 1 cada 6, 2 cada 12, 3 mai */
  int css;             /* estils del llibre */
  char face[64];       /* font triada; buit = la de la família */
} Cfg;
static Cfg cfg = { 42, 140, 1, 1, 1, 68, 56, 1, 1, 1, "" };
static Store cfgst = { 0, 0, 0, CFG "escriptori_lector_cfg.txt" };
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static void cfg_load(void) {
  static const int OSZ[8] = { 30, 34, 38, 42, 46, 52, 58, 66 }, OSP[3] = { 100, 120, 145 }, OMR[3] = { 40, 68, 100 };
  int a, b, c, d, e, f, g, h, i, j, k, n; char face[64] = "";
  cfgst.loaded = 0; st_load(&cfgst);
  if (cfgst.n <= 0) return;
  if (!strncmp(cfgst.it[0], "v2 ", 3)) {
    n = sscanf(cfgst.it[0] + 3, "%d %d %d %d %d %d %d %d %d %d %63[^\n]", &a, &b, &c, &d, &e, &f, &g, &h, &i, &j, face);
    if (n >= 10) {
      cfg.size = clampi(a, 24, 80); cfg.lh = clampi(b, 100, 220); cfg.align = clampi(c, 0, 2); cfg.hyph = !!d; cfg.fam = clampi(e, 0, 2);
      cfg.mside = clampi(f, 0, 200); cfg.mtb = clampi(g, 0, 200); cfg.hf = clampi(h, 0, 2); cfg.refresh = clampi(i, 0, 3); cfg.css = !!j;
      snprintf(cfg.face, sizeof(cfg.face), "%s", n >= 11 ? face : "");
    }
    return;
  }
  /* format del Lector 5 i 6: mida (0-7), família, interlineat (0-2), marges (0-2), justificat, partició */
  e = 1; f = 1; (void)k;
  if ((n = sscanf(cfgst.it[0], "%d %d %d %d %d %d", &a, &b, &c, &d, &e, &f)) >= 4 && a >= 0 && a < 8 && b >= 0 && b < 2 && c >= 0 && c < 3 && d >= 0 && d < 3) {
    cfg.size = OSZ[a]; cfg.fam = b; cfg.lh = OSP[c]; cfg.mside = OMR[d]; cfg.align = n >= 5 ? !!e : 1; cfg.hyph = n >= 6 ? !!f : 1;
  }
}
static void cfg_save(void) {
  char t[160];
  snprintf(t, sizeof(t), "v2 %d %d %d %d %d %d %d %d %d %d %s", cfg.size, cfg.lh, cfg.align, cfg.hyph, cfg.fam, cfg.mside, cfg.mtb, cfg.hf, cfg.refresh, cfg.css, cfg.face);
  st_load(&cfgst);
  st_clear(&cfgst); st_insert(&cfgst, 0, t); st_save(&cfgst);
}
/* Predefinits (prototip: Escriptori, Llibresc, Modern, Del llibre) */
typedef struct { const char *name; int fam, size, lh; } Preset;
static const Preset PRESETS[4] = { { "Escriptori", 1, 42, 140 }, { "Llibresc", 1, 46, 145 }, { "Modern", 0, 40, 150 }, { "Del llibre", 2, 42, 135 } };
static int preset_cur(void) {
  int i;
  for (i = 0; i < 4; i++) if (cfg.fam == PRESETS[i].fam && cfg.size == PRESETS[i].size && cfg.lh == PRESETS[i].lh && !cfg.face[0]) return i;
  return -1;
}
/* Geometria de la pàgina (pantalla completa): la capçalera i el peu van dins dels marges de dalt i de baix */
#define K(v) ((v) * SW / 1072)
#define HEADH K(80)
#define FOOTH K(88)
static void cur_layout(CreLayout *l) {
  memset(l, 0, sizeof(*l));
  l->w = SW; l->h = SH;
  l->font_px = cfg.size; l->serif = cfg.fam != 0; l->interline = cfg.lh;
  l->margin_x = cfg.mside; l->margin_top = cfg.mtb + (cfg.hf == 2 ? HEADH : 0); l->margin_bottom = cfg.mtb + (cfg.hf >= 1 ? FOOTH : 0);
  if (l->margin_top < K(24)) l->margin_top = K(24);
  if (l->margin_bottom < K(24)) l->margin_bottom = K(24);
  l->align = cfg.align; l->justify = cfg.align == 1; l->hyphenate = cfg.hyph;
  l->embedded_styles = cfg.css; l->embedded_fonts = cfg.css && cfg.fam == 2;
  snprintf(l->face, sizeof(l->face), "%s", cfg.fam == 2 ? "" : cfg.face);
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

/* ═════════════ Fonts de la interfície per a les mostres «Aa» ═════════════
   El CREngine anomena les fonts per família («Liberation Serif»); l'InkView, pel nom del fitxer («LiberationSerif»).
   Es busca a EnumFonts() el nom sense espais (o amb «-Regular»); si no hi és, la lletra del sistema. */
static int squeeze_eq(const char *a, const char *b) {
  for (;;) {
    while (*a == ' ') a++;
    while (*b == ' ') b++;
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    if (!*a) return 1;
    a++; b++;
  }
}
typedef struct { char name[64]; int size; ifont *f; } FCache;
static FCache fcache[12]; static int fcNext;
static ifont *face_font(const char *face, int size) {
  static char **L; char t[90]; const char *found = NULL; int i; char **p;
  if (!face || !face[0]) face = "";
  for (i = 0; i < 12; i++) if (fcache[i].f && fcache[i].size == size && !strcmp(fcache[i].name, face)) return fcache[i].f;
  if (!L) L = EnumFonts();
  for (p = L; face[0] && p && *p && !found; p++) if (squeeze_eq(*p, face)) found = *p;
  snprintf(t, sizeof(t), "%s-Regular", face);
  for (p = L; face[0] && p && *p && !found; p++) if (squeeze_eq(*p, t)) found = *p;
  i = fcNext; fcNext = (fcNext + 1) % 12;
  if (fcache[i].f) CloseFont(fcache[i].f);
  fcache[i].f = OpenFont(found ? found : DEFAULTFONT, size, 1);
  if (!fcache[i].f) fcache[i].f = OpenFont(DEFAULTFONT, size, 1);
  fcache[i].size = size; snprintf(fcache[i].name, sizeof(fcache[i].name), "%s", face);
  return fcache[i].f ? fcache[i].f : fText;
}
/* família del lector: 0 sense serifa, 1 amb serifa, 2 la del llibre (es mostra amb serifa) */
static ifont *getfontf(int fam, int size) { return cre_setup() == 0 ? face_font(cre_font_name(fam != 0), size) : fText; }
static ifont *cur_font(int size) { return cfg.face[0] && cfg.fam != 2 ? face_font(cfg.face, size) : getfontf(cfg.fam, size); }
static void fonts_free(void) { int i; for (i = 0; i < 12; i++) if (fcache[i].f) { CloseFont(fcache[i].f); fcache[i].f = NULL; } }

/* ═════════════ Lectura: llibre obert, posició, marcadors ═════════════ */
static int cur_pm(void) { return cre_is_open() ? cre_percent() / 10 : 0; }
static void save_pos(void) {
  Book *b;
  if (rb < 0 || rb >= nbk || !cre_is_open()) return;
  b = &bk[rb];
  cre_bookmark(b->xp, sizeof(b->xp));
  b->pm = cur_pm(); b->chap = 0; b->off = cre_page(); b->last = time(NULL);
  prog_save(b);
}
static void thumbs_forget(void);
static void book_close(void) {
  if (cre_is_open()) { save_pos(); cre_close(); }
  thumbs_forget();
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
/* Marcadors: «camí \t posició \t títol del capítol \t hora», per a tots els llibres */
static Store marks = { 0, 0, 0, CFG "escriptori_lector_marcadors.txt" };
typedef struct { int idx, page; char xp[400], label[160]; } Mark;
static int marks_of_book(Mark *out, int max) {
  int i, n = 0; char t[1200], *f[4];
  if (rb < 0) return 0;
  st_load(&marks);
  for (i = 0; i < marks.n && n < max; i++) {
    snprintf(t, sizeof(t), "%s", marks.it[i]);
    if (split_tab(t, f, 4) >= 3 && !strcmp(f[0], bk[rb].path)) {
      out[n].idx = i; snprintf(out[n].xp, sizeof(out[n].xp), "%s", f[1]); snprintf(out[n].label, sizeof(out[n].label), "%s", f[2]);
      out[n].page = cre_bookmark_page(out[n].xp); n++;
    }
  }
  /* per ordre de pàgina */
  { int a, b2; for (a = 1; a < n; a++) { Mark m = out[a]; for (b2 = a - 1; b2 >= 0 && out[b2].page > m.page; b2--) out[b2 + 1] = out[b2]; out[b2 + 1] = m; } }
  return n;
}
static int mark_on_page(int page) {
  Mark m[64]; int i, n = marks_of_book(m, 64);
  for (i = 0; i < n; i++) if (m[i].page == page) return m[i].idx;
  return -1;
}
static void mark_toggle(void) {
  int k = mark_on_page(cre_page());
  if (k >= 0) { st_del(&marks, k); st_save(&marks); return; }
  { char xp[400], ch[160] = "", t[1300];
    cre_bookmark(xp, sizeof(xp));
    if (cre_chapter_title(ch, sizeof(ch)) != 0 || !ch[0]) snprintf(ch, sizeof(ch), "%s", bk[rb].title);
    { char *q; for (q = ch; *q; q++) if (*q == '\t' || *q == '\n') *q = ' '; }
    snprintf(t, sizeof(t), "%s\t%s\t%s\t%ld", bk[rb].path, xp, ch, (long)time(NULL));
    st_insert(&marks, marks.n, t); st_save(&marks); }
}

/* ═════════════ Dibuix: peces comunes ═════════════ */
#define LK_FG PAL(P_FG)
#define LK_PAPER PAL(P_CARD)
#define LK_GRAY PAL(P_BG)
#define LK_LINE PAL(P_LINE)
#define LK_SUB PAL(P_SUB)
static ifont *fMono;
static ifont *mono(void) { if (!fMono) fMono = OpenFont(DEFAULTFONTM, K(21), 1); return fMono ? fMono : fSmall; }
static void hline(int x, int y, int w, int t, int c) { FillArea(x, y, w, t, c); }
static void vline(int x, int y, int h, int t, int c) { FillArea(x, y, t, h, c); }
static void lk_icon(const char *d, int x, int y, int s, int fg, int bg) { ic_path(d, x, y, s, 1.8, fg, bg); }
/* majúscules per a la franja (ASCII i Latin-1 en UTF-8: «pàg.» → «PÀG.») */
static void upcase(char *s) {
  unsigned char *p = (unsigned char *)s;
  while (*p) {
    if (*p < 0x80) { *p = (unsigned char)toupper(*p); p++; }
    else if (*p == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7) { p[1] = (unsigned char)(p[1] - 0x20); p += 2; }
    else p++;
  }
}
/* La pàgina la pinta el CREngine en grisos; es guarda i només es torna a compondre quan canvia. */
static ibitmap *pageBmp; static int bmpPage = -1; static CreLayout bmpLay;
static void page_paint(void) {
  CreLayout l; int w, h;
  cur_layout(&l); cre_layout(&l);
  w = l.w; h = l.h;
  if (!pageBmp || pageBmp->width != w || pageBmp->height != h) {
    free(pageBmp); bmpPage = -1;
    pageBmp = (ibitmap *)malloc(sizeof(ibitmap) + (size_t)w * (size_t)h);
    if (!pageBmp) return;
    pageBmp->width = (unsigned short)w; pageBmp->height = (unsigned short)h; pageBmp->depth = 8; pageBmp->scanline = (unsigned short)w;
  }
  if (bmpPage != cre_page() || memcmp(&bmpLay, &l, sizeof(l))) {
    if (cre_render(pageBmp->data, w, h) != 0) return;
    bmpPage = cre_page(); bmpLay = l;
  }
  DrawBitmap(0, 0, pageBmp);
}
static void chapter_at(int page, char *out, int max) {
  int k, n = cre_toc_count(); out[0] = 0;
  for (k = 0; k < n; k++) { int lv, pg; char tt[200]; if (cre_toc_get(k, tt, sizeof(tt), &lv, &pg) == 0 && pg <= page) snprintf(out, (size_t)max, "%s", tt); }
}
static void strip_text(char *out, int max, int page) {
  char ch[200];
  chapter_at(page, ch, sizeof(ch));
  snprintf(out, (size_t)max, "pàg. %d de %d%s%s", page + 1, cre_pages(), ch[0] ? "  · " : "", ch);
  upcase(out);
}
/* Capçalera i peu de la pàgina (Disposició › Capçalera i peu) */
static void page_furniture(void) {
  char t[220], ch[200] = ""; int pm = cre_percent();
  if (cfg.hf == 2) {
    snprintf(t, sizeof(t), "%s", bk[rb].title); upcase(t);
    SetFont(fSmall, LK_SUB); DrawTextRect(cfg.mside, K(30), SW - 2 * cfg.mside, HEADH - K(30), t, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
  }
  if (cfg.hf >= 1) {
    if (cre_chapter_title(ch, sizeof(ch)) != 0 || !ch[0]) snprintf(ch, sizeof(ch), "%s", bk[rb].title);
    snprintf(t, sizeof(t), "pàg. %d de %d · %d %%", cre_page() + 1, cre_pages(), pm / 100);
    SetFont(fSmall, LK_SUB);
    DrawTextRect(cfg.mside, SH - FOOTH, SW - 2 * cfg.mside - K(340), FOOTH - K(16), ch, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    DrawTextRect(SW - cfg.mside - K(340), SH - FOOTH, K(340), FOOTH - K(16), t, ALIGN_RIGHT | VALIGN_MIDDLE);
  }
  if (mark_on_page(cre_page()) >= 0) {           /* cinta de marcador a la cantonada */
    IcLayer L[1] = { { IC_LK_MARK, LK_FG, 1 } };
    ic_layers(L, 1, SW - K(110), -K(6), K(64), 1.8, LK_PAPER);
  }
}

/* ═════════════ Menú de lectura (estil Kobalt) ═════════════
   A dalt: Biblioteca, títol, cerca, marcadors i ajustos. A baix, segons la pestanya (v->a): res (0), Navega (1, miniatures),
   Text (2) o Disposició (3); sota, la franja d'estat i el desplaçador (0 i 1) i les pestanyes. v->b = pàgina en vista prèvia + 1. */
enum { A_LK_MENU = 200, A_LK_CLOSEMENU, A_LK_LIB, A_LK_SEARCH, A_LK_MARKS, A_LK_SET, A_LK_TAB, A_LK_SCRUB, A_LK_CHPREV, A_LK_CHNEXT,
       A_LK_THUMB, A_LK_GO, A_LK_PRESET, A_LK_FONTS, A_LK_SIZE, A_LK_SIZESET, A_LK_LH, A_LK_LHSET, A_LK_ALIGN, A_LK_HYPH,
       A_LK_MSIDE, A_LK_MSIDESET, A_LK_MTB, A_LK_MTBSET, A_LK_HF, A_LK_REFRESH, A_LK_CSS, A_LK_FONTPICK, A_LK_TOCGO, A_LK_CLOSEV,
       A_LK_QUERY, A_LK_HITGO, A_LK_MARKADD, A_LK_MARKGO, A_LK_MARKDEL, A_LK_RESCAN, A_LK_CACHE, A_LK_TURNPREV, A_LK_TURNNEXT, A_LK_PRESETS, A_LK_LPAGE };
#define TBH K(104)
#define TABH K(132)
#define STRIPH K(52)
#define SCRUBH K(116)
static const char *const TABICON[4] = { IC_LK_TOC, IC_LK_NAV, IC_LK_TEXT, IC_LK_LAYOUT };
static const char *const TABNAME[4] = { "Capítols", "Navega", "Text", "Disposició" };
static void toolbar(void) {
  int x, bw = K(104), s = K(44), i; static const char *const IC[3] = { IC_SEARCH, IC_LK_MARK, IC_LK_SET }; static const int ACT[3] = { A_LK_SEARCH, A_LK_MARKS, A_LK_SET };
  FillArea(0, 0, SW, TBH, LK_PAPER); hline(0, TBH - K(4), SW, K(4), LK_FG);
  lk_icon(IC_LK_BACK, K(20), (TBH - K(40)) / 2, K(40), LK_FG, LK_PAPER);
  SetFont(fBold, LK_FG); x = K(68); DrawTextRect(x, 0, K(200), TBH - K(4), "Biblioteca", ALIGN_LEFT | VALIGN_MIDDLE);
  x = K(68) + StringWidth("Biblioteca") + K(28);
  vline(x, 0, TBH - K(4), K(3), LK_LINE);
  add_hit(0, 0, x, TBH, A_LK_LIB, 0);
  SetFont(fBold, LK_FG); DrawTextRect(x + K(24), K(16), SW - x - K(24) - 3 * bw, K(40), bk[rb].title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fSmall, LK_SUB); DrawTextRect(x + K(24), K(56), SW - x - K(24) - 3 * bw, K(32), bk[rb].author, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  for (i = 0; i < 3; i++) {
    int bx = SW - (3 - i) * bw;
    vline(bx, 0, TBH - K(4), K(3), LK_LINE);
    if (i == 1 && mark_on_page(cre_page()) >= 0) { IcLayer L[1] = { { IC_LK_MARK, LK_FG, 1 } }; ic_layers(L, 1, bx + (bw - s) / 2, (TBH - s) / 2, s, 1.8, LK_PAPER); }
    else lk_icon(IC[i], bx + (bw - s) / 2, (TBH - s) / 2, s, LK_FG, LK_PAPER);
    add_hit(bx, 0, bw, TBH, ACT[i], 0);
  }
}
static void tabs(int y, int sel) {
  int i, w = SW / 4, s = K(44);
  hline(0, y, SW, K(4), LK_FG);
  for (i = 0; i < 4; i++) {
    int x = i * w, on = i == sel, fg = on ? LK_PAPER : LK_FG, bg = on ? LK_FG : LK_PAPER;
    FillArea(x, y + K(4), i == 3 ? SW - x : w, TABH - K(4), bg);
    if (i) vline(x, y + K(4), TABH - K(4), K(3), LK_LINE);
    lk_icon(TABICON[i], x + (w - s) / 2, y + K(20), s, fg, bg);
    SetFont(fSmall, fg); DrawTextRect(x, y + K(70), w, K(44), TABNAME[i], ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(x, y, w, TABH, A_LK_TAB, i);
  }
}
static void strip(int y, int page) {
  char t[260], tm[40]; time_t now = time(NULL); struct tm *tt = localtime(&now); int s = K(32), rw;
  FillArea(0, y, SW, STRIPH, LK_GRAY); hline(0, y, SW, K(3), LK_LINE);
  strip_text(t, sizeof(t), page);
  snprintf(tm, sizeof(tm), "%d:%02d · %d%%", tt->tm_hour, tt->tm_min, GetBatteryPower());
  SetFont(mono(), LK_FG);
  rw = StringWidth(tm) + K(8);
  DrawTextRect(K(28), y, SW - K(28) - rw - s - K(40), STRIPH, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  DrawTextRect(SW - K(28) - s - K(16) - rw, y, rw, STRIPH, tm, ALIGN_RIGHT | VALIGN_MIDDLE);
  ic_path(IC_LK_UP, SW - K(28) - s, y + (STRIPH - s) / 2, s, 2.0, LK_FG, LK_GRAY);
  add_hit(0, y, SW, STRIPH, A_LK_TAB, 1);          /* la franja desplega (o plega) les miniatures */
}
/* Desplaçador del llibre: punts als inicis de capítol (nivell 0), botons de capítol anterior i següent */
#define SCRUBX0 K(104)
#define SCRUBX1 (SW - K(104))
static void scrubber(int y, int page) {
  int n = cre_pages(), i, k, tw = SCRUBX1 - SCRUBX0, ty = y + SCRUBH / 2, kx, s = K(44), seg = 40;
  hline(0, y, SW, K(3), LK_LINE);
  lk_icon(IC_LK_PREVCH, K(30), ty - s / 2, s, LK_FG, LK_PAPER); add_hit(0, y, SCRUBX0 - K(10), SCRUBH, A_LK_CHPREV, 0);
  lk_icon(IC_LK_NEXTCH, SW - K(30) - s, ty - s / 2, s, LK_FG, LK_PAPER); add_hit(SCRUBX1 + K(10), y, SW - SCRUBX1 - K(10), SCRUBH, A_LK_CHNEXT, 0);
  if (n <= 0) return;
  kx = SCRUBX0 + (n > 1 ? (int)((long)tw * page / (n - 1)) : 0);
  rfill(SCRUBX0, ty - K(3), tw, K(6), K(3), LK_LINE);
  if (kx > SCRUBX0) rfill(SCRUBX0, ty - K(3), kx - SCRUBX0, K(6), K(3), LK_FG);
  for (k = 0; k < cre_toc_count(); k++) {
    int lv, pg; char tt[8];
    if (cre_toc_get(k, tt, sizeof(tt), &lv, &pg) == 0 && lv == 0 && n > 1) {
      int dx = SCRUBX0 + (int)((long)tw * pg / (n - 1));
      rcard(dx - K(9), ty - K(9), K(18), K(18), K(9), K(4), LK_FG, LK_PAPER);
    }
  }
  rcard(kx - K(26), ty - K(26), K(52), K(52), K(26), K(6), LK_FG, LK_PAPER);
  for (i = 0; i < seg; i++) add_hit(SCRUBX0 + tw * i / seg - tw / seg / 2, y, tw / seg + 1, SCRUBH, A_LK_SCRUB, i * 1000 / (seg - 1));
}
/* Miniatures de Navega: la pàgina anterior, la del mig (en vista prèvia) i la següent */
typedef struct { int page, w, h; ibitmap *b; } Thumb;
static Thumb thumbs[6]; static unsigned char *fullBuf;
static void thumbs_forget(void) { int i; for (i = 0; i < 6; i++) { free(thumbs[i].b); thumbs[i].b = NULL; thumbs[i].page = -1; } free(fullBuf); fullBuf = NULL; }
static ibitmap *thumb_get(int page, int tw, int th) {
  int i, slot = -1, x, y, W = SW, Hh = SH; ibitmap *b;
  for (i = 0; i < 6; i++) if (thumbs[i].b && thumbs[i].page == page && thumbs[i].w == tw && thumbs[i].h == th) return thumbs[i].b;
  for (i = 0; i < 6; i++) if (!thumbs[i].b) { slot = i; break; }
  if (slot < 0) { slot = (page + 6) % 6; free(thumbs[slot].b); thumbs[slot].b = NULL; }
  if (!fullBuf) fullBuf = (unsigned char *)malloc((size_t)W * (size_t)Hh);
  b = (ibitmap *)malloc(sizeof(ibitmap) + (size_t)tw * (size_t)th);
  if (!fullBuf || !b) { free(b); return NULL; }
  if (cre_render_page(page, fullBuf, W, Hh) != 0) { free(b); return NULL; }
  b->width = (unsigned short)tw; b->height = (unsigned short)th; b->depth = 8; b->scanline = (unsigned short)tw;
  for (y = 0; y < th; y++) {           /* reducció per mitjana de blocs */
    int y0 = y * Hh / th, y1 = (y + 1) * Hh / th; if (y1 <= y0) y1 = y0 + 1;
    for (x = 0; x < tw; x++) {
      int x0 = x * W / tw, x1 = (x + 1) * W / tw, yy, xx; long sum = 0, cnt = 0; if (x1 <= x0) x1 = x0 + 1;
      for (yy = y0; yy < y1; yy++) for (xx = x0; xx < x1; xx++) { sum += fullBuf[(size_t)yy * W + xx]; cnt++; }
      b->data[(size_t)y * tw + x] = (unsigned char)(sum / cnt);
    }
  }
  thumbs[slot].b = b; thumbs[slot].page = page; thumbs[slot].w = tw; thumbs[slot].h = th;
  return b;
}
#define THUMBH K(420)
static void thumb_row(int y, int mid) {
  int n = cre_pages(), mw = K(264), mh = K(364), sw2 = K(192), sh2 = K(264), gap = K(28), cx = SW / 2, d;
  FillArea(0, y, SW, THUMBH, LK_PAPER);
  for (d = -1; d <= 1; d++) {
    int p = mid + d, w = d ? sw2 : mw, h = d ? sh2 : mh, x = d < 0 ? cx - mw / 2 - gap - sw2 : d > 0 ? cx + mw / 2 + gap : cx - mw / 2, yy = y + THUMBH - K(28) - h;
    ibitmap *b; char t[16];
    if (p < 0 || p >= n) continue;
    b = thumb_get(p, w, h);
    if (b) DrawBitmap(x, yy, b);
    rcard(x - K(3), yy - K(3), w + K(6), h + K(6), K(10), d ? K(3) : K(6), d ? LK_LINE : LK_FG, LK_PAPER);
    if (b) DrawBitmap(x, yy, b);
    snprintf(t, sizeof(t), "%d", p + 1);
    SetFont(fSmall, LK_SUB); DrawTextRect(x, yy - K(34), w, K(30), t, ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(x, yy, w, h, d ? A_LK_THUMB : A_LK_GO, p);
  }
}
/* ── Controls dels plafons (prototip: lliscador amb − i +, segmentat, interruptor) ── */
/* Plafons: etiqueta a l'esquerra (13 px en negreta al prototip) i el control a partir de labX */
static ifont *fLab; static int labX;
static ifont *lab(void) { if (!fLab) fLab = OpenFont(DEFAULTFONTB, K(28), 1); return fLab ? fLab : fBold; }
static void row_label(int y, int h, const char *t, const char *sub) {
  if (sub) {
    SetFont(lab(), LK_FG); DrawTextRect(K(28), y + K(12), SW - K(220), h / 2 - K(6), t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, LK_SUB); DrawTextRect(K(28), y + h / 2 + K(4), SW - K(220), h / 2 - K(12), sub, ALIGN_LEFT | VALIGN_TOP | DOTS);
  } else { SetFont(lab(), LK_FG); DrawTextRect(K(28), y, labX - K(36), h, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS); }
}
static void slider(int y, const char *label, int val, int lo, int hi, const char *vtxt, int actStep, int actSet) {
  int h = K(96), x0 = labX, bx = K(68), vw = K(136), x1 = SW - K(28) - vw - bx - K(16), tx0 = x0 + bx + K(16), tw = x1 - tx0, ty = y + h / 2, kx, i, seg = 20;
  hline(0, y + h - K(3), SW, K(3), LK_LINE);
  row_label(y, h, label, NULL);
  rcard(x0, ty - bx / 2, bx, bx, bx / 2, K(3), LK_LINE, LK_PAPER); SetFont(fTitle, LK_FG); DrawTextRect(x0, ty - bx / 2, bx, bx, "−", ALIGN_CENTER | VALIGN_MIDDLE);
  add_hit(x0 - K(8), y, bx + K(16), h, actStep, -1);
  rcard(x1 + K(16), ty - bx / 2, bx, bx, bx / 2, K(3), LK_LINE, LK_PAPER); DrawTextRect(x1 + K(16), ty - bx / 2, bx, bx, "+", ALIGN_CENTER | VALIGN_MIDDLE);
  add_hit(x1 + K(8), y, bx + K(16), h, actStep, 1);
  kx = tx0 + (int)((long)tw * (val - lo) / (hi - lo));
  rfill(tx0, ty - K(3), tw, K(6), K(3), LK_LINE); if (kx > tx0) rfill(tx0, ty - K(3), kx - tx0, K(6), K(3), LK_FG);
  rcard(kx - K(24), ty - K(24), K(48), K(48), K(24), K(6), LK_FG, LK_PAPER);
  for (i = 0; i < seg; i++) add_hit(tx0 + tw * i / seg - tw / seg / 2, y, tw / seg + 1, h, actSet, lo + (hi - lo) * i / (seg - 1));
  SetFont(fBold, LK_FG); DrawTextRect(SW - K(28) - vw, y, vw, h, vtxt, ALIGN_RIGHT | VALIGN_MIDDLE);
}
static void segmented(int x, int y, int w, int h, const char *const *opt, int n, int sel, int act) {
  int i, cw = w / n;
  rcard(x, y, w, h, K(20), K(3), LK_FG, LK_PAPER);
  for (i = 0; i < n; i++) {
    int cx = x + i * cw, ww = i == n - 1 ? w - i * cw : cw, on = i == sel;
    if (on) {                                   /* el segment triat, ple; els extrems segueixen el contorn arrodonit */
      if (i == 0 || i == n - 1) rfill(cx, y, ww, h, K(20), LK_FG); else FillArea(cx, y, ww, h, LK_FG);
      if (i == 0 && n > 1) FillArea(cx + ww / 2, y, ww - ww / 2, h, LK_FG);
      if (i == n - 1 && n > 1) FillArea(cx, y, ww / 2, h, LK_FG);
    }
    if (i) vline(cx, y, h, K(3), LK_FG);
    SetFont(fSmall, on ? LK_PAPER : LK_FG); DrawTextRect(cx, y, ww, h, opt[i], ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    add_hit(cx, y, ww, h, act, i);
  }
}
static void toggle(int y, int h, const char *label, const char *sub, int on, int act) {
  int w = K(104), hh = K(60), x = SW - K(28) - w, yy = y + (h - hh) / 2;
  row_label(y, h, label, sub);
  rcard(x, yy, w, hh, hh / 2, K(4), LK_FG, on ? LK_FG : LK_PAPER);
  if (on) rfill(x, yy, w, hh, hh / 2, LK_FG);
  rfill(on ? x + w - hh + K(6) : x + K(6), yy + K(6), hh - K(12), hh - K(12), (hh - K(12)) / 2, on ? LK_PAPER : LK_FG);
  add_hit(0, y, SW, h, act, 0);
}
/* Text (prototip C) */
static int text_panel_h(void) { return K(68) + K(148) + K(100) + 2 * K(96) + K(92) + K(104); }
static void text_panel(int y) {
  static const char *const AL[3] = { "Esquerra", "Justificat", "Del llibre" };
  int i, pc = preset_cur(), cw = (SW - K(48) - K(104) - 4 * K(16)) / 4; char t[80];
  FillArea(0, y, SW, text_panel_h(), LK_PAPER); labX = K(232);
  FillArea(0, y, SW, K(68), LK_GRAY);
  SetFont(lab(), LK_FG); DrawTextRect(K(28), y, K(400), K(68), "Predefinits", ALIGN_LEFT | VALIGN_MIDDLE);
  SetFont(fBold, LK_FG); DrawTextRect(SW - K(28) - K(44) - K(420), y, K(420), K(68), pc >= 0 ? PRESETS[pc].name : "Cap (modificat)", ALIGN_RIGHT | VALIGN_MIDDLE);
  lk_icon(IC_LK_DOWN, SW - K(28) - K(32), y + K(18), K(32), LK_FG, LK_GRAY);
  add_hit(SW / 2, y, SW / 2, K(68), A_LK_PRESETS, 0);
  y += K(68);
  for (i = 0; i < 4; i++) {
    int x = K(24) + i * (cw + K(16)), on = i == pc, cy = y + K(20), ch = K(128);
    rcard(x, cy, cw, ch, K(20), on ? K(6) : K(3), on ? LK_FG : LK_LINE, LK_PAPER);
    SetFont(getfontf(PRESETS[i].fam, K(44)), LK_FG); DrawTextRect(x, cy, cw, ch - K(40), "Aa", ALIGN_CENTER | VALIGN_MIDDLE);
    FillArea(x + K(6), cy + ch - K(40), cw - K(12), K(34), on ? LK_FG : LK_GRAY);
    SetFont(fSmall, on ? LK_PAPER : LK_FG); DrawTextRect(x, cy + ch - K(40), cw, K(34), PRESETS[i].name, ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
    add_hit(x, cy, cw, ch, A_LK_PRESET, i);
  }
  { int x = SW - K(24) - K(104), cy = y + K(20), ch = K(128);
    rcard(x, cy, K(104), ch, K(20), K(3), LK_LINE, LK_PAPER);
    lk_icon(IC_LK_GRID, x + K(32), cy + K(26), K(40), LK_FG, LK_PAPER);
    SetFont(fSmall, LK_FG); DrawTextRect(x, cy + K(76), K(104), K(40), "Tots", ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(x, cy, K(104), ch, A_LK_PRESETS, 0); }
  y += K(148); hline(0, y - K(3), SW, K(3), LK_LINE);
  row_label(y, K(100), "Lletra", NULL);
  { int x = labX, w = SW - K(28) - x, by = y + K(14), bh = K(72);
    rcard(x, by, w, bh, K(20), K(3), LK_LINE, LK_PAPER);
    SetFont(cur_font(K(38)), LK_FG); DrawTextRect(x + K(20), by, K(70), bh, "Aa", ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(fBold, LK_FG); DrawTextRect(x + K(100), by, w - K(160), bh, cfg.fam == 2 ? "Fonts del llibre" : cfg.face[0] ? cfg.face : cre_font_name(cfg.fam), ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    lk_icon(IC_LK_RIGHT, x + w - K(52), by + K(20), K(32), LK_FG, LK_PAPER);
    add_hit(0, y, SW, K(100), A_LK_FONTS, 0); }
  y += K(100); hline(0, y - K(3), SW, K(3), LK_LINE);
  snprintf(t, sizeof(t), "%d,%d pt", cfg.size * 72 / 300, cfg.size * 720 / 300 % 10);
  slider(y, "Mida", cfg.size, 24, 80, t, A_LK_SIZE, A_LK_SIZESET); y += K(96);
  snprintf(t, sizeof(t), "%d %%", cfg.lh);
  slider(y, "Interlineat", cfg.lh, 100, 220, t, A_LK_LH, A_LK_LHSET); y += K(96);
  row_label(y, K(92), "Alineació", NULL);
  segmented(labX, y + K(14), SW - K(28) - labX, K(64), AL, 3, cfg.align, A_LK_ALIGN);
  y += K(92); hline(0, y - K(3), SW, K(3), LK_LINE);
  toggle(y, K(104), "Partició de mots", "Segons la llengua del llibre; si no en diu cap, en català", cfg.hyph, A_LK_HYPH);
}
/* Disposició (prototip D) */
static int layout_panel_h(void) { return 2 * K(96) + K(56) + K(218) + K(96) + K(104); }
static void hf_card(int x, int y, int w, int h, int kind, int on) {
  int px = x + K(26), pw = w - K(52), i;
  rcard(x, y, w, h, K(20), on ? K(6) : K(3), on ? LK_FG : LK_LINE, LK_PAPER);
  if (kind == 2) rfill(x + w / 2 - pw / 5, y + K(18), pw * 2 / 5, K(8), K(4), LK_SUB);
  for (i = 0; i < 6; i++) rfill(px, y + K(36) + i * K(14), pw, K(6), K(3), LK_LINE);
  if (kind >= 1) { rfill(px, y + h - K(62), pw * 46 / 100, K(8), K(4), LK_SUB); rfill(px + pw - pw * 30 / 100, y + h - K(62), pw * 30 / 100, K(8), K(4), LK_SUB); }
  FillArea(x + K(6), y + h - K(46), w - K(12), K(40), on ? LK_FG : LK_GRAY);
  { static const char *const N[3] = { "Net", "Capítol i pàgina", "Complet" };
    SetFont(fSmall, on ? LK_PAPER : LK_FG); DrawTextRect(x, y + h - K(46), w, K(40), N[kind], ALIGN_CENTER | VALIGN_MIDDLE | DOTS); }
}
static void layout_panel(int y) {
  static const char *const RF[4] = { "Sempre", "Cada 6", "Cada 12", "Mai" };
  int i, cw = (SW - K(56) - 2 * K(20)) / 3; char t[40];
  FillArea(0, y, SW, layout_panel_h(), LK_PAPER); labX = K(264);
  snprintf(t, sizeof(t), "%d", cfg.mside); slider(y, "Marges laterals", cfg.mside, 0, 200, t, A_LK_MSIDE, A_LK_MSIDESET); y += K(96);
  snprintf(t, sizeof(t), "%d", cfg.mtb); slider(y, "Dalt i baix", cfg.mtb, 0, 200, t, A_LK_MTB, A_LK_MTBSET); y += K(96);
  SetFont(lab(), LK_FG); DrawTextRect(K(28), y, SW - K(56), K(56), "Capçalera i peu", ALIGN_LEFT | VALIGN_MIDDLE); y += K(56);
  for (i = 0; i < 3; i++) { int x = K(28) + i * (cw + K(20)); hf_card(x, y, cw, K(196), i, cfg.hf == i); add_hit(x, y, cw, K(196), A_LK_HF, i); }
  y += K(218); hline(0, y - K(3), SW, K(3), LK_LINE);
  row_label(y, K(96), "Refresc total", "neteja la tinta");
  segmented(labX, y + K(14), SW - K(28) - labX, K(64), RF, 4, cfg.refresh, A_LK_REFRESH);
  y += K(96); hline(0, y - K(3), SW, K(3), LK_LINE);
  toggle(y, K(104), "Estils del llibre", "Fa servir el CSS i les fonts que porta el llibre", cfg.css, A_LK_CSS);
}

/* ═════════════ Vistes del llibre obert ═════════════ */
static int no_book(void) {
  if (rb >= 0 && cre_is_open()) return 0;
  header("Lector", "No hi ha cap llibre obert."); return 1;
}
/* Lectura (prototip A): tercer esquerre enrere, centre menú, tercer dret endavant. Els botons de pàgina del lector
   arriben com a canvi de pàgina de la vista (v->page 0 o 2; la pàgina 1 és el repòs). */
static void v_book(View *v) {
  if (no_book()) return;
  pages = 3;
  if (v->page != 1) {
    int d = v->page - 1; v->page = 1;
    if (d < 0 ? cre_prev() : cre_next()) { save_pos(); turns++; }
  }
  page_paint();
  page_furniture();
  add_hit(0, 0, SW * 3 / 10, SH, A_LK_TURNPREV, 0);
  add_hit(SW * 3 / 10, 0, SW * 4 / 10, SH, A_LK_MENU, 0);
  add_hit(SW * 7 / 10, 0, SW - SW * 7 / 10, SH, A_LK_TURNNEXT, 0);
}
/* Capítols (prototip E): l'índex del llibre, paginat, amb el capítol actual marcat */
#define TOCROW K(116)
static void toc_panel(View *v) {
  int n = cre_toc_count(), cur = -1, i, y, per, p = cre_page(), num = 0, s = K(44), pgY, k; char t[300];
  for (i = 0; i < n; i++) { int lv, pg; if (cre_toc_get(i, t, sizeof(t), &lv, &pg) == 0 && pg <= p) cur = i; }
  FillArea(0, 0, SW, SH, LK_PAPER);
  /* barra de dalt */
  hline(0, TBH - K(4), SW, K(4), LK_FG);
  SetFont(fTitle, LK_FG); DrawTextRect(K(32), 0, SW - K(150), TBH - K(4), "Capítols", ALIGN_LEFT | VALIGN_MIDDLE);
  vline(SW - K(112), 0, TBH - K(4), K(3), LK_LINE);
  ic_path(IC_LK_CLOSE, SW - K(112) + (K(112) - s) / 2, (TBH - s) / 2, s, 2.0, LK_FG, LK_PAPER);
  add_hit(SW - K(112), 0, K(112), TBH, A_LK_CLOSEV, 0);
  /* a baix: pàgines de la llista, franja i pestanyes */
  tabs(SH - TABH, 0);
  pages = 1;
  strip(SH - TABH - STRIPH, cre_page()); hline(0, SH - TABH - STRIPH, SW, K(6), LK_FG);
  pgY = SH - TABH - STRIPH - K(104);
  per = (pgY - TBH) / TOCROW; if (per < 1) per = 1;
  pages = n > 0 ? (n + per - 1) / per : 1;
  if (v->sel == 0) { v->sel = 1; if (cur >= 0) v->page = cur / per; }
  if (v->page >= pages) v->page = pages - 1;
  if (v->page < 0) v->page = 0;
  hline(0, pgY, SW, K(3), LK_LINE);
  { char pt[40]; int can0 = v->page > 0, can1 = v->page < pages - 1;
    SetFont(fBold, can0 ? LK_FG : LK_LINE); DrawTextRect(K(28), pgY, K(300), K(104), "‹ Anterior", ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(fBold, can1 ? LK_FG : LK_LINE); DrawTextRect(SW - K(328), pgY, K(300), K(104), "Següent ›", ALIGN_RIGHT | VALIGN_MIDDLE);
    snprintf(pt, sizeof(pt), "%d de %d", v->page + 1, pages);
    SetFont(fBold, LK_SUB); DrawTextRect(SW / 2 - K(150), pgY, K(300), K(104), pt, ALIGN_CENTER | VALIGN_MIDDLE);
    if (can0) add_hit(0, pgY, SW / 3, K(104), A_LK_LPAGE, -1);
    if (can1) add_hit(SW - SW / 3, pgY, SW / 3, K(104), A_LK_LPAGE, 1); }
  if (n == 0) { SetFont(fText, LK_SUB); DrawTextRect(K(32), TBH + K(30), SW - K(64), K(200), "Aquest llibre no té índex.", ALIGN_LEFT | VALIGN_TOP); return; }
  /* número dels capítols de primer nivell fins a la primera fila de la pàgina */
  for (k = 0; k < v->page * per && k < n; k++) { int lv, pg; if (cre_toc_get(k, t, 8, &lv, &pg) == 0 && lv == 0) num++; }
  y = TBH;
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    int lv = 0, pg = 0, pad, on = i == cur, bw = 0; char nb[12] = "", pn[16];
    cre_toc_get(i, t, sizeof(t), &lv, &pg);
    if (lv == 0) snprintf(nb, sizeof(nb), "%d", ++num);
    pad = K(32) + (lv > 3 ? 3 : lv) * K(48);
    if (on) FillArea(0, y, SW, TOCROW - K(3), LK_GRAY);
    hline(0, y + TOCROW - K(3), SW, K(3), LK_GRAY == LK_PAPER ? LK_LINE : LK_GRAY);
    SetFont(mono(), on ? LK_FG : LK_SUB); DrawTextRect(pad, y, K(52), TOCROW, nb, ALIGN_LEFT | VALIGN_MIDDLE);
    snprintf(pn, sizeof(pn), "%d", pg + 1);
    if (on) {
      SetFont(fSmall, LK_PAPER); bw = StringWidth("ets aquí") + K(32);
      rfill(SW - K(32) - K(80) - K(24) - bw, y + (TOCROW - K(44)) / 2, bw, K(44), K(16), LK_FG);
      DrawTextRect(SW - K(32) - K(80) - K(24) - bw, y + (TOCROW - K(44)) / 2, bw, K(44), "ets aquí", ALIGN_CENTER | VALIGN_MIDDLE);
      bw += K(24);
    }
    SetFont(on || lv == 0 ? fBold : fText, LK_FG);
    DrawTextRect(pad + K(76), y, SW - pad - K(76) - K(32) - K(80) - K(24) - bw, TOCROW, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fText, LK_SUB); DrawTextRect(SW - K(32) - K(80), y, K(80), TOCROW, pn, ALIGN_RIGHT | VALIGN_MIDDLE);
    add_hit(0, y, SW, TOCROW, A_LK_TOCGO, i);
    y += TOCROW;
  }
}

static int menu_mid(const View *v) { int n = cre_pages(), p = v->b > 0 ? v->b - 1 : cre_page(); return p >= n ? n - 1 : p < 0 ? 0 : p; }
/* Menú de lectura (prototip B, C, D): la pàgina queda a la vista entre la barra de dalt i el plafó de baix. */
static void v_menu(View *v) {
  int y, top;
  if (no_book()) return;
  if (v->a == 4) { toc_panel(v); return; }
  pages = 1;
  page_paint();
  toolbar();
  y = SH - TABH;
  tabs(y, v->a == 0 ? -1 : v->a);
  if (v->a <= 1) {
    y -= SCRUBH; FillArea(0, y, SW, SCRUBH, LK_PAPER); scrubber(y, menu_mid(v));
    y -= STRIPH; strip(y, menu_mid(v));
    if (v->a == 1) { y -= THUMBH; thumb_row(y, menu_mid(v)); }
  } else if (v->a == 2) { y -= text_panel_h(); text_panel(y); }
  else { y -= layout_panel_h(); layout_panel(y); }
  top = y - K(6); hline(0, top, SW, K(6), LK_FG);
  if (top > TBH) add_hit(0, TBH, SW, top - TBH, A_LK_CLOSEMENU, 0);
}
/* ── Pantalles secundàries (amb la capçalera de l'Escriptori) ── */
static void list_row(int y, int h, const char *title, const char *sub, const char *right, int bold, int act, int arg) {
  SetFont(bold ? fBold : fText, LK_FG);
  DrawTextRect(U / 2, y + (sub ? U / 6 : 0), SW - U - (right ? 5 * U : 0), sub ? h / 2 : h - U / 8, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  if (sub) { SetFont(fSmall, LK_SUB); DrawTextRect(U / 2, y + h / 2, SW - U - (right ? 5 * U : 0), h / 2 - U / 6, sub, ALIGN_LEFT | VALIGN_TOP | DOTS); }
  if (right) { SetFont(fSmall, LK_SUB); DrawTextRect(SW - U / 2 - 5 * U, y, 5 * U, h - U / 8, right, ALIGN_RIGHT | VALIGN_MIDDLE | DOTS); }
  DrawLine(U / 2, y + h - U / 8, SW - U / 2, y + h - U / 8, LK_LINE);
  if (act) add_hit(U / 2, y, SW - U, h - U / 8, act, arg);
}
static int list_pages(View *v, int n, int rh, int y0) {
  int per = (BOTTOM - y0) / rh; if (per < 1) per = 1;
  pages = n > 0 ? (n + per - 1) / per : 1;
  if (v->page >= pages) v->page = pages - 1;
  if (v->page < 0) v->page = 0;
  return per;
}
/* Lletra: les fonts del llibre i totes les que troba el motor, cadascuna amb una mostra */
static void v_fonts(View *v) {
  int n, y = TOP, rh = 2 * U + U / 2, per, i; char sub[40], name[64];
  if (no_book()) return;
  n = cre_font_count() + 1;
  snprintf(sub, sizeof(sub), "%d fonts", n - 1);
  header("Lletra", sub);
  per = list_pages(v, n, rh, y);
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    int on;
    if (i == 0) { snprintf(name, sizeof(name), "Fonts del llibre"); on = cfg.fam == 2; }
    else { cre_font_get(i - 1, name, sizeof(name)); on = cfg.fam != 2 && (cfg.face[0] ? !strcmp(cfg.face, name) : !strcmp(cre_font_name(cfg.fam), name)); }
    SetFont(i == 0 ? getfontf(1, U * 3 / 2) : face_font(name, U * 3 / 2), LK_FG);
    DrawTextRect(U / 2, y, 3 * U, rh - U / 8, "Aa", ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(on ? fBold : fText, LK_FG);
    DrawTextRect(U / 2 + 3 * U + U / 2, y + (i == 0 ? U / 6 : 0), SW - U - 7 * U, i == 0 ? rh / 2 : rh - U / 8, name, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (i == 0) { SetFont(fSmall, LK_SUB); DrawTextRect(U / 2 + 3 * U + U / 2, y + rh / 2 + U / 6, SW - U - 7 * U, rh / 2 - U / 3, cfg.css ? "Si el llibre no en porta, amb serifa" : "Cal «Estils del llibre» (Disposició)", ALIGN_LEFT | VALIGN_TOP | DOTS); }
    if (on) ic_path(IC_CHECK, SW - U / 2 - 2 * U, y + (rh - 2 * U) / 2, 2 * U, 2.2, LK_FG, PAL(P_BG));
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LK_LINE);
    add_hit(U / 2, y, SW - U, rh - U / 8, A_LK_FONTPICK, i - 1);
    y += rh;
  }
}
/* Tots els predefinits, amb el detall de cadascun */
static void v_presets(View *v) {
  int y = TOP, rh = 3 * U, i, pc = preset_cur(); char sub[120];
  (void)v;
  if (no_book()) return;
  header("Predefinits", pc >= 0 ? PRESETS[pc].name : "Cap (modificat)");
  for (i = 0; i < 4; i++) {
    const Preset *p = &PRESETS[i];
    snprintf(sub, sizeof(sub), "%s · %d,%d pt · interlineat %d %%", p->fam == 2 ? "Fonts del llibre" : cre_font_name(p->fam), p->size * 72 / 300, p->size * 720 / 300 % 10, p->lh);
    SetFont(getfontf(p->fam, U * 3 / 2), LK_FG); DrawTextRect(U / 2, y, 3 * U, rh - U / 8, "Aa", ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(i == pc ? fBold : fText, LK_FG); DrawTextRect(U / 2 + 3 * U + U / 2, y + U / 4, SW - U - 7 * U, rh / 2, p->name, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, LK_SUB); DrawTextRect(U / 2 + 3 * U + U / 2, y + rh / 2 + U / 4, SW - U - 7 * U, rh / 2 - U / 2, sub, ALIGN_LEFT | VALIGN_TOP | DOTS);
    if (i == pc) ic_path(IC_CHECK, SW - U / 2 - 2 * U, y + (rh - 2 * U) / 2, 2 * U, 2.2, LK_FG, PAL(P_BG));
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LK_LINE);
    add_hit(U / 2, y, SW - U, rh - U / 8, A_LK_PRESET, i + 100);
    y += rh;
  }
  SetFont(fSmall, LK_SUB);
  DrawTextRect(U / 2, y + U / 2, SW - U, 3 * U, "Un predefinit tria la lletra, la mida i l'interlineat. Els marges i la resta de la disposició no canvien.", ALIGN_LEFT | VALIGN_TOP);
}
/* Cerca: els resultats de l'última cerca al llibre obert */
#define MAXHITS 200
static CreHit *hits; static int nhitsq; static char lastq[120];
static void v_search(View *v) {
  int y = TOP, rh = 3 * U, per, i; char sub[160], pg[24];
  if (no_book()) return;
  if (nhitsq == 0) snprintf(sub, sizeof(sub), "Cap resultat per «%s»", lastq);
  else if (nhitsq == 1) snprintf(sub, sizeof(sub), "1 resultat per «%s»", lastq);
  else if (nhitsq >= MAXHITS) snprintf(sub, sizeof(sub), "Més de %d resultats per «%s»", MAXHITS, lastq);
  else snprintf(sub, sizeof(sub), "%d resultats per «%s»", nhitsq, lastq);
  header("Cerca", sub);
  button(U / 2, y, SW - U, 2 * U, "Cerca una altra cosa", 0, A_LK_SEARCH, 0); y += 2 * U + U / 2;
  per = list_pages(v, nhitsq, rh, y);
  for (i = v->page * per; i < nhitsq && i < (v->page + 1) * per; i++) {
    char ch[200];
    chapter_at(hits[i].page, ch, sizeof(ch));
    snprintf(pg, sizeof(pg), "pàg. %d", hits[i].page + 1);
    list_row(y, rh, hits[i].text, ch[0] ? ch : NULL, pg, 0, A_LK_HITGO, i);
    y += rh;
  }
}
/* Marcadors del llibre obert */
static void v_marks(View *v) {
  static Mark m[200]; int n, y = TOP, rh = 2 * U + U / 2, per, i, on; char sub[40], pg[24];
  if (no_book()) return;
  n = marks_of_book(m, 200); on = mark_on_page(cre_page()) >= 0;
  snprintf(sub, sizeof(sub), n == 1 ? "1 marcador" : "%d marcadors", n);
  header("Marcadors", sub);
  button(U / 2, y, SW - U, 2 * U, on ? "Treu el marcador d'aquesta pàgina" : "Marca aquesta pàgina", 0, A_LK_MARKADD, 0); y += 2 * U + U / 2;
  if (n == 0) { SetFont(fText, LK_SUB); DrawTextRect(U / 2, y, SW - U, 4 * U, "Encara no hi ha cap marcador en aquest llibre. També pots marcar una pàgina amb el botó del marcador del menú de lectura.", ALIGN_LEFT | VALIGN_TOP); return; }
  per = list_pages(v, n, rh, y);
  for (i = v->page * per; i < n && i < (v->page + 1) * per; i++) {
    int bs = 2 * U;
    snprintf(pg, sizeof(pg), "pàg. %d", m[i].page + 1);
    SetFont(fBold, LK_FG); DrawTextRect(U / 2, y, SW - U - 9 * U, rh - U / 8, m[i].label, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    SetFont(fSmall, LK_SUB); DrawTextRect(SW - U / 2 - 3 * U - bs - 4 * U, y, 4 * U, rh - U / 8, pg, ALIGN_RIGHT | VALIGN_MIDDLE);
    add_hit(U / 2, y, SW - U - 3 * U - bs, rh - U / 8, A_LK_MARKGO, m[i].idx);
    ic_path(IC_LK_CLOSE, SW - U / 2 - bs + U / 3, y + (rh - bs) / 2 + U / 3, bs - 2 * U / 3, 2.0, LK_FG, PAL(P_BG));
    add_hit(SW - U / 2 - bs - U / 2, y, bs + U / 2, rh - U / 8, A_LK_MARKDEL, m[i].idx);
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LK_LINE);
    y += rh;
  }
}
/* Ajustos del lector */
static void v_set(View *v) {
  int y = TOP, bh = 2 * U, gap = U / 3; char t[200];
  (void)v;
  header("Ajustos del lector", bk && rb >= 0 ? bk[rb].title : "");
  button(U / 2, y, SW - U, bh, "Actualitza la biblioteca", 0, A_LK_RESCAN, 0); y += bh + gap;
  button(U / 2, y, SW - U, bh, "Esborra la memòria cau de pàgines", 0, A_LK_CACHE, 0); y += bh + 2 * gap;
  SetFont(fBold, LK_FG); DrawTextRect(U / 2, y, SW - U, U + U / 4, "Sobre el motor", ALIGN_LEFT | VALIGN_MIDDLE); y += U + U / 2;
  snprintf(t, sizeof(t), "Lletra per defecte: %s (amb serifa) i %s (sense serifa).", cre_font_name(1), cre_font_name(0));
  SetFont(fSmall, LK_FG);
  DrawTextRect(U / 2, y, SW - U, 9 * U,
    "Les pàgines les compon el CREngine, el motor del KOReader i del Kobalt (GPL v2 o posterior), amb FreeType, HarfBuzz, "
    "libunibreak, utf8proc, zlib, libpng, libjpeg-turbo i xxHash. La partició de mots fa servir els diccionaris de patrons "
    "del TeX (llicències lliures; vegeu-ne la llista al codi font, lector/LLEGEIX.md).\n\nEl paquet Lector és programari lliure (GPL).",
    ALIGN_LEFT | VALIGN_TOP);
  y += 6 * U;
  SetFont(fSmall, LK_SUB); DrawTextRect(U / 2, y, SW - U, 2 * U, t, ALIGN_LEFT | VALIGN_TOP);
}

/* ═════════════ Paquet ═════════════ */
static void mod_icon(int x, int y, int s) { ic_path(IC_BOOK, x, y, s, 1.8, ICON_FG, ICON_BG); }   /* icona del prototip (icons.h) */
static void mod_draw(View *v) {
  switch (v->type) {
    case V_LIB: v_lib(v); break;
    case V_BOOK: v_book(v); break;
    case V_MENU: v_menu(v); break;
    case V_FONTS: v_fonts(v); break;
    case V_PRESETS: v_presets(v); break;
    case V_SEARCH: v_search(v); break;
    case V_MARKS: v_marks(v); break;
    case V_SET: v_set(v); break;
  }
}
static void turn(int dir) {
  static const int EVERY[4] = { 1, 6, 12, 0 };
  int e = EVERY[cfg.refresh];
  if (!cre_is_open()) return;
  if (!(dir > 0 ? cre_next() : cre_prev())) return;
  save_pos();
  ++turns;
  draw(e > 0 && turns % e == 0);    /* refresc complet segons Disposició › Refresc total */
}
static void relayout(void) { cfg_save(); thumbs_forget(); draw(0); }
static void search_fn(const char *q) {
  if (!q || !q[0] || !cre_is_open()) return;
  snprintf(lastq, sizeof(lastq), "%s", q);
  if (!hits) hits = (CreHit *)malloc(sizeof(CreHit) * MAXHITS);
  if (!hits) return;
  ShowHourglass(); nhitsq = cre_search(q, hits, MAXHITS); HideHourglass();
  push(V_SEARCH, 0, 0, 0, NULL);
}
static void cache_clear(void) {
  DIR *d = opendir(LDIR "/cache"); struct dirent *e; char p[400]; int n = 0;
  if (d) {
    while ((e = readdir(d))) { if (e->d_name[0] == '.') continue; snprintf(p, sizeof(p), "%s/%s", LDIR "/cache", e->d_name); if (unlink(p) == 0) n++; }
    closedir(d);
  }
  say(n ? "S'ha esborrat la memòria cau. Els llibres es tornaran a compondre en obrir-los." : "La memòria cau ja era buida.");
}
/* capítols per saltar amb ‹‹ i ››: els de primer nivell (o tots, si el llibre no en té) */
static int chapter_jump(int from, int dir) {
  int n = cre_toc_count(), k, best = -1, lvl0 = 0, lv, pg; char t[8];
  for (k = 0; k < n; k++) if (cre_toc_get(k, t, sizeof(t), &lv, &pg) == 0 && lv == 0) lvl0++;
  for (k = 0; k < n; k++) {
    if (cre_toc_get(k, t, sizeof(t), &lv, &pg) != 0 || (lvl0 > 1 && lv != 0)) continue;
    if (dir > 0 && pg > from && (best < 0 || pg < best)) best = pg;
    if (dir < 0 && pg < from && (best < 0 || pg > best)) best = pg;
  }
  if (best < 0) best = dir > 0 ? cre_pages() - 1 : 0;
  return best;
}
static void go_page(View *v, int p) {
  if (v->type == V_MENU && v->a == 1) { v->b = p + 1; draw(0); return; }
  cre_goto_page(p); save_pos(); draw(0);
}
static void mod_action(View *v, int a, int arg) {
  switch (a) {
    case A_OPEN: {
      int r = book_open(arg);
      if (r == -2) { say("No s'han trobat fonts per compondre el llibre."); draw(1); }
      else if (r != 0) { say("No s'ha pogut obrir aquest llibre."); draw(1); }
      else push(V_BOOK, arg, 0, 0, NULL);
      break; }
    case A_SCAN: lib_scan(); v->page = 0; draw(1); break;
    case A_LK_TURNPREV: turn(-1); break;
    case A_LK_TURNNEXT: turn(1); break;
    case A_LK_MENU: save_pos(); push(V_MENU, 0, 0, 0, NULL); break;
    case A_LK_CLOSEMENU: POP(); draw(1); break;
    case A_LK_LIB: book_close(); POP(); POP(); draw(1); break;
    case A_LK_TAB:
      if (v->type != V_MENU) break;
      if (arg == 0) arg = 4;                    /* Capítols */
      v->a = v->a == arg ? 0 : arg; v->b = 0; v->page = 0; v->sel = 0;
      draw(0);
      break;
    case A_LK_LPAGE: v->page += arg; draw(0); break;
    case A_LK_SCRUB: { int n = cre_pages(); if (n > 0) go_page(v, (int)((long)arg * (n - 1) / 1000)); break; }
    case A_LK_CHPREV: go_page(v, chapter_jump(v->type == V_MENU ? menu_mid(v) : cre_page(), -1)); break;
    case A_LK_CHNEXT: go_page(v, chapter_jump(v->type == V_MENU ? menu_mid(v) : cre_page(), 1)); break;
    case A_LK_THUMB: v->b = arg + 1; draw(0); break;
    case A_LK_GO: cre_goto_page(arg); save_pos(); POP(); draw(1); break;
    case A_LK_PRESET: {
      int i = arg >= 100 ? arg - 100 : arg;
      if (i < 0 || i > 3) break;
      cfg.fam = PRESETS[i].fam; cfg.size = PRESETS[i].size; cfg.lh = PRESETS[i].lh; cfg.face[0] = 0;
      if (cfg.fam == 2) cfg.css = 1;
      if (arg >= 100) { cfg_save(); thumbs_forget(); POP(); draw(1); } else relayout();
      break; }
    case A_LK_PRESETS: push(V_PRESETS, 0, 0, 0, NULL); break;
    case A_LK_FONTS: push(V_FONTS, 0, 0, 0, NULL); break;
    case A_LK_FONTPICK:
      if (arg < 0) { cfg.fam = 2; cfg.css = 1; cfg.face[0] = 0; }
      else { char nm[64]; if (cre_font_get(arg, nm, sizeof(nm)) != 0) break;
             if (cfg.fam == 2) cfg.fam = 1;
             if (!strcmp(nm, cre_font_name(0))) { cfg.fam = 0; cfg.face[0] = 0; }
             else if (!strcmp(nm, cre_font_name(1))) { cfg.fam = 1; cfg.face[0] = 0; }
             else snprintf(cfg.face, sizeof(cfg.face), "%s", nm); }
      cfg_save(); thumbs_forget(); POP(); draw(1);
      break;
    case A_LK_SIZE: cfg.size = clampi(cfg.size + 2 * arg, 24, 80); relayout(); break;
    case A_LK_SIZESET: cfg.size = clampi(arg / 2 * 2, 24, 80); relayout(); break;
    case A_LK_LH: cfg.lh = clampi(cfg.lh + 5 * arg, 100, 220); relayout(); break;
    case A_LK_LHSET: cfg.lh = clampi((arg + 2) / 5 * 5, 100, 220); relayout(); break;
    case A_LK_MSIDE: cfg.mside = clampi(cfg.mside + 8 * arg, 0, 200); relayout(); break;
    case A_LK_MSIDESET: cfg.mside = clampi((arg + 4) / 8 * 8, 0, 200); relayout(); break;
    case A_LK_MTB: cfg.mtb = clampi(cfg.mtb + 8 * arg, 0, 200); relayout(); break;
    case A_LK_MTBSET: cfg.mtb = clampi((arg + 4) / 8 * 8, 0, 200); relayout(); break;
    case A_LK_ALIGN: cfg.align = clampi(arg, 0, 2); relayout(); break;
    case A_LK_HYPH: cfg.hyph = !cfg.hyph; relayout(); break;
    case A_LK_HF: cfg.hf = clampi(arg, 0, 2); relayout(); break;
    case A_LK_REFRESH: cfg.refresh = clampi(arg, 0, 3); cfg_save(); draw(0); break;
    case A_LK_CSS: cfg.css = !cfg.css; relayout(); break;
    case A_LK_TOCGO: {
      char t[8]; int lv, pg;
      if (cre_toc_get(arg, t, sizeof(t), &lv, &pg) == 0) { cre_goto_page(pg); save_pos(); POP(); draw(1); }
      break; }
    case A_LK_CLOSEV: if (v->type == V_MENU) { v->a = 0; v->page = 0; draw(0); } else { POP(); draw(0); } break;
    case A_LK_SEARCH: ask("Cerca al llibre", lastq, search_fn); break;
    case A_LK_HITGO:
      if (arg >= 0 && arg < nhitsq) { cre_goto_page(hits[arg].page); save_pos(); POP(); POP(); draw(1); }
      break;
    case A_LK_MARKS: push(V_MARKS, 0, 0, 0, NULL); break;
    case A_LK_MARKADD: mark_toggle(); draw(0); break;
    case A_LK_MARKGO: {
      char t[1200], *f[4]; st_load(&marks);
      if (arg < 0 || arg >= marks.n) break;
      snprintf(t, sizeof(t), "%s", marks.it[arg]);
      if (split_tab(t, f, 4) >= 2 && cre_goto_bookmark(f[1]) == 0) { save_pos(); POP(); POP(); draw(1); }
      break; }
    case A_LK_MARKDEL: st_load(&marks); if (arg >= 0 && arg < marks.n) { st_del(&marks, arg); st_save(&marks); } draw(0); break;
    case A_LK_SET: push(V_SET, 0, 0, 0, NULL); break;
    case A_LK_RESCAN:
      book_close(); lib_scan();
      POP(); POP(); POP(); draw(1);       /* ajustos › menú › llibre › biblioteca */
      break;
    case A_LK_CACHE: cache_clear(); draw(1); break;
  }
}
/* Enrere: al menú, primer plega el plafó obert; a la lectura, desa la posició i torna a la biblioteca. */
static int mod_back(View *v) {
  if (v->type == V_BOOK || v->type == V_MENU) save_pos();
  if (v->type == V_MENU && v->a != 0) { v->a = 0; v->b = 0; v->page = 0; return 1; }
  return 0;
}
static void on_unload(void) {
  book_close(); thumbs_forget(); fonts_free();
  free(pageBmp); pageBmp = NULL; bmpPage = -1; free(hits); hits = NULL; nhitsq = 0;
  if (fMono) { CloseFont(fMono); fMono = NULL; }
  if (fLab) { CloseFont(fLab); fLab = NULL; }
}
static void mod_open(View *v) {
  static int reg;
  (void)v; cfg_load(); lib_scan();
  if (!reg && H->api >= 4 && H->on_unload) { reg = 1; H->on_unload("lector", on_unload); }
}
static Module M = { 8, "lector", "Lector", 0, mod_icon, mod_draw, mod_action, mod_back, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
/* Es pot obrir en un panell de la pantalla dividida (el dibuix fa servir SB, SH, TOP i BOTTOM). */
__attribute__((visibility("default"))) const int escriptori_split = 1;
/* La lectura, el menú i els capítols ocupen tota la pantalla, sense la barra d'estat (API 8). */
__attribute__((visibility("default"))) int escriptori_fullscreen(const View *v) { return v->type == V_BOOK || v->type == V_MENU; }
