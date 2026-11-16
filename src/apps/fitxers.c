/* Escriptori — paquet «Fitxers». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_FILES, V_FILE };
enum { A_FOPEN = 100, A_FACT, A_FMOVEHERE, A_FMOVECANCEL };
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
static void file_delete(void) { if (remove(fsel) != 0) say("No s'ha pogut esborrar el fitxer."); POP(); }
static void files_up(void) { char *p = strrchr(fpath, '/'); if (p && strlen(fpath) > strlen(FLASHDIR)) *p = 0; }
static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      FillArea(x + s / 8, y + s / 4, s / 3, s / 8, BLACK); FillArea(x + s / 8, y + s / 4 + s / 10, s * 3 / 4, s / 2, BLACK);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_FILES: v_files(v); break;
    case V_FILE: v_file(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_FOPEN:
      if (arg < 0 || arg >= nfents) break;
      if (fents[arg].dir) { if (strlen(fpath) + strlen(fents[arg].name) + 2 < sizeof(fpath)) { strcat(fpath, "/"); strcat(fpath, fents[arg].name); v->page = 0; } draw(1); }
      else { snprintf(fsel, sizeof(fsel), "%s/%s", fpath, fents[arg].name); push(V_FILE, 0, 0, 0, NULL); }
      break;
    case A_FACT:
      if (arg == 0) { if (!OpenBook(fsel, "", 0)) { say("No s'ha pogut obrir aquest fitxer."); draw(0); } }
      else if (arg == 1) confirm("Esborra el fitxer", "S'esborrarà definitivament. Vols continuar?", "Esborra", file_delete);
      else { snprintf(moveSrc, sizeof(moveSrc), "%s", fsel); POP(); draw(1); }
      break;
    case A_FMOVEHERE: {
      char dest[1000]; struct stat st2; const char *nm = strrchr(moveSrc, '/');
      snprintf(dest, sizeof(dest), "%s/%s", fpath, nm ? nm + 1 : moveSrc);
      if (!strcmp(dest, moveSrc)) say("El fitxer ja és en aquesta carpeta.");
      else if (stat(dest, &st2) == 0) say("Ja hi ha un fitxer amb aquest nom en aquesta carpeta.");
      else if (rename(moveSrc, dest) != 0) say("No s'ha pogut moure el fitxer.");
      moveSrc[0] = 0; draw(1);
    } break;
    case A_FMOVECANCEL: moveSrc[0] = 0; draw(0); break;
  }
}
static void mod_open(View *v) {
  (void)v;
  snprintf(fpath, sizeof(fpath), "%s", FLASHDIR);
}
static int mod_back(View *v) { if (v->type == V_FILES && strlen(fpath) > strlen(FLASHDIR)) { files_up(); v->page = 0; return 1; } return 0; }
static Module M = { ESC_API, "fitxers", "Fitxers", 0, mod_icon, mod_draw, mod_action, mod_back, mod_open };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
