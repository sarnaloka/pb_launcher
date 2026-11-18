/* Unió entre el Doom (PBDoom) i l'Escriptori. Afegit per a l'Escriptori; es distribueix amb la GPL v2, com la resta del Doom. */
#ifndef ESC_GLUE_H
#define ESC_GLUE_H
#include <setjmp.h>

/* Costat de l'Escriptori (src/apps/doom.c) */
extern char esc_doom_dir[512];          /* carpeta del WAD: també hi van les partides desades i la configuració */
extern jmp_buf esc_doom_jmp;             /* I_Error hi torna en lloc de tancar el programa */
extern int esc_doom_jmp_ok;              /* hi ha un setjmp actiu */
extern int esc_doom_dark;                /* tema fosc de l'Escriptori: el joc es pinta invertit i la zona es torna a invertir */
extern int esc_doom_quit_req;            /* el menú del Doom ha demanat sortir */
void esc_doom_update(int x, int y, int w, int h, int a2);
int esc_doom_active(void);               /* el Doom és a primer pla: el bucle pot continuar */
void esc_doom_failed(const char *msg);   /* I_Error: desa el missatge (després es fa longjmp) */
void esc_doom_quit_done(void);           /* el joc ha tornat a la pantalla de títol: l'Escriptori torna a l'inici */

/* Costat del Doom */
void esc_doom_gui(int x, int y, int w, int h, int dpi);
void esc_doom_draw_all(void);            /* controls i últim fotograma, sense actualitzar la pantalla */
int esc_doom_handle(int t, int a, int b);
void esc_doom_resume(void);              /* torna a engegar el bucle (s'atura sol quan el Doom no és a primer pla) */
void esc_doom_title(void);               /* torna a la pantalla de títol */
void esc_doom_free(void);                /* allibera la memòria abans de descarregar el paquet */
void esc_doom_open_colors(void);
void esc_doom_set_mode(int mode);
int esc_doom_get_mode(void);
int doom_main(int argc, char **argv);
#endif
