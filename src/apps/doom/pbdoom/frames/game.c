#include <math.h>

#include "game.h"
#include "../../esc_glue.h"
#include "keyboard.h"

#include "../../sdldoom-1.10/v_video.h"
#include "../../sdldoom-1.10/i_system.h"
#include "../../sdldoom-1.10/doomstat.h"
#include "../../sdldoom-1.10/m_menu.h"

void game_frame_install(int x, int y, int w, int h) {
  game_frame.x = x;
  game_frame.y = y;
  game_frame.w = w;
  game_frame.h = h;
}

void game_frame_uninstall() {
  
}

ink_render_mode i_render_mode = DYNAMIC_A2;

// All <r, g, b> in [0, 1]
typedef struct {
  int r;
  int g;
  int b;
} rgb;

rgb ink_palette[256];

void game_frame_set_render_mode(ink_render_mode x) {
  i_render_mode = x;
}

void game_frame_set_palette(byte *palette) {
  for (int i = 0; i < 256; i++) {
    ink_palette[i].r = gammatable[usegamma][*palette++];
    ink_palette[i].g = gammatable[usegamma][*palette++];
    ink_palette[i].b = gammatable[usegamma][*palette++];
  }
}

double game_alpha = GAME_DEFAULT_ALPHA;
double game_beta = GAME_DEFAULT_BETA;
double game_gamma = GAME_DEFAULT_GAMMA;

int adjust(double c) {
  double linear = c * game_alpha + game_beta;
  int x = pow(linear / 255, game_gamma) * 255;

  if (x < 0) {
    return 0;
  }
  if (x > 255) {
    return 255;
  }
  return x;
}

int adjusted_color(int idx) {
  rgb c = ink_palette[idx];

  int r = adjust(c.r);
  int g = adjust(c.g);
  int b = adjust(c.b);

  return (r << 16) | (g << 8) | b;
}

static void draw() {
  // do nothing, real draws come through game_frame_draw_screen
}

void update_keyboard_mode() {
  if (menuactive) {
    if (messageToPrint && messageNeedsInput) {
      set_keyboard_mode(keyboard_mode_message_input);
    } else {
      set_keyboard_mode(keyboard_mode_menu);
    }
  } else {
    set_keyboard_mode(keyboard_mode_level);
  }
}
/* Escriptori: en lloc d'un FillArea per píxel del Doom (64.000 per fotograma), es construeix un sol mapa de bits de grisos
   ampliat k vegades i es dibuixa d'un cop. En tema fosc es pinta invertit, perquè l'Escriptori el torna a invertir. */
static ibitmap *frame_bmp;
static unsigned char *last_screen;
static int fk, fx, fy;

static void frame_layout(void) {
  int by_w = game_frame.w / SCREENWIDTH;
  int by_h = game_frame.h / SCREENHEIGHT;
  fk = by_w < by_h ? by_w : by_h; // size of one pixel
  if (fk < 1) fk = 1;
  fx = game_frame.x + (game_frame.w - (fk * SCREENWIDTH)) / 2;
  fy = game_frame.y + (game_frame.h - (fk * SCREENHEIGHT)) / 2;
}

static int frame_build(unsigned char *screen) {
  unsigned char lut[256];
  int i, j, r, w, h;
  frame_layout();
  w = fk * SCREENWIDTH; h = fk * SCREENHEIGHT;
  if (!frame_bmp || frame_bmp->width != w || frame_bmp->height != h) {
    free(frame_bmp);
    frame_bmp = malloc(sizeof(ibitmap) + (size_t)w * h);
    if (!frame_bmp) return 0;
    frame_bmp->width = w; frame_bmp->height = h; frame_bmp->depth = 8; frame_bmp->scanline = w;
  }
  for (i = 0; i < 256; i++) {
    int c = adjusted_color(i), g = (((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114) / 1000;
    lut[i] = esc_doom_dark ? 255 - g : g;
  }
  for (i = 0; i < SCREENHEIGHT; i++) {
    unsigned char *row = frame_bmp->data + (size_t)i * fk * w, *src = screen + i * SCREENWIDTH;
    for (j = 0; j < SCREENWIDTH; j++) { unsigned char g = lut[src[j]]; for (r = 0; r < fk; r++) row[j * fk + r] = g; }
    for (r = 1; r < fk; r++) memcpy(row + (size_t)r * w, row, w);
  }
  return 1;
}

/* Torna a dibuixar l'últim fotograma (quan l'Escriptori redibuixa la pantalla); no actualitza la pantalla. */
void game_frame_paint(void) {
  if (last_screen && frame_build(last_screen)) DrawBitmap(fx, fy, frame_bmp);
  else { frame_layout(); FillArea(fx, fy, fk * SCREENWIDTH, fk * SCREENHEIGHT, BLACK); }
}

void game_frame_free(void) { free(frame_bmp); frame_bmp = NULL; last_screen = NULL; }

void game_frame_draw_screen(unsigned char *screen) {
  int uw, uh;
  last_screen = screen;
  if (!esc_doom_active()) {
    return;
  }
  update_keyboard_mode();
  if (!frame_build(screen)) return;
  DrawBitmap(fx, fy, frame_bmp);
  uw = fk * SCREENWIDTH; uh = fk * SCREENHEIGHT;

  switch (i_render_mode) {
  case DYNAMIC_A2:
    esc_doom_update(fx, fy, uw, uh, 1);
    break;
  case DITHER_AREA_PATTERN_2_LEVEL:
    DitherAreaPattern2Level(fx, fy, uw, uh);
    esc_doom_update(fx, fy, uw, uh, 0);
    break;
  case DITHER_MANUAL_2_PATTERN:
    DitherArea(fx, fy, uw, uh, 2, DITHER_PATTERN);
    esc_doom_update(fx, fy, uw, uh, 0);
    break;
  default:
    esc_doom_update(fx, fy, uw, uh, 0);
    break;
  }
}

void esc_doom_set_mode(int mode) { if (mode >= DYNAMIC_A2 && mode <= NO_DITHER) i_render_mode = mode; }
int esc_doom_get_mode(void) { return i_render_mode; }

frame game_frame = {
  .install = *game_frame_install,
  .uninstall = *game_frame_uninstall,
  .draw = *draw
};
