/* PBDoom: disposició de la pantalla. Adaptat per a l'Escriptori: sense el plafó superior de PBDoom (l'Escriptori hi
   posa la seva capçalera); el joc i els controls ocupen la zona que dona l'Escriptori. */
#include "inkview.h"

#include "gui.h"
#include "render.h"
#include "../esc_glue.h"

#include "frames/game.h"
#include "frames/keyboard.h"
#include "frames/color_settings.h"

int dpi = 0;
double dp;

static void set_dpi(int gdpi) {
  dpi = gdpi > 0 ? gdpi : 212;
  // like Android's dp
  dp = (double) dpi / 160;
}

int gui_x, gui_y, gui_w, gui_h;
gui_size_t gui_size;

static void set_gui_size() {
  double h = (double) gui_h / dpi;
  double w = (double) gui_w / dpi;
  double dsq = h * h + w * w;

  if (dsq <= (10 * 10) - 0.9) {
    gui_size = size_m;
  } else {
    gui_size = size_l;
  }
}

int controls_h() {
  if (gui_size > size_m) {
    return 400 * dp;
  } else {
    return 250 * dp;
  }
}

frame *controls_frame;
void switch_controls_frame(frame *new_frame) {
  if (controls_frame) {
    controls_frame->uninstall();
  }
  int ch = controls_h();
  controls_frame = new_frame;
  controls_frame->install(gui_x, gui_y + gui_h - ch, gui_w, ch);
}

static void install_frames() {
  switch_controls_frame(&keyboard_frame);
  game_frame.install(gui_x, gui_y, gui_w, gui_h - keyboard_frame.h);
}

void esc_doom_gui(int x, int y, int w, int h, int gdpi) {
  set_dpi(gdpi);
  gui_x = x; gui_y = y; gui_w = w; gui_h = h;
  set_gui_size();
  install_frames();
}

void esc_doom_draw_all(void) {
  if (controls_frame) {
    FillArea(controls_frame->x, controls_frame->y, controls_frame->w, controls_frame->h, WHITE);
    controls_frame->draw();
  }
  game_frame_paint();
}

static void redraw_frame(frame *f) {
  FillArea(f->x, f->y, f->w, f->h, WHITE);
  f->draw();
  esc_doom_update(f->x, f->y, f->w, f->h, 0);
}

void gui_open_color_settings() {
  switch_controls_frame(&color_settings_frame);
  redraw_frame(controls_frame);
}

void esc_doom_open_colors(void) { switch_controls_frame(&color_settings_frame); }

void gui_open_keyboard() {
  switch_controls_frame(&keyboard_frame);
  redraw_frame(controls_frame);
}

int esc_doom_handle(int t, int a, int b) {
  return controls_frame && controls_frame->handle && controls_frame->handle(t, a, b);
}
