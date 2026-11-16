/* Escriptori — paquet «El temps». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_WEATHER };
enum { A_WX_REFRESH = 100 };
static const char *WD2[7] = { "dg", "dl", "dt", "dc", "dj", "dv", "ds" };
#define WXFILE CFG "escriptori_temps.json"
#define WXURL "https://api.open-meteo.com/v1/forecast?latitude=41.39&longitude=2.17&current=temperature_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=Europe%2FMadrid"
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
static void wx_refresh(void) {
  int len = 0; char *b = net_get(WXURL, &len); FILE *f;
  if (!b || !strstr(b, "\"daily\"")) { free(b); say("No s'ha pogut descarregar la previsió. Comprova el Wi-Fi."); return; }
  f = fopen(WXFILE, "wb");
  if (f) { fwrite(b, 1, (size_t)len, f); fclose(f); }
  free(b);
}
static void v_weather(View *v) {
  char *b = read_file(WXFILE), sub[100], t[80]; struct stat st; int y = TOP, bh = 2 * U + U / 4, i, n, rh = 2 * U + U / 4;
  const char *cur, *daily, *p; double code[8], tmax[8], tmin[8], pp[8], cv[1];
  (void)v;
  if (b && stat(WXFILE, &st) == 0) { struct tm *tm = localtime(&st.st_mtime); snprintf(sub, sizeof(sub), "Barcelona · actualitzat el %02d/%02d a les %02d:%02d", tm->tm_mday, tm->tm_mon + 1, tm->tm_hour, tm->tm_min); }
  else snprintf(sub, sizeof(sub), "Barcelona");
  header("El temps", sub);
  button(U / 2, BOTTOM - bh, SW - U, bh, "Actualitza (cal Wi-Fi)", 0, A_WX_REFRESH, 0);
  if (!b) { SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, 3 * U, "Encara no hi ha cap previsió desada.", ALIGN_LEFT | VALIGN_TOP); return; }
  cur = jkey(b, "current"); daily = jkey(b, "daily");
  if (cur) {
    double temp = -9999; int wc = -1;
    if ((p = jkey(cur, "temperature_2m"))) temp = strtod(p, NULL);
    if ((p = jkey(cur, "weather_code")) || (p = jkey(cur, "weathercode"))) wc = (int)strtod(p, NULL);
    if (temp > -999) { snprintf(t, sizeof(t), "%d°", (int)floor(temp + 0.5)); SetFont(fHuge, BLACK); DrawTextRect(U / 2, y, 8 * U, 4 * U, t, ALIGN_LEFT | VALIGN_MIDDLE); }
    SetFont(fBig, BLACK); DrawTextRect(9 * U, y, SW - 9 * U - U / 2, 4 * U, wc >= 0 ? wx_desc(wc) : "", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    y += 4 * U + U / 2;
  }
  FillArea(U / 2, y, SW - U, 2, BLACK); y += U / 2;
  n = jarr(jkey(daily, "weather_code"), code, 8); if (!n) n = jarr(jkey(daily, "weathercode"), code, 8);
  jarr(jkey(daily, "temperature_2m_max"), tmax, 8); jarr(jkey(daily, "temperature_2m_min"), tmin, 8);
  if (!jarr(jkey(daily, "precipitation_probability_max"), pp, 8)) for (i = 0; i < 8; i++) pp[i] = -9999;
  (void)cv;
  p = jkey(daily, "time");
  for (i = 0; i < n && i < 7 && y + rh <= BOTTOM - bh - U / 2; i++) {
    int yy = 0, mm = 0, dd = 0;
    if (p) { p = strchr(p, '"'); if (p) { sscanf(p + 1, "%d-%d-%d", &yy, &mm, &dd); p = strchr(p + 1, '"'); if (p) p++; } }
    if (mm >= 1 && mm <= 12) snprintf(t, sizeof(t), "%s. %d", WD2[weekday_of(yy, mm - 1, dd)], dd); else snprintf(t, sizeof(t), "Dia %d", i + 1);
    SetFont(fBold, BLACK); DrawTextRect(U / 2, y, 4 * U, rh, t, ALIGN_LEFT | VALIGN_MIDDLE);
    SetFont(fText, BLACK); DrawTextRect(5 * U, y, SW - 5 * U - 10 * U, rh, wx_desc((int)code[i]), ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    snprintf(t, sizeof(t), "%d° / %d°", (int)floor(tmin[i] + 0.5), (int)floor(tmax[i] + 0.5));
    DrawTextRect(SW - 10 * U, y, 6 * U, rh, t, ALIGN_RIGHT | VALIGN_MIDDLE);
    if (pp[i] > -999) { snprintf(t, sizeof(t), "%d%%", (int)pp[i]); SetFont(fSmall, BLACK); DrawTextRect(SW - 4 * U, y, 3 * U + U / 2, rh, t, ALIGN_RIGHT | VALIGN_MIDDLE); }
    DrawLine(U / 2, y + rh - 1, SW - U / 2, y + rh - 1, LGRAY);
    y += rh;
  }
  free(b);
}
static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      thick(x + s / 2, y + s / 8, x + s / 2, y + s * 7 / 8, s / 20 > 2 ? s / 20 : 3, BLACK); thick(x + s / 8, y + s / 2, x + s * 7 / 8, y + s / 2, s / 20 > 2 ? s / 20 : 3, BLACK);
      thick(x + s / 4, y + s / 4, x + s * 3 / 4, y + s * 3 / 4, s / 20 > 2 ? s / 20 : 3, BLACK); thick(x + s * 3 / 4, y + s / 4, x + s / 4, y + s * 3 / 4, s / 20 > 2 ? s / 20 : 3, BLACK);
      disc(x + s / 2, y + s / 2, s / 4 + s / 20, WHITE); disc(x + s / 2, y + s / 2, s / 5, BLACK);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_WEATHER: v_weather(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_WX_REFRESH: wx_refresh(); draw(1); break;
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API, "temps", "El temps", 0, mod_icon, mod_draw, mod_action, NULL, mod_open };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
