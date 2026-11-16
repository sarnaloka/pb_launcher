/* Escriptori — suite d'eines. S'inclou des de main.c (comparteix les seves funcions de dibuix). */
#include <dirent.h>
#include <strings.h>

#define CFG FLASHDIR "/system/config/"
#define MAXL 300

/* ───────────── utilitats comunes ───────────── */
static char gk[NOTELEN]; static void (*askFn)(const char *);
static void on_ask(char *text) {
  if (text) {
    char *s = text, *p; size_t n;
    while (*s == ' ') s++;
    for (p = s; *p; p++) if (*p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
    n = strlen(s); while (n > 0 && s[n - 1] == ' ') s[--n] = 0;
    if (n > 0 && askFn) askFn(s);
  }
  draw(1);
}
static void ask(const char *title, const char *initial, void (*fn)(const char *)) {
  askFn = fn; snprintf(gk, sizeof(gk), "%s", initial ? initial : "");
  OpenKeyboard(title, gk, NOTELEN - 20, KBD_NORMAL, on_ask);
}
static void (*confFn)(void);
static void on_conf(int b) { if (b == 1 && confFn) confFn(); draw(1); }
static void confirm(const char *title, const char *text, const char *yes, void (*fn)(void)) { confFn = fn; Dialog(ICON_QUESTION, title, text, yes, "Cancel·la", on_conf); }

typedef struct { char **it; int n, loaded; const char *path; } Store;
static void st_load(Store *s) {
  FILE *f; char ln[4096];
  if (s->loaded) return;
  s->loaded = 1; s->n = 0;
  if (!s->it) s->it = (char **)calloc(MAXL, sizeof(char *));
  if (!s->it) return;
  f = fopen(s->path, "r");
  if (!f) return;
  while (s->n < MAXL && fgets(ln, sizeof(ln), f)) {
    char *nl = strpbrk(ln, "\r\n");
    if (nl) *nl = 0;
    if (ln[0] && (s->it[s->n] = strdup(ln))) s->n++;
  }
  fclose(f);
}
static void st_save(Store *s) {
  FILE *f = fopen(s->path, "w"); int i;
  if (!f) return;
  for (i = 0; i < s->n; i++) fprintf(f, "%s\n", s->it[i]);
  fclose(f);
}
static void st_insert(Store *s, int pos, const char *t) {
  char *c;
  if (!s->it || s->n >= MAXL || !(c = strdup(t))) return;
  if (pos < 0) pos = 0;
  if (pos > s->n) pos = s->n;
  memmove(&s->it[pos + 1], &s->it[pos], sizeof(char *) * (size_t)(s->n - pos));
  s->it[pos] = c; s->n++;
}
static void st_del(Store *s, int i) {
  if (i < 0 || i >= s->n) return;
  free(s->it[i]);
  memmove(&s->it[i], &s->it[i + 1], sizeof(char *) * (size_t)(s->n - i - 1));
  s->n--;
}
static void st_clear(Store *s) { while (s->n > 0) st_del(s, s->n - 1); }
static void fmt_num(char *out, int sz, double v) {
  char *p;
  double a = v < 0 ? -v : v;
  if (a >= 100000 && a < 1e15) snprintf(out, (size_t)sz, (a - floor(a) < 0.005) ? "%.0f" : "%.2f", v);
  else snprintf(out, (size_t)sz, "%.6g", v);
  for (p = out; *p; p++) if (*p == '.') *p = ',';
}
static double parse_num(const char *s) { char t[64]; int i; for (i = 0; s[i] && i < 63; i++) t[i] = s[i] == ',' ? '.' : s[i]; t[i] = 0; return atof(t); }
static int days_in_month(int y, int m) {
  static const int MD[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  return (m == 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) ? 29 : MD[m];
}
static int weekday_of(int y, int m, int d) { struct tm tm; memset(&tm, 0, sizeof(tm)); tm.tm_year = y - 1900; tm.tm_mon = m; tm.tm_mday = d; tm.tm_hour = 12; tm.tm_isdst = -1; mktime(&tm); return tm.tm_wday; }

/* ═════════════ Fitxers ═════════════ */
typedef struct { char name[256]; int dir; long size; } FEnt;
static FEnt *fents; static int nfents;
static char fpath[700] = FLASHDIR, fsel[1000], moveSrc[1000];
static int cmp_fent(const void *a, const void *b) {
  const FEnt *x = (const FEnt *)a, *y = (const FEnt *)b;
  if (x->dir != y->dir) return y->dir - x->dir;
  return strcasecmp(x->name, y->name);
}
static void files_read(void) {
  DIR *d = opendir(fpath); struct dirent *e; struct stat st; char full[1000];
  nfents = 0;
  if (!fents) fents = (FEnt *)malloc(sizeof(FEnt) * 600);
  if (!d || !fents) { if (d) closedir(d); return; }
  while (nfents < 600 && (e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    snprintf(full, sizeof(full), "%s/%s", fpath, e->d_name);
    if (stat(full, &st) != 0) continue;
    snprintf(fents[nfents].name, sizeof(fents[nfents].name), "%s", e->d_name);
    fents[nfents].dir = S_ISDIR(st.st_mode) ? 1 : 0; fents[nfents].size = (long)st.st_size; nfents++;
  }
  closedir(d);
  qsort(fents, (size_t)nfents, sizeof(FEnt), cmp_fent);
}
static void fmt_size(char *out, int sz, long b) {
  if (b < 1024) snprintf(out, (size_t)sz, "%ld B", b);
  else if (b < 1024 * 1024) snprintf(out, (size_t)sz, "%ld kB", b / 1024);
  else snprintf(out, (size_t)sz, "%ld,%ld MB", b / (1024 * 1024), (b % (1024 * 1024)) * 10 / (1024 * 1024));
}
static void v_files(View *v) {
  int y = TOP, rh = 2 * U + U / 4, per, i, bh = 2 * U; char sub[64], sz[32];
  const char *rel = fpath + strlen(FLASHDIR);
  files_read();
  snprintf(sub, sizeof(sub), nfents == 1 ? "%d element" : "%d elements", nfents);
  header(rel[0] ? rel : "Fitxers", sub);
  if (moveSrc[0]) {
    int bw = (SW - U - U / 3) / 2;
    button(U / 2, y, bw, bh, "Mou aquí", 1, A_FMOVEHERE, 0); button(U / 2 + bw + U / 3, y, bw, bh, "Cancel·la", 0, A_FMOVECANCEL, 0);
    y += bh + U / 3;
  }
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = nfents ? (nfents + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!nfents) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "Carpeta buida.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < nfents && i < (v->page + 1) * per; i++) {
    int rw = 5 * U;
    box(U / 2, y, SW - U, rh - U / 4);
    if (fents[i].dir) { FillArea(U, y + rh / 3 - U / 8, U * 2 / 3, U / 4, BLACK); FillArea(U, y + rh / 3, U + U / 3, rh / 3, BLACK); }
    SetFont(fents[i].dir ? fBold : fText, BLACK);
    DrawTextRect(fents[i].dir ? 3 * U : U, y, SW - (fents[i].dir ? 4 : 2) * U - rw, rh - U / 4, fents[i].name, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    if (!fents[i].dir) { fmt_size(sz, sizeof(sz), fents[i].size); SetFont(fSmall, BLACK); DrawTextRect(SW - U - rw, y, rw, rh - U / 4, sz, ALIGN_RIGHT | VALIGN_MIDDLE); }
    add_hit(U / 2, y, SW - U, rh - U / 4, A_FOPEN, i);
    y += rh;
  }
}
static void v_file(View *v) {
  struct stat st; char sub[100], sz[32], d[32]; const char *name = strrchr(fsel, '/'); int y = TOP + U, bh = 2 * U + U / 2;
  (void)v;
  name = name ? name + 1 : fsel;
  if (stat(fsel, &st) != 0) { header(name, "Aquest fitxer ja no existeix."); return; }
  fmt_size(sz, sizeof(sz), (long)st.st_size); fmt_date(d, sizeof(d), st.st_mtime);
  snprintf(sub, sizeof(sub), "%s · modificat el %s", sz, d);
  header(name, sub);
  button(U / 2, y, SW - U, bh, "Obre", 0, A_FACT, 0); y += bh + U / 3;
  button(U / 2, y, SW - U, bh, "Mou a una altra carpeta", 0, A_FACT, 2); y += bh + U / 3;
  button(U / 2, y, SW - U, bh, "Esborra", 0, A_FACT, 1);
}
static void file_delete(void) { if (remove(fsel) != 0) say("No s'ha pogut esborrar el fitxer."); if (depth > 1) depth--; }
static void files_up(void) { char *p = strrchr(fpath, '/'); if (p && strlen(fpath) > strlen(FLASHDIR)) *p = 0; }

/* ═════════════ Tasques ═════════════ */
static Store todo = { 0, 0, 0, CFG "escriptori_tasques.txt" };
static void todo_add(const char *t) { char ln[NOTELEN + 8]; snprintf(ln, sizeof(ln), "0\t%s", t); st_insert(&todo, 0, ln); st_save(&todo); }
static void todo_clear(void) { int i; for (i = todo.n - 1; i >= 0; i--) if (todo.it[i][0] == '1') st_del(&todo, i); st_save(&todo); }
static void v_todo(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U / 4, per, i, done = 0, bw = (SW - U - U / 3) / 2; char sub[64];
  st_load(&todo);
  for (i = 0; i < todo.n; i++) if (todo.it[i][0] == '1') done++;
  snprintf(sub, sizeof(sub), "%d pendents · %d fetes", todo.n - done, done);
  header("Tasques", sub);
  button(U / 2, y, bw, bh, "Nova tasca", 0, A_TODO_NEW, 0);
  if (done) button(U / 2 + bw + U / 3, y, bw, bh, "Esborra les fetes", 0, A_TODO_CLEAR, 0);
  y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = todo.n ? (todo.n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!todo.n) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "No hi ha cap tasca.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < todo.n && i < (v->page + 1) * per; i++) {
    int isDone = todo.it[i][0] == '1', cs = U + U / 4, cy = y + (rh - U / 4 - cs) / 2; const char *txt = strchr(todo.it[i], '\t');
    txt = txt ? txt + 1 : todo.it[i];
    box(U, cy, cs, cs);
    if (isDone) { thick(U + cs / 5, cy + cs / 2, U + cs * 2 / 5, cy + cs * 3 / 4, 4, BLACK); thick(U + cs * 2 / 5, cy + cs * 3 / 4, U + cs * 4 / 5, cy + cs / 5, 4, BLACK); }
    SetFont(fText, isDone ? DGRAY : BLACK);
    DrawTextRect(U + cs + U / 2, y, SW - 2 * U - cs - U / 2, rh - U / 4, txt, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LGRAY);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_TODO_TOGGLE, i);
    y += rh;
  }
}

/* ═════════════ Conversor ═════════════ */
typedef struct { const char *name; double f; } Unit;
static const Unit UL[] = { { "km", 1000 }, { "m", 1 }, { "cm", 0.01 }, { "mm", 0.001 }, { "milles", 1609.344 }, { "peus", 0.3048 }, { "polzades", 0.0254 } };
static const Unit UW[] = { { "kg", 1 }, { "g", 0.001 }, { "lliures", 0.45359237 }, { "unces", 0.028349523125 } };
static const Unit UV[] = { { "l", 1 }, { "ml", 0.001 }, { "galons EUA", 3.785411784 }, { "unces líq.", 0.0295735295625 } };
static const Unit UT[] = { { "°C", 0 }, { "°F", 0 }, { "K", 0 } };
static const struct { const char *name; const Unit *u; int n; } CATS[4] = { { "Longitud", UL, 7 }, { "Pes", UW, 4 }, { "Volum", UV, 4 }, { "Temp.", UT, 3 } };
static int cvCat, cvFrom; static double cvVal = 1;
static double conv(int cat, int from, int to, double v) {
  if (cat == 3) {
    double c = from == 0 ? v : from == 1 ? (v - 32) * 5 / 9 : v - 273.15;
    return to == 0 ? c : to == 1 ? c * 9 / 5 + 32 : c + 273.15;
  }
  return v * CATS[cat].u[from].f / CATS[cat].u[to].f;
}
static void conv_set(const char *t) { cvVal = parse_num(t); }
static void v_conv(View *v) {
  int y = TOP, gap = U / 4, bw = (SW - U - 3 * gap) / 4, bh = 2 * U, i, x, rh = 2 * U + U / 4; char num[48], line[120];
  (void)v;
  header("Conversor", "Toca el valor per canviar-lo");
  for (i = 0; i < 4; i++) button(U / 2 + i * (bw + gap), y, bw, bh, CATS[i].name, i == cvCat, A_CONV_CAT, i);
  y += bh + U / 2;
  x = U / 2; SetFont(fBold, BLACK);
  for (i = 0; i < CATS[cvCat].n; i++) {
    int w = StringWidth(CATS[cvCat].u[i].name) + U;
    if (x + w > SW - U / 2) { x = U / 2; y += bh + gap; }
    button(x, y, w, bh, CATS[cvCat].u[i].name, i == cvFrom, A_CONV_UNIT, i);
    SetFont(fBold, BLACK);
    x += w + gap;
  }
  y += bh + U / 2;
  fmt_num(num, sizeof(num), cvVal);
  snprintf(line, sizeof(line), "%s %s", num, CATS[cvCat].u[cvFrom].name);
  box(U / 2, y, SW - U, 4 * U);
  SetFont(fHuge, BLACK); DrawTextRect(U, y, SW - 2 * U, 4 * U, line, ALIGN_RIGHT | VALIGN_MIDDLE | DOTS);
  add_hit(U / 2, y, SW - U, 4 * U, A_CONV_VAL, 0);
  y += 4 * U + U / 2;
  for (i = 0; i < CATS[cvCat].n; i++) {
    if (i == cvFrom) continue;
    fmt_num(num, sizeof(num), conv(cvCat, cvFrom, i, cvVal));
    SetFont(fBig, BLACK); DrawTextRect(U / 2, y, SW - U - 7 * U, rh, num, ALIGN_RIGHT | VALIGN_MIDDLE | DOTS);
    SetFont(fText, BLACK); DrawTextRect(SW - U / 2 - 6 * U - U / 2, y, 6 * U + U / 2, rh, CATS[cvCat].u[i].name, ALIGN_LEFT | VALIGN_MIDDLE);
    DrawLine(U / 2, y + rh - 1, SW - U / 2, y + rh - 1, LGRAY);
    y += rh;
  }
}

/* ═════════════ Agenda ═════════════ */
static Store agenda = { 0, 0, 0, CFG "escriptori_agenda.txt" };   /* "AAAAMMDD\ttext", ordenat per data */
static int agDay;
static int ag_count(int date) { int i, n = 0; for (i = 0; i < agenda.n; i++) if (atoi(agenda.it[i]) == date) n++; return n; }
static void ag_add(const char *t) {
  char ln[NOTELEN + 16]; int i;
  snprintf(ln, sizeof(ln), "%08d\t%s", agDay, t);
  for (i = 0; i < agenda.n && atoi(agenda.it[i]) <= agDay; i++) {}
  st_insert(&agenda, i, ln); st_save(&agenda);
}
static int agDel = -1;
static void ag_del(void) { st_del(&agenda, agDel); st_save(&agenda); agDel = -1; }
static void v_agenda(View *v) {
  time_t now = time(NULL); struct tm tm = *localtime(&now);
  int y = TOP, yy = v->a / 12, mm = v->a % 12, today = (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday, cw = (SW - U) / 7, ch = 2 * U, i, start, dim, bw = (SW - U - U / 3) / 2, bh = 2 * U, rows;
  static const char *WD[7] = { "dl", "dt", "dc", "dj", "dv", "ds", "dg" };
  char t[200];
  st_load(&agenda);
  snprintf(t, sizeof(t), "%s de %d", MONTHN[mm], yy);
  header("Agenda", t);
  button(U / 2, y, bw, bh, "< Mes anterior", 0, A_AG_MONTH, -1); button(U / 2 + bw + U / 3, y, bw, bh, "Mes següent >", 0, A_AG_MONTH, 1);
  y += bh + U / 2;
  SetFont(fSmall, BLACK);
  for (i = 0; i < 7; i++) DrawTextRect(U / 2 + i * cw, y, cw, U + U / 4, WD[i], ALIGN_CENTER | VALIGN_MIDDLE);
  y += U + U / 4; FillArea(U / 2, y, 7 * cw, 2, BLACK); y += U / 4;
  start = (weekday_of(yy, mm, 1) + 6) % 7; dim = days_in_month(yy, mm);
  for (i = 1; i <= dim; i++) {
    int pos = start + i - 1, x = U / 2 + (pos % 7) * cw, cy = y + (pos / 7) * ch, date = yy * 10000 + (mm + 1) * 100 + i, has = ag_count(date);
    snprintf(t, sizeof(t), "%d", i);
    if (date == today) { FillArea(x + cw / 8, cy + ch / 8, cw - cw / 4, ch - ch / 4, BLACK); SetFont(fBold, WHITE); } else SetFont(fText, BLACK);
    DrawTextRect(x, cy, cw, ch, t, ALIGN_CENTER | VALIGN_MIDDLE);
    if (has) FillArea(x + cw / 2 - U / 6, cy + ch - ch / 5, U / 3, U / 6, date == today ? WHITE : BLACK);
    add_hit(x, cy, cw, ch, A_AG_DAY, date);
  }
  rows = (start + dim + 6) / 7;
  y += rows * ch + U / 2;
  SetFont(fBold, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, "Propers esdeveniments", ALIGN_LEFT | VALIGN_MIDDLE); y += U + U * 3 / 4;
  {
    int shown = 0;
    for (i = 0; i < agenda.n && y + U + U / 2 <= BOTTOM + NAVH; i++) {
      int date = atoi(agenda.it[i]); const char *txt = strchr(agenda.it[i], '\t');
      if (date < today) continue;
      snprintf(t, sizeof(t), "%02d/%02d   %s", date % 100, date / 100 % 100, txt ? txt + 1 : "");
      SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, t, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
      y += U + U * 3 / 4; shown++;
    }
    if (!shown) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, U + U / 2, "Cap. Toca un dia per afegir-ne.", ALIGN_LEFT | VALIGN_MIDDLE); }
  }
}
static void v_day(View *v) {
  int date = v->a, yy = date / 10000, mm = date / 100 % 100 - 1, dd = date % 100, y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U / 4, per, i, n = 0, shown = 0;
  char title[100], sub[40];
  st_load(&agenda);
  n = ag_count(date);
  if (mm < 0 || mm > 11) mm = 0;
  snprintf(title, sizeof(title), "%s, %d %s", WDAY[weekday_of(yy, mm, dd)], dd, MONTH[mm]);
  snprintf(sub, sizeof(sub), n == 1 ? "%d esdeveniment" : "%d esdeveniments", n);
  header(title, sub);
  button(U / 2, y, SW - U, bh, "Nou esdeveniment", 0, A_AG_NEW, date); y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = n ? (n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (i = 0; i < agenda.n; i++) {
    const char *txt;
    if (atoi(agenda.it[i]) != date) continue;
    if (shown >= v->page * per && shown < (v->page + 1) * per) {
      txt = strchr(agenda.it[i], '\t');
      box(U / 2, y, SW - U, rh - U / 4);
      SetFont(fText, BLACK); DrawTextRect(U, y, SW - 2 * U, rh - U / 4, txt ? txt + 1 : "", ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
      add_hit(U / 2, y, SW - U, rh - U / 4, A_AG_DEL, i);
      y += rh;
    }
    shown++;
  }
  if (n) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y + U / 4, SW - U, U + U / 4, "Toca un esdeveniment per esborrar-lo.", ALIGN_LEFT | VALIGN_MIDDLE); }
}

