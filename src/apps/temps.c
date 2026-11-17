/* Escriptori — paquet «El temps». Previsió per hores i de 10 dies, detalls del dia i diverses ciutats.
   Dades d'Open-Meteo (sense clau). Cada ciutat desa la seva previsió en un fitxer i es pot consultar sense connexió. */
#define ESC_MODULE
#include "../mod.h"
enum { V_WEATHER, V_CITIES, V_RESULTS, V_DAY };
enum { A_WX_REFRESH = 100, A_WX_CITIES, A_WX_ADD, A_WX_EDIT, A_WX_OPEN, A_WX_DEL, A_WX_PICK, A_WX_MAKEMAIN, A_WX_ALL, A_WX_TAB, A_WX_DAY };
enum { TAB_NOW, TAB_DAYS, TAB_MORE };
static const char *WD2[7] = { "dg", "dl", "dt", "dc", "dj", "dv", "ds" };
#define WXFILE_OLD CFG "escriptori_temps.json"                 /* versió 2: una sola ciutat (Barcelona) */
#define WXFILE_FMT CFG "escriptori_temps_%.2f_%.2f.json"       /* previsió desada per ciutat */
#define WXURL_FMT "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f" \
  "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,weather_code,pressure_msl,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index,visibility" \
  "&hourly=temperature_2m,weather_code,precipitation_probability,is_day,pressure_msl" \
  "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,precipitation_sum,sunrise,sunset,uv_index_max,wind_speed_10m_max,wind_gusts_10m_max" \
  "&timezone=auto&forecast_days=10"
#define GEOURL_FMT "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=6&language=ca&format=json"
#define MAXC 12
static const char *wx_desc(int c) {
  if (c == 0) return "Serè"; if (c == 1) return "Poc ennuvolat"; if (c == 2) return "Mig ennuvolat"; if (c == 3) return "Ennuvolat";
  if (c == 45 || c == 48) return "Boira"; if (c >= 51 && c <= 57) return "Plugim"; if (c == 61) return "Pluja feble"; if (c == 63) return "Pluja";
  if (c == 65) return "Pluja forta"; if (c == 66 || c == 67) return "Pluja gelada"; if (c >= 71 && c <= 77) return "Neu";
  if (c >= 80 && c <= 82) return "Ruixats"; if (c == 85 || c == 86) return "Ruixats de neu"; if (c == 95) return "Tempesta"; if (c == 96 || c == 99) return "Tempesta amb calamarsa";
  return "—";
}
static int is_wet(int c) { return (c >= 51 && c <= 67) || (c >= 80 && c <= 82) || c >= 95; }
static int is_snow(int c) { return (c >= 71 && c <= 77) || c == 85 || c == 86; }
/* icona de traç (icons.h) per a un codi WMO; de nit, lluna en lloc de sol */
static const char *wx_icon(int c, int day) {
  if (c < 0) return IC_WX_CLOUD;
  if (c == 0) return day ? IC_SUN : IC_MOON;
  if (c == 1 || c == 2) return day ? IC_WX_PARTLY : IC_WX_PARTLYN;
  if (c == 3) return IC_WX_CLOUD;
  if (c == 45 || c == 48) return IC_WX_FOG;
  if (c >= 51 && c <= 57) return IC_WX_DRIZZLE;
  if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return IC_WX_RAIN;
  if (is_snow(c)) return IC_WX_SNOW;
  if (c >= 95) return IC_WX_STORM;
  return IC_WX_CLOUD;
}

/* ───── JSON mínim ───── */
/* troba "clau": dins de `from` i retorna el punter al valor */
static const char *jkey(const char *from, const char *key) {
  char pat[80]; const char *p;
  if (!from) return NULL;
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  p = strstr(from, pat);
  return p ? p + strlen(pat) : NULL;
}
static int jarr(const char *p, double *out, int max) {     /* llegeix [n, n, null, ...] */
  int n = 0;
  if (!p) return 0;
  while (*p == ' ') p++;
  if (*p != '[') return 0;
  p++;
  while (*p && *p != ']' && n < max) {
    char *e; double v = strtod(p, &e);
    if (e == p) { out[n++] = -9999; while (*p && *p != ',' && *p != ']') p++; } else { out[n++] = v; p = e; }
    while (*p == ',' || *p == ' ') p++;
  }
  return n;
}
/* llegeix ["a", "b", ...] en files de w caràcters */
static int jsarr(const char *p, char *out, int w, int max) {
  int n = 0;
  if (!p) return 0;
  while (*p == ' ') p++;
  if (*p != '[') return 0;
  p++;
  while (*p && n < max) {
    while (*p == ' ' || *p == ',') p++;
    if (*p == ']' || !*p) break;
    if (*p == '"') {
      const char *q = strchr(p + 1, '"'); size_t l;
      if (!q) break;
      l = (size_t)(q - p - 1); if (l > (size_t)w - 1) l = (size_t)w - 1;
      memcpy(out + n * w, p + 1, l); out[n * w + l] = 0; n++; p = q + 1;
    } else { out[n * w] = 0; n++; while (*p && *p != ',' && *p != ']') p++; }
  }
  return n;
}
/* còpia de l'objecte {...} de la clau, perquè les cerques no se n'escapin */
static char *jsub(const char *b, const char *key) {
  const char *p = jkey(b, key), *q; int d = 0, s = 0; char *o;
  if (!p) return NULL;
  while (*p == ' ') p++;
  if (*p != '{') return NULL;
  for (q = p; *q; q++) {
    if (s) { if (*q == '\\' && q[1]) q++; else if (*q == '"') s = 0; continue; }
    if (*q == '"') s = 1;
    else if (*q == '{' || *q == '[') d++;
    else if ((*q == '}' || *q == ']') && --d == 0) { q++; break; }
  }
  o = (char *)malloc((size_t)(q - p) + 1);
  if (o) { memcpy(o, p, (size_t)(q - p)); o[q - p] = 0; }
  return o;
}
static double jnum(const char *o, const char *k) {
  const char *p = jkey(o, k); char *e; double v;
  if (!p) return -9999;
  while (*p == ' ') p++;
  v = strtod(p, &e);
  return e == p ? -9999 : v;
}
/* llegeix una cadena JSON ("..."), amb \" \\ i \uXXXX, i la deixa en UTF-8 */
static int jstr(const char *p, char *out, int sz) {
  int n = 0;
  out[0] = 0;
  if (!p) return 0;
  while (*p == ' ') p++;
  if (*p != '"') return 0;
  p++;
  while (*p && *p != '"' && n < sz - 4) {
    if (*p == '\\' && p[1]) {
      p++;
      if (*p == 'u') {
        char hx[5]; char *e; unsigned long c;
        memcpy(hx, p + 1, 4); hx[4] = 0; c = strtoul(hx, &e, 16);
        if (e == hx + 4) {
          if (c < 0x80) out[n++] = (char)c;
          else if (c < 0x800) { out[n++] = (char)(0xC0 | (c >> 6)); out[n++] = (char)(0x80 | (c & 0x3F)); }
          else { out[n++] = (char)(0xE0 | (c >> 12)); out[n++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[n++] = (char)(0x80 | (c & 0x3F)); }
          p += 5;
        } else p++;
      } else { out[n++] = *p == 'n' || *p == 't' ? ' ' : *p; p++; }
    } else out[n++] = *p++;
  }
  out[n] = 0;
  return n;
}
static void urlenc(const char *s, char *out, int sz) {
  int n = 0;
  for (; *s && n < sz - 4; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '.' || c == '_') out[n++] = (char)c;
    else n += snprintf(out + n, (size_t)(sz - n), "%%%02X", c);
  }
  out[n] = 0;
}

/* ───── ciutats desades: «nom \t latitud \t longitud \t regió» ───── */
static Store cities = { 0, 0, 0, CFG "escriptori_temps_ciutats.txt" };
static void wx_path(char *out, int sz, double lat, double lon) { snprintf(out, (size_t)sz, WXFILE_FMT, lat, lon); }
static void cities_load(void) {
  st_load(&cities);
  if (cities.n == 0) {                                   /* primera vegada: Barcelona, com a la versió 2 */
    char path[256];
    st_insert(&cities, 0, "Barcelona\t41.3900\t2.1700\tCatalunya, Espanya"); st_save(&cities);
    wx_path(path, sizeof(path), 41.39, 2.17);
    rename(WXFILE_OLD, path);                            /* aprofita la previsió ja desada, si n'hi ha */
  }
}
static int city_get(int i, char *name, int nsz, double *lat, double *lon, char *region, int rsz) {
  const char *t1, *t2, *t3; size_t n;
  if (i < 0 || i >= cities.n) return 0;
  t1 = strchr(cities.it[i], '\t'); if (!t1) return 0;
  n = (size_t)(t1 - cities.it[i]); if (n > (size_t)nsz - 1) n = (size_t)nsz - 1;
  memcpy(name, cities.it[i], n); name[n] = 0;
  *lat = strtod(t1 + 1, NULL);
  t2 = strchr(t1 + 1, '\t'); if (!t2) return 0;
  *lon = strtod(t2 + 1, NULL);
  t3 = strchr(t2 + 1, '\t');
  if (region) snprintf(region, (size_t)rsz, "%s", t3 ? t3 + 1 : "");
  return 1;
}

