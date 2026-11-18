/* Pont entre el paquet Lector (C) i el CREngine (C++): el motor de pàgines del KOReader i del Kobalt.
   Només hi ha un llibre obert alhora. Les posicions es desen com a XPointer (text), que no depèn de la mida de lletra. */
#ifndef LECTOR_CRE_H
#define LECTOR_CRE_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int w, h;                 /* mida de la pàgina en píxels */
  int font_px;              /* mida de la lletra */
  int serif;                /* 1 = amb serifa */
  int interline;            /* interlineat en % (100 = el del tipus de lletra) */
  int margin_x, margin_top, margin_bottom;
  int justify;              /* 1 = text justificat (vegeu align) */
  int align;                /* 0 esquerra, 1 justificat, 2 el que digui el llibre */
  char face[64];            /* font triada; buit = la de serif/sense serif per defecte */
  int embedded_fonts;       /* 1 = les fonts que porta el llibre tenen preferència */
  int hyphenate;            /* 1 = partició de mots (segons la llengua del llibre; el català per defecte) */
  int embedded_styles;      /* 1 = respecta el CSS i les fonts del llibre */
} CreLayout;

/* datadir: on hi ha els diccionaris (hyph/<llengua>.pattern) i els CSS; fontdirs: carpetes de fonts, acabades en NULL;
   cachedir: memòria cau de llibres ja compostos (pot ser NULL). Torna 0 si va bé. */
int cre_init(const char *datadir, const char *const *fontdirs, const char *cachedir);
int cre_open(const char *path);                   /* 0 si va bé */
void cre_close(void);
int cre_is_open(void);
void cre_layout(const CreLayout *l);             /* només torna a compondre si canvia res */
int cre_pages(void);
int cre_page(void);                               /* 0..pàgines-1 */
void cre_goto_page(int p);
int cre_next(void);                               /* 1 si ha canviat de pàgina */
int cre_prev(void);
int cre_percent(void);                            /* 0..10000 */
void cre_goto_percent(int p10000);
/* Pinta la pàgina actual: un byte per píxel (0 negre, 255 blanc), w*h, amb la mida de la composició. */
int cre_render(unsigned char *gray, int w, int h);
int cre_bookmark(char *out, int max);             /* posició actual (XPointer) */
int cre_goto_bookmark(const char *xp);
int cre_toc_count(void);                          /* índex aplanat */
int cre_toc_get(int i, char *title, int max, int *level, int *page);
int cre_chapter_title(char *out, int max);        /* títol de l'entrada de l'índex de la pàgina actual */
const char *cre_font_name(int serif);             /* nom de la font per defecte, amb serifa o sense */
int cre_font_count(void);                         /* fonts disponibles, per ordre alfabètic */
int cre_font_get(int i, char *out, int max);
/* Pinta qualsevol pàgina (miniatures de Navega) sense moure la lectura. */
int cre_render_page(int page, unsigned char *gray, int w, int h);
typedef struct { int page; char text[200]; } CreHit;
int cre_search(const char *q, CreHit *hits, int max);   /* torna quants n'ha trobat (fins a max) */
int cre_bookmark_page(const char *xp);            /* pàgina d'una posició desada, o -1 */

#ifdef __cplusplus
}
#endif
#endif