/* ═════════════ Sudoku ═════════════ */
#define SUFILE CFG "escriptori_sudoku.txt"
static char suG[81], suC[81], suSol[81]; static int suSel = -1, suReady;
static int su_cands(const char *g, int i) {
  int r = i / 9, c = i % 9, m = 0, k, br = r / 3 * 3, bc = c / 3 * 3;
  for (k = 0; k < 9; k++) { m |= 1 << g[r * 9 + k]; m |= 1 << g[k * 9 + c]; m |= 1 << g[(br + k / 3) * 9 + bc + k % 3]; }
  return ~m & 0x3FE;
}
static int su_solve(char *g, int limit, char *sol) {      /* compta solucions fins a `limit` */
  int best = -1, bestN = 10, i, m, d, count = 0;
  for (i = 0; i < 81; i++) if (!g[i]) {
    int n = 0; m = su_cands(g, i);
    for (d = 1; d <= 9; d++) if (m & (1 << d)) n++;
    if (n < bestN) { bestN = n; best = i; if (n <= 1) break; }
  }
  if (best < 0) { if (sol) memcpy(sol, g, 81); return 1; }
  m = su_cands(g, best);
  for (d = 1; d <= 9 && count < limit; d++) if (m & (1 << d)) { g[best] = (char)d; count += su_solve(g, limit - count, sol); g[best] = 0; }
  return count;
}
static void su_shuffle(int *a, int n) { int i; for (i = n - 1; i > 0; i--) { int j = rand() % (i + 1), t = a[i]; a[i] = a[j]; a[j] = t; } }
static void su_save(void) {
  FILE *f = fopen(SUFILE, "w"); int i;
  if (!f) return;
  for (i = 0; i < 81; i++) fputc('0' + suG[i], f);
  fputc('\n', f);
  for (i = 0; i < 81; i++) fputc('0' + suC[i], f);
  fputc('\n', f); fclose(f);
}
static void su_new(void) {
  int rows[9], cols[9], dig[9], bands[3] = { 0, 1, 2 }, stacks[3] = { 0, 1, 2 }, order[81], i, r, c, removed = 0;
  char g[81], tmp[81];
  for (i = 0; i < 9; i++) dig[i] = i + 1;
  su_shuffle(dig, 9); su_shuffle(bands, 3); su_shuffle(stacks, 3);
  for (i = 0; i < 3; i++) { int a[3] = { 0, 1, 2 }, b[3] = { 0, 1, 2 }; su_shuffle(a, 3); su_shuffle(b, 3); for (r = 0; r < 3; r++) { rows[i * 3 + r] = bands[i] * 3 + a[r]; cols[i * 3 + r] = stacks[i] * 3 + b[r]; } }
  for (r = 0; r < 9; r++) for (c = 0; c < 9; c++) g[r * 9 + c] = (char)dig[(rows[r] * 3 + rows[r] / 3 + cols[c]) % 9];
  memcpy(suSol, g, 81);
  for (i = 0; i < 81; i++) order[i] = i;
  su_shuffle(order, 81);
  for (i = 0; i < 81 && removed < 46; i++) {
    char keep = g[order[i]];
    g[order[i]] = 0; memcpy(tmp, g, 81);
    if (su_solve(tmp, 2, NULL) != 1) g[order[i]] = keep; else removed++;
  }
  memcpy(suG, g, 81); memcpy(suC, g, 81); suSel = -1; suReady = 1; su_save();
}
static void su_load(void) {
  FILE *f; char a[100], b[100], tmp[81]; int i, ok = 0;
  if (suReady) return;
  f = fopen(SUFILE, "r");
  if (f) {
    if (fgets(a, sizeof(a), f) && fgets(b, sizeof(b), f) && strlen(a) >= 81 && strlen(b) >= 81) {
      ok = 1;
      for (i = 0; i < 81; i++) { if (a[i] < '0' || a[i] > '9' || b[i] < '0' || b[i] > '9') ok = 0; suG[i] = (char)(a[i] - '0'); suC[i] = (char)(b[i] - '0'); }
      if (ok) { memcpy(tmp, suG, 81); if (su_solve(tmp, 1, suSol) != 1) ok = 0; }
    }
    fclose(f);
  }
  if (ok) suReady = 1; else su_new();
}
static void v_sudoku(View *v) {
  int cs = (SW - U) / 9, x0, y0 = TOP, i, left = 0, y, bw, bh = 2 * U + U / 4, gap = U / 6; char t[4], sub[64];
  (void)v;
  if (cs > (SH - U / 2 - 2 * bh - U - TOP) / 9) cs = (SH - U / 2 - 2 * bh - U - TOP) / 9;
  x0 = (SW - 9 * cs) / 2;
  su_load();
  for (i = 0; i < 81; i++) if (!suC[i]) left++;
  if (left) snprintf(sub, sizeof(sub), "Falten %d caselles", left); else snprintf(sub, sizeof(sub), memcmp(suC, suSol, 81) ? "Ple, però hi ha errors" : "Resolt!");
  header("Sudoku", sub);
  for (i = 0; i < 81; i++) {
    int x = x0 + (i % 9) * cs, yy = y0 + (i / 9) * cs;
    if (i == suSel) FillArea(x, yy, cs, cs, BLACK);
    if (suC[i]) { t[0] = (char)('0' + suC[i]); t[1] = 0; SetFont(suG[i] ? fBig : fText, i == suSel ? WHITE : BLACK); DrawTextRect(x, yy, cs, cs, t, ALIGN_CENTER | VALIGN_MIDDLE); }
    if (!suG[i]) add_hit(x, yy, cs, cs, A_SU_CELL, i);
  }
  for (i = 0; i <= 9; i++) {
    int t2 = i % 3 == 0 ? 5 : 1;
    FillArea(x0 + i * cs - t2 / 2, y0, t2, 9 * cs + 1, BLACK); FillArea(x0, y0 + i * cs - t2 / 2, 9 * cs + 1, t2, BLACK);
  }
  y = y0 + 9 * cs + U / 2; bw = (SW - U - 9 * gap) / 10;
  for (i = 1; i <= 9; i++) { t[0] = (char)('0' + i); t[1] = 0; button(U / 2 + (i - 1) * (bw + gap), y, bw, bh, t, 0, A_SU_NUM, i); }
  button(U / 2 + 9 * (bw + gap), y, bw, bh, "X", 0, A_SU_NUM, 0);
  y += bh + U / 3; bw = (SW - U - U / 3) / 2;
  button(U / 2, y, bw, bh, "Comprova", 0, A_SU_CHECK, 0); button(U / 2 + bw + U / 3, y, bw, bh, "Sudoku nou", 0, A_SU_NEW, 0);
}

