/* Escriptori — paquet «Lector»: biblioteca i lector de llibres EPUB. */
#define ESC_MODULE
#include "../mod.h"
#include <ctype.h>

enum { V_LIB, V_BOOK, V_MENU, V_TOC };
enum { A_OPEN = 100, A_SCAN, A_TAPPREV, A_TAPNEXT, A_MENU, A_TOC, A_TOCGO, A_FSIZE, A_FFAM, A_FSPC, A_FMAR, A_GOTO, A_CLOSE, A_READ };
enum { K_P = 1, K_H1, K_H2, K_Q, K_L, K_C };      /* tipus de bloc de text */

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

/* ═════════════ HTML -> blocs de text ═════════════
   Format: per bloc, un byte de tipus (K_*), el text (0x0B = salt de línia forçat) i «\n». */
typedef struct { char *id; size_t off; } Anchor;
static Anchor *anc; static int nanc, ancCap;
static void anc_free(void) { int i; for (i = 0; i < nanc; i++) free(anc[i].id); free(anc); anc = NULL; nanc = ancCap = 0; }
static void anc_add(const char *id, size_t off) {
  if (!id[0] || nanc >= 4000) return;
  if (nanc >= ancCap) { int c = ancCap ? ancCap * 2 : 128; Anchor *na = (Anchor *)realloc(anc, sizeof(Anchor) * (size_t)c); if (!na) return; anc = na; ancCap = c; }
  anc[nanc].id = strdup(id); anc[nanc].off = off;
  if (anc[nanc].id) nanc++;
}
typedef struct { Buf out, cur; int sp, hdr, inli, qd, ckind; } HC;
static int hc_kind(const HC *c) { return c->hdr ? c->hdr : c->inli ? K_L : c->qd ? K_Q : K_P; }
static void hc_flush(HC *c) {
  size_t k = c->cur.n;
  while (k > 0 && (c->cur.p[k - 1] == ' ' || c->cur.p[k - 1] == 0x0B)) k--;
  if (k > 0) { char m = (char)c->ckind; buf_put(&c->out, &m, 1); buf_put(&c->out, c->cur.p, k); buf_put(&c->out, "\n", 1); }
  c->cur.n = 0; if (c->cur.p) c->cur.p[0] = 0;
  c->sp = 1;
}
static void hc_special(HC *c, int kind, const char *text) {
  char m = (char)kind;
  hc_flush(c);
  buf_put(&c->out, &m, 1); buf_put(&c->out, text, strlen(text)); buf_put(&c->out, "\n", 1);
}
static void hc_char(HC *c, const char *s, size_t n) {
  if (!c->cur.n) c->ckind = hc_kind(c);
  buf_put(&c->cur, s, n); c->sp = 0;
}
static void hc_space(HC *c) { if (!c->sp && c->cur.n) { buf_put(&c->cur, " ", 1); c->sp = 1; } }
static void hc_text(HC *c, const char *s, const char *e) {
  while (s < e) {
    unsigned char ch = (unsigned char)*s;
    if (ch == '&') { unsigned cp; int k = ent_decode(s, e, &cp); if (k) { Buf t; memset(&t, 0, sizeof(t)); if (cp == ' ' || cp == 0xA0 || cp == '\t' || cp == '\n') hc_space(c); else if (cp >= 32 && cp != 0xAD && cp != 0x200B) { u8put(&t, cp); if (t.p) hc_char(c, t.p, t.n); } free(t.p); s += k; continue; } }
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') { hc_space(c); s++; continue; }
    if (ch < 32) { s++; continue; }
    if (ch == 0xC2 && s + 1 < e && ((unsigned char)s[1] == 0xA0)) { hc_space(c); s += 2; continue; }
    if (ch == 0xC2 && s + 1 < e && ((unsigned char)s[1] == 0xAD)) { s += 2; continue; }
    if (ch == 0xE2 && s + 2 < e && (unsigned char)s[1] == 0x80 && (unsigned char)s[2] == 0x8B) { s += 3; continue; }
    hc_char(c, s, 1); s++;
  }
}
static int is_in(const char *n, const char *list) {         /* list: «a,b,c,» */
  size_t k = strlen(n); const char *p = list;
  while (*p) { const char *q = strchr(p, ','); if (!q) break; if ((size_t)(q - p) == k && !memcmp(p, n, k)) return 1; p = q + 1; }
  return 0;
}
static char *html_convert(const char *h, size_t n, size_t *outn) {
  HC c; const char *p = h, *end = h + n, *lt, *te; char nm[24], val[200]; int closing;
  memset(&c, 0, sizeof(c)); c.sp = 1;
  anc_free();
  while ((lt = tag_next(p, end, &te)) != NULL) {
    if (lt > p) hc_text(&c, p, lt);
    p = te + 1;
    if (lt[1] == '!' || lt[1] == '?') { if (!memcmp(lt, "<![CDATA[", 9) && te - 2 >= lt + 9) hc_text(&c, lt + 9, te - 2); continue; }
    closing = tag_name(lt, te, nm, sizeof(nm));
    if (!closing && attr_get(lt, te, "id", val, sizeof(val))) anc_add(val, c.out.n + (c.cur.n ? 1 + c.cur.n : 0));
    if (!closing && nm[0] == 'a' && !nm[1] && attr_get(lt, te, "name", val, sizeof(val))) anc_add(val, c.out.n + (c.cur.n ? 1 + c.cur.n : 0));
    if (!closing && is_in(nm, "head,script,style,")) {      /* salta fins a l'etiqueta de tancament */
      const char *t2, *l2;
      if (te[-1] == '/') continue;
      while ((l2 = tag_next(p, end, &t2)) != NULL) { char n2[24]; int cl2 = tag_name(l2, t2, n2, sizeof(n2)); p = t2 + 1; if (cl2 && !strcmp(n2, nm)) break; }
      if (!l2) break;
      continue;
    }
    if (nm[0] == 'h' && nm[1] >= '1' && nm[1] <= '6' && !nm[2]) { hc_flush(&c); c.hdr = closing ? 0 : (nm[1] <= '2' ? K_H1 : K_H2); }
    else if (!strcmp(nm, "li")) { hc_flush(&c); c.inli = closing ? 0 : 1; }
    else if (!strcmp(nm, "blockquote")) { hc_flush(&c); if (closing) { if (c.qd > 0) c.qd--; } else c.qd++; }
    else if (!strcmp(nm, "br")) { if (c.cur.n && c.cur.p[c.cur.n - 1] != 0x0B) { buf_put(&c.cur, "\x0B", 1); c.sp = 1; } }
    else if (!strcmp(nm, "hr")) hc_special(&c, K_C, "* * *");
    else if (!closing && is_in(nm, "img,image,")) {
      if (!c.cur.n) {
        char alt[120], t[160];
        if (attr_get(lt, te, "alt", alt, sizeof(alt)) && alt[0]) { plain_text(alt, alt + strlen(alt), t + 0, sizeof(t) - 12); memmove(t + 9, t, strlen(t) + 1); memcpy(t, "[imatge: ", 9); strcat(t, "]"); hc_special(&c, K_C, t); }
        else if (!c.out.n) hc_special(&c, K_C, "[imatge]");
      }
    }
    else if (is_in(nm, "td,th,")) { if (!closing) hc_space(&c); }
    else if (is_in(nm, "p,div,section,article,aside,header,footer,tr,ul,ol,dl,dt,dd,figure,figcaption,table,center,pre,nav,body,")) hc_flush(&c);
  }
  if (p < end) hc_text(&c, p, end);
  hc_flush(&c);
  free(c.cur.p);
  if (!c.out.p) { c.out.p = (char *)calloc(1, 1); c.out.n = 0; }
  *outn = c.out.n;
  return c.out.p;
}

