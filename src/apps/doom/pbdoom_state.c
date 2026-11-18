#include "sdldoom-1.10/doomdef.h"
#include "sdldoom-1.10/d_main.h"
#include "sdldoom-1.10/i_system.h"

#include "inkview.h"
#include "esc_glue.h"

#define D_DOOM_LOOP_PERIOD (1000 / TICRATE)

/* Escriptori: el bucle només corre mentre el Doom és a primer pla; un error (I_Error) torna aquí amb longjmp. */
static int loop_on;

void D_DoomLoop_iter_pbdoom() {
  long start, end, next;
  if (!esc_doom_active()) { loop_on = 0; return; }
  if (setjmp(esc_doom_jmp)) { esc_doom_jmp_ok = 0; loop_on = 0; return; }
  esc_doom_jmp_ok = 1;

  start = I_GetTimeMS();

  D_DoomLoop_iter();

  end = I_GetTimeMS();
  esc_doom_jmp_ok = 0;

  if (esc_doom_quit_req) { esc_doom_quit_req = 0; loop_on = 0; esc_doom_title(); esc_doom_quit_done(); return; }

  next = D_DOOM_LOOP_PERIOD - (end - start);
  if (next < 1) {
    next = 1;
  }

  loop_on = 1;
  SetWeakTimer("D_DoomLoop", D_DoomLoop_iter_pbdoom, next);
}

void start_D_DoomLoop() {
  D_DoomLoop_pre();

  D_DoomLoop_iter_pbdoom();
}

void esc_doom_resume(void) {
  if (!loop_on) { loop_on = 1; SetWeakTimer("D_DoomLoop", D_DoomLoop_iter_pbdoom, 20); }
}
