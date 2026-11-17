/* Escriptori — paquet «El temps». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_WEATHER, V_CITIES, V_RESULTS };
enum { A_WX_REFRESH = 100, A_WX_CITIES, A_WX_ADD, A_WX_EDIT, A_WX_OPEN, A_WX_DEL, A_WX_PICK, A_WX_MAKEMAIN, A_WX_ALL };
static const char *WD2[7] = { "dg", "dl", "dt", "dc", "dj", "dv", "ds" };
#define WXFILE_OLD CFG "escriptori_temps.json"                 /* versió 2: una sola ciutat (Barcelona) */
#define WXFILE_FMT CFG "escriptori_temps_%.2f_%.2f.json"       /* previsió desada per ciutat */
#define WXURL_FMT "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=auto"
#define GEOURL_FMT "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=6&language=ca&format=json"
#define MAXC 12
static const char *wx_desc(int c) {
  if (c == 0) return "Serè"; if (c == 1) return "Poc ennuvolat"; if (c == 2) return "Mig ennuvolat"; if (c == 3) return "Ennuvolat";
  if (c == 45 || c == 48) return "Boira"; if (c >= 51 && c <= 57) return "Plugim"; if (c == 61) return "Pluja feble"; if (c == 63) return "Pluja";
  if (c == 65) return "Pluja forta"; if (c == 66 || c == 67) return "Pluja gelada"; if (c >= 71 && c <= 77) return "Neu";
  if (c >= 80 && c <= 82) return "Ruixats"; if (c == 85 || c == 86) return "Ruixats de neu"; if (c == 95) return "Tempesta"; if (c == 96 || c == 99) return "Tempesta amb calamarsa";
  return "—";
}
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
static int wx_fetch(double lat, double lon) {
  char url[600], path[256]; int len = 0; char *b; FILE *f;
  snprintf(url, sizeof(url), WXURL_FMT, lat, lon);
  b = net_get(url, &len);
  if (!b || !strstr(b, "\"daily\"")) { free(b); return 0; }
  wx_path(path, sizeof(path), lat, lon);
  f = fopen(path, "wb");
  if (f) { fwrite(b, 1, (size_t)len, f); fclose(f); }
  free(b);
  return 1;
}
static void wx_current(const char *b, double *temp, int *wc) {
  const char *cur = jkey(b, "current"), *p;
  *temp = -9999; *wc = -1;
  if (!cur) return;
  if ((p = jkey(cur, "temperature_2m"))) *temp = strtod(p, NULL);
  if ((p = jkey(cur, "weather_code")) || (p = jkey(cur, "weathercode"))) *wc = (int)strtod(p, NULL);
}
/* «13° · Mig ennuvolat» a partir de la previsió desada de la ciutat i */
static void city_now(int i, char *out, int sz) {
  char name[64], path[256], *b; double lat, lon, temp; int wc;
  snprintf(out, (size_t)sz, "sense dades");
  if (!city_get(i, name, sizeof(name), &lat, &lon, NULL, 0)) return;
  wx_path(path, sizeof(path), lat, lon);
  b = read_file(path);
  if (!b) return;
  wx_current(b, &temp, &wc);
  if (temp > -999) snprintf(out, (size_t)sz, "%d° · %s", (int)floor(temp + 0.5), wc >= 0 ? wx_desc(wc) : "");
  free(b);
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
    if (jstr(jkey(obj, "name"), res[nres].name, sizeof(res[nres].name)) && (k = jkey(obj, "latitude")) ) {
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
  st_del(&cities, delIdx); st_save(&cities); delIdx = -1;
}

/* ───── vistes ───── */
static void v_weather(View *v) {
  char *b, sub[160], t[80], name[64], path[256]; struct stat st; int y = TOP, bh = 2 * U + U / 4, i, n, rh = 2 * U + U / 4, bw = (SW - U - U / 3) / 2, extra, ci;
  const char *daily, *p; double code[8], tmax[8], tmin[8], pp[8], lat = 0, lon = 0, temp; int wc;
  cities_load();
  ci = v->a; if (ci < 0 || ci >= cities.n) ci = v->a = 0;
  city_get(ci, name, sizeof(name), &lat, &lon, NULL, 0);
  wx_path(path, sizeof(path), lat, lon);
  b = read_file(path);
  if (b && stat(path, &st) == 0) { struct tm *tm = localtime(&st.st_mtime); snprintf(sub, sizeof(sub), "%s · actualitzat el %02d/%02d a les %02d:%02d", name, tm->tm_mday, tm->tm_mon + 1, tm->tm_hour, tm->tm_min); }
  else snprintf(sub, sizeof(sub), "%s", name);
  header("El temps", sub);
  extra = ci > 0 ? bh + U / 3 : 0;
  button(U / 2, BOTTOM - bh, bw, bh, "Actualitza (cal Wi-Fi)", 0, A_WX_REFRESH, 0);
  button(U / 2 + bw + U / 3, BOTTOM - bh, bw, bh, "Ciutats", 0, A_WX_CITIES, 0);
  if (ci > 0) button(U / 2, BOTTOM - bh - extra, SW - U, bh, "Fes-la la ciutat principal", 0, A_WX_MAKEMAIN, 0);
  if (!b) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 3 * U, "Encara no hi ha cap previsió desada.", ALIGN_LEFT | VALIGN_TOP); return; }
  daily = jkey(b, "daily");
  wx_current(b, &temp, &wc);
  if (jkey(b, "current")) {
    if (temp > -999) { snprintf(t, sizeof(t), "%d°", (int)floor(temp + 0.5)); SetFont(fHuge, BLACK); DrawTextRect(U / 2, y, 8 * U, 4 * U, t, ALIGN_LEFT | VALIGN_MIDDLE); }
    SetFont(fBig, BLACK); DrawTextRect(9 * U, y, SW - 9 * U - U / 2, 4 * U, wc >= 0 ? wx_desc(wc) : "", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    y += 4 * U + U / 2;
  }
  FillArea(U / 2, y, SW - U, 2, BLACK); y += U / 2;
  for (i = 0; i < 8; i++) { code[i] = tmax[i] = tmin[i] = pp[i] = -9999; }
  n = jarr(jkey(daily, "weather_code"), code, 8); if (!n) n = jarr(jkey(daily, "weathercode"), code, 8);
  jarr(jkey(daily, "temperature_2m_max"), tmax, 8); jarr(jkey(daily, "temperature_2m_min"), tmin, 8);
  jarr(jkey(daily, "precipitation_probability_max"), pp, 8);
  p = jkey(daily, "time");
  for (i = 0; i < n && i < 7 && y + rh <= BOTTOM - bh - extra - U / 2; i++) {
    int yy = 0, mm = 0, dd = 0;
    if (p) { p = strchr(p, '"'); if (p) { sscanf(p + 1, "%d-%d-%d", &yy, &mm, &dd); p = strchr(p + 1, '"'); if (p) p++; } }
    if (mm >= 1 && mm <= 12) snprintf(t, sizeof(t), "%s. %d", WD2[weekday_of(yy, mm - 1, dd)], dd); else snprintf(t, sizeof(t), "Dia %d", i + 1);
    SetFont(fBold, BLACK); DrawTextRect(U / 2, y, 4 * U, rh, t, ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(fText, BLACK); DrawTextRect(5 * U, y, SW - 5 * U - 10 * U, rh, wx_desc((int)code[i]), ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (tmin[i] > -999 && tmax[i] > -999) snprintf(t, sizeof(t), "%d° / %d°", (int)floor(tmin[i] + 0.5), (int)floor(tmax[i] + 0.5)); else snprintf(t, sizeof(t), "—");
    DrawTextRect(SW - 10 * U, y, 6 * U, rh, t, ALIGN_RIGHT | VALIGN_MIDDLE);
    if (pp[i] > -999) { snprintf(t, sizeof(t), "%d%%", (int)pp[i]); SetFont(fSmall, BLACK); DrawTextRect(SW - 4 * U, y, 3 * U + U / 2, rh, t, ALIGN_RIGHT | VALIGN_MIDDLE); }
    DrawLine(U / 2, y + rh - 1, SW - U / 2, y + rh - 1, LGRAY);
    y += rh;
  }
  free(b);
}
static void v_cities(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 3 * U, bw = (SW - U - U / 3) / 2, per, i; char sub[80];
  cities_load();
  if (editing) snprintf(sub, sizeof(sub), "Toca una ciutat per esborrar-la");
  else snprintf(sub, sizeof(sub), cities.n == 1 ? "%d ciutat" : "%d ciutats", cities.n);
  header("Ciutats", sub);
  button(U / 2, y, bw, bh, "Afegeix una ciutat", 0, A_WX_ADD, 0);
  button(U / 2 + bw + U / 3, y, bw, bh, editing ? "Fet" : "Edita", 0, A_WX_EDIT, 0);
  button(U / 2, BOTTOM - bh, SW - U, bh, "Actualitza-les totes (cal Wi-Fi)", 0, A_WX_ALL, 0);
  y += bh + U / 2;
  per = (BOTTOM - bh - U / 2 - y) / rh; if (per < 1) per = 1;
  pages = cities.n ? (cities.n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < cities.n && i < (v->page + 1) * per; i++) {
    char nm[64], rg[96], now[80]; double la, lo;
    if (!city_get(i, nm, sizeof(nm), &la, &lo, rg, sizeof(rg))) continue;
    city_now(i, now, sizeof(now));
    ui_row(U / 2, y, SW - U, rh - U / 4, nm, rg[0] ? rg : NULL, editing ? "Esborra" : now, editing ? A_WX_DEL : A_WX_OPEN, i);
    y += rh;
  }
}
static void v_results(View *v) {
  int y = TOP, rh = 3 * U, i; (void)v;
  header("Resultats", "Tria una ciutat");
  for (i = 0; i < nres; i++) { ui_row(U / 2, y, SW - U, rh - U / 4, res[i].name, res[i].region[0] ? res[i].region : NULL, NULL, A_WX_PICK, i); y += rh; }
}
/* ───── widget ───── */
static void w_today(int x, int y, int w, int h) {
  char t[80] = "sense dades", lab[100] = "El temps", name[64], path[256], *b; double lat, lon, temp; int wc;
  cities_load();
  if (city_get(0, name, sizeof(name), &lat, &lon, NULL, 0)) {
    snprintf(lab, sizeof(lab), "El temps · %s", name);
    wx_path(path, sizeof(path), lat, lon);
    b = read_file(path);
    if (b) { wx_current(b, &temp, &wc); if (temp > -999) snprintf(t, sizeof(t), "%d° · %s", (int)floor(temp + 0.5), wc >= 0 ? wx_desc(wc) : ""); free(b); }
  }
  ui_lv(x, y, w, h, lab, t);
}
static const Widget WG[] = { { "avui", "El temps d'avui", 2, 1, w_today } };
static void mod_icon(int x, int y, int s) { ic_path(IC_SUN, x, y, s, 1.8, ICON_FG, ICON_BG); }   /* icona del prototip (icons.h) */
static void mod_draw(View *v) {
  switch (v->type) {
    case V_WEATHER: v_weather(v); break;
    case V_CITIES: v_cities(v); break;
    case V_RESULTS: v_results(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  switch (a) {
    case A_WX_REFRESH: {
      char nm[64]; double la, lo;
      cities_load();
      if (city_get(v->a, nm, sizeof(nm), &la, &lo, NULL, 0) && !wx_fetch(la, lo)) say("No s'ha pogut descarregar la previsió. Comprova el Wi-Fi.");
      draw(1); break;
    }
    case A_WX_CITIES: editing = 0; push(V_CITIES, 0, 0, 0, NULL); break;
    case A_WX_ADD: ask("Nom de la ciutat", "", city_search); break;
    case A_WX_EDIT: editing = !editing; draw(0); break;
    case A_WX_OPEN: push(V_WEATHER, arg, 0, 0, NULL); break;
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
      if (v->a > 0 && v->a < cities.n) { snprintf(ln, sizeof(ln), "%s", cities.it[v->a]); st_del(&cities, v->a); st_insert(&cities, 0, ln); st_save(&cities); v->a = 0; }
      draw(1); break;
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
static void mod_open(View *v) {
  v->a = 0; editing = 0;
}
static Module M = { ESC_API_MIN, "temps", "El temps", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 1, WG };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