/* ───── previsió d'una ciutat ───── */
#define NA (-9999.0)
#define OK(x) ((x) > -999)
#define NH 264
#define ND 16
typedef struct {
  int ok, utc, nh, nd, h0, d0, stale; time_t mt;
  double t, feel, hum, wind, wdir, gust, pres, vis, uv, prec; int code, isday;
  double nt; int ncode, nday;                            /* «ara»: l'actual o, si el fitxer és vell, el de l'hora */
  char ht[NH][17]; double htemp[NH], hcode[NH], hpp[NH], hday[NH], hpres[NH];
  char dt[ND][11], rise[ND][17], set[ND][17];
  double dcode[ND], dmax[ND], dmin[ND], dpp[ND], dsum[ND], duv[ND], dwind[ND], dgust[ND];
} Wx;
static Wx W;
static char wPath[256]; static time_t wMt;
static void wx_fill(double *a, int n) { int i; for (i = 0; i < n; i++) a[i] = NA; }
/* situa l'hora i el dia d'avui de la ciutat (hora local de la ciutat, segons utc_offset_seconds) */
static void wx_now(void) {
  time_t now = time(NULL) + W.utc; struct tm *g = gmtime(&now); char hk[16], dk[11]; int i;
  snprintf(hk, sizeof(hk), "%04d-%02d-%02dT%02d", g->tm_year + 1900, g->tm_mon + 1, g->tm_mday, g->tm_hour);
  memcpy(dk, hk, 10); dk[10] = 0;
  W.h0 = -1;
  for (i = 0; i < W.nh; i++) if (!strncmp(W.ht[i], hk, 13)) { W.h0 = i; break; }
  if (W.h0 < 0 && W.nh && strncmp(hk, W.ht[0], 13) < 0) W.h0 = 0;          /* rellotge per darrere de la previsió */
  W.d0 = -1;
  for (i = 0; i < W.nd; i++) if (!strcmp(W.dt[i], dk)) { W.d0 = i; break; }
  if (W.d0 < 0 && W.nd && strcmp(dk, W.dt[0]) < 0) W.d0 = 0;
  W.stale = W.d0 < 0;                                                       /* la previsió ja ha passat del tot */
  if (W.d0 < 0) W.d0 = 0;
  W.nt = W.t; W.ncode = W.code; W.nday = W.isday;
  if (W.h0 >= 0 && time(NULL) - W.mt > 2 * 3600) {                          /* dades velles: fes servir l'hora d'ara */
    if (OK(W.htemp[W.h0])) W.nt = W.htemp[W.h0];
    if (OK(W.hcode[W.h0])) W.ncode = (int)W.hcode[W.h0];
    if (OK(W.hday[W.h0])) W.nday = (int)W.hday[W.h0];
  }
}
static int wx_load(double lat, double lon) {
  char path[256], *b, *cur, *hr, *dy; struct stat st; double v;
  wx_path(path, sizeof(path), lat, lon);
  if (stat(path, &st) != 0) { W.ok = 0; wPath[0] = 0; return 0; }
  if (W.ok && !strcmp(path, wPath) && st.st_mtime == wMt) { wx_now(); return 1; }
  if (!(b = read_file(path))) { W.ok = 0; wPath[0] = 0; return 0; }
  memset(&W, 0, sizeof(W)); W.mt = st.st_mtime;
  v = jnum(b, "utc_offset_seconds"); W.utc = OK(v) ? (int)v : 0;
  cur = jsub(b, "current"); hr = jsub(b, "hourly"); dy = jsub(b, "daily");
  W.t = jnum(cur, "temperature_2m"); W.feel = jnum(cur, "apparent_temperature"); W.hum = jnum(cur, "relative_humidity_2m");
  W.wind = jnum(cur, "wind_speed_10m"); W.wdir = jnum(cur, "wind_direction_10m"); W.gust = jnum(cur, "wind_gusts_10m");
  W.pres = jnum(cur, "pressure_msl"); W.vis = jnum(cur, "visibility"); W.uv = jnum(cur, "uv_index"); W.prec = jnum(cur, "precipitation");
  v = jnum(cur, "weather_code"); if (!OK(v)) v = jnum(cur, "weathercode"); W.code = OK(v) ? (int)v : -1;
  v = jnum(cur, "is_day"); W.isday = OK(v) ? (int)v : 1;
  wx_fill(W.htemp, NH); wx_fill(W.hcode, NH); wx_fill(W.hpp, NH); wx_fill(W.hday, NH); wx_fill(W.hpres, NH);
  W.nh = jsarr(jkey(hr, "time"), &W.ht[0][0], 17, NH);
  jarr(jkey(hr, "temperature_2m"), W.htemp, NH); jarr(jkey(hr, "weather_code"), W.hcode, NH); jarr(jkey(hr, "precipitation_probability"), W.hpp, NH);
  jarr(jkey(hr, "is_day"), W.hday, NH); jarr(jkey(hr, "pressure_msl"), W.hpres, NH);
  wx_fill(W.dcode, ND); wx_fill(W.dmax, ND); wx_fill(W.dmin, ND); wx_fill(W.dpp, ND); wx_fill(W.dsum, ND); wx_fill(W.duv, ND); wx_fill(W.dwind, ND); wx_fill(W.dgust, ND);
  W.nd = jsarr(jkey(dy, "time"), &W.dt[0][0], 11, ND);
  if (!jarr(jkey(dy, "weather_code"), W.dcode, ND)) jarr(jkey(dy, "weathercode"), W.dcode, ND);
  jarr(jkey(dy, "temperature_2m_max"), W.dmax, ND); jarr(jkey(dy, "temperature_2m_min"), W.dmin, ND);
  jarr(jkey(dy, "precipitation_probability_max"), W.dpp, ND); jarr(jkey(dy, "precipitation_sum"), W.dsum, ND);
  jarr(jkey(dy, "uv_index_max"), W.duv, ND); jarr(jkey(dy, "wind_speed_10m_max"), W.dwind, ND); jarr(jkey(dy, "wind_gusts_10m_max"), W.dgust, ND);
  jsarr(jkey(dy, "sunrise"), &W.rise[0][0], 17, ND); jsarr(jkey(dy, "sunset"), &W.set[0][0], 17, ND);
  free(cur); free(hr); free(dy); free(b);
  W.ok = W.nd > 0 || OK(W.t);
  snprintf(wPath, sizeof(wPath), "%s", path); wMt = st.st_mtime;
  wx_now();
  return W.ok;
}
static int wx_fetch(double lat, double lon) {
  char url[1200], path[256]; int len = 0; char *b; FILE *f;
  snprintf(url, sizeof(url), WXURL_FMT, lat, lon);
  b = net_get(url, &len);
  if (!b || !strstr(b, "\"daily\"")) { free(b); return 0; }
  wx_path(path, sizeof(path), lat, lon);
  f = fopen(path, "wb");
  if (f) { fwrite(b, 1, (size_t)len, f); fclose(f); }
  free(b);
  wPath[0] = 0;                                          /* torna a llegir-la encara que l'hora del fitxer no canviï */
  return 1;
}
/* Sincronització automàtica (API 6): baixa la previsió de cada ciutat en segon pla. */
typedef struct { char path[256]; } WxCtx;
static void wx_bg_done(void *ctx, char *b, int len) {
  WxCtx *c = (WxCtx *)ctx; FILE *f;
  if (b && strstr(b, "\"daily\"") && (f = fopen(c->path, "wb"))) { fwrite(b, 1, (size_t)len, f); fclose(f); wPath[0] = 0; }
  free(c);
}
static void wx_bg(void) {
  int i;
  cities_load();
  for (i = 0; i < cities.n; i++) {
    char name[80], url[1200], t[120]; double la, lo; WxCtx *c;
    if (!city_get(i, name, sizeof(name), &la, &lo, NULL, 0) || !(c = (WxCtx *)malloc(sizeof(WxCtx)))) continue;
    wx_path(c->path, sizeof(c->path), la, lo);
    snprintf(url, sizeof(url), WXURL_FMT, la, lo); snprintf(t, sizeof(t), "El temps · %s", name);
    H->fetch(url, t, wx_bg_done, c);
  }
}

