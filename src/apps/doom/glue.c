/* Unió entre el Doom i l'Escriptori: costat del Doom. Afegit per a l'Escriptori; GPL v2, com la resta del Doom. */
#include <stdio.h>
#include <stdlib.h>

#include "sdldoom-1.10/doomdef.h"
#include "sdldoom-1.10/doomstat.h"
#include "sdldoom-1.10/d_main.h"
#include "sdldoom-1.10/m_menu.h"
#include "sdldoom-1.10/w_wad.h"
#include "sdldoom-1.10/z_zone.h"
#include "sdldoom-1.10/v_video.h"
#include "pbdoom/frames/game.h"
#include "esc_glue.h"

extern boolean menuactive;
extern char *wadfiles[MAXWADFILES];
void keyboard_free_font(void);
extern void *mainzone;                 /* memzone_t* a z_zone.c */
void M_ClearMenus (void);

/* El menú del Doom ha demanat sortir: es tanca el menú i es torna a la pantalla de títol (la demostració). */
void esc_doom_title(void) {
  M_ClearMenus ();
  menuactive = false;
  D_StartTitle ();
}

/* Abans de descarregar el paquet: el que no és a la zona de memòria del Doom (dlclose no ho allibera). */
void esc_doom_free(void) {
  int i, j;
  game_frame_free();
  if (lumpinfo) {
    for (i = 0; i < numlumps; i++) {
      int h = lumpinfo[i].handle, seen = 0;
      if (h == -1 || h == 0) continue;
      for (j = 0; j < i && !seen; j++) if (lumpinfo[j].handle == h) seen = 1;
      if (!seen) fclose((FILE *) h);
    }
    free(lumpinfo); lumpinfo = NULL;
  }
  free(lumpcache); lumpcache = NULL;
  /* V_Init reserva les 4 pantalles seguides (screens[0] n'és l'inici); I_InitGraphics substitueix screens[0] per una
     de pròpia, però si el Doom s'ha aturat abans, encara és la del bloc. */
  { byte *base = screens[1] ? screens[1] - SCREENWIDTH * SCREENHEIGHT : NULL;
    if (screens[0] && screens[0] != base) free(screens[0]);
    free(base);
    for (i = 0; i < 5; i++) screens[i] = NULL; }
  free(mainzone); mainzone = NULL;
  for (i = 0; i < MAXWADFILES && wadfiles[i]; i++) { free(wadfiles[i]); wadfiles[i] = NULL; }
  free(doomcom); doomcom = NULL;
  keyboard_free_font();
}
