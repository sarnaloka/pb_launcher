/* Pont entre el Lector i el CREngine (vegeu cre.h). El CREngine és GPL v2 o posterior; aquest fitxer també. */
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "crengine.h"
#include "lvdocview.h"
#include "lvdocviewprops.h"
#include "hyphman.h"
#include "textlang.h"
#include "cre.h"

static LVDocView *dv;
static bool inited, opened;
static CreLayout cur;
static bool haveLayout;
static lString8 baseCss;
static lString8 faceSans, faceSerif;
static LVArray<LVTocItem *> tocFlat;

static void toc_flatten(LVTocItem *it) {
  if (!it) return;
  for (int i = 0; i < it->getChildCount(); i++) { tocFlat.add(it->getChild(i)); toc_flatten(it->getChild(i)); }
}
static lString8 read_file(const lString8 &path) {
  lString8 s; FILE *f = fopen(path.c_str(), "rb"); char b[4096]; size_t n;
  if (!f) return s;
  while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, (int)n);
  fclose(f);
  return s;
}
static void register_fonts(const char *dir, int depth) {
  DIR *d = opendir(dir); struct dirent *e;
  if (!d) return;
  while ((e = readdir(d)) != NULL) {
    char p[600]; struct stat st; size_t n = strlen(e->d_name);
    if (e->d_name[0] == '.') continue;
    snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
    if (stat(p, &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) { if (depth < 2) register_fonts(p, depth + 1); continue; }
    if (n > 4 && (!strcasecmp(e->d_name + n - 4, ".ttf") || !strcasecmp(e->d_name + n - 4, ".otf"))) fontMan->RegisterFont(lString8(p));
  }
  closedir(d);
}
/* Tria la primera font disponible d'una llista de preferència. */
static lString8 pick_face(const char *const *prefs, const lString32Collection &faces) {
  for (int k = 0; prefs[k]; k++) for (int i = 0; i < faces.length(); i++) if (faces[i] == Utf8ToUnicode(prefs[k])) return lString8(prefs[k]);
  return faces.length() ? UnicodeToUtf8(faces[0]) : lString8("DejaVu Sans");
}

extern "C" int cre_init(const char *datadir, const char *const *fontdirs, const char *cachedir) {
  static const char *SANS[] = { "Liberation Sans", "DejaVu Sans", "PT Sans", "Noto Sans", "Open Sans", "Roboto", NULL };
  static const char *SERIF[] = { "Liberation Serif", "DejaVu Serif", "PT Serif", "Noto Serif", "Georgia", "Charis SIL", "Times New Roman", NULL };
  lString32Collection faces;
  if (inited) return 0;
  InitFontManager(lString8::empty_str);
  for (int i = 0; fontdirs && fontdirs[i]; i++) register_fonts(fontdirs[i], 0);
  fontMan->getFaceList(faces);
  if (faces.length() == 0) return -1;
  faceSans = pick_face(SANS, faces); faceSerif = pick_face(SERIF, faces);
  fontMan->SetFallbackFontFaces(lString8("DejaVu Sans;Liberation Sans;Noto Sans"));
  fontMan->SetKerningMode(KERNING_MODE_HARFBUZZ);
  fontMan->SetHintingMode(HINTING_MODE_BYTECODE_INTERPRETOR);
  fontMan->SetAntialiasMode(font_aa_all);
  HyphMan::initDictionaries(Utf8ToUnicode(lString8(datadir) + "/hyph/"));
  baseCss = read_file(lString8(datadir) + "/epub.css");
  if (cachedir && cachedir[0]) ldomDocCache::init(Utf8ToUnicode(lString8(cachedir)), 48 * 1024 * 1024);
  dv = new LVDocView(8, false);
  dv->setPageHeaderInfo(PGHDR_NONE);
  dv->setViewMode(DVM_PAGES, 1);
  dv->setBackgroundColor(0xFFFFFF);
  dv->setTextColor(0x000000);
  dv->setMinFileSizeToCache(256 * 1024);
  inited = true;
  return 0;
}

/* Compon el llibre i fixa la posició: si no, el primer goToPage després de compondre el desfà checkPos(), que encara
   té pendent posar la posició inicial. */
static void settle(void) {
  dv->checkRender();
  dv->getCurPage();
}
static void apply_props(const CreLayout *l) {
  CRPropRef p = LVCreatePropsContainer();
  lString8 css = baseCss;
  p->setString(PROP_FONT_FACE, (l->serif ? faceSerif : faceSans).c_str());
  p->setInt(PROP_FONT_SIZE, l->font_px);
  p->setInt(PROP_INTERLINE_SPACE, l->interline);
  p->setInt(PROP_EMBEDDED_STYLES, l->embedded_styles ? 1 : 0);
  p->setInt(PROP_EMBEDDED_FONTS, l->embedded_styles ? 1 : 0);
  p->setString(PROP_TEXTLANG_MAIN_LANG, "ca");
  p->setInt(PROP_TEXTLANG_EMBEDDED_LANGS_ENABLED, 1);
  p->setInt(PROP_TEXTLANG_HYPHENATION_ENABLED, l->hyphenate ? 1 : 0);
  p->setInt(PROP_FONT_KERNING, 3);                 /* HarfBuzz */
  p->setInt(PROP_PAGE_MARGIN_LEFT, l->margin_x); p->setInt(PROP_PAGE_MARGIN_RIGHT, l->margin_x);
  p->setInt(PROP_PAGE_MARGIN_TOP, l->margin_top); p->setInt(PROP_PAGE_MARGIN_BOTTOM, l->margin_bottom);
  p->setInt(PROP_STATUS_LINE, 0);
  p->setInt(PROP_SHOW_TIME, 0); p->setInt(PROP_SHOW_TITLE, 0); p->setInt(PROP_SHOW_PAGE_NUMBER, 0); p->setInt(PROP_SHOW_PAGE_COUNT, 0);
  dv->propsApply(p);
  dv->setPageHeaderInfo(PGHDR_NONE);               /* la capçalera i el peu els dibuixa el Lector */
  /* justificació: el text dels paràgrafs; els títols i el que el llibre centra no es toquen */
  css += l->justify ? "\np { text-align: justify; }\n" : "\np { text-align: start; }\n";
  dv->setStyleSheet(css);
  dv->setPageMargins(lvRect(l->margin_x, l->margin_top, l->margin_x, l->margin_bottom));
  dv->Resize(l->w, l->h);
}

extern "C" int cre_open(const char *path) {
  if (!inited) return -1;
  cre_close();
  if (haveLayout) apply_props(&cur);
  if (!dv->LoadDocument(path)) return -1;
  opened = true;
  if (haveLayout) apply_props(&cur);
  settle();
  tocFlat.clear(); toc_flatten(dv->getToc());
  return 0;
}
extern "C" void cre_close(void) {
  if (!opened) return;
  dv->savePosition();
  dv->close();
  tocFlat.clear();
  opened = false;
}
extern "C" int cre_is_open(void) { return opened ? 1 : 0; }
extern "C" void cre_layout(const CreLayout *l) {
  if (!inited) return;
  if (haveLayout && !memcmp(l, &cur, sizeof(cur))) return;
  cur = *l; haveLayout = true;
  if (!opened) return;
  { ldomXPointer bm = dv->getBookmark();
    apply_props(&cur);
    settle();
    if (!bm.isNull()) dv->goToBookmark(bm); }
  tocFlat.clear(); toc_flatten(dv->getToc());
}
extern "C" int cre_pages(void) { return opened ? dv->getPageCount() : 0; }
extern "C" int cre_page(void) { return opened ? dv->getCurPage() : 0; }
extern "C" void cre_goto_page(int p) { if (opened) dv->goToPage(p); }
extern "C" int cre_next(void) { int p = cre_page(); if (opened && p + 1 < cre_pages()) { dv->goToPage(p + 1); return cre_page() != p; } return 0; }
extern "C" int cre_prev(void) { int p = cre_page(); if (opened && p > 0) { dv->goToPage(p - 1); return cre_page() != p; } return 0; }
extern "C" int cre_percent(void) {
  int n = cre_pages();
  if (!opened || n <= 0) return 0;
  if (cre_page() >= n - 1) return 10000;
  return (int)((long long)cre_page() * 10000 / n);
}
extern "C" void cre_goto_percent(int p10000) {
  int n = cre_pages(), p;
  if (!opened || n <= 0) return;
  p = (int)((long long)p10000 * n / 10000);
  if (p >= n) p = n - 1;
  if (p < 0) p = 0;
  dv->goToPage(p);
}
extern "C" int cre_render(unsigned char *gray, int w, int h) {
  if (!opened || w <= 0 || h <= 0) return -1;
  LVGrayDrawBuf buf(w, h, 8);
  buf.Clear(0xFFFFFF);
  dv->Draw(buf, false);
  for (int y = 0; y < h; y++) memcpy(gray + (size_t)y * w, buf.GetScanLine(y), (size_t)w);
  return 0;
}
extern "C" int cre_bookmark(char *out, int max) {
  if (!opened || max <= 0) return -1;
  ldomXPointer bm = dv->getBookmark();
  lString8 s = bm.isNull() ? lString8() : UnicodeToUtf8(bm.toString());
  snprintf(out, (size_t)max, "%s", s.c_str());
  return 0;
}
extern "C" int cre_goto_bookmark(const char *xp) {
  if (!opened || !xp || !xp[0]) return -1;
  ldomXPointer bm = dv->getDocument()->createXPointer(Utf8ToUnicode(lString8(xp)));
  if (bm.isNull()) return -1;
  dv->goToBookmark(bm);
  return 0;
}
extern "C" int cre_toc_count(void) { return opened ? tocFlat.length() : 0; }
extern "C" int cre_toc_get(int i, char *title, int max, int *level, int *page) {
  if (!opened || i < 0 || i >= tocFlat.length()) return -1;
  LVTocItem *t = tocFlat[i];
  snprintf(title, (size_t)max, "%s", UnicodeToUtf8(t->getName()).c_str());
  if (level) *level = t->getLevel() > 0 ? t->getLevel() - 1 : 0;
  if (page) *page = t->getPage();
  return 0;
}
extern "C" int cre_chapter_title(char *out, int max) {
  int p = cre_page(), best = -1;
  out[0] = 0;
  for (int i = 0; i < tocFlat.length(); i++) if (tocFlat[i]->getPage() <= p) best = i;
  if (best < 0) return -1;
  snprintf(out, (size_t)max, "%s", UnicodeToUtf8(tocFlat[best]->getName()).c_str());
  return 0;
}
extern "C" const char *cre_font_name(int serif) { return (serif ? faceSerif : faceSans).c_str(); }