/* ═════════════ Buscamines ═════════════ */
#define MN 9
#define MINES 10
static char miMine[MN * MN], miSt[MN * MN]; static int miState, miStarted, miFlagMode, miInit;
static void mi_new(void) { memset(miMine, 0, sizeof(miMine)); memset(miSt, 0, sizeof(miSt)); miState = 0; miStarted = 0; miFlagMode = 0; miInit = 1; }
static int mi_adj(int i) {
  int r = i / MN, c = i % MN, dr, dc, n = 0;
  for (dr = -1; dr <= 1; dr++) for (dc = -1; dc <= 1; dc++) { int rr = r + dr, cc = c + dc; if ((dr || dc) && rr >= 0 && rr < MN && cc >= 0 && cc < MN && miMine[rr * MN + cc]) n++; }
  return n;
}
static void mi_reveal(int start) {
  int stack2[MN * MN * 9], sp = 0, k;
  if (!miStarted) {
    int placed = 0;
    while (placed < MINES) { int p = rand() % (MN * MN), dr = p / MN - start / MN, dc = p % MN - start % MN; if (miMine[p] || (dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1)) continue; miMine[p] = 1; placed++; }
    miStarted = 1;
  }
  if (miSt[start] != 0) return;
  if (miMine[start]) { miState = 1; for (k = 0; k < MN * MN; k++) if (miMine[k]) miSt[k] = 1; return; }
  stack2[sp++] = start;
  while (sp > 0) {
    int i = stack2[--sp], r = i / MN, c = i % MN, dr, dc;
    if (miSt[i] != 0) continue;
    miSt[i] = 1;
    if (mi_adj(i)) continue;
    for (dr = -1; dr <= 1; dr++) for (dc = -1; dc <= 1; dc++) { int rr = r + dr, cc = c + dc; if ((dr || dc) && rr >= 0 && rr < MN && cc >= 0 && cc < MN && miSt[rr * MN + cc] == 0 && sp < MN * MN * 9) stack2[sp++] = rr * MN + cc; }
  }
  for (k = 0, sp = 0; k < MN * MN; k++) if (miSt[k] == 1) sp++;
  if (sp == MN * MN - MINES) miState = 2;
}
static void v_mines(View *v) {
  int cs = (SW - U) / MN, x0, y0 = TOP, i, flags = 0, y, bw = (SW - U - U / 3) / 2, bh = 2 * U + U / 4; char sub[80], t[4];
  (void)v;
  if (cs > (SH - U - bh - TOP) / MN) cs = (SH - U - bh - TOP) / MN;
  x0 = (SW - MN * cs) / 2;
  if (!miInit) mi_new();
  for (i = 0; i < MN * MN; i++) if (miSt[i] == 2) flags++;
  if (miState == 1) snprintf(sub, sizeof(sub), "Has trepitjat una mina"); else if (miState == 2) snprintf(sub, sizeof(sub), "Has guanyat!");
  else snprintf(sub, sizeof(sub), "%d mines · %d banderes", MINES, flags);
  header("Buscamines", sub);
  for (i = 0; i < MN * MN; i++) {
    int x = x0 + (i % MN) * cs, yy = y0 + (i / MN) * cs;
    if (miSt[i] == 1) {
      if (miMine[i]) disc(x + cs / 2, yy + cs / 2, cs / 4, BLACK);
      else { int n = mi_adj(i); if (n) { t[0] = (char)('0' + n); t[1] = 0; SetFont(fBig, BLACK); DrawTextRect(x, yy, cs, cs, t, ALIGN_CENTER | VALIGN_MIDDLE); } }
    } else {
      FillArea(x + 3, yy + 3, cs - 6, cs - 6, LGRAY);
      if (miSt[i] == 2) { FillArea(x + cs / 2 - 2, yy + cs / 4, 4, cs / 2, BLACK); FillArea(x + cs / 2, yy + cs / 4, cs / 4, cs / 5, BLACK); FillArea(x + cs / 3, yy + cs * 3 / 4 - 3, cs / 3, 4, BLACK); }
      if (miState == 0) add_hit(x, yy, cs, cs, A_MI_CELL, i);
    }
    DrawRect(x, yy, cs + 1, cs + 1, BLACK);
  }
  y = y0 + MN * cs + U / 2;
  button(U / 2, y, bw, bh, miFlagMode ? "Mode: bandera" : "Mode: destapar", miFlagMode, A_MI_MODE, 0);
  button(U / 2 + bw + U / 3, y, bw, bh, "Joc nou", 0, A_MI_NEW, 0);
}