/* ═════════════ Llibre obert ═════════════ */
typedef struct { char *path; unsigned long usize; double start; } Chap;
typedef struct { char *title; int chap, lvl; char frag[48]; } Toc;
typedef struct {
  char path[300], title[120], author[80]; long size, mtime; int nch; long total;
  int chap; long off; int pm; time_t last;
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

/* ═════════════ Configuració de lectura i fonts ═════════════ */
static const int FSZ[8] = { 30, 34, 38, 42, 46, 52, 58, 66 };
static const int SPC[3] = { 118, 133, 152 };
typedef struct { int size, fam, spc, mar; } Cfg;
static Cfg cfg = { 3, 1, 1, 1 };
static Store cfgst = { 0, 0, 0, CFG "escriptori_lector_cfg.txt" };
static void cfg_load(void) {
  int a, b, c, d;
  cfgst.loaded = 0; st_load(&cfgst);
  if (cfgst.n > 0 && sscanf(cfgst.it[0], "%d %d %d %d", &a, &b, &c, &d) == 4 && a >= 0 && a < 8 && b >= 0 && b < 2 && c >= 0 && c < 3 && d >= 0 && d < 3) { cfg.size = a; cfg.fam = b; cfg.spc = c; cfg.mar = d; }
}
static void cfg_save(void) {
  char t[40];
  snprintf(t, sizeof(t), "%d %d %d %d", cfg.size, cfg.fam, cfg.spc, cfg.mar);
  st_load(&cfgst);
  st_clear(&cfgst); st_insert(&cfgst, 0, t); st_save(&cfgst);
}
static int margin_px(void) { return U * (cfg.mar == 0 ? 2 : cfg.mar == 1 ? 4 : 6) / 4 + U / 4; }
typedef struct { int fam, bold, size; ifont *f; } FCache;
static FCache fcache[24]; static int nfc;
static ifont *getfont(int fam, int bold, int size) {
  int i; ifont *f;
  for (i = 0; i < nfc; i++) if (fcache[i].fam == fam && fcache[i].bold == bold && fcache[i].size == size) return fcache[i].f;
  f = OpenFont(fam ? (bold ? "LiberationSerif-Bold" : "LiberationSerif") : (bold ? DEFAULTFONTB : DEFAULTFONT), size, 1);
  if (!f) f = OpenFont(bold ? DEFAULTFONTB : DEFAULTFONT, size, 1);
  if (!f) return bold ? fBold : fText;
  if (nfc >= 24) nfc = 0;
  fcache[nfc].fam = fam; fcache[nfc].bold = bold; fcache[nfc].size = size; fcache[nfc].f = f; nfc++;
  return f;
}
static int block_size(int kind) { int sz = FSZ[cfg.size]; return kind == K_H1 ? sz * 5 / 4 : sz; }
static ifont *block_font(int kind) { return getfont(cfg.fam, kind == K_H1 || kind == K_H2, block_size(kind)); }

/* ═════════════ Capítol: càrrega, composició i pàgines ═════════════ */
typedef struct { int off, len; short x, pre, h; unsigned char kind, first; } Line;
static char *txt; static size_t txtlen;
static Line *L; static int nL, capL;
static int *pgl, npg, cp, curch = -1;
static int needLayout;
static void ch_free(void) {
  free(txt); txt = NULL; txtlen = 0;
  free(L); L = NULL; nL = capL = 0;
  free(pgl); pgl = NULL; npg = 0; cp = 0;
  anc_free();
}
static void emit(size_t ls, size_t le, int kind, int x, int pre, int h, int first) {
  Line *l;
  if (nL >= 200000) return;
  if (nL >= capL) { int c = capL ? capL * 2 : 512; Line *nl = (Line *)realloc(L, sizeof(Line) * (size_t)c); if (!nl) return; L = nl; capL = c; }
  l = &L[nL++];
  l->off = (int)ls; l->len = (int)(le - ls); l->x = (short)x; l->pre = (short)pre; l->h = (short)h; l->kind = (unsigned char)kind; l->first = (unsigned char)first;
}
static int wwidth(size_t s, size_t n) {
  char t[256]; size_t k = n > 255 ? 255 : n; int w;
  memcpy(t, txt + s, k); t[k] = 0;
  w = StringWidth(t);
  return n > 255 ? (int)((double)w * (double)n / 255.0) : w;
}
static void layout_block(int kind, size_t bs, size_t be, int prevkind, int *padnext) {
  int sz = block_size(kind), lh, spw, x0, ind = 0, avail, lw = 0, first = 1, pend, Wd = SW - 2 * margin_px();
  size_t i = bs, ls = bs, le = bs;
  SetFont(block_font(kind), BLACK);
  lh = sz * SPC[cfg.spc] / 100;
  spw = StringWidth(" "); if (spw < 2) spw = sz / 4;
  x0 = kind == K_Q ? U * 3 / 2 : kind == K_L ? U + U / 4 : 0;
  if (kind == K_P && prevkind == K_P) ind = sz;
  pend = *padnext; *padnext = 0;
  if (prevkind == 0) pend = 0;
  switch (kind) {
    case K_H1: if (prevkind) pend += lh * 6 / 5; *padnext = lh * 4 / 5; break;
    case K_H2: if (prevkind) pend += lh * 7 / 10; *padnext = lh / 5; break;
    case K_C: pend += lh * 6 / 10; *padnext = lh * 6 / 10; break;
    case K_Q: case K_L: if (prevkind != kind) pend += lh * 3 / 10; break;
    default: if ((prevkind == K_Q || prevkind == K_L) && prevkind != kind) pend += lh * 3 / 10; break;
  }
  avail = Wd - x0 - ind;
  while (i < be) {
    unsigned char c = (unsigned char)txt[i]; size_t ws; int ww, need;
    if (c == ' ') { i++; continue; }
    if (c == 0x0B) { if (le > ls || lw == 0) emit(ls, le, kind, x0 + (first ? ind : 0), pend, lh, first && kind == K_L); pend = 0; first = 0; ls = le = i + 1; lw = 0; avail = Wd - x0; i++; continue; }
    ws = i;
    while (i < be && txt[i] != ' ' && txt[i] != 0x0B) i++;
    ww = wwidth(ws, i - ws);
    need = lw ? lw + spw + ww : ww;
    if (lw && need > avail) { emit(ls, le, kind, x0 + (first ? ind : 0), pend, lh, first && kind == K_L); pend = 0; first = 0; avail = Wd - x0; lw = 0; need = ww; }
    if (!lw) ls = ws;
    if (ww > avail) {                      /* paraula més ampla que la línia: es parteix per caràcters */
      size_t j = ws, ps = ws; int cw = 0;
      while (j < i) {
        size_t k = j + 1; int w1;
        while (k < i && ((unsigned char)txt[k] & 0xC0) == 0x80) k++;
        w1 = wwidth(j, k - j);
        if (cw + w1 > avail && j > ps) { emit(ps, j, kind, x0 + (first ? ind : 0), pend, lh, first && kind == K_L); pend = 0; first = 0; avail = Wd - x0; ps = j; cw = 0; }
        cw += w1; j = k;
      }
      ls = ps; le = i; lw = cw;
      continue;
    }
    lw = need; le = i;
  }
  if (le > ls) emit(ls, le, kind, x0 + (first ? ind : 0), pend, lh, first && kind == K_L);
}
static int page_area(void) { return SH - 3 * U - (SB + U); }
static void paginate(void) {
  int i, j, y = 0, start = 0, AH = page_area();
  free(pgl);
  pgl = (int *)malloc(sizeof(int) * (size_t)(nL + 2));
  npg = 1;
  if (!pgl) { npg = 0; return; }
  pgl[0] = 0;
  for (i = 0; i < nL; i++) {
    int adv = L[i].h + (i == start ? 0 : L[i].pre);
    if (i != start && y + adv > AH) {
      int s = i;
      if (i - 1 > start && (L[i - 1].kind == K_H1 || L[i - 1].kind == K_H2)) {      /* un títol no es queda sol al final */
        s = i - 1;
        while (s - 1 > start && L[s - 1].kind == L[i - 1].kind) s--;
      }
      pgl[npg++] = s; start = s;
      y = 0;
      for (j = s; j < i; j++) y += L[j].h + (j == s ? 0 : L[j].pre);
      adv = L[i].h + L[i].pre;
    }
    y += adv;
  }
  pgl[npg] = nL;
}
static void layout_all(void) {
  size_t p = 0; int prevkind = 0, padnext = 0;
  nL = 0;
  while (p < txtlen) {
    int kind = (unsigned char)txt[p]; size_t bs = p + 1, be = bs;
    while (be < txtlen && txt[be] != '\n') be++;
    if (kind < K_P || kind > K_C) kind = K_P;
    layout_block(kind, bs, be, prevkind, &padnext);
    prevkind = kind; p = be + 1;
  }
  paginate();
}
static int page_of(long off) {
  int lo = 0, hi = nL - 1, ln = 0, pg;
  if (nL == 0 || npg == 0) return 0;
  while (lo <= hi) { int m = (lo + hi) / 2; if (L[m].off <= off) { ln = m; lo = m + 1; } else hi = m - 1; }
  for (pg = npg - 1; pg > 0; pg--) if (pgl[pg] <= ln) break;
  return pg;
}
static long page_off(void) { return (nL && npg && pgl[cp] < nL) ? L[pgl[cp]].off : 0; }
static int ch_load(int ci) {
  int zi; size_t n = 0; char *html = NULL;
  ch_free();
  curch = ci;
  zi = zip_find(&Z, chs[ci].path);
  if (zi >= 0) html = zip_read(&Z, zi, &n);
  if (html) { if (n > 150000) ShowHourglass(); txt = html_convert(html, n, &txtlen); free(html); if (n > 150000) HideHourglass(); }
  if (!txt) { txt = strdup("\x01(No s'ha pogut llegir aquest capítol.)\n"); txtlen = txt ? strlen(txt) : 0; }
  layout_all();
  return 0;
}
static int anc_off(const char *id) { int i; for (i = 0; i < nanc; i++) if (!strcmp(anc[i].id, id)) return (int)anc[i].off + 1; return 0; }

/* ═════════════ Biblioteca i progrés ═════════════ */
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
static void prog_apply(Book *b) {
  int i; char *f[6], t[900];
  b->chap = 0; b->off = 0; b->pm = 0; b->last = 0;
  for (i = 0; i < prog.n; i++) {
    snprintf(t, sizeof(t), "%s", prog.it[i]);
    if (split_tab(t, f, 6) >= 5 && !strcmp(f[0], b->path)) { b->chap = atoi(f[1]); b->off = atol(f[2]); b->pm = atoi(f[3]); b->last = (time_t)atol(f[4]); return; }
  }
}
static void prog_save(const Book *b) {
  int i, k = -1; char t[900], pre[340];
  snprintf(t, sizeof(t), "%s\t%d\t%ld\t%d\t%ld", b->path, b->chap, b->off, b->pm, (long)b->last);
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
static void lib_scan(void) {
  int i, j; char t[900], *f[8]; Book *nb; int nn = 0;
  ch_free(); curch = -1; rb = -1; bk_close();
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

/* ═════════════ Lectura: posició, progrés, canvi de pàgina ═════════════ */
static int cur_pm(void) {
  double frac, pos;
  if (curch < 0 || !nch) return 0;
  if (curch == nch - 1 && npg > 0 && cp >= npg - 1) return 1000;
  frac = txtlen ? (double)page_off() / (double)txtlen : 0;
  pos = chs[curch].start + frac * (double)(chs[curch].usize ? chs[curch].usize : 1);
  return totalBytes > 0 ? (int)(pos * 1000.0 / totalBytes) : 0;
}
static void save_pos(void) {
  Book *b;
  if (rb < 0 || rb >= nbk || curch < 0) return;
  b = &bk[rb];
  b->chap = curch; b->off = page_off(); b->pm = cur_pm(); b->last = time(NULL);
  prog_save(b);
}
/* un capítol només amb una imatge (portada) no té res per llegir: es salta */
static int chap_has_text(void) { return nL > 0 && !(txtlen == 10 && !memcmp(txt, "\x06[imatge]", 9)); }
static void go_next(void) {
  int c0 = curch;
  if (cp < npg - 1) { cp++; return; }
  while (curch + 1 < nch) { ch_load(curch + 1); cp = 0; if (chap_has_text()) return; }
  ch_load(c0); cp = npg > 0 ? npg - 1 : 0;       /* no hi ha més text: et quedes al final */
}
static void go_prev(void) {
  int c0 = curch;
  if (cp > 0) { cp--; return; }
  while (curch > 0) { ch_load(curch - 1); cp = npg > 0 ? npg - 1 : 0; if (chap_has_text()) return; }
  ch_load(c0); cp = 0;
}
static void goto_chapter(int ci, const char *frag) {
  long off = 0;
  if (ci < 0 || ci >= nch) return;
  ch_load(ci);
  if (frag && frag[0]) off = anc_off(frag);
  cp = page_of(off);
}
static int book_open(int i) {
  Book *b; int ci;
  if (i < 0 || i >= nbk) return -1;
  b = &bk[i];
  ch_free(); curch = -1;
  ShowHourglass();
  if (bk_load(b, 1) != 0) { HideHourglass(); return -1; }
  HideHourglass();
  rb = i;
  ci = b->chap >= 0 && b->chap < nch ? b->chap : 0;
  ch_load(ci);
  cp = page_of(b->off);
  if (!b->last && !chap_has_text()) go_next();       /* llibre nou: comença pel primer text */
  needLayout = 0;
  return 0;
}
static void relayout(void) {
  long keep = page_off();
  if (curch < 0) return;
  layout_all();
  cp = page_of(keep);
  needLayout = 0;
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
static void draw_reading(void) {
  int i, ya, a, b, mar = margin_px(), Wd = SW - 2 * mar;
  if (npg == 0) return;
  a = pgl[cp]; b = pgl[cp + 1]; ya = SB + U;
  for (i = a; i < b; i++) {
    const Line *l = &L[i]; char t[600]; int n = l->len, flags = ALIGN_LEFT | VALIGN_TOP, x = mar + l->x, w = Wd - l->x, sz = block_size(l->kind);
    if (i > a) ya += l->pre;
    if (n > (int)sizeof(t) - 1) n = (int)sizeof(t) - 1;
    memcpy(t, txt + l->off, (size_t)n); t[n] = 0;
    { char *q; for (q = t; *q; q++) if (*q == 0x0B) *q = ' '; }
    if (l->kind == K_H1 || l->kind == K_C) { flags = ALIGN_CENTER | VALIGN_TOP; x = mar; w = Wd; }
    SetFont(block_font(l->kind), BLACK);
    if (n > 0) DrawTextRect(x, ya, w, l->h * 3 / 2, t, flags);
    if (l->kind == K_L && l->first) disc(mar + U / 2, ya + sz * 3 / 5, sz / 8 + 1, BLACK);
    ya += l->h;
  }
}
static void v_book(View *v) {
  int by = SH - 3 * U, mar = margin_px(), pm; char info[160], title[120] = "";
  (void)v;
  if (curch < 0) { header("Lector", "No hi ha cap llibre obert."); return; }
  if (needLayout) relayout();
  draw_reading();
  /* zones de tacte: tercer esquerre = enrere, la resta = endavant; la barra inferior obre el menú */
  add_hit(0, SB, SW * 3 / 10, by - SB, A_TAPPREV, 0);
  add_hit(SW * 3 / 10, SB, SW - SW * 3 / 10, by - SB, A_TAPNEXT, 0);
  pm = cur_pm();
  progress_bar(mar, by + U / 4, SW - 2 * mar, 4, pm);
  {
    int ti = -1, k;
    for (k = 0; k < ntoc; k++) if (toc[k].chap <= curch) ti = k;
    if (ti >= 0) snprintf(title, sizeof(title), "%s", toc[ti].title);
    else if (rb >= 0) snprintf(title, sizeof(title), "%s", bk[rb].title);
  }
  snprintf(info, sizeof(info), "pàg. %d de %d  ·  %d,%d %%", cp + 1, npg, pm / 10, pm % 10);
  SetFont(fSmall, BLACK);
  DrawTextRect(mar, by + U / 2, SW - 2 * mar - 9 * U, 2 * U, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  DrawTextRect(SW - mar - 9 * U, by + U / 2, 9 * U, 2 * U, info, ALIGN_RIGHT | VALIGN_MIDDLE);
  add_hit(0, by, SW, 3 * U, A_MENU, 0);
}
static void v_menu(View *v) {
  int y = TOP, bh = 2 * U, gap = U / 3, w3 = (SW - U - 2 * gap) / 3, w2 = (SW - U - gap) / 2, x0 = U / 2; char t[60];
  (void)v;
  if (rb < 0 || curch < 0) { header("Lector", "No hi ha cap llibre obert."); return; }
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
  button(x0 + 2 * (w3 + gap), y, w3, bh, "Amples", cfg.mar == 2, A_FMAR, 2); y += bh + 2 * gap;
  button(x0, y, SW - U, bh, "Continguts", 0, A_TOC, 0); y += bh + gap;
  button(x0, y, SW - U, bh, "Anar a un percentatge", 0, A_GOTO, 0); y += bh + gap;
  button(x0, y, SW - U, bh, "Tanca el llibre", 0, A_CLOSE, 0); y += bh + gap;
  if (y + 3 * U < BOTTOM + NAVH) {
    SetFont(block_font(K_P), BLACK);
    DrawTextRect(x0, y + gap, SW - U, BOTTOM + NAVH - y - gap, "Així es veurà el text del llibre: lletra, interlineat i marges.", ALIGN_LEFT | VALIGN_TOP);
  }
}
static void v_toc(View *v) {
  int y = TOP, rh = 2 * U + U / 4, per, i, cur = -1, k; char sub[40];
  if (rb < 0 || curch < 0) { header("Continguts", ""); return; }
  for (k = 0; k < ntoc; k++) if (toc[k].chap <= curch) cur = k;
  snprintf(sub, sizeof(sub), ntoc == 1 ? "%d entrada" : "%d entrades", ntoc);
  header("Continguts", sub);
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = ntoc ? (ntoc + per - 1) / per : 1;
  if (v->a == 0) { v->a = 1; if (cur >= 0) v->page = cur / per; }
  if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < ntoc && i < (v->page + 1) * per; i++) {
    int ind = toc[i].lvl * U;
    SetFont(i == cur ? fBold : fText, BLACK);
    DrawTextRect(U / 2 + ind, y, SW - U - ind - (i == cur ? 2 * U : 0), rh - U / 4, toc[i].title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (i == cur) disc(SW - U - U / 2, y + (rh - U / 4) / 2, U / 4, BLACK);
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LGRAY);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_TOCGO, i);
    y += rh;
  }
}
static void mod_icon(int x, int y, int s) {
  int w = s * 3 / 8, h = s * 5 / 8, ty = y + s / 5, x0 = x + s / 8, i, g = s / 16;
  FillArea(x0, ty, 2 * w + g, h, BLACK);
  FillArea(x0 + 4, ty + 4, w - 6, h - 8, WHITE); FillArea(x0 + w + g + 2, ty + 4, w - 6, h - 8, WHITE);
  for (i = 0; i < 4; i++) { FillArea(x0 + 10, ty + h / 6 + i * h / 5, w - 22, 3, BLACK); FillArea(x0 + w + g + 12, ty + h / 6 + i * h / 5, w - 22, 3, BLACK); }
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_LIB: v_lib(v); break;
    case V_BOOK: v_book(v); break;
    case V_MENU: v_menu(v); break;
    case V_TOC: v_toc(v); break;
  }
}
static void goto_fn(const char *t) {
  double pct = parse_num(t), pos, frac; int k, ci;
  if (curch < 0 || nch < 1) return;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  pos = totalBytes * pct / 100.0; ci = nch - 1;
  for (k = 0; k < nch; k++) if (chs[k].start + (double)(chs[k].usize ? chs[k].usize : 1) > pos) { ci = k; break; }
  frac = (pos - chs[ci].start) / (double)(chs[ci].usize ? chs[ci].usize : 1);
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  ch_load(ci);
  cp = page_of((long)(frac * (double)txtlen));
  save_pos();
  POP();
}
static void turn(int dir) {
  int c0 = curch, p0 = cp;
  if (curch < 0) return;
  if (dir > 0) go_next(); else go_prev();
  if (c0 == curch && p0 == cp) return;
  save_pos();
  draw(++turns % 6 == 0);        /* cada sis pàgines, refresc complet per netejar la tinta */
}
static void mod_action(View *v, int a, int arg) {
  (void)v;
  switch (a) {
    case A_OPEN:
      if (book_open(arg) != 0) { say("No s'ha pogut obrir aquest llibre."); draw(1); }
      else push(V_BOOK, arg, 0, 0, NULL);
      break;
    case A_SCAN: lib_scan(); v->page = 0; draw(1); break;
    case A_TAPPREV: turn(-1); break;
    case A_TAPNEXT: turn(1); break;
    case A_MENU: save_pos(); push(V_MENU, 0, 0, 0, NULL); break;
    case A_TOC: push(V_TOC, 0, 0, 0, NULL); break;
    case A_TOCGO:
      if (arg >= 0 && arg < ntoc) { goto_chapter(toc[arg].chap, toc[arg].frag); save_pos(); POP(); POP(); draw(1); }
      break;
    case A_FSIZE: cfg.size += arg; if (cfg.size < 0) cfg.size = 0; if (cfg.size > 7) cfg.size = 7; cfg_save(); needLayout = 1; draw(0); break;
    case A_FFAM: cfg.fam = arg; cfg_save(); needLayout = 1; draw(0); break;
    case A_FSPC: cfg.spc = arg; cfg_save(); needLayout = 1; draw(0); break;
    case A_FMAR: cfg.mar = arg; cfg_save(); needLayout = 1; draw(0); break;
    case A_GOTO: ask("Anar al percentatge del llibre (0-100)", "", goto_fn); break;
    case A_READ: POP(); draw(1); break;
    case A_CLOSE: save_pos(); ch_free(); curch = -1; rb = -1; bk_close(); POP(); POP(); draw(1); break;
  }
}
static int mod_back(View *v) { if (v->type == V_BOOK) save_pos(); return 0; }
static void mod_open(View *v) { (void)v; cfg_load(); lib_scan(); }
static Module M = { ESC_API, "lector", "Lector", 0, mod_icon, mod_draw, mod_action, mod_back, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
