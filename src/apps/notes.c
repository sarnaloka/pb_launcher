/* Escriptori — paquet «Notes». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_NOTES, V_NOTE };
enum { A_NOTE_NEW = 100, A_NOTE_OPEN, A_NOTE_EDIT, A_NOTE_DEL };
#define NOTESFILE FLASHDIR "/system/config/escriptori_notes.txt"
#define MAXNOTES 200
#define NOTELEN 400
static char (*notes)[NOTELEN]; static int nnotes, notesLoaded, noteEdit = -1;
static char nbuf[NOTELEN];
static void notes_load(void) {
  FILE *f;
  if (notesLoaded) return;
  notesLoaded = 1; nnotes = 0;
  if (!notes) notes = (char (*)[NOTELEN])calloc(MAXNOTES, NOTELEN);
  if (!notes) return;
  f = fopen(NOTESFILE, "r");
  if (!f) return;
  while (nnotes < MAXNOTES && fgets(notes[nnotes], NOTELEN, f)) {
    char *nl = strpbrk(notes[nnotes], "\r\n");
    if (nl) *nl = 0;
    if (notes[nnotes][0]) nnotes++;
  }
  fclose(f);
}
static void notes_save(void) {
  FILE *f = fopen(NOTESFILE, "w"); int i;
  if (!f) return;
  for (i = 0; i < nnotes; i++) fprintf(f, "%s\n", notes[i]);
  fclose(f);
}
static void v_notes(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U / 2, per, i; char sub[40];
  notes_load();
  snprintf(sub, sizeof(sub), nnotes == 1 ? "%d nota" : "%d notes", nnotes);
  header("Notes", sub);
  button(U / 2, y, SW - U, bh, "Nova nota", 0, A_NOTE_NEW, 0); y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = nnotes ? (nnotes + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!nnotes) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "Encara no hi ha cap nota.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < nnotes && i < (v->page + 1) * per; i++) {
    box(U / 2, y, SW - U, rh - U / 4);
    SetFont(fText, BLACK); DrawTextRect(U, y, SW - 2 * U, rh - U / 4, notes[i], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_NOTE_OPEN, i);
    y += rh;
  }
}
static void v_note(View *v) {
  int ah = 2 * U + U / 4, bw = (SW - U - U / 3) / 2, y = BOTTOM - ah;
  notes_load();
  if (v->a < 0 || v->a >= nnotes) { header("Nota", ""); return; }
  header("Nota", "");
  SetFont(fText, BLACK);
  DrawTextRect(U / 2, TOP, SW - U, y - TOP - U, notes[v->a], ALIGN_LEFT | VALIGN_TOP);
  button(U / 2, y, bw, ah, "Edita", 0, A_NOTE_EDIT, v->a);
  button(U / 2 + bw + U / 3, y, bw, ah, "Esborra", 0, A_NOTE_DEL, v->a);
}
static void on_note_kbd(char *text) {
  if (text && notes) {
    char *s = text, *p; size_t n;
    while (*s == ' ') s++;
    for (p = s; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
    n = strlen(s); while (n > 0 && s[n - 1] == ' ') s[--n] = 0;
    if (n > 0) {
      if (noteEdit >= 0 && noteEdit < nnotes) snprintf(notes[noteEdit], NOTELEN, "%s", s);
      else if (nnotes < MAXNOTES) { memmove(notes[1], notes[0], (size_t)nnotes * NOTELEN); snprintf(notes[0], NOTELEN, "%s", s); nnotes++; }
      notes_save();
    }
  }
  draw(1);
}
static int noteDel = -1;
static void on_note_del(int btn) {
  if (btn == 1 && noteDel >= 0 && noteDel < nnotes) {
    memmove(notes[noteDel], notes[noteDel + 1], (size_t)(nnotes - noteDel - 1) * NOTELEN);
    nnotes--; notes_save();
    POP();
  }
  noteDel = -1;
  draw(1);
}

static void mod_icon(int x, int y, int s) {      /* icona de línia, mateix gruix que la resta (ic_t) */
  int t = ic_t(s), i;
  ol(x + s / 4, y + s / 8, s / 2, s * 3 / 4, s / 12, t);
  for (i = 0; i < 3; i++) FillArea(x + s * 3 / 8, y + s * 5 / 16 + i * (s * 5 / 32), s / 4, t * 2 / 3 + 1, BLACK);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_NOTES: v_notes(v); break;
    case V_NOTE: v_note(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_NOTE_NEW: noteEdit = -1; nbuf[0] = 0; OpenKeyboard("Nova nota", nbuf, NOTELEN - 20, KBD_NORMAL, on_note_kbd); break;
    case A_NOTE_OPEN: push(V_NOTE, arg, 0, 0, NULL); break;
    case A_NOTE_EDIT: if (arg >= 0 && arg < nnotes) { noteEdit = arg; snprintf(nbuf, sizeof(nbuf), "%s", notes[arg]); OpenKeyboard("Edita la nota", nbuf, NOTELEN - 20, KBD_NORMAL, on_note_kbd); } break;
    case A_NOTE_DEL: noteDel = arg; Dialog(ICON_QUESTION, "Esborra la nota", "Vols esborrar aquesta nota?", "Esborra", "Cancel·la", on_note_del); break;
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API_MIN, "notes", "Notes", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 0, NULL };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