/* ═════════════ Xarxa ═════════════ */
static char *net_get(const char *url, int *outLen) {
  int size = 0; void *buf; char *copy;
  if (!(QueryNetwork() & NET_CONNECTED)) NetConnect(NULL);
  if (!(QueryNetwork() & NET_CONNECTED)) return NULL;
  ShowHourglass();
  buf = QuickDownload(url, &size, 25);
  HideHourglass();
  if (!buf || size <= 0) { if (buf) free(buf); return NULL; }
  copy = (char *)malloc((size_t)size + 1);
  if (copy) { memcpy(copy, buf, (size_t)size); copy[size] = 0; }
  free(buf);
  if (outLen) *outLen = size;
  return copy;
}
static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb"); long n; char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  if (n <= 0 || n > 8 * 1024 * 1024) { fclose(f); return NULL; }
  b = (char *)malloc((size_t)n + 1);
  if (b) { n = (long)fread(b, 1, (size_t)n, f); b[n] = 0; }
  fclose(f);
  return b;
}

/* ═════════════ El temps ═════════════ */
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

/* ═════════════ Notícies (RSS) ═════════════ */
static Store feeds = { 0, 0, 0, CFG "escriptori_canals.txt" };
static Store news = { 0, 0, 0, CFG "escriptori_noticies.txt" };    /* "títol\tresum" */
static void feeds_load(void) {
  st_load(&feeds);
  if (!feeds.n && feeds.it) { st_insert(&feeds, 0, "https://www.vilaweb.cat/feed/"); st_save(&feeds); }
}
static void utf8_put(char *out, int *n, int max, unsigned c) {
  if (*n + 4 >= max) return;
  if (c < 0x80) out[(*n)++] = (char)c;
  else if (c < 0x800) { out[(*n)++] = (char)(0xC0 | (c >> 6)); out[(*n)++] = (char)(0x80 | (c & 63)); }
  else if (c < 0x10000) { out[(*n)++] = (char)(0xE0 | (c >> 12)); out[(*n)++] = (char)(0x80 | ((c >> 6) & 63)); out[(*n)++] = (char)(0x80 | (c & 63)); }
  else { out[(*n)++] = (char)(0xF0 | (c >> 18)); out[(*n)++] = (char)(0x80 | ((c >> 12) & 63)); out[(*n)++] = (char)(0x80 | ((c >> 6) & 63)); out[(*n)++] = (char)(0x80 | (c & 63)); }
}
/* text net: sense etiquetes, CDATA ni entitats; espais compactats */
static void clean_text(const char *s, const char *end, char *out, int max, int pass) {
  int n = 0, sp = 1; char *tmp = NULL;
  while (s < end && n < max - 5) {
    if (!strncmp(s, "<![CDATA[", 9)) { s += 9; continue; }
    if (!strncmp(s, "]]>", 3)) { s += 3; continue; }
    if (*s == '<') { while (s < end && *s != '>') s++; if (s < end) s++; if (!sp) { out[n++] = ' '; sp = 1; } continue; }
    if (*s == '&') {
      const char *sc = memchr(s, ';', (size_t)(end - s > 10 ? 10 : end - s)); unsigned c = 0;
      if (sc) {
        if (s[1] == '#') c = (s[2] == 'x' || s[2] == 'X') ? (unsigned)strtoul(s + 3, NULL, 16) : (unsigned)strtoul(s + 2, NULL, 10);
        else if (!strncmp(s, "&amp;", 5)) c = '&'; else if (!strncmp(s, "&lt;", 4)) c = '<'; else if (!strncmp(s, "&gt;", 4)) c = '>';
        else if (!strncmp(s, "&quot;", 6)) c = '"'; else if (!strncmp(s, "&apos;", 6)) c = '\''; else if (!strncmp(s, "&nbsp;", 6)) c = ' ';
        if (c) { if (c == ' ' || c == '\n' || c == '\t') { if (!sp) { out[n++] = ' '; sp = 1; } } else { utf8_put(out, &n, max, c); sp = 0; } s = sc + 1; continue; }
      }
    }
    if (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') { if (!sp) { out[n++] = ' '; sp = 1; } s++; continue; }
    out[n++] = *s++; sp = 0;
  }
  while (n > 0 && out[n - 1] == ' ') n--;
  out[n] = 0;
  /* el resum sovint porta HTML escapat (&lt;p&gt;): segona passada */
  if (pass == 0 && strchr(out, '<') && (tmp = strdup(out))) { clean_text(tmp, tmp + strlen(tmp), out, max, 1); free(tmp); }
}
static const char *tag_body(const char *from, const char *end, const char *tag, const char **bodyEnd) {
  char open[40], close[40]; const char *p, *q;
  snprintf(open, sizeof(open), "<%s", tag); snprintf(close, sizeof(close), "</%s>", tag);
  for (p = from; (p = strstr(p, open)) && p < end; p += strlen(open)) {
    char nx = p[strlen(open)];
    if (nx != '>' && nx != ' ' && nx != '\t' && nx != '\n') continue;
    q = strchr(p, '>');
    if (!q || q >= end) return NULL;
    if (q[-1] == '/') continue;
    *bodyEnd = strstr(q + 1, close);
    if (!*bodyEnd || *bodyEnd > end) return NULL;
    return q + 1;
  }
  return NULL;
}
static int rss_parse(const char *xml) {
  const char *p = xml, *end = xml + strlen(xml); int added = 0; static char title[400], desc[2400], ln[2900];
  const char *itag = strstr(xml, "<item") ? "item" : "entry";
  while (news.n < MAXL && added < 40) {
    const char *ie, *ib = tag_body(p, end, itag, &ie), *te, *tb, *de, *dbody;
    if (!ib) break;
    tb = tag_body(ib, ie, "title", &te);
    dbody = tag_body(ib, ie, "description", &de);
    if (!dbody) dbody = tag_body(ib, ie, "summary", &de);
    if (!dbody) dbody = tag_body(ib, ie, "content", &de);
    title[0] = desc[0] = 0;
    if (tb) clean_text(tb, te, title, sizeof(title), 0);
    if (dbody) clean_text(dbody, de, desc, sizeof(desc), 0);
    if (title[0]) { snprintf(ln, sizeof(ln), "%s\t%s", title, desc); st_insert(&news, news.n, ln); added++; }
    p = ie + 1;
  }
  return added;
}
static void rss_refresh(void) {
  int i, total = 0, okFeeds = 0;
  feeds_load(); st_load(&news);
  for (i = 0; i < feeds.n; i++) {
    char *b = net_get(feeds.it[i], NULL);
    if (!b) continue;
    if (!okFeeds) st_clear(&news);
    okFeeds++; total += rss_parse(b);
    free(b);
  }
  if (!okFeeds) { say("No s'ha pogut descarregar cap canal. Comprova el Wi-Fi."); return; }
  st_save(&news);
  (void)total;
}
static void v_rss(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U * 3 / 4, per, i, bw = (SW - U - U / 3) / 2; char sub[64], title[400];
  feeds_load(); st_load(&news);
  snprintf(sub, sizeof(sub), "%d titulars · %d canals", news.n, feeds.n);
  header("Notícies", sub);
  button(U / 2, y, bw, bh, "Actualitza (Wi-Fi)", 0, A_RSS_REFRESH, 0); button(U / 2 + bw + U / 3, y, bw, bh, "Canals", 0, A_RSS_FEEDS, 0);
  y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = news.n ? (news.n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!news.n) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "Encara no hi ha titulars desats.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < news.n && i < (v->page + 1) * per; i++) {
    const char *tab = strchr(news.it[i], '\t'); size_t n = tab ? (size_t)(tab - news.it[i]) : strlen(news.it[i]);
    if (n > sizeof(title) - 1) n = sizeof(title) - 1;
    memcpy(title, news.it[i], n); title[n] = 0;
    SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, rh - U / 4, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LGRAY);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_RSS_OPEN, i);
    y += rh;
  }
}
static void v_rssitem(View *v) {
  char title[400]; const char *tab; size_t n; int th;
  st_load(&news);
  if (v->a < 0 || v->a >= news.n) { header("Notícia", ""); return; }
  tab = strchr(news.it[v->a], '\t'); n = tab ? (size_t)(tab - news.it[v->a]) : strlen(news.it[v->a]);
  if (n > sizeof(title) - 1) n = sizeof(title) - 1;
  memcpy(title, news.it[v->a], n); title[n] = 0;
  header("Notícia", "");
  SetFont(fBold, BLACK); th = TextRectHeight(SW - U, title, ALIGN_LEFT | VALIGN_TOP);
  if (th < U) th = U + U / 2;
  if (th > 8 * U) th = 8 * U;
  DrawTextRect(U / 2, TOP, SW - U, th, title, ALIGN_LEFT | VALIGN_TOP);
  SetFont(fText, BLACK);
  DrawTextRect(U / 2, TOP + th + U / 2, SW - U, BOTTOM + NAVH - TOP - th - U / 2, tab && tab[1] ? tab + 1 : "(Sense resum.)", ALIGN_LEFT | VALIGN_TOP | DOTS);
}
static int feedDel = -1;
static void feed_del(void) { st_del(&feeds, feedDel); st_save(&feeds); feedDel = -1; }
static void feed_add(const char *t) { if (!strncmp(t, "http://", 7) || !strncmp(t, "https://", 8)) { st_insert(&feeds, feeds.n, t); st_save(&feeds); } else say("L'adreça ha de començar per http:// o https://"); }
static void v_feeds(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U / 4, per, i; char sub[40];
  feeds_load();
  snprintf(sub, sizeof(sub), feeds.n == 1 ? "%d canal" : "%d canals", feeds.n);
  header("Canals de notícies", sub);
  button(U / 2, y, SW - U, bh, "Afegeix un canal RSS", 0, A_FEED_ADD, 0); y += bh + U / 2;
  per = (BOTTOM - y - 2 * U) / rh; if (per < 1) per = 1;
  pages = feeds.n ? (feeds.n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < feeds.n && i < (v->page + 1) * per; i++) {
    box(U / 2, y, SW - U, rh - U / 4);
    SetFont(fSmall, BLACK); DrawTextRect(U, y, SW - 2 * U, rh - U / 4, feeds.it[i], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_FEED_DEL, i);
    y += rh;
  }
  SetFont(fSmall, BLACK); DrawTextRect(U / 2, y + U / 4, SW - U, U + U / 4, "Toca un canal per esborrar-lo.", ALIGN_LEFT | VALIGN_MIDDLE);
}