/* ───── formats ───── */
static void deg(char *o, int sz, double t) { if (OK(t)) snprintf(o, (size_t)sz, "%d°", (int)floor(t + 0.5)); else snprintf(o, (size_t)sz, "—"); }
static void hilo(char *o, int sz, int d) {
  char a[16], b[16];
  deg(a, sizeof(a), d >= 0 && d < W.nd ? W.dmax[d] : NA); deg(b, sizeof(b), d >= 0 && d < W.nd ? W.dmin[d] : NA);
  snprintf(o, (size_t)sz, "Màx. %s   Mín. %s", a, b);
}
static void hm(char *o, const char *iso) { if (iso && strlen(iso) >= 16) { memcpy(o, iso + 11, 5); o[5] = 0; } else snprintf(o, 6, "—"); }
static int iso_min(const char *iso) { return iso && strlen(iso) >= 16 ? atoi(iso + 11) * 60 + atoi(iso + 14) : -1; }
static int city_min(void) { time_t now = time(NULL) + W.utc; struct tm *g = gmtime(&now); return g->tm_hour * 60 + g->tm_min; }
static void city_clock(char *o, int sz) { time_t now = time(NULL) + W.utc; struct tm *g = gmtime(&now); snprintf(o, (size_t)sz, "%02d:%02d", g->tm_hour, g->tm_min); }
static void day_parts(int d, int *yy, int *mm, int *dd) { *yy = *mm = *dd = 0; if (d >= 0 && d < W.nd) sscanf(W.dt[d], "%d-%d-%d", yy, mm, dd); }
static void day_short(char *o, int sz, int d) {
  int yy, mm, dd; day_parts(d, &yy, &mm, &dd);
  if (d == W.d0 && !W.stale) snprintf(o, (size_t)sz, "Avui");
  else if (mm >= 1 && mm <= 12) snprintf(o, (size_t)sz, "%s. %d", WD2[weekday_of(yy, mm - 1, dd)], dd);
  else snprintf(o, (size_t)sz, "—");
}
static void num1(char *o, int sz, double v) {                       /* 3 · 0,4 · 12 */
  if (v >= 10 || fabs(v - floor(v + 0.5)) < 0.05) snprintf(o, (size_t)sz, "%d", (int)floor(v + 0.5));
  else { snprintf(o, (size_t)sz, "%.1f", v); { char *p = strchr(o, '.'); if (p) *p = ','; } }
}
static const char *compass(double d, int full) {
  static const char *S[8] = { "N", "NE", "E", "SE", "S", "SO", "O", "NO" };
  static const char *L[8] = { "nord", "nord-est", "est", "sud-est", "sud", "sud-oest", "oest", "nord-oest" };
  int k = ((int)floor(d / 45.0 + 0.5)) & 7;
  return full ? L[k] : S[k];
}
static const char *uv_level(double u) { return u < 3 ? "Baix" : u < 6 ? "Moderat" : u < 8 ? "Alt" : u < 11 ? "Molt alt" : "Extrem"; }

