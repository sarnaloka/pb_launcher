/* Bus AMB per a PocketBook — lògica portable. Port de src/core.js. */
#include "amb.h"
#include "afm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

DB db;
static int *g_rank;          /* posició de cada línia en l'ordre natural */
static uint8_t *g_mark;      /* marques temporals per parada */

/* ───────────── càrrega ───────────── */
int amb_load(const uint8_t *blob, long size) {
  const int32_t *h = (const int32_t *)blob;
  const uint8_t *p = blob + 48;
  int nSr, strBytes, i, t;
  if (size < 48 || h[0] != 0x31424d41) return -1;
  db.nR = h[1]; db.nS = h[2]; db.nSvc = h[3]; db.nEx = h[4]; db.nT = h[5]; db.nRows = h[6]; db.nHeads = h[7];
  nSr = h[8]; strBytes = h[9]; db.made = h[10];
#define TAKE(field, type, count) do { db.field = (const type *)p; p += (((size_t)(count) * sizeof(type)) + 3) & ~(size_t)3; } while (0)
  TAKE(svcDays, uint8_t, db.nSvc); TAKE(svcStart, int32_t, db.nSvc); TAKE(svcEnd, int32_t, db.nSvc); TAKE(ex, int32_t, db.nEx * 3);
  TAKE(tRoute, uint16_t, db.nT); TAKE(tSvc, uint16_t, db.nT); TAKE(tDir, uint8_t, db.nT); TAKE(tHead, uint16_t, db.nT);
  TAKE(tStart, int32_t, db.nT); TAKE(tLen, uint16_t, db.nT); TAKE(tT0, int32_t, db.nT);
  TAKE(sStop, uint16_t, db.nRows); TAKE(sDelta, uint16_t, db.nRows);
  TAKE(srStart, int32_t, db.nS + 1); TAKE(srList, uint16_t, nSr); TAKE(routeOrder, uint16_t, db.nR);
#undef TAKE
  if (p + strBytes > blob + size) return -2;
  {
    const char *s = (const char *)p;
    int total = db.nR * 3 + db.nS * 3 + db.nHeads, k = 0;
    const char **all = (const char **)malloc(sizeof(char *) * (size_t)total);
    if (!all) return -3;
    for (k = 0; k < total; k++) { all[k] = s; s += strlen(s) + 1; }
    db.rShort = (const char **)malloc(sizeof(char *) * (size_t)db.nR * 3);
    db.rLong = db.rShort + db.nR; db.rColor = db.rLong + db.nR;
    db.sCode = (const char **)malloc(sizeof(char *) * (size_t)db.nS * 3);
    db.sName = db.sCode + db.nS; db.sQ = db.sName + db.nS;
    db.heads = (const char **)malloc(sizeof(char *) * (size_t)(db.nHeads + 1));
    if (!db.rShort || !db.sCode || !db.heads) return -3;
    k = 0;
    for (i = 0; i < db.nR; i++) { db.rShort[i] = all[k++]; db.rLong[i] = all[k++]; db.rColor[i] = all[k++]; }
    for (i = 0; i < db.nS; i++) { db.sCode[i] = all[k++]; db.sName[i] = all[k++]; db.sQ[i] = all[k++]; }
    for (i = 0; i < db.nHeads; i++) db.heads[i] = all[k++];
    free(all);
  }
  db.sTime = (int32_t *)malloc(sizeof(int32_t) * (size_t)db.nRows);
  db.rowTrip = (int32_t *)malloc(sizeof(int32_t) * (size_t)db.nRows);
  db.stopStart = (int32_t *)calloc((size_t)db.nS + 1, sizeof(int32_t));
  db.stopRows = (int32_t *)malloc(sizeof(int32_t) * (size_t)db.nRows);
  g_rank = (int *)malloc(sizeof(int) * (size_t)db.nR);
  g_mark = (uint8_t *)calloc((size_t)db.nS, 1);
  if (!db.sTime || !db.rowTrip || !db.stopStart || !db.stopRows || !g_rank || !g_mark) return -3;
  for (t = 0; t < db.nT; t++) {
    int a = db.tStart[t], e, acc;
    if (a < 0) continue;
    e = a + db.tLen[t]; acc = db.tT0[t];
    for (i = a; i < e; i++) { if (i > a) acc += db.sDelta[i]; db.sTime[i] = acc; db.rowTrip[i] = t; }
  }
  {
    int32_t *fill = (int32_t *)malloc(sizeof(int32_t) * (size_t)db.nS);
    if (!fill) return -3;
    for (i = 0; i < db.nRows; i++) db.stopStart[db.sStop[i] + 1]++;
    for (i = 0; i < db.nS; i++) { db.stopStart[i + 1] += db.stopStart[i]; fill[i] = db.stopStart[i]; }
    for (i = 0; i < db.nRows; i++) db.stopRows[fill[db.sStop[i]]++] = i;
    free(fill);
  }
  for (i = 0; i < db.nR; i++) g_rank[db.routeOrder[i]] = i;
  return 0;
}

const char *amb_route_label(int r) { return db.rShort[r][0] ? db.rShort[r] : (db.rLong[r][0] ? db.rLong[r] : "?"); }