/* ═════════════ Apps instal·lades (carpeta «applications») ═════════════ */
#define APPDIR FLASHDIR "/applications"
static char extApps[40][200]; static int nExt; static char selfName[200] = "Escriptori.app";
static void ext_scan(void) {
  DIR *d = opendir(APPDIR); struct dirent *e; int i, j;
  nExt = 0;
  if (!d) return;
  while (nExt < 40 && (e = readdir(d))) {
    size_t n = strlen(e->d_name);
    if (n < 5 || n > 190 || e->d_name[0] == '.' || strcmp(e->d_name + n - 4, ".app") || !strcmp(e->d_name, selfName) || !strcmp(e->d_name, "Escriptori.app")) continue;
    snprintf(extApps[nExt++], sizeof(extApps[0]), "%s", e->d_name);
  }
  closedir(d);
  for (i = 1; i < nExt; i++) { char t[200]; memcpy(t, extApps[i], 200); for (j = i - 1; j >= 0 && strcasecmp(extApps[j], t) > 0; j--) memcpy(extApps[j + 1], extApps[j], 200); memcpy(extApps[j + 1], t, 200); }
}
static void ext_launch(int i) {
  char path[500];
  if (i < 0 || i >= nExt) return;
  snprintf(path, sizeof(path), APPDIR "/%s", extApps[i]);
  OpenBook(path, "", 0);
}