/* ───── peces de dibuix ───── */
static ifont *fGiant, *fMid;
static const char *fname(ifont *f, const char *def) { const char *n = f ? f->name : NULL; return n && n[0] ? n : def; }
static void fonts_open(void) {
  fGiant = OpenFont(fname(fText, DEFAULTFONT), U * 4, 1); if (!fGiant) fGiant = OpenFont(DEFAULTFONT, U * 4, 1);
  fMid = OpenFont(fname(fBold, DEFAULTFONTB), U * 3 / 2, 1); if (!fMid) fMid = OpenFont(DEFAULTFONTB, U * 3 / 2, 1);
}
static void fonts_close(void) { if (fGiant) CloseFont(fGiant); if (fMid) CloseFont(fMid); fGiant = fMid = NULL; }
#define GIANT (fGiant ? fGiant : fHuge)
#define MID (fMid ? fMid : fBig)
/* superfície aixecada (targeta o píndola) amb ombra, o amb vora si el fons i la targeta són iguals */
static void raised(int x, int y, int w, int h, int r) {
  if (PAL(P_BG) == PAL(P_CARD)) { rcard(x, y, w, h, r, 3, PAL(P_LINE), PAL(P_CARD)); return; }
  rfill(x, y + 5, w, h, r, PAL(P_SHADOW)); rfill(x, y, w, h, r, PAL(P_CARD));
}
static void rbtn(int x, int y, int d, const char *ic, int act) {
  int s = d * 50 / 100;
  raised(x, y, d, d, d / 2);
  ic_path(ic, x + (d - s) / 2, y + (d - s) / 2, s, 2.2, PAL(P_FG), PAL(P_CARD));
  add_hit(x - U / 8, y - U / 8, d + U / 4, d + U / 4, act, 0);
}
/* etiqueta de secció: icona petita i text en majúscules, en gris */
static void sec(int x, int y, int w, const char *ic, const char *txt) {
  int s = U * 8 / 10;
  if (ic) ic_path(ic, x, y + (U - s) / 2, s, 2.2, PAL(P_SUB), PAL(P_CARD));
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + (ic ? s + U / 4 : 0), y, w - (ic ? s + U / 4 : 0), U, txt, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
}
/* pestanyes (Ara · 10 dies · Detalls) i botons rodons d'actualitzar i de ciutats */
static int tabs(View *v, int y) {
  static const char *T[3] = { "Ara", "10 dies", "Detalls" };
  int h = 2 * U, gap = U / 3, w = SW - U - 2 * (h + gap), x = U / 2, sw = w / 3, i;
  raised(x, y, w, h, h / 2);
  for (i = 0; i < 3; i++) {
    int sx = x + i * sw, on = i == v->sel;
    if (on) rfill(sx + 5, y + 5, sw - 10, h - 10, (h - 10) / 2, PAL(P_ACC));
    SetFont(fBold, on ? PAL(P_ACCFG) : PAL(P_FG)); DrawTextRect(sx, y, sw, h, T[i], ALIGN_CENTER | VALIGN_MIDDLE);
    add_hit(sx, y - U / 8, sw, h + U / 4, A_WX_TAB, i);
  }
  rbtn(x + w + gap, y, h, IC_REFRESH, A_WX_REFRESH);
  rbtn(x + w + gap + h + gap, y, h, IC_WX_LIST, A_WX_CITIES);
  return y + h + U / 2;
}
/* barra de temperatures d'un dia (estil iOS): pista, tram mín.–màx. i, avui, un punt a la temperatura d'ara */
static void range_bar(int x, int cy, int w, double lo, double hi, double mn, double mx, double now) {
  int th = U / 4 > 6 ? U / 4 : 6, a, b;
  rfill(x, cy - th / 2, w, th, th / 2, PAL(P_LINE));
  if (!OK(mn) || !OK(mx) || hi - lo < 0.5) return;
  a = x + (int)((mn - lo) / (hi - lo) * w); b = x + (int)((mx - lo) / (hi - lo) * w);
  if (b - a < th) b = a + th;
  rfill(a, cy - th / 2, b - a, th, th / 2, PAL(P_FG));
  if (OK(now)) {
    int c = x + (int)((now - lo) / (hi - lo) * w);
    if (c < x + th) c = x + th;
    if (c > x + w - th) c = x + w - th;
    disc(c, cy, th, PAL(P_CARD)); disc(c, cy, th / 2, PAL(P_FG));
  }
}
/* fila d'un dia: nom, icona (i probabilitat de pluja), mínima, barra i màxima */
static void day_row(int x, int y, int w, int h, int d, double lo, double hi, int act, int arg) {
  char t[32]; int pad = U / 2, is = U + U / 4, ix = x + pad + 3 * U + U / 2, tx, bx, bw, pp = OK(W.dpp[d]) && W.dpp[d] >= 20;
  day_short(t, sizeof(t), d);
  SetFont(fBold, PAL(P_FG)); DrawTextRect(x + pad, y, 3 * U + U / 4, h, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  ic_path(wx_icon(OK(W.dcode[d]) ? (int)W.dcode[d] : -1, 1), ix, y + (h - is) / 2 - (pp ? U / 3 : 0), is, 1.9, PAL(P_FG), PAL(P_CARD));
  if (pp) { snprintf(t, sizeof(t), "%d%%", (int)W.dpp[d]); SetFont(fSmall, PAL(P_SUB)); DrawTextRect(ix - U / 2, y + (h + is) / 2 - U / 3, is + U, U * 3 / 4, t, ALIGN_CENTER | VALIGN_MIDDLE); }
  tx = ix + is + U / 2;
  deg(t, sizeof(t), W.dmin[d]); SetFont(fText, PAL(P_SUB)); DrawTextRect(tx, y, 2 * U, h, t, ALIGN_RIGHT | VALIGN_MIDDLE);
  bx = tx + 2 * U + U / 2; bw = x + w - pad - 2 * U - U / 2 - bx;
  range_bar(bx, y + h / 2, bw, lo, hi, W.dmin[d], W.dmax[d], d == W.d0 && !W.stale ? W.nt : NA);
  deg(t, sizeof(t), W.dmax[d]); SetFont(fBold, PAL(P_FG)); DrawTextRect(x + w - pad - 2 * U, y, 2 * U, h, t, ALIGN_RIGHT | VALIGN_MIDDLE);
  if (act) add_hit(x, y, w, h, act, arg);
}
static void day_span(int from, int n, double *lo, double *hi) {
  int i; *lo = 999; *hi = -999;
  for (i = from; i < from + n && i < W.nd; i++) { if (OK(W.dmin[i]) && W.dmin[i] < *lo) *lo = W.dmin[i]; if (OK(W.dmax[i]) && W.dmax[i] > *hi) *hi = W.dmax[i]; }
  if (!W.stale && OK(W.nt)) { if (W.nt < *lo) *lo = W.nt; if (W.nt > *hi) *hi = W.nt; }
}
/* graella d'hores: cols columnes per fila; now = índex de l'hora actual (les anteriors, en gris) */
static int hour_grid(int x, int y, int w, int from, int n, int cols, int rh, int now) {
  int cw = w / cols, k, is = rh * 32 / 100;
  if (is > cw * 55 / 100) is = cw * 55 / 100;
  for (k = 0; k < n && from + k < W.nh; k++) {
    int i = from + k, cx = x + (k % cols) * cw, cy = y + (k / cols) * rh, past = now >= 0 && i < now, fg = past ? PAL(P_SUB) : PAL(P_FG);
    char t[24];
    if (i == now) snprintf(t, sizeof(t), "Ara"); else snprintf(t, sizeof(t), "%d h", atoi(W.ht[i] + 11));
    SetFont(i == now ? fBold : fSmall, fg); DrawTextRect(cx, cy, cw, rh * 20 / 100, t, ALIGN_CENTER | VALIGN_MIDDLE);
    ic_path(wx_icon(OK(W.hcode[i]) ? (int)W.hcode[i] : -1, OK(W.hday[i]) ? (int)W.hday[i] : 1), cx + (cw - is) / 2, cy + rh * 22 / 100, is, 1.9, fg, PAL(P_CARD));
    if (OK(W.hpp[i]) && W.hpp[i] >= 20) { snprintf(t, sizeof(t), "%d%%", (int)W.hpp[i]); SetFont(fSmall, PAL(P_SUB)); DrawTextRect(cx, cy + rh * 22 / 100 + is, cw, rh * 18 / 100, t, ALIGN_CENTER | VALIGN_MIDDLE); }
    deg(t, sizeof(t), W.htemp[i]); SetFont(fBold, fg); DrawTextRect(cx, cy + rh * 76 / 100, cw, rh * 22 / 100, t, ALIGN_CENTER | VALIGN_MIDDLE);
  }
  return y + ((n + cols - 1) / cols) * rh;
}
/* frase de resum per a les properes hores, com la de la targeta de l'iOS */
static void wx_summary(char *o, int sz) {
  int k, h = W.h0, c0; char hh[8], g[64] = "";
  if (h < 0 || !W.nh) { snprintf(o, (size_t)sz, W.nh ? "Aquesta previsió ja és antiga. Actualitza-la amb Wi-Fi." : "Actualitza per veure la previsió per hores."); return; }
  c0 = OK(W.hcode[h]) ? (int)W.hcode[h] : W.ncode;
  if (OK(W.dgust[W.d0]) && W.dgust[W.d0] >= 40) snprintf(g, sizeof(g), " Ratxes de vent de fins a %d km/h.", (int)floor(W.dgust[W.d0] + 0.5));
  if (is_wet(c0) || is_snow(c0)) {
    for (k = 1; k <= 12 && h + k < W.nh; k++) if (OK(W.hcode[h + k]) && !is_wet((int)W.hcode[h + k]) && !is_snow((int)W.hcode[h + k])) break;
    if (k <= 12 && h + k < W.nh) { hm(hh, W.ht[h + k]); snprintf(o, (size_t)sz, "%s fins a les %s.%s", is_snow(c0) ? "Neu" : c0 >= 95 ? "Tempesta" : "Pluja", hh, g); }
    else snprintf(o, (size_t)sz, "%s durant les properes hores.%s", is_snow(c0) ? "Neu" : "Pluja", g);
    return;
  }
  for (k = 1; k <= 12 && h + k < W.nh; k++) {
    int c = OK(W.hcode[h + k]) ? (int)W.hcode[h + k] : 0;
    if (is_wet(c) || is_snow(c) || (OK(W.hpp[h + k]) && W.hpp[h + k] >= 50)) {
      hm(hh, W.ht[h + k]);
      snprintf(o, (size_t)sz, "%s probable cap a les %s.%s", is_snow(c) ? "Neu" : c >= 95 ? "Tempesta" : "Pluja", hh, g);
      return;
    }
  }
  snprintf(o, (size_t)sz, "Sense pluja prevista les properes 12 hores.%s", g);
}

/* ───── pestanya «Ara» ───── */
static int hero(int y, int with_hilo) {
  char t[24], hl[64]; int is = 3 * U, hh = 4 * U, tw, x0;
  deg(t, sizeof(t), W.nt);
  SetFont(GIANT, PAL(P_FG)); tw = StringWidth(t);
  x0 = (SW - is - U / 2 - tw) / 2;
  ic_path(wx_icon(W.ncode, W.nday), x0, y + (hh - is) / 2, is, 1.5, PAL(P_FG), PAL(P_BG));
  DrawTextRect(x0 + is + U / 2, y, tw + U, hh, t, ALIGN_LEFT | VALIGN_MIDDLE);
  y += hh;
  SetFont(fBig, PAL(P_FG)); DrawTextRect(U / 2, y, SW - U, U + U / 2, W.ncode >= 0 ? wx_desc(W.ncode) : "", ALIGN_CENTER | VALIGN_MIDDLE | DOTS);
  y += U + U / 2;
  if (with_hilo) { hilo(hl, sizeof(hl), W.d0); SetFont(fText, PAL(P_FG)); DrawTextRect(U / 2, y, SW - U, U + U / 4, hl, ALIGN_CENTER | VALIGN_MIDDLE); y += U + U / 4; }
  return y;
}
static void tab_now(int y, int ybot) {
  char s[200]; int x = U / 2, w = SW - U, pad = U / 2, ch, rh = 3 * U + U / 2, n, rows, i, dh = 2 * U + U / 4, sh = 2 * U + U / 2;
  double lo, hi;
  y = hero(y, 1) + U / 2;
  /* targeta per hores: resum i 12 hores en dues files */
  ch = pad + sh + U / 4 + (W.h0 >= 0 ? 2 * rh : 0) + pad / 2;
  ui_card(x, y, w, ch, UI_LINE);
  wx_summary(s, sizeof(s));
  SetFont(fText, PAL(P_FG)); DrawTextRect(x + pad, y + pad, w - 2 * pad, sh, s, ALIGN_LEFT | VALIGN_MIDDLE);
  if (W.h0 >= 0) {
    DrawLine(x + pad, y + pad + sh + U / 8, x + w - pad, y + pad + sh + U / 8, PAL(P_LINE));
    hour_grid(x + pad / 2, y + pad + sh + U / 4, w - pad, W.h0, 12, 6, rh, W.h0);
  }
  y += ch + U / 2;
  /* avançament de la previsió de 10 dies (toca-la per veure-la sencera) */
  rows = (ybot - y - pad - U - U / 4 - pad) / dh;
  n = W.nd - W.d0; if (n > 10) n = 10;
  if (rows > n) rows = n;
  if (rows < 1) return;
  ui_card(x, y, w, pad + U + U / 4 + rows * dh + pad / 2, UI_LINE);
  sec(x + pad, y + pad, w - 2 * pad, IC_AGENDA, "PREVISIÓ DE 10 DIES");
  add_hit(x, y, w, pad + U + U / 4, A_WX_TAB, TAB_DAYS);
  day_span(W.d0, n, &lo, &hi);
  for (i = 0; i < rows; i++) {
    int ry = y + pad + U + U / 4 + i * dh;
    if (i) DrawLine(x + pad, ry, x + w - pad, ry, PAL(P_LINE));
    day_row(x, ry, w, dh, W.d0 + i, lo, hi, A_WX_DAY, i);
  }
}
/* ───── pestanya «10 dies» ───── */
static void tab_days(int y, int ybot) {
  int x = U / 2, w = SW - U, pad = U / 3, n = W.nd - W.d0, rh, i; double lo, hi;
  if (n > 10) n = 10;
  if (n < 1) return;
  rh = (ybot - y - 2 * pad) / n; if (rh > 2 * U + U / 2) rh = 2 * U + U / 2;
  ui_card(x, y, w, n * rh + 2 * pad, UI_LINE);
  day_span(W.d0, n, &lo, &hi);
  for (i = 0; i < n; i++) {
    if (i) DrawLine(x + U / 2, y + pad + i * rh, x + w - U / 2, y + pad + i * rh, PAL(P_LINE));
    day_row(x, y + pad + i * rh, w, rh, W.d0 + i, lo, hi, A_WX_DAY, i);
  }
}
/* ───── pestanya «Detalls»: rajoles ───── */
static int tile(int x, int y, int w, int h, const char *ic, const char *label, const char *value) {
  ui_card(x, y, w, h, UI_LINE);
  sec(x + U / 2, y + U / 3, w - U, ic, label);
  if (value) { SetFont(MID, PAL(P_FG)); DrawTextRect(x + U / 2, y + U / 3 + U, w - U, 2 * U, value, ALIGN_LEFT | VALIGN_MIDDLE | DOTS); }
  return y + U / 3 + 3 * U;
}
static void caption(int x, int y, int w, int h, const char *t) { SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + U / 2, y + h - U / 3 - 2 * U, w - U, 2 * U, t, ALIGN_LEFT | VALIGN_BOTTOM); }
static void t_uv(int x, int y, int w, int h) {
  char v[16], c[64]; int vy, bx = x + U / 2, bw = w - U, th = U / 4, cx; double u = OK(W.uv) ? W.uv : (OK(W.duv[W.d0]) ? W.duv[W.d0] : NA);
  if (OK(u)) snprintf(v, sizeof(v), "%d", (int)floor(u + 0.5)); else snprintf(v, sizeof(v), "—");
  vy = tile(x, y, w, h, IC_SUN, "ÍNDEX UV", v);
  if (OK(u)) { SetFont(MID, PAL(P_FG)); cx = StringWidth(v); SetFont(fBold, PAL(P_FG)); DrawTextRect(x + U / 2 + cx + U / 3, y + U / 3 + U, w - U - cx, 2 * U, uv_level(u), ALIGN_LEFT | VALIGN_MIDDLE | DOTS); }
  rfill(bx, vy, bw, th, th / 2, PAL(P_LINE));
  if (OK(u)) { cx = bx + (int)((u > 11 ? 11 : u) / 11.0 * bw); if (cx < bx + th) cx = bx + th; rfill(bx, vy, cx - bx, th, th / 2, PAL(P_FG)); disc(cx, vy + th / 2, th, PAL(P_CARD)); disc(cx, vy + th / 2, th / 2, PAL(P_FG)); }
  if (OK(W.duv[W.d0])) { snprintf(c, sizeof(c), "Màxim d'avui: %d (%s).", (int)floor(W.duv[W.d0] + 0.5), uv_level(W.duv[W.d0])); caption(x, y, w, h, c); }
}
static void t_sun(int x, int y, int w, int h) {
  char v[8], c[48], a[8]; int d = W.d0, rise = iso_min(W.rise[d]), set = iso_min(W.set[d]), now = city_min(), gx = x + U / 2, gw = w - U, gy, gh, k, px = -1, py = 0;
  const char *lab;
  if (rise < 0 || set <= rise) { tile(x, y, w, h, IC_WX_SUNSET, "SOL", "—"); return; }
  if (now < rise) { lab = "SORTIDA DEL SOL"; hm(v, W.rise[d]); hm(a, W.set[d]); snprintf(c, sizeof(c), "Posta: %s", a); }
  else if (now < set) { lab = "POSTA DE SOL"; hm(v, W.set[d]); hm(a, W.rise[d]); snprintf(c, sizeof(c), "Sortida: %s", a); }
  else { lab = "SORTIDA DEL SOL"; hm(v, d + 1 < W.nd ? W.rise[d + 1] : W.rise[d]); hm(a, W.set[d]); snprintf(c, sizeof(c), "Posta: %s", a); }
  gy = tile(x, y, w, h, IC_WX_SUNSET, lab, v) - U / 4; gh = y + h - U / 3 - U - U / 4 - gy;
  /* corba del sol: per sobre de l'horitzó de dia, per sota de nit */
  {
    int hz = gy + gh * 6 / 10, A1 = gh * 6 / 10 - 2, A2 = gh * 4 / 10 - 2;
    DrawLine(gx, hz, gx + gw, hz, PAL(P_LINE));
    for (k = 0; k <= gw; k += 2) {
      int m = k * 1440 / gw, yy;
      if (m >= rise && m <= set) yy = hz - (int)(A1 * sin(3.14159265 * (m - rise) / (set - rise)));
      else { int nl = 1440 - (set - rise), mm = m > set ? m - set : m + 1440 - set; yy = hz + (int)(A2 * sin(3.14159265 * mm / nl)); }
      FillArea(gx + k, yy - 1, 2, 3, PAL(P_SUB));
      if (px < 0 && m >= now) { px = gx + k; py = yy; }
    }
    if (px >= 0) { disc(px, py, U / 4 + 2, PAL(P_CARD)); disc(px, py, U / 4 - 1, PAL(P_FG)); }
  }
  SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + U / 2, y + h - U / 3 - U, w - U, U, c, ALIGN_LEFT | VALIGN_MIDDLE);
}
static void t_wind(int x, int y, int w, int h) {
  char v[24], c[64]; int r = h * 30 / 100, cx = x + w - U / 2 - r, cy = y + h / 2 + U / 3, tw = w - U - 2 * r - U / 3, k;
  if (r > (w - U) / 4) r = (w - U) / 4;
  cx = x + w - U / 2 - r; tw = w - U - 2 * r - U / 3;
  if (OK(W.wind)) snprintf(v, sizeof(v), "%d km/h", (int)floor(W.wind + 0.5)); else snprintf(v, sizeof(v), "—");
  ui_card(x, y, w, h, UI_LINE);
  sec(x + U / 2, y + U / 3, w - U, IC_WX_WIND, "VENT");
  SetFont(MID, PAL(P_FG)); DrawTextRect(x + U / 2, y + U / 3 + U, tw, 2 * U, v, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  SetFont(fSmall, PAL(P_SUB));
  if (OK(W.gust)) { snprintf(c, sizeof(c), "Ratxes: %d km/h", (int)floor(W.gust + 0.5)); DrawTextRect(x + U / 2, y + h - U / 3 - 2 * U, tw, U, c, ALIGN_LEFT | VALIGN_MIDDLE | DOTS); }
  if (OK(W.wdir)) { snprintf(c, sizeof(c), "Del %s", compass(W.wdir, 1)); DrawTextRect(x + U / 2, y + h - U / 3 - U, tw, U, c, ALIGN_LEFT | VALIGN_MIDDLE | DOTS); }
  /* rosa dels vents: la fletxa apunta cap on bufa */
  disc(cx, cy, r, PAL(P_LINE)); disc(cx, cy, r - 3, PAL(P_CARD));
  {
    static const char *NS[4] = { "N", "E", "S", "O" };
    for (k = 0; k < 4; k++) {
      double a = k * 3.14159265 / 2; int lx = cx + (int)(sin(a) * (r - U / 2)), ly = cy - (int)(cos(a) * (r - U / 2));
      SetFont(fSmall, PAL(P_SUB)); DrawTextRect(lx - U / 2, ly - U / 2, U, U, NS[k], ALIGN_CENTER | VALIGN_MIDDLE);
    }
  }
  if (OK(W.wdir)) {
    double a = (W.wdir + 180) * 3.14159265 / 180, L = r * 0.5; int t = U / 8 > 3 ? U / 8 : 3;
    int hx = cx + (int)(sin(a) * L), hy = cy - (int)(cos(a) * L), bx = cx - (int)(sin(a) * L), by = cy + (int)(cos(a) * L);
    thick(bx, by, hx, hy, t, PAL(P_FG));
    thick(hx, hy, hx - (int)(sin(a - 0.5) * r * 0.3), hy + (int)(cos(a - 0.5) * r * 0.3), t, PAL(P_FG));
    thick(hx, hy, hx - (int)(sin(a + 0.5) * r * 0.3), hy + (int)(cos(a + 0.5) * r * 0.3), t, PAL(P_FG));
    disc(bx, by, t, PAL(P_FG));
  }
}
static void t_prec(int x, int y, int w, int h) {
  char v[24], n[16], c[80], dn[16]; int k; double s = W.dsum[W.d0];
  if (OK(s)) { num1(n, sizeof(n), s); snprintf(v, sizeof(v), "%s mm", n); } else snprintf(v, sizeof(v), "—");
  tile(x, y, w, h, IC_WX_DROP, "PRECIPITACIÓ D'AVUI", v);
  for (k = W.d0 + 1; k < W.nd && k < W.d0 + 10; k++) if (OK(W.dsum[k]) && W.dsum[k] >= 1) break;
  if (k < W.nd && k < W.d0 + 10) {
    int yy, mm, dd; day_parts(k, &yy, &mm, &dd); num1(n, sizeof(n), W.dsum[k]);
    if (k == W.d0 + 1) snprintf(dn, sizeof(dn), "demà"); else snprintf(dn, sizeof(dn), "%s", mm ? WDAY[weekday_of(yy, mm - 1, dd)] : "més endavant");
    snprintf(c, sizeof(c), "S'esperen %s mm %s.", n, dn);
  } else snprintf(c, sizeof(c), "No se n'espera els propers dies.");
  caption(x, y, w, h, c);
}
static void t_feel(int x, int y, int w, int h) {
  char v[16], c[80]; double d;
  deg(v, sizeof(v), W.feel); tile(x, y, w, h, IC_WX_THERMO, "SENSACIÓ TÈRMICA", v);
  if (!OK(W.feel) || !OK(W.t)) return;
  d = W.feel - W.t;
  if (fabs(d) < 1.5) snprintf(c, sizeof(c), "Semblant a la temperatura real.");
  else if (d < 0) snprintf(c, sizeof(c), OK(W.wind) && W.wind >= 15 ? "El vent fa que sembli més fred." : "Sembla més fred que la temperatura real.");
  else snprintf(c, sizeof(c), OK(W.hum) && W.hum >= 60 ? "La humitat fa que sembli més calorós." : "Sembla més calorós que la temperatura real.");
  caption(x, y, w, h, c);
}
static void t_hum(int x, int y, int w, int h) {
  char v[16], c[80], dp[16];
  if (OK(W.hum)) snprintf(v, sizeof(v), "%d%%", (int)floor(W.hum + 0.5)); else snprintf(v, sizeof(v), "—");
  tile(x, y, w, h, IC_WX_DROP, "HUMITAT", v);
  if (OK(W.hum) && OK(W.t) && W.hum > 0) {                     /* punt de rosada (fórmula de Magnus) */
    double g = 17.27 * W.t / (237.7 + W.t) + log(W.hum / 100.0);
    deg(dp, sizeof(dp), 237.7 * g / (17.27 - g)); snprintf(c, sizeof(c), "El punt de rosada és de %s ara.", dp); caption(x, y, w, h, c);
  }
}
static void t_vis(int x, int y, int w, int h) {
  char v[24], n[16]; const char *c = NULL; double km = OK(W.vis) ? W.vis / 1000.0 : NA;
  if (!OK(km)) snprintf(v, sizeof(v), "—");
  else if (km < 1) snprintf(v, sizeof(v), "%d m", (int)(W.vis / 100) * 100);
  else { num1(n, sizeof(n), km > 50 ? 50 : km); snprintf(v, sizeof(v), "%s km", n); }
  tile(x, y, w, h, IC_WX_EYE, "VISIBILITAT", v);
  if (OK(km)) c = km >= 10 ? "Visibilitat perfecta." : km >= 5 ? "Bona visibilitat." : km >= 1 ? "Visibilitat reduïda." : "Boira densa: molt poca visibilitat.";
  if (c) caption(x, y, w, h, c);
}
static void t_pres(int x, int y, int w, int h) {
  char v[24], c[64]; const char *tr = NULL;
  if (OK(W.pres)) snprintf(v, sizeof(v), "%d hPa", (int)floor(W.pres + 0.5)); else snprintf(v, sizeof(v), "—");
  tile(x, y, w, h, IC_WX_GAUGE, "PRESSIÓ", v);
  if (W.h0 >= 0 && W.h0 + 3 < W.nh && OK(W.hpres[W.h0]) && OK(W.hpres[W.h0 + 3])) {
    double d = W.hpres[W.h0 + 3] - W.hpres[W.h0];
    tr = d > 1 ? "pujant" : d < -1 ? "baixant" : "estable";
  }
  if (tr) { snprintf(c, sizeof(c), "Tendència: %s.", tr); caption(x, y, w, h, c); }
}
static void tab_more(int y, int ybot) {
  typedef void (*TileFn)(int, int, int, int);
  static const TileFn T[8] = { t_uv, t_sun, t_wind, t_prec, t_feel, t_hum, t_vis, t_pres };
  int gap = U / 3, w = (SW - U - gap) / 2, h = (ybot - y - 3 * gap) / 4, i;
  if (h > 7 * U) h = 7 * U;
  for (i = 0; i < 8; i++) T[i](U / 2 + (i % 2) * (w + gap), y + (i / 2) * (h + gap), w, h);
}

/* ───── vistes ───── */
static int pendingCity = -1;
static void v_weather(View *v) {
  char sub[160], name[64]; int ci, y, ybot = BOTTOM - U / 4; double lat = 0, lon = 0;
  cities_load();
  if (pendingCity >= 0) { v->page = pendingCity; pendingCity = -1; }
  pages = cities.n > 0 ? cities.n : 1;
  ci = v->page; if (ci < 0 || ci >= cities.n) ci = v->page = 0;
  city_get(ci, name, sizeof(name), &lat, &lon, NULL, 0);
  wx_load(lat, lon);
  if (W.ok) {
    time_t now = time(NULL); struct tm tn = *localtime(&now), tu = *localtime(&W.mt), *tm = &tu;
    if (tm->tm_mday == tn.tm_mday && tm->tm_mon == tn.tm_mon) snprintf(sub, sizeof(sub), "Actualitzat a les %02d:%02d", tm->tm_hour, tm->tm_min);
    else snprintf(sub, sizeof(sub), "Actualitzat el %d/%d a les %02d:%02d", tm->tm_mday, tm->tm_mon + 1, tm->tm_hour, tm->tm_min);
    if (W.stale) snprintf(sub + strlen(sub), sizeof(sub) - strlen(sub), " · previsió antiga");
  } else snprintf(sub, sizeof(sub), "Sense previsió desada");
  header(name, sub);
  y = tabs(v, TOP);
  if (!W.ok) {
    int bh = 2 * U + U / 4;
    ic_path(IC_WX_PARTLY, (SW - 4 * U) / 2, y + U, 4 * U, 1.5, PAL(P_SUB), PAL(P_BG));
    SetFont(fText, PAL(P_FG)); DrawTextRect(U, y + 6 * U, SW - 2 * U, 2 * U, "Encara no hi ha cap previsió desada per a aquesta ciutat.", ALIGN_CENTER | VALIGN_MIDDLE);
    button(2 * U, y + 9 * U, SW - 4 * U, bh, "Actualitza (cal Wi-Fi)", 1, A_WX_REFRESH, 0);
    return;
  }
  if (v->sel == TAB_DAYS) tab_days(y, ybot);
  else if (v->sel == TAB_MORE) tab_more(y, ybot);
  else tab_now(y, ybot);
}
/* un dia: resum, les 24 hores i dades del dia; el peu passa al dia següent */
static void v_day(View *v) {
  char name[64], ti[40], sub[120], t[64], a[8], b[8]; int d, yy, mm, dd, x = U / 2, w = SW - U, pad = U / 2, y = TOP, is = 3 * U, ch, h, n, rh, i;
  double lat = 0, lon = 0;
  cities_load();
  city_get(v->a, name, sizeof(name), &lat, &lon, NULL, 0);
  if (!wx_load(lat, lon)) { header("El temps", name); return; }
  if (v->b) { v->page = v->b - 1; v->b = 0; }
  pages = W.nd - W.d0; if (pages > 10) pages = 10; if (pages < 1) pages = 1;
  if (v->page >= pages) v->page = pages - 1;
  d = W.d0 + v->page; day_parts(d, &yy, &mm, &dd);
  if (v->page == 0 && !W.stale) snprintf(ti, sizeof(ti), "Avui");
  else if (v->page == 1 && !W.stale) snprintf(ti, sizeof(ti), "Demà");
  else snprintf(ti, sizeof(ti), "%c%s", mm ? WDAY[weekday_of(yy, mm - 1, dd)][0] - 32 : '-', mm ? WDAY[weekday_of(yy, mm - 1, dd)] + 1 : "");
  if (mm) snprintf(sub, sizeof(sub), "%s, %d %s · %s", WDAY[weekday_of(yy, mm - 1, dd)], dd, MONTH[mm - 1], name); else snprintf(sub, sizeof(sub), "%s", name);
  header(ti, sub);
  /* resum del dia */
  ch = 4 * U + U / 2;
  ui_card(x, y, w, ch, UI_LINE);
  ic_path(wx_icon(OK(W.dcode[d]) ? (int)W.dcode[d] : -1, 1), x + pad, y + (ch - is) / 2, is, 1.6, PAL(P_FG), PAL(P_CARD));
  SetFont(fBig, PAL(P_FG)); DrawTextRect(x + pad + is + U / 2, y + pad, w - 2 * pad - is - U / 2, U + U / 2, OK(W.dcode[d]) ? wx_desc((int)W.dcode[d]) : "—", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  hilo(t, sizeof(t), d); SetFont(fText, PAL(P_FG)); DrawTextRect(x + pad + is + U / 2, y + pad + U + U / 2, w - 2 * pad - is - U / 2, U + U / 8, t, ALIGN_LEFT | VALIGN_MIDDLE);
  {
    char n1[16]; t[0] = 0;
    if (OK(W.dpp[d])) snprintf(t, sizeof(t), "Probabilitat de pluja: %d%%", (int)W.dpp[d]);
    if (OK(W.dsum[d]) && W.dsum[d] >= 0.1) { num1(n1, sizeof(n1), W.dsum[d]); snprintf(t + strlen(t), sizeof(t) - strlen(t), "%s%s mm", t[0] ? " · " : "", n1); }
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + pad + is + U / 2, y + pad + 2 * U + U / 2, w - 2 * pad - is - U / 2, U, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
  }
  y += ch + U / 2;
  /* les hores del dia */
  for (h = 0; h < W.nh && strncmp(W.ht[h], W.dt[d], 10); h++) ;
  for (n = 0; h + n < W.nh && !strncmp(W.ht[h + n], W.dt[d], 10) && n < 24; n++) ;
  rh = 3 * U + U * 3 / 4;
  if (n > 0) {
    ch = pad + U + U / 4 + ((n + 5) / 6) * rh + pad / 2;
    ui_card(x, y, w, ch, UI_LINE);
    sec(x + pad, y + pad, w - 2 * pad, IC_CLOCK, "PER HORES");
    hour_grid(x + pad / 2, y + pad + U + U / 4, w - pad, h, n, 6, rh, W.h0 >= h && W.h0 < h + n ? W.h0 : -1);
    y += ch + U / 2;
  }
  /* dades del dia en quatre caselles */
  {
    const char *L[4] = { "Vent màxim", "Ratxes", "Índex UV màxim", "Sol" }; char V[4][32]; int cw = (w - 2 * pad) / 2, bh = 2 * U + U / 4;
    if (OK(W.dwind[d])) snprintf(V[0], 32, "%d km/h", (int)floor(W.dwind[d] + 0.5)); else snprintf(V[0], 32, "—");
    if (OK(W.dgust[d])) snprintf(V[1], 32, "%d km/h", (int)floor(W.dgust[d] + 0.5)); else snprintf(V[1], 32, "—");
    if (OK(W.duv[d])) snprintf(V[2], 32, "%d · %s", (int)floor(W.duv[d] + 0.5), uv_level(W.duv[d])); else snprintf(V[2], 32, "—");
    hm(a, W.rise[d]); hm(b, W.set[d]); snprintf(V[3], 32, "%s – %s", a, b);
    if (y + 2 * bh + 2 * pad > BOTTOM) return;
    ui_card(x, y, w, 2 * bh + pad, UI_LINE);
    for (i = 0; i < 4; i++) {
      int cx = x + pad + (i % 2) * cw, cy = y + pad / 2 + (i / 2) * bh;
      SetFont(fSmall, PAL(P_SUB)); DrawTextRect(cx, cy, cw - U / 4, bh * 4 / 10, L[i], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
      SetFont(fBold, PAL(P_FG)); DrawTextRect(cx, cy + bh * 4 / 10, cw - U / 4, bh * 6 / 10, V[i], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    }
  }
}

/* ───── cerca de ciutats ───── */
static struct { char name[64], region[96]; double lat, lon; } res[6];
static int nres;
static void city_search(const char *q) {
  char enc[200], url[400], *b; int len = 0; const char *p;
  while (*q == ' ') q++;
  if (!*q) return;
  urlenc(q, enc, sizeof(enc));
  snprintf(url, sizeof(url), GEOURL_FMT, enc);
  b = net_get(url, &len);
  if (!b) { say("No s'ha pogut fer la cerca. Comprova el Wi-Fi."); return; }
  nres = 0;
  p = strstr(b, "\"results\":");
  if (p) p = strstr(p, "{\"id\":");
  while (p && nres < 6) {
    const char *nx = strstr(p + 1, "{\"id\":"); size_t sl = nx ? (size_t)(nx - p) : strlen(p);
    char obj[3000], a1[60], co[60], *t;
    const char *k;
    if (sl > sizeof(obj) - 1) sl = sizeof(obj) - 1;
    memcpy(obj, p, sl); obj[sl] = 0;
    if (jstr(jkey(obj, "name"), res[nres].name, sizeof(res[nres].name)) && (k = jkey(obj, "latitude"))) {
      res[nres].lat = strtod(k, NULL);
      res[nres].lon = (k = jkey(obj, "longitude")) ? strtod(k, NULL) : 0;
      jstr(jkey(obj, "admin1"), a1, sizeof(a1)); jstr(jkey(obj, "country"), co, sizeof(co));
      snprintf(res[nres].region, sizeof(res[nres].region), "%s%s%s", a1, a1[0] && co[0] ? ", " : "", co);
      while ((t = strchr(res[nres].name, '\t'))) *t = ' ';
      nres++;
    }
    p = nx;
  }
  free(b);
  if (!nres) { say("No s'ha trobat cap ciutat amb aquest nom."); return; }
  push(V_RESULTS, 0, 0, 0, NULL);
}
static void pick_city(int i) {
  char ln[320]; int k, ok;
  if (i < 0 || i >= nres) return;
  cities_load();
  if (cities.n >= MAXC) { say("Has arribat al màxim de ciutats. Esborra'n alguna."); return; }
  for (k = 0; k < cities.n; k++) {
    char nm[64]; double la, lo;
    if (city_get(k, nm, sizeof(nm), &la, &lo, NULL, 0) && fabs(la - res[i].lat) < 0.05 && fabs(lo - res[i].lon) < 0.05) { say("Aquesta ciutat ja és a la llista."); return; }
  }
  snprintf(ln, sizeof(ln), "%s\t%.4f\t%.4f\t%s", res[i].name, res[i].lat, res[i].lon, res[i].region);
  st_insert(&cities, cities.n, ln); st_save(&cities);
  ok = wx_fetch(res[i].lat, res[i].lon);
  POP(); draw(1);
  if (!ok) say("S'ha afegit la ciutat, però no s'ha pogut baixar la previsió. Prem Actualitza.");
}
static int delIdx = -1, editing;
static void city_del(void) {
  char nm[64], path[256]; double la, lo;
  if (city_get(delIdx, nm, sizeof(nm), &la, &lo, NULL, 0)) { wx_path(path, sizeof(path), la, lo); remove(path); }
  st_del(&cities, delIdx); st_save(&cities); delIdx = -1; wPath[0] = 0;
}
/* llista de ciutats: targetes amb l'hora local, l'estat, la temperatura i la màxima i la mínima */
static void v_cities(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 3 * U + U / 2, gap = U / 3, bw = (SW - U - U / 3) / 2, per, i, x = U / 2, w = SW - U, pad = U / 2; char sub[80];
  cities_load();
  if (editing) snprintf(sub, sizeof(sub), "Tria la ciutat principal o esborra'n");
  else snprintf(sub, sizeof(sub), cities.n == 1 ? "%d ciutat" : "%d ciutats", cities.n);
  header("Ciutats", sub);
  button(x, y, bw, bh, "Afegeix una ciutat", 1, A_WX_ADD, 0);
  button(x + bw + U / 3, y, bw, bh, editing ? "Fet" : "Edita", 0, A_WX_EDIT, 0);
  button(x, BOTTOM - bh, w, bh, "Actualitza-les totes (cal Wi-Fi)", 0, A_WX_ALL, 0);
  y += bh + U / 2;
  per = (BOTTOM - bh - U / 2 - y) / (rh + gap); if (per < 1) per = 1;
  pages = cities.n ? (cities.n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < cities.n && i < (v->page + 1) * per; i++, y += rh + gap) {
    char nm[64], rg[96], t[80], c[24]; double la, lo; int ok;
    if (!city_get(i, nm, sizeof(nm), &la, &lo, rg, sizeof(rg))) continue;
    ok = wx_load(la, lo);
    ui_card(x, y, w, rh, UI_LINE);
    SetFont(fBig, PAL(P_FG)); DrawTextRect(x + pad, y + pad / 2, w * 55 / 100, rh / 2, nm, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (ok) { city_clock(c, sizeof(c)); snprintf(t, sizeof(t), "%s%s · %s", i == 0 ? "Principal · " : "", c, W.ncode >= 0 ? wx_desc(W.ncode) : ""); }
    else snprintf(t, sizeof(t), "%sSense dades", i == 0 ? "Principal · " : "");
    SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + pad, y + rh / 2, w * 55 / 100, rh / 2 - pad / 2, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (editing) {
      int pw = 5 * U, ph = 2 * U - U / 4, py = y + (rh - ph) / 2;
      button(x + w - pad - pw, py, pw, ph, "Esborra", 0, A_WX_DEL, i);
      if (i > 0) button(x + w - pad - 2 * pw - U / 3, py, pw, ph, "Principal", 0, A_WX_MAKEMAIN, i);
      continue;
    }
    add_hit(x, y, w, rh, A_WX_OPEN, i);
    if (ok) {
      int is = U + U / 2, tw;
      deg(t, sizeof(t), W.nt); SetFont(MID, PAL(P_FG)); tw = StringWidth(t);
      DrawTextRect(x + w - pad - tw - 4, y + pad / 2, tw + 4, rh / 2, t, ALIGN_RIGHT | VALIGN_MIDDLE);
      ic_path(wx_icon(W.ncode, W.nday), x + w - pad - tw - U / 3 - is, y + pad / 2 + (rh / 2 - is) / 2, is, 1.9, PAL(P_FG), PAL(P_CARD));
      hilo(t, sizeof(t), W.d0); SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x + w * 45 / 100, y + rh / 2, w * 55 / 100 - pad, rh / 2 - pad / 2, t, ALIGN_RIGHT | VALIGN_MIDDLE);
    }
  }
}
static void v_results(View *v) {
  int y = TOP, rh = 3 * U, i; (void)v;
  header("Resultats", "Tria una ciutat");
  for (i = 0; i < nres; i++) { ui_row(U / 2, y, SW - U, rh - U / 4, res[i].name, res[i].region[0] ? res[i].region : NULL, NULL, A_WX_PICK, i); y += rh; }
}
/* ───── widget ───── */
static void w_today(int x, int y, int w, int h) {
  char t[80] = "sense dades", lab[100] = "El temps", name[64]; double lat, lon; int is = h * 7 / 10;
  cities_load();
  if (city_get(0, name, sizeof(name), &lat, &lon, NULL, 0)) {
    snprintf(lab, sizeof(lab), "El temps · %s", name);
    if (wx_load(lat, lon) && OK(W.nt)) {
      char d[16], mx[16], mn[16]; int tw;
      ic_path(wx_icon(W.ncode, W.nday), x + w - is, y + (h - is) / 2, is, 1.8, PAL(P_FG), PAL(P_CARD));
      w -= is + U / 4;
      SetFont(fSmall, PAL(P_SUB)); DrawTextRect(x, y, w, h * 4 / 10, lab, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
      deg(d, sizeof(d), W.nt); SetFont(fBig, PAL(P_FG)); tw = StringWidth(d);
      DrawTextRect(x, y + h * 4 / 10, tw + 4, h * 6 / 10, d, ALIGN_LEFT | VALIGN_MIDDLE);
      deg(mx, sizeof(mx), W.dmax[W.d0]); deg(mn, sizeof(mn), W.dmin[W.d0]);
      snprintf(t, sizeof(t), "%s · %s/%s", W.ncode >= 0 ? wx_desc(W.ncode) : "", mx, mn);
      SetFont(fSmall, PAL(P_FG)); DrawTextRect(x + tw + U / 3, y + h * 4 / 10, w - tw - U / 3, h * 6 / 10, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
      return;
    }
  }
  ui_lv(x, y, w, h, lab, t);
}
static const Widget WG[] = { { "avui", "El temps d'avui", 2, 1, w_today } };
static void mod_icon(int x, int y, int s) { ic_path(IC_SUN, x, y, s, 1.8, ICON_FG, ICON_BG); }   /* icona del prototip (icons.h) */
static void mod_draw(View *v) {
  fonts_open();
  switch (v->type) {
    case V_WEATHER: v_weather(v); break;
    case V_CITIES: v_cities(v); break;
    case V_RESULTS: v_results(v); break;
    case V_DAY: v_day(v); break;
  }
  fonts_close();
}
static void mod_action(View *v, int a, int arg) {
  switch (a) {
    case A_WX_REFRESH: {
      char nm[64]; double la, lo;
      cities_load();
      if (city_get(v->type == V_DAY ? v->a : v->page, nm, sizeof(nm), &la, &lo, NULL, 0) && !wx_fetch(la, lo)) say("No s'ha pogut descarregar la previsió. Comprova el Wi-Fi.");
      draw(1); break;
    }
    case A_WX_TAB: v->sel = arg; draw(0); break;
    case A_WX_DAY: push(V_DAY, v->page, arg + 1, 0, NULL); break;
    case A_WX_CITIES: editing = 0; push(V_CITIES, 0, 0, 0, NULL); break;
    case A_WX_ADD: ask("Nom de la ciutat", "", city_search); break;
    case A_WX_EDIT: editing = !editing; draw(0); break;
    case A_WX_OPEN: pendingCity = arg; POP(); draw(1); break;
    case A_WX_DEL: {
      static char msg[120]; char nm[64]; double la, lo;
      if (cities.n <= 1) { say("Ha de quedar almenys una ciutat."); break; }
      if (!city_get(arg, nm, sizeof(nm), &la, &lo, NULL, 0)) break;
      delIdx = arg; snprintf(msg, sizeof(msg), "Vols esborrar %s de la llista?", nm);
      confirm("Ciutats", msg, "Esborra", city_del); break;
    }
    case A_WX_PICK: pick_city(arg); break;
    case A_WX_MAKEMAIN: {
      char ln[320];
      if (arg > 0 && arg < cities.n) { snprintf(ln, sizeof(ln), "%s", cities.it[arg]); st_del(&cities, arg); st_insert(&cities, 0, ln); st_save(&cities); }
      draw(0); break;
    }
    case A_WX_ALL: {
      int i, bad = 0;
      cities_load();
      for (i = 0; i < cities.n; i++) { char nm[64]; double la, lo; if (city_get(i, nm, sizeof(nm), &la, &lo, NULL, 0) && !wx_fetch(la, lo)) bad++; }
      if (bad) say("No s'han pogut actualitzar totes les ciutats. Comprova el Wi-Fi.");
      draw(1); break;
    }
  }
}
static void mod_open(View *v) { v->page = 0; v->sel = TAB_NOW; editing = 0; pendingCity = -1; }
static Module M = { ESC_API_MIN, "temps", "El temps", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 1, WG };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) {
  H = h;
  if (h->api >= 6 && h->bg_register) h->bg_register("temps", "El temps", 60, wx_bg);
  return &M;
}