/* ───────────── text ───────────── */
static int utf8_next(const unsigned char **pp) {
  const unsigned char *p = *pp; int c = *p++;
  if (c >= 0xF0 && p[0] && p[1] && p[2]) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
  else if (c >= 0xE0 && p[0] && p[1]) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
  else if (c >= 0xC0 && p[0]) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
  *pp = p; return c;
}
void amb_fold(const char *s, char *out, int outsz) {
  const unsigned char *p = (const unsigned char *)s; int n = 0;
  while (*p && n < outsz - 1) {
    int c = utf8_next(&p), o;
    if (c >= 'A' && c <= 'Z') o = c + 32;
    else if (c < 128) o = c;
    else if ((c >= 0xC0 && c <= 0xC4) || (c >= 0xE0 && c <= 0xE4)) o = 'a';
    else if ((c >= 0xC8 && c <= 0xCB) || (c >= 0xE8 && c <= 0xEB)) o = 'e';
    else if ((c >= 0xCC && c <= 0xCF) || (c >= 0xEC && c <= 0xEF)) o = 'i';
    else if ((c >= 0xD2 && c <= 0xD6) || (c >= 0xF2 && c <= 0xF6)) o = 'o';
    else if ((c >= 0xD9 && c <= 0xDC) || (c >= 0xF9 && c <= 0xFC)) o = 'u';
    else if (c == 0xC7 || c == 0xE7) o = 'c';
    else if (c == 0xD1 || c == 0xF1) o = 'n';
    else if (c == 0xB7) continue;
    else o = '?';
    if (o == '.') continue;
    out[n++] = (char)o;
  }
  out[n] = 0;
}
/* UTF-8 → WinAnsi (com pdfText a core.js) */
static void to_win(const char *s, char *out, int outsz) {
  const unsigned char *p = (const unsigned char *)s; int n = 0;
  while (*p && n < outsz - 4) {
    int c = utf8_next(&p);
    if (c == 0x2018 || c == 0x2019) out[n++] = '\'';
    else if (c == 0x201C || c == 0x201D) out[n++] = '"';
    else if (c == 0x2013 || c == 0x2014) out[n++] = '-';
    else if (c == 0x2026) { out[n++] = '.'; out[n++] = '.'; out[n++] = '.'; }
    else if (c == 0x140) { out[n++] = 'l'; out[n++] = (char)0xB7; }
    else if (c == 0x13F) { out[n++] = 'L'; out[n++] = (char)0xB7; }
    else if (c > 0xFF) out[n++] = '?';
    else out[n++] = (char)c;
  }
  out[n] = 0;
}