/* ═════════════ Actualització per Wi-Fi (GitHub) ═════════════ */
#define APP_VERSION 1
#define UPD_BASE "https://raw.githubusercontent.com/sarnaloka/pb_launcher/main/"
static int updState;            /* 0 sense comprovar, 1 al dia, 2 hi ha versió nova, 3 instal·lada, -1 error */
static int updVersion; static long updSize; static unsigned long updCrc; static char updNotes[300]; static char selfPath[600];
static unsigned long crc32_buf(const unsigned char *b, long n) {
  unsigned long c = 0xFFFFFFFFUL; long i; int k;
  for (i = 0; i < n; i++) { c ^= b[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320UL & (0UL - (c & 1UL))); }
  return (c ^ 0xFFFFFFFFUL) & 0xFFFFFFFFUL;
}
static char *net_get_t(const char *url, int *outLen, int timeout) {
  int size = 0; void *buf; char *copy;
  if (!(QueryNetwork() & NET_CONNECTED)) NetConnect(NULL);
  if (!(QueryNetwork() & NET_CONNECTED)) return NULL;
  ShowHourglass();
  buf = QuickDownload(url, &size, timeout);
  HideHourglass();
  if (!buf || size <= 0) { if (buf) free(buf); return NULL; }
  copy = (char *)malloc((size_t)size + 1);
  if (copy) { memcpy(copy, buf, (size_t)size); copy[size] = 0; }
  free(buf);
  if (outLen) *outLen = size;
  return copy;
}
/* version.txt: línia 1 versió, línia 2 mida en bytes, línia 3 CRC32 en hexadecimal, línia 4 novetats */
static void upd_check(void) {
  char url[300], *b, *l2, *l3, *l4, *e;
  snprintf(url, sizeof(url), UPD_BASE "version.txt?t=%ld", (long)time(NULL));
  b = net_get_t(url, NULL, 25);
  updState = -1; updNotes[0] = 0;
  if (!b) { say("No s'ha pogut consultar la versió. Comprova el Wi-Fi."); return; }
  updVersion = atoi(b);
  l2 = strchr(b, '\n'); l3 = l2 ? strchr(l2 + 1, '\n') : NULL; l4 = l3 ? strchr(l3 + 1, '\n') : NULL;
  if (updVersion <= 0 || !l2 || !l3) { free(b); say("La informació de versió del servidor no és vàlida."); return; }
  updSize = atol(l2 + 1); updCrc = strtoul(l3 + 1, NULL, 16);
  if (l4) { snprintf(updNotes, sizeof(updNotes), "%s", l4 + 1); if ((e = strpbrk(updNotes, "\r\n"))) *e = 0; }
  free(b);
  updState = updVersion > APP_VERSION ? 2 : 1;
}
static void upd_install(void) {
  char url[300], part[700], *b; int len = 0; FILE *f; size_t w;
  snprintf(url, sizeof(url), UPD_BASE "Escriptori.app?t=%ld", (long)time(NULL));
  b = net_get_t(url, &len, 180);
  if (!b) { say("No s'ha pogut descarregar l'actualització."); return; }
  if ((long)len != updSize || crc32_buf((const unsigned char *)b, len) != updCrc || len < 100000 || memcmp(b, "\177ELF", 4)) {
    free(b); say("La descàrrega no coincideix amb la versió publicada. No s'ha canviat res."); return;
  }
  snprintf(part, sizeof(part), "%s.part", selfPath);
  f = fopen(part, "wb");
  if (!f) { free(b); say("No s'ha pogut desar l'actualització al lector."); return; }
  w = fwrite(b, 1, (size_t)len, f);
  free(b);
  if (fclose(f) != 0 || w != (size_t)len) { remove(part); say("No s'ha pogut desar l'actualització al lector."); return; }
  chmod(part, 0755);
  if (rename(part, selfPath) != 0) {           /* alguns sistemes de fitxers no deixen substituir directament */
    char old[700];
    snprintf(old, sizeof(old), "%s.old", selfPath);
    remove(old);
    if (rename(selfPath, old) != 0 || rename(part, selfPath) != 0) { rename(old, selfPath); remove(part); say("No s'ha pogut substituir l'aplicació. No s'ha canviat res."); return; }
    remove(old);
  }
  updState = 3;
}
static void v_update(View *v) {
  char t[400]; int y = TOP, bh = 2 * U + U / 2;
  (void)v;
  snprintf(t, sizeof(t), "Versió instal·lada: %d", APP_VERSION);
  header("Actualització", t);
  SetFont(fText, BLACK);
  if (updState == 0) snprintf(t, sizeof(t), "Connecta el Wi-Fi i comprova si hi ha una versió nova.");
  else if (updState == 1) snprintf(t, sizeof(t), "Ja tens l'última versió (%d).", updVersion);
  else if (updState == 2) snprintf(t, sizeof(t), "Hi ha la versió %d disponible.%s%s", updVersion, updNotes[0] ? "\n\nNovetats: " : "", updNotes);
  else if (updState == 3) snprintf(t, sizeof(t), "S'ha instal·lat la versió %d.\n\nTanca l'Escriptori amb la X i torna'l a obrir per fer-la servir.", updVersion);
  else snprintf(t, sizeof(t), "No s'ha pogut completar. Torna-ho a provar amb el Wi-Fi connectat.");
  DrawTextRect(U / 2, y, SW - U, 8 * U, t, ALIGN_LEFT | VALIGN_TOP); y += 9 * U;
  if (updState == 2) button(U / 2, y, SW - U, bh, "Descarrega i instal·la", 1, A_UPD_INSTALL, 0);
  else if (updState != 3) button(U / 2, y, SW - U, bh, "Comprova si hi ha una versió nova", 0, A_UPD_CHECK, 0);
  SetFont(fSmall, BLACK);
  DrawTextRect(U / 2, BOTTOM - 2 * U, SW - U, 2 * U, "Les actualitzacions es baixen de github.com/sarnaloka/pb_launcher", ALIGN_LEFT | VALIGN_MIDDLE);
}