/* ───────────── dates i calendari ───────────── */
static time_t day_start(time_t t, int addDays, struct tm *outTm) {
  struct tm tm = *localtime(&t);
  tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0; tm.tm_mday += addDays; tm.tm_isdst = -1;
  t = mktime(&tm);
  if (outTm) *outTm = tm;
  return t;
}
#define NCACHE 12
static struct { int date; uint8_t *act; } g_cache[NCACHE];
static int g_cacheNext;
static const uint8_t *active_services(const struct tm *tm) {
  int di = (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday, i, bit = 1 << ((tm->tm_wday + 6) % 7);
  uint8_t *a;
  for (i = 0; i < NCACHE; i++) if (g_cache[i].date == di && g_cache[i].act) return g_cache[i].act;
  i = g_cacheNext; g_cacheNext = (g_cacheNext + 1) % NCACHE;
  if (!g_cache[i].act) g_cache[i].act = (uint8_t *)malloc((size_t)db.nSvc + 1);
  a = g_cache[i].act; g_cache[i].date = di;
  for (i = 0; i < db.nSvc; i++) a[i] = (uint8_t)((db.svcDays[i] & bit) && di >= db.svcStart[i] && di <= db.svcEnd[i]);
  for (i = 0; i < db.nEx; i++) if (db.ex[i * 3 + 1] == di) a[db.ex[i * 3]] = (uint8_t)(db.ex[i * 3 + 2] == 1);
  return a;
}

const char *amb_trip_dest(int t) {
  const char *h = db.heads[db.tHead[t]];
  if (h[0]) return h;
  return db.sName[db.sStop[db.tStart[t] + db.tLen[t] - 1]];
}

/* ───────────── sortides ───────────── */
static int cmp_dep(const void *a, const void *b) {
  const Dep *x = (const Dep *)a, *y = (const Dep *)b;
  if (x->when != y->when) return x->when < y->when ? -1 : 1;
  return x->row - y->row;
}
int amb_departures(int stop, time_t now, time_t from, time_t to, Dep **out) {
  int cap = 64, n = 0, off, k, a = db.stopStart[stop], b = db.stopStart[stop + 1];
  Dep *d = (Dep *)malloc(sizeof(Dep) * (size_t)cap);
  *out = d;
  if (!d) return 0;
  for (off = -1; off <= 1; off++) {
    struct tm tm; time_t base = day_start(now, off, &tm);
    const uint8_t *act;
    if (base > to || base + 30 * 3600 < from) continue;
    act = active_services(&tm);
    for (k = a; k < b; k++) {
      int row = db.stopRows[k], t = db.rowTrip[row]; time_t when;
      if (!act[db.tSvc[t]]) continue;
      if (row == db.tStart[t] + db.tLen[t] - 1) continue;
      when = base + db.sTime[row];
      if (when < from || when > to) continue;
      if (n == cap) { Dep *nd; cap *= 2; nd = (Dep *)realloc(d, sizeof(Dep) * (size_t)cap); if (!nd) break; d = nd; *out = d; }
      d[n].when = when; d[n].trip = t; d[n].row = row; d[n].route = db.tRoute[t]; n++;
    }
  }
  qsort(d, (size_t)n, sizeof(Dep), cmp_dep);
  return n;
}

/* ───────────── línies i sentits d'una parada ───────────── */
static PatKey pat_key(int row, int t) {
  PatKey k; int a = db.tStart[t];
  k.last = db.sStop[a + db.tLen[t] - 1]; k.next = db.sStop[row + 1]; k.prev = row > a ? db.sStop[row - 1] : -1;
  return k;
}
static int key_eq(PatKey a, PatKey b) { return a.last == b.last && a.next == b.next && a.prev == b.prev; }
static int dest_key(int t) { return db.heads[db.tHead[t]][0] ? (int)db.tHead[t] : -(int)(db.sStop[db.tStart[t] + db.tLen[t] - 1]) - 1; }
static const char *dest_str(int k) { return k >= 0 ? db.heads[k] : db.sName[-k - 1]; }
#define MAXD 8
typedef struct { PatKey key; int route, row, trip, n, rem, nd, dk[MAXD], dc[MAXD], used; } Pat;

static int cmp_group(const void *a, const void *b) {
  const Group *x = (const Group *)a, *y = (const Group *)b;
  if (g_rank[x->route] != g_rank[y->route]) return g_rank[x->route] - g_rank[y->route];
  return y->n - x->n;
}
int amb_lines_at_stop(int stop, Group **out) {
  int a = db.stopStart[stop], b = db.stopStart[stop + 1], k, np = 0, cap = 32, ng = 0, gcap = 16, i, j;
  Pat *ps = (Pat *)malloc(sizeof(Pat) * (size_t)cap);
  Group *gs = (Group *)malloc(sizeof(Group) * (size_t)gcap);
  int *left, *grp, *rest;
  *out = gs;
  if (!ps || !gs) { free(ps); return 0; }
  for (k = a; k < b; k++) {
    int row = db.stopRows[k], t = db.rowTrip[row], end = db.tStart[t] + db.tLen[t], r = db.tRoute[t], dk;
    PatKey key; Pat *p = NULL;
    if (row == end - 1) continue;
    key = pat_key(row, t);
    for (i = 0; i < np; i++) if (ps[i].route == r && key_eq(ps[i].key, key)) { p = &ps[i]; break; }
    if (!p) {
      if (np == cap) { Pat *n2; cap *= 2; n2 = (Pat *)realloc(ps, sizeof(Pat) * (size_t)cap); if (!n2) break; ps = n2; }
      p = &ps[np++]; memset(p, 0, sizeof(Pat));
      p->key = key; p->route = r; p->row = row; p->trip = t; p->rem = end - row;
    } else if (end - row > p->rem) { p->rem = end - row; p->row = row; p->trip = t; }
    p->n++;
    dk = dest_key(t);
    for (i = 0; i < p->nd; i++) if (p->dk[i] == dk) { p->dc[i]++; break; }
    if (i == p->nd && p->nd < MAXD) { p->dk[p->nd] = dk; p->dc[p->nd] = 1; p->nd++; }
  }
  left = (int *)malloc(sizeof(int) * (size_t)(np + 1) * 3);
  if (!left) { free(ps); return 0; }
  grp = left + np + 1; rest = grp + np + 1;
  for (i = 0; i < np; i++) {
    int route, nl = 0, guard = 0;
    if (ps[i].used) continue;
    route = ps[i].route;
    for (j = i; j < np; j++) if (ps[j].route == route) { left[nl++] = j; ps[j].used = 1; }
    while (nl > 0) {
      int x, y, ngp = 0, nrest = 0, ref, n = 0, dkeys[64], dcnt[64], ndk = 0, best;
      Group *g;
      /* ordenació estable: més parades restants primer, després més viatges */
      for (x = 1; x < nl; x++) {
        int v = left[x];
        for (y = x - 1; y >= 0 && (ps[left[y]].rem < ps[v].rem || (ps[left[y]].rem == ps[v].rem && ps[left[y]].n < ps[v].n)); y--) left[y + 1] = left[y];
        left[y + 1] = v;
      }
      ref = left[0]; grp[ngp++] = ref;
      if (++guard < 6) {
        int ta = db.tStart[ps[ref].trip], te = ta + db.tLen[ps[ref].trip], q;
        for (q = ps[ref].row + 1; q < te; q++) g_mark[db.sStop[q]] |= 1;
        for (q = ta; q < ps[ref].row; q++) g_mark[db.sStop[q]] |= 2;
        for (x = 1; x < nl; x++) {
          Pat *p = &ps[left[x]]; int pe = db.tStart[p->trip] + db.tLen[p->trip], score = 0;
          for (q = p->row + 1; q < pe; q++) { int m = g_mark[db.sStop[q]]; if (m == 1) score++; else if (m == 2) score--; }
          if (score > 0 || (score == 0 && db.tDir[p->trip] == db.tDir[ps[ref].trip])) grp[ngp++] = left[x]; else rest[nrest++] = left[x];
        }
        for (q = ta; q < te; q++) g_mark[db.sStop[q]] = 0;
      } else for (x = 1; x < nl; x++) grp[ngp++] = left[x];
      if (ng == gcap) { Group *n2; gcap *= 2; n2 = (Group *)realloc(gs, sizeof(Group) * (size_t)gcap); if (!n2) break; gs = n2; *out = gs; }
      g = &gs[ng++];
      g->stop = stop; g->route = route; g->nkeys = ngp; g->keys = (PatKey *)malloc(sizeof(PatKey) * (size_t)ngp);
      for (x = 0; x < ngp; x++) {
        Pat *p = &ps[grp[x]];
        if (g->keys) g->keys[x] = p->key;
        n += p->n;
        for (y = 0; y < p->nd; y++) {
          int z;
          for (z = 0; z < ndk; z++) if (dkeys[z] == p->dk[y]) { dcnt[z] += p->dc[y]; break; }
          if (z == ndk && ndk < 64) { dkeys[ndk] = p->dk[y]; dcnt[ndk] = p->dc[y]; ndk++; }
        }
      }
      best = 0;
      for (x = 1; x < ndk; x++) if (dcnt[x] > dcnt[best]) best = x;
      g->n = n; g->dest = ndk ? dest_str(dkeys[best]) : "";
      for (x = 0; x < nrest; x++) left[x] = rest[x];
      nl = nrest;
    }
  }
  free(left); free(ps);
  qsort(gs, (size_t)ng, sizeof(Group), cmp_group);
  return ng;
}
void amb_free_groups(Group *g, int n) { int i; if (!g) return; for (i = 0; i < n; i++) free(g[i].keys); free(g); }

/* ───────────── horari ───────────── */
typedef struct { int time, trip, row, last, idx; } TD;
static int cmp_td(const void *a, const void *b) {
  const TD *x = (const TD *)a, *y = (const TD *)b;
  return x->time != y->time ? x->time - y->time : x->idx - y->idx;
}
static int deps_on(const Group *g, const struct tm *tm, TD **out) {
  const uint8_t *act = active_services(tm);
  int a = db.stopStart[g->stop], b = db.stopStart[g->stop + 1], k, n = 0, cap = 64, i;
  TD *d = (TD *)malloc(sizeof(TD) * (size_t)cap);
  *out = d;
  if (!d) return 0;
  for (k = a; k < b; k++) {
    int row = db.stopRows[k], t = db.rowTrip[row], last; PatKey key;
    if (db.tRoute[t] != g->route || !act[db.tSvc[t]]) continue;
    last = db.tStart[t] + db.tLen[t] - 1;
    if (row == last) continue;
    key = pat_key(row, t);
    for (i = 0; i < g->nkeys; i++) if (key_eq(g->keys[i], key)) break;
    if (i == g->nkeys) continue;
    if (n == cap) { TD *n2; cap *= 2; n2 = (TD *)realloc(d, sizeof(TD) * (size_t)cap); if (!n2) break; d = n2; *out = d; }
    d[n].time = db.sTime[row]; d[n].trip = t; d[n].row = row; d[n].last = db.sStop[last]; d[n].idx = n; n++;
  }
  qsort(d, (size_t)n, sizeof(TD), cmp_td);
  return n;
}
void amb_timetable(const Group *g, time_t ref, Timetable *tt) {
  struct tm wk[5], sat, sun, tm; int nwk = 0, haveSat = 0, haveSun = 0, i, j, c;
  TD *wd[5]; int wn[5], wcount[5], bestI = 0;
  TD *cd[3]; int cn[3];
  int *lastStops, *lastCnt, nl = 0, mainStop = -1;
  int repRow = -1, repTrip = -1, repRem = -1;
  int *hc, *hfirst, hseen = 0, bestHead = -1;
  memset(tt, 0, sizeof(*tt));
  tt->stop = g->stop; tt->route = g->route;
  for (i = 0; i < 14 && (nwk < 5 || !haveSat || !haveSun); i++) {
    day_start(ref, i, &tm);
    if (tm.tm_wday == 6) { if (!haveSat) { sat = tm; haveSat = 1; } }
    else if (tm.tm_wday == 0) { if (!haveSun) { sun = tm; haveSun = 1; } }
    else if (nwk < 5) wk[nwk++] = tm;
  }
  for (i = 0; i < nwk; i++) { wn[i] = deps_on(g, &wk[i], &wd[i]); wcount[i] = 0; }
  /* feiners: el patró d'horari més repetit */
  for (i = 0; i < nwk; i++) {
    int firstSame = i;
    for (j = 0; j < i; j++) {
      int k, same = wn[j] == wn[i];
      for (k = 0; same && k < wn[i]; k++) if (wd[j][k].time != wd[i][k].time) same = 0;
      if (same) { firstSame = j; break; }
    }
    wcount[firstSame]++;
    if (wcount[firstSame] > wcount[bestI]) bestI = firstSame;
  }
  cd[0] = wd[bestI]; cn[0] = wn[bestI];
  for (i = 0; i < nwk; i++) if (i != bestI) free(wd[i]);
  cn[1] = deps_on(g, &sat, &cd[1]); cn[2] = deps_on(g, &sun, &cd[2]);
  tt->cols[0].y = wk[bestI].tm_year + 1900; tt->cols[0].m = wk[bestI].tm_mon + 1; tt->cols[0].d = wk[bestI].tm_mday;
  tt->cols[1].y = sat.tm_year + 1900; tt->cols[1].m = sat.tm_mon + 1; tt->cols[1].d = sat.tm_mday;
  tt->cols[2].y = sun.tm_year + 1900; tt->cols[2].m = sun.tm_mon + 1; tt->cols[2].d = sun.tm_mday;
  tt->empty = (cn[0] + cn[1] + cn[2]) == 0;

  /* destí principal = parada final més freqüent; la resta són notes A, B... */
  lastStops = (int *)malloc(sizeof(int) * 64); lastCnt = (int *)calloc(64, sizeof(int));
  for (c = 0; c < 3; c++) for (i = 0; i < cn[c]; i++) {
    for (j = 0; j < nl; j++) if (lastStops[j] == cd[c][i].last) break;
    if (j == nl) { if (nl == 64) continue; lastStops[nl++] = cd[c][i].last; }
    lastCnt[j]++;
  }
  for (i = 1; i < nl; i++) {   /* ordenació estable descendent per freqüència */
    int s = lastStops[i], n = lastCnt[i];
    for (j = i - 1; j >= 0 && lastCnt[j] < n; j--) { lastStops[j + 1] = lastStops[j]; lastCnt[j + 1] = lastCnt[j]; }
    lastStops[j + 1] = s; lastCnt[j + 1] = n;
  }
  if (nl) mainStop = lastStops[0];
  tt->nNotes = nl > 1 ? (nl - 1 > 26 ? 26 : nl - 1) : 0;
  for (i = 0; i < tt->nNotes; i++) { tt->notes[i].letter = (char)('A' + i); tt->notes[i].stop = lastStops[i + 1]; }

  hc = (int *)calloc((size_t)db.nHeads + 1, sizeof(int)); hfirst = (int *)malloc(sizeof(int) * ((size_t)db.nHeads + 1));
  for (c = 0; c < 3; c++) for (i = 0; i < cn[c]; i++) {
    TD *x = &cd[c][i]; int rem, h;
    if (x->last != mainStop) continue;
    rem = db.tStart[x->trip] + db.tLen[x->trip] - x->row;
    if (rem > repRem) { repRem = rem; repRow = x->row; repTrip = x->trip; }
    h = db.tHead[x->trip];
    if (db.heads[h][0]) { if (!hc[h]) hfirst[h] = hseen++; hc[h]++; }
  }
  for (i = 0; i < db.nHeads; i++) if (hc[i] && (bestHead < 0 || hc[i] > hc[bestHead] || (hc[i] == hc[bestHead] && hfirst[i] < hfirst[bestHead]))) bestHead = i;
  free(hc); free(hfirst);

  if (repTrip >= 0) {
    int end = db.tStart[repTrip] + db.tLen[repTrip];
    tt->nStrip = end - repRow;
    tt->strip = (TtStrip *)malloc(sizeof(TtStrip) * (size_t)tt->nStrip);
    for (i = repRow; i < end; i++) {
      tt->strip[i - repRow].name = db.sName[db.sStop[i]];
      tt->strip[i - repRow].min = (int)floor((db.sTime[i] - db.sTime[repRow]) / 60.0 + 0.5);
      g_mark[db.sStop[i]] = 4;
    }
  }
  for (i = 0; i < tt->nNotes; i++)
    snprintf(tt->notes[i].text, sizeof(tt->notes[i].text), "%s%s", g_mark[tt->notes[i].stop] == 4 ? "fins a " : "continua fins a ", db.sName[tt->notes[i].stop]);
  if (repTrip >= 0) { int end = db.tStart[repTrip] + db.tLen[repTrip]; for (i = repRow; i < end; i++) g_mark[db.sStop[i]] = 0; }
  tt->dest = bestHead >= 0 ? db.heads[bestHead] : (mainStop >= 0 ? db.sName[mainStop] : g->dest);

  tt->hMin = 99; tt->hMax = -1;
  for (c = 0; c < 3; c++) {
    tt->cols[c].n = cn[c];
    tt->cols[c].deps = (TtDep *)malloc(sizeof(TtDep) * (size_t)(cn[c] + 1));
    for (i = 0; i < cn[c]; i++) {
      int h = cd[c][i].time / 3600; char note = 0;
      for (j = 0; j < tt->nNotes; j++) if (tt->notes[j].stop == cd[c][i].last) { note = tt->notes[j].letter; break; }
      tt->cols[c].deps[i].time = cd[c][i].time; tt->cols[c].deps[i].note = note;
      if (h < tt->hMin) tt->hMin = h;
      if (h > tt->hMax) tt->hMax = h;
    }
    free(cd[c]);
  }
  free(lastStops); free(lastCnt);
}
void amb_free_timetable(Timetable *tt) { int c; for (c = 0; c < 3; c++) free(tt->cols[c].deps); free(tt->strip); memset(tt, 0, sizeof(*tt)); }

/* ───────────── PDF ───────────── */
typedef struct { char *b; size_t len, cap; } Buf;
static void bput(Buf *o, const char *s, size_t n) {
  if (o->len + n + 1 > o->cap) { size_t nc = (o->cap ? o->cap * 2 : 8192) + n; char *nb = (char *)realloc(o->b, nc); if (!nb) return; o->b = nb; o->cap = nc; }
  memcpy(o->b + o->len, s, n); o->len += n; o->b[o->len] = 0;
}
static void bfmt(Buf *o, const char *fmt, ...) {
  char tmp[512]; va_list ap; int n;
  va_start(ap, fmt); n = vsnprintf(tmp, sizeof(tmp), fmt, ap); va_end(ap);
  if (n > 0) bput(o, tmp, (size_t)(n < (int)sizeof(tmp) ? n : (int)sizeof(tmp) - 1));
}
#define PW 595.28
#define PH 841.89
static Buf *P;
static int g_font; static double g_size; static int g_tc[3];
static void set_font(int bold, double size) { g_font = bold; g_size = size; }
static void set_tc(int r, int g, int b) { g_tc[0] = r; g_tc[1] = g; g_tc[2] = b; }
static double tw(const char *win) {
  const unsigned char *p = (const unsigned char *)win; double w = 0; const unsigned short *W = g_font ? W_BOLD : W_REG;
  for (; *p; p++) w += *p >= 32 ? W[*p - 32] : 0;
  return w * g_size / 1000.0;
}
static void fillc(int r, int g, int b) { bfmt(P, "%.3f %.3f %.3f rg\n", r / 255.0, g / 255.0, b / 255.0); }
static void strokec(int r, int g, int b) { bfmt(P, "%.3f %.3f %.3f RG\n", r / 255.0, g / 255.0, b / 255.0); }
static void lw(double w) { bfmt(P, "%.2f w\n", w); }
static void rect_f(double x, double y, double w, double h) { bfmt(P, "%.2f %.2f %.2f %.2f re f\n", x, PH - y - h, w, h); }
static void line(double x1, double y1, double x2, double y2) { bfmt(P, "%.2f %.2f m %.2f %.2f l S\n", x1, PH - y1, x2, PH - y2); }
static void circle(double x, double y, double r, int fill) {
  double k = 0.5523 * r, Y = PH - y;
  bfmt(P, "%.2f %.2f m %.2f %.2f %.2f %.2f %.2f %.2f c ", x + r, Y, x + r, Y + k, x + k, Y + r, x, Y + r);
  bfmt(P, "%.2f %.2f %.2f %.2f %.2f %.2f c ", x - k, Y + r, x - r, Y + k, x - r, Y);
  bfmt(P, "%.2f %.2f %.2f %.2f %.2f %.2f c ", x - r, Y - k, x - k, Y - r, x, Y - r);
  bfmt(P, "%.2f %.2f %.2f %.2f %.2f %.2f c %s\n", x + k, Y - r, x + r, Y - k, x + r, Y, fill ? "f" : "S");
}
static void tri_f(double x1, double y1, double x2, double y2, double x3, double y3) { bfmt(P, "%.2f %.2f m %.2f %.2f l %.2f %.2f l f\n", x1, PH - y1, x2, PH - y2, x3, PH - y3); }
/* align: 0 esquerra, 1 centre, 2 dreta; angle en graus (antihorari) */
static void text(const char *win, double x, double y, int align, double angle) {
  const unsigned char *p = (const unsigned char *)win;
  if (align == 1) x -= tw(win) / 2; else if (align == 2) x -= tw(win);
  bfmt(P, "BT %.3f %.3f %.3f rg /F%d %.2f Tf ", g_tc[0] / 255.0, g_tc[1] / 255.0, g_tc[2] / 255.0, g_font + 1, g_size);
  if (angle != 0) { double c = cos(angle * M_PI / 180), s = sin(angle * M_PI / 180); bfmt(P, "%.4f %.4f %.4f %.4f %.2f %.2f Tm (", c, s, -s, c, x, PH - y); }
  else bfmt(P, "1 0 0 1 %.2f %.2f Tm (", x, PH - y);
  for (; *p; p++) {
    if (*p == '(' || *p == ')' || *p == '\\') { char e[2]; e[0] = '\\'; e[1] = (char)*p; bput(P, e, 2); }
    else if (*p < 32 || *p > 126) bfmt(P, "\\%03o", *p);
    else bput(P, (const char *)p, 1);
  }
  bput(P, ") Tj ET\n", 8);
}
/* retalla amb "..." si no hi cap (text ja en WinAnsi, modificat in situ) */
static void fit(char *s, double maxW) {
  size_t n; char tmp[400];
  if (tw(s) <= maxW) return;
  n = strlen(s);
  for (;;) {
    if (n <= 2) break;
    snprintf(tmp, sizeof(tmp), "%.*s...", (int)n, s);
    if (tw(tmp) <= maxW) break;
    n--;
  }
  while (n > 0 && s[n - 1] == ' ') n--;
  s[n] = 0; strcat(s, "...");
}
static int hexc(const char *h, int *rgb) {
  int i, v[6];
  if (strlen(h) != 6) return 0;
  for (i = 0; i < 6; i++) { char c = h[i]; if (c >= '0' && c <= '9') v[i] = c - '0'; else if (c >= 'a' && c <= 'f') v[i] = c - 'a' + 10; else if (c >= 'A' && c <= 'F') v[i] = c - 'A' + 10; else return 0; }
  rgb[0] = v[0] * 16 + v[1]; rgb[1] = v[2] * 16 + v[3]; rgb[2] = v[4] * 16 + v[5];
  return 1;
}
static void clock_icon(double x, double y, double r) {
  strokec(255, 255, 255); lw(0.9);
  circle(x, y, r, 0); line(x, y, x, y - r * 0.6); line(x, y, x + r * 0.45, y + r * 0.3);
}
static const char *COL_LABEL[3] = { "Feiners (dl.-dv.)", "Dissabte", "Diumenge i festius" };
static const char *COL_SHORT[3] = { "Feiners", "Dissabte", "Festius" };

static void draw_timetable(const Timetable *tt, time_t generated) {
  const double L = 24, R = PW - 24, W = R - L;
  int bg[3] = { 16, 42, 84 }, fg[3] = { 255, 255, 255 };
  char buf[400], buf2[400];
  double y, lum, fs, bx, bs, by, bw, dw;
  int i, k, n, c, h;
  /* colors de la línia (text blanc o negre segons la lluminositat del fons) */
  hexc(db.rColor[tt->route], bg);
  lum = (0.299 * bg[0] + 0.587 * bg[1] + 0.114 * bg[2]) / 255;
  if (lum > 0.6) { fg[0] = fg[1] = fg[2] = 0; }

  /* capçalera */
  set_tc(120, 120, 120); set_font(1, 7); text("PARADA", L, 34, 0, 0);
  set_tc(16, 42, 84); set_font(1, 13);
  {
    char name[400], l1[400], l2[400]; char *sp; int nl = 1;
    to_win(db.sName[tt->stop], name, sizeof(name));
    strcpy(l1, name); l2[0] = 0;
    if (tw(name) > 190) {      /* parteix en dues línies per paraules */
      size_t cut = 0, pos;
      for (pos = 0; name[pos]; pos++) if (name[pos] == ' ') { char t[400]; snprintf(t, sizeof(t), "%.*s", (int)pos, name); if (tw(t) <= 190) cut = pos; else break; }
      if (cut) { snprintf(l1, sizeof(l1), "%.*s", (int)cut, name); snprintf(l2, sizeof(l2), "%s", name + cut + 1); fit(l2, 190); nl = 2; }
      else fit(l1, 190);
    }
    (void)sp;
    text(l1, L, 49, 0, 0);
    if (nl == 2) text(l2, L, 49 + 13 * 1.15, 0, 0);
    if (db.sCode[tt->stop][0]) {
      set_font(0, 8.5); set_tc(90, 90, 90);
      snprintf(buf2, sizeof(buf2), "Codi %s", db.sCode[tt->stop]); to_win(buf2, buf, sizeof(buf));
      text(buf, L, 49 + nl * 14, 0, 0);
    }
  }
  strokec(190, 190, 190); lw(0.6); line(L + 200, 26, L + 200, 80);
  bx = L + 214; bs = 40; by = 32;
  to_win(amb_route_label(tt->route), buf, sizeof(buf));
  fs = 26; set_font(1, fs);
  bw = tw(buf) + 14; if (bw > 90) bw = 90; if (bw < bs) bw = bs;
  while (tw(buf) > bw - 8 && fs > 8) { fs -= 1; set_font(1, fs); }
  fillc(bg[0], bg[1], bg[2]); rect_f(bx, by, bw, bs);
  set_tc(fg[0], fg[1], fg[2]); text(buf, bx + bw / 2, by + bs / 2 + fs * 0.35, 1, 0);
  set_tc(16, 42, 84);
  to_win(tt->dest ? tt->dest : "", buf, sizeof(buf));
  fs = 26; set_font(1, fs); dw = R - (bx + bw + 10);
  while (tw(buf) > dw && fs > 13) { fs -= 1; set_font(1, fs); }
  fit(buf, dw); text(buf, bx + bw + 10, by + bs / 2 + fs * 0.35, 0, 0);

  /* tira de parades */
  y = 96; n = tt->nStrip;
  if (n > 1) {
    const double K = 0.70710678, maxLab = 170, x0 = L + 8, avail = R - x0;
    double fsz = n > 50 ? 5 : n > 38 ? 6 : n > 26 ? 7 : 8, dx = 24, hTop = 0, ly;
    char (*labs)[200] = (char (*)[200])malloc((size_t)n * 200);
    double *lwid = (double *)malloc(sizeof(double) * (size_t)n);
    for (i = 0; i < n; i++) {
      set_font(i == 0 || i == n - 1, fsz);
      to_win(tt->strip[i].name, labs[i], 190); fit(labs[i], maxLab); lwid[i] = tw(labs[i]);
      if (lwid[i] > hTop) hTop = lwid[i];
    }
    for (i = 1; i < n; i++) { double v = (avail - lwid[i] * K - fsz) / i; if (v < dx) dx = v; }
    if (dx < 4) dx = 4;
    hTop = hTop * K + fsz; ly = y + hTop + 4;
    strokec(16, 42, 84); lw(1.4); line(x0, ly, x0 + dx * (n - 1), ly);
    fillc(16, 42, 84);
    for (i = 0; i < n; i++) {
      double x = x0 + dx * i, r = dx * 0.32 < 2.6 ? dx * 0.32 : 2.6;
      fillc(16, 42, 84); circle(x, ly, r, 1);
      set_font(i == 0 || i == n - 1, fsz); set_tc(20, 20, 20);
      text(labs[i], x + fsz * 0.3, ly - 5, 0, 45);
      if (i > 0) {
        double ms = dx * 0.62; if (ms < 4) ms = 4; if (ms > 6.5) ms = 6.5;
        set_font(0, ms); set_tc(40, 40, 40);
        snprintf(buf, sizeof(buf), "%d", tt->strip[i].min); text(buf, x, ly + 11, 1, 0);
      }
    }
    fillc(16, 42, 84); tri_f(x0 - 3, ly + 5.5, x0 - 3, ly + 12.5, x0 + 3, ly + 9);
    y = ly + 16;
    free(labs); free(lwid);
  }
  set_font(0, 7.5); set_tc(40, 40, 40);
  to_win("Minuts de trajecte des d'aquesta parada. Horari teòric, sense garantia.", buf, sizeof(buf));
  text(buf, R, y + 6, 2, 0);
  y += 10;

  /* graella */
  {
    const double HW = 26, avail = W - 2 * HW, hdrH = 20;
    int cnt[3][48], per[3], nh = tt->empty ? 0 : tt->hMax - tt->hMin + 1, lines[48], total = 0, idx[3] = { 0, 0, 0 };
    double slot, cx[4], legendH, bottom, unit, mfs, hfs, top;
    if (nh > 48) nh = 48;
    memset(cnt, 0, sizeof(cnt));
    for (c = 0; c < 3; c++) for (i = 0; i < tt->cols[c].n; i++) { h = tt->cols[c].deps[i].time / 3600 - tt->hMin; if (h >= 0 && h < 48) cnt[c][h]++; }
    for (c = 0; c < 3; c++) { per[c] = 3; for (h = 0; h < nh; h++) if (cnt[c][h] > per[c]) per[c] = cnt[c][h]; }
    while (avail / (per[0] + per[1] + per[2]) < 15.5) {
      k = 0; if (per[1] > per[k]) k = 1; if (per[2] > per[k]) k = 2;
      if (per[k] <= 3) break;
      per[k]--;
    }
    slot = avail / (per[0] + per[1] + per[2]);
    cx[0] = L + HW; for (k = 0; k < 3; k++) cx[k + 1] = cx[k] + per[k] * slot;
    for (h = 0; h < nh; h++) {
      lines[h] = 1;
      for (c = 0; c < 3; c++) { int l = (cnt[c][h] + per[c] - 1) / per[c]; if (l > lines[h]) lines[h] = l; }
      total += lines[h];
    }
    legendH = tt->nNotes ? 14 * ((tt->nNotes + 1) / 2) + 8 : 0;
    bottom = PH - 46 - legendH;
    unit = (bottom - y - hdrH) / (total > 0 ? total : 1);
    if (unit > 19.5) unit = 19.5; if (unit < 8) unit = 8;
    mfs = unit * 0.6; if (mfs > 10.5) mfs = 10.5; if (mfs > slot * 0.62) mfs = slot * 0.62;
    hfs = unit * 0.66; if (hfs > 11.5) hfs = 11.5;

    fillc(16, 42, 84); rect_f(L, y, W, hdrH);
    clock_icon(L + HW / 2, y + hdrH / 2, 5.2); clock_icon(R - HW / 2, y + hdrH / 2, 5.2);
    set_tc(255, 255, 255);
    for (k = 0; k < 3; k++) {
      double w = cx[k + 1] - cx[k], fz = 12.5; const char *lab = COL_LABEL[k];
      set_font(1, fz);
      if (tw(lab) > w - 6) lab = COL_SHORT[k];
      while (tw(lab) > w - 4 && fz > 6) { fz -= 1; set_font(1, fz); }
      text(lab, cx[k] + w / 2, y + hdrH / 2 + fz * 0.35, 1, 0);
    }
    y += hdrH; top = y;
    for (h = 0; h < nh; h++) {
      double rh = lines[h] * unit, base = y + unit / 2 + hfs * 0.36;
      if (h % 2 == 1) { fillc(208, 208, 208); rect_f(L, y, W, rh); }
      set_tc(0, 0, 0); set_font(1, hfs);
      snprintf(buf, sizeof(buf), "%d", (h + tt->hMin) % 24);
      text(buf, L + HW - 3, base, 2, 0); text(buf, R - 3, base, 2, 0);
      for (c = 0; c < 3; c++) {
        int j = 0;
        while (idx[c] < tt->cols[c].n && tt->cols[c].deps[idx[c]].time / 3600 - tt->hMin == h) {
          const TtDep *e = &tt->cols[c].deps[idx[c]];
          double x = cx[c] + 3 + (j % per[c]) * slot, yy = y + (j / per[c]) * unit + unit / 2 + mfs * 0.36, w;
          set_font(0, mfs); set_tc(0, 0, 0);
          snprintf(buf, sizeof(buf), "%02d", (e->time % 3600) / 60);
          text(buf, x, yy, 0, 0);
          if (e->note) { w = tw(buf); set_font(1, mfs * 0.62); buf[0] = e->note; buf[1] = 0; text(buf, x + w + 0.6, yy + mfs * 0.18, 0, 0); }
          idx[c]++; j++;
        }
      }
      y += rh;
    }
    strokec(16, 42, 84); lw(0.8);
    for (k = 0; k <= 3; k++) line(cx[k], top, cx[k], y);
    lw(0.5); line(L, y, R, y);
    if (tt->empty) {
      set_font(0, 10); set_tc(90, 90, 90);
      text("Sense sortides previstes en les dates consultades.", PW / 2, top + 30, 1, 0);
    }
  }
  /* notes */
  y += 14;
  for (i = 0; i < tt->nNotes; i++) {
    double x = L + (i % 2) * (W / 2), yy = y + (i / 2) * 14, lwd;
    set_font(1, 9.5); set_tc(0, 0, 0);
    buf[0] = tt->notes[i].letter; buf[1] = 0; text(buf, x, yy, 0, 0); lwd = tw(buf);
    set_font(0, 9.5);
    snprintf(buf2, sizeof(buf2), " = %s", tt->notes[i].text); to_win(buf2, buf, sizeof(buf)); fit(buf, W / 2 - 16);
    text(buf, x + lwd, yy, 0, 0);
  }
  /* peu */
  {
    struct tm g = *localtime(&generated);
    fillc(226, 231, 240); rect_f(L, PH - 40, W, 20);
    set_font(0, 7); set_tc(16, 42, 84);
    snprintf(buf2, sizeof(buf2), "Dates consultades: %02d/%02d/%d, %02d/%02d/%d, %02d/%02d/%d.  Font: dades obertes GTFS de l'AMB.",
      tt->cols[0].d, tt->cols[0].m, tt->cols[0].y, tt->cols[1].d, tt->cols[1].m, tt->cols[1].y, tt->cols[2].d, tt->cols[2].m, tt->cols[2].y);
    to_win(buf2, buf, sizeof(buf)); text(buf, L + 6, PH - 27.5, 0, 0);
    snprintf(buf, sizeof(buf), "Generat el %02d/%02d/%d %02d:%02d", g.tm_mday, g.tm_mon + 1, g.tm_year + 1900, g.tm_hour, g.tm_min);
    text(buf, R - 6, PH - 27.5, 2, 0);
    fillc(16, 42, 84); rect_f(0, PH - 10, PW, 10);
  }
}

int amb_write_pdf(const char *path, const Timetable *tt, time_t generated) {
  Buf content = { 0, 0, 0 }, out = { 0, 0, 0 };
  long off[7]; int i; FILE *f; size_t xref;
  P = &content;
  draw_timetable(tt, generated);
  if (!content.b) return -1;
  bfmt(&out, "%%PDF-1.4\n%%\xE2\xE3\xCF\xD3\n");
  off[1] = (long)out.len; bfmt(&out, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
  off[2] = (long)out.len; bfmt(&out, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
  off[3] = (long)out.len; bfmt(&out, "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f] /Contents 4 0 R /Resources << /Font << /F1 5 0 R /F2 6 0 R >> >> >>\nendobj\n", PW, PH);
  off[4] = (long)out.len; bfmt(&out, "4 0 obj\n<< /Length %lu >>\nstream\n", (unsigned long)content.len);
  bput(&out, content.b, content.len); bfmt(&out, "\nendstream\nendobj\n");
  off[5] = (long)out.len; bfmt(&out, "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n");
  off[6] = (long)out.len; bfmt(&out, "6 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>\nendobj\n");
  xref = out.len;
  bfmt(&out, "xref\n0 7\n0000000000 65535 f \n");
  for (i = 1; i <= 6; i++) bfmt(&out, "%010ld 00000 n \n", off[i]);
  bfmt(&out, "trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n%lu\n%%%%EOF\n", (unsigned long)xref);
  free(content.b);
  if (!out.b) return -1;
  f = fopen(path, "wb");
  if (!f) { free(out.b); return -2; }
  i = fwrite(out.b, 1, out.len, f) == out.len ? 0 : -3;
  if (fclose(f) != 0) i = -3;
  free(out.b);
  return i;
}
