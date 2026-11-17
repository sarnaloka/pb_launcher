/* Escriptori — dibuix de camins SVG de traç (les icones del prototip) amb vores suavitzades.
   ic_path(d, x, y, s, gruix, color, fons): dibuixa el camí d (viewBox 24 × 24) dins el quadrat x,y de costat s.
   El traç és rodó (extrems i unions), com «stroke-linecap/linejoin: round». Es calcula, per a cada píxel, la
   distància al segment més proper; amb el color de fons conegut es barregen els dos colors (antialiàsing).
   Si el fons és -1 (desconegut, p. ex. sobre una imatge), només es pinten els píxels coberts més de la meitat. */
#ifndef ESC_GFX_H
#define ESC_GFX_H
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include "icons.h"

typedef struct { float x1, y1, x2, y2; } GSeg;
typedef struct { GSeg *s; int n, cap; double sc; } GPath;

static UNUSED void gp_line(GPath *p, double x1, double y1, double x2, double y2) {
  if (p->n == p->cap) {
    int c = p->cap ? p->cap * 2 : 64; GSeg *ns = (GSeg *)realloc(p->s, sizeof(GSeg) * (size_t)c);
    if (!ns) return;
    p->s = ns; p->cap = c;
  }
  p->s[p->n].x1 = (float)(x1 * p->sc); p->s[p->n].y1 = (float)(y1 * p->sc);
  p->s[p->n].x2 = (float)(x2 * p->sc); p->s[p->n].y2 = (float)(y2 * p->sc);
  p->n++;
}
static UNUSED void gp_skip(const char **q) { while (**q == ' ' || **q == ',' || **q == '\t' || **q == '\n' || **q == '\r') (*q)++; }
static UNUSED int gp_num(const char **q, double *v) {
  char *e;
  gp_skip(q);
  if (!(isdigit((unsigned char)**q) || **q == '-' || **q == '+' || **q == '.')) return 0;
  *v = strtod(*q, &e);
  if (e == *q) return 0;
  *q = e;
  return 1;
}
static UNUSED int gp_flag(const char **q, double *v) { gp_skip(q); if (**q != '0' && **q != '1') return 0; *v = **q - '0'; (*q)++; return 1; }
static UNUSED void gp_cubic(GPath *p, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3) {
  int i, n = 14; double px = x0, py = y0;
  for (i = 1; i <= n; i++) {
    double t = (double)i / n, u = 1 - t, x = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3, y = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
    gp_line(p, px, py, x, y); px = x; py = y;
  }
}
/* Arc el·líptic amb la parametrització de punts finals de l'SVG (apèndix F.6.5 de l'especificació). */
static UNUSED void gp_arc(GPath *p, double x1, double y1, double rx, double ry, double phi, int fa, int fs, double x2, double y2) {
  const double PI = 3.14159265358979;
  double cp, sp, dx, dy, x1p, y1p, lam, num, den, co, cxp, cyp, ccx, ccy, t1, t2, dt, px = x1, py = y1;
  int i, n;
  if (rx == 0 || ry == 0 || (x1 == x2 && y1 == y2)) { gp_line(p, x1, y1, x2, y2); return; }
  rx = fabs(rx); ry = fabs(ry);
  cp = cos(phi * PI / 180); sp = sin(phi * PI / 180);
  dx = (x1 - x2) / 2; dy = (y1 - y2) / 2;
  x1p = cp * dx + sp * dy; y1p = -sp * dx + cp * dy;
  lam = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry);
  if (lam > 1) { rx *= sqrt(lam); ry *= sqrt(lam); }
  num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
  den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
  co = (num > 0 && den > 0) ? sqrt(num / den) : 0;
  if (fa == fs) co = -co;
  cxp = co * rx * y1p / ry; cyp = -co * ry * x1p / rx;
  ccx = cp * cxp - sp * cyp + (x1 + x2) / 2; ccy = sp * cxp + cp * cyp + (y1 + y2) / 2;
  t1 = atan2((y1p - cyp) / ry, (x1p - cxp) / rx);
  t2 = atan2((-y1p - cyp) / ry, (-x1p - cxp) / rx);
  dt = t2 - t1;
  if (!fs && dt > 0) dt -= 2 * PI; else if (fs && dt < 0) dt += 2 * PI;
  n = (int)ceil(fabs(dt) / (PI / 16)); if (n < 2) n = 2;
  for (i = 1; i <= n; i++) {
    double t = t1 + dt * i / n, x = ccx + rx * cos(t) * cp - ry * sin(t) * sp, y = ccy + rx * cos(t) * sp + ry * sin(t) * cp;
    if (i == n) { x = x2; y = y2; }
    gp_line(p, px, py, x, y); px = x; py = y;
  }
}
/* Converteix el camí en segments (en píxels, amb escala p->sc). */
static UNUSED void gp_parse(GPath *p, const char *d) {
  double cx = 0, cy = 0, sx = 0, sy = 0, lcx = 0, lcy = 0, a[7];
  char cmd = 0, prev = 0; const char *q = d;
  for (;;) {
    int rel, ok = 1; char C;
    gp_skip(&q);
    if (!*q) break;
    if (isalpha((unsigned char)*q)) cmd = *q++;
    else if (!cmd) break;
    rel = islower((unsigned char)cmd) != 0; C = (char)toupper((unsigned char)cmd);
    switch (C) {
      case 'Z': gp_line(p, cx, cy, sx, sy); cx = sx; cy = sy; cmd = 0; break;
      case 'M':
        if (!(ok = gp_num(&q, &a[0]) && gp_num(&q, &a[1]))) break;
        if (rel) { a[0] += cx; a[1] += cy; }
        cx = sx = a[0]; cy = sy = a[1]; cmd = rel ? 'l' : 'L';
        break;
      case 'L':
        if (!(ok = gp_num(&q, &a[0]) && gp_num(&q, &a[1]))) break;
        if (rel) { a[0] += cx; a[1] += cy; }
        gp_line(p, cx, cy, a[0], a[1]); cx = a[0]; cy = a[1];
        break;
      case 'H': if (!(ok = gp_num(&q, &a[0]))) break; if (rel) a[0] += cx; gp_line(p, cx, cy, a[0], cy); cx = a[0]; break;
      case 'V': if (!(ok = gp_num(&q, &a[0]))) break; if (rel) a[0] += cy; gp_line(p, cx, cy, cx, a[0]); cy = a[0]; break;
      case 'C': case 'S': case 'Q': case 'T': {
        int k, nn = C == 'C' ? 6 : (C == 'T' ? 2 : 4); double x1, y1, x2, y2, ex, ey;
        for (k = 0; k < nn && ok; k++) ok = gp_num(&q, &a[k]);
        if (!ok) break;
        if (rel) for (k = 0; k < nn; k += 2) { a[k] += cx; a[k + 1] += cy; }
        if (C == 'C') { x1 = a[0]; y1 = a[1]; x2 = a[2]; y2 = a[3]; ex = a[4]; ey = a[5]; }
        else if (C == 'S') {
          if (prev == 'C' || prev == 'S') { x1 = 2 * cx - lcx; y1 = 2 * cy - lcy; } else { x1 = cx; y1 = cy; }
          x2 = a[0]; y2 = a[1]; ex = a[2]; ey = a[3];
        } else {                                  /* quadràtiques convertides a cúbiques */
          double qx, qy;
          if (C == 'Q') { qx = a[0]; qy = a[1]; ex = a[2]; ey = a[3]; }
          else { if (prev == 'Q' || prev == 'T') { qx = 2 * cx - lcx; qy = 2 * cy - lcy; } else { qx = cx; qy = cy; } ex = a[0]; ey = a[1]; }
          x1 = cx + 2.0 / 3 * (qx - cx); y1 = cy + 2.0 / 3 * (qy - cy); x2 = ex + 2.0 / 3 * (qx - ex); y2 = ey + 2.0 / 3 * (qy - ey);
          gp_cubic(p, cx, cy, x1, y1, x2, y2, ex, ey); lcx = qx; lcy = qy; cx = ex; cy = ey;
          break;
        }
        gp_cubic(p, cx, cy, x1, y1, x2, y2, ex, ey); lcx = x2; lcy = y2; cx = ex; cy = ey;
      } break;
      case 'A': {
        double fa, fs;
        if (!(ok = gp_num(&q, &a[0]) && gp_num(&q, &a[1]) && gp_num(&q, &a[2]) && gp_flag(&q, &fa) && gp_flag(&q, &fs) && gp_num(&q, &a[5]) && gp_num(&q, &a[6]))) break;
        if (rel) { a[5] += cx; a[6] += cy; }
        gp_arc(p, cx, cy, a[0], a[1], a[2], (int)fa, (int)fs, a[5], a[6]); cx = a[5]; cy = a[6];
      } break;
      default: ok = 0;
    }
    if (!ok) break;
    prev = C;
  }
}
static UNUSED int gray_of(int c) { return (((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114) / 1000; }
/* Dibuixa una icona de traç. sw = gruix en unitats del viewBox (el prototip fa servir 1,8). */
static UNUSED void ic_path(const char *d, int x, int y, int s, double sw, int fg, int bg) {
  GPath p; float *cov; double r; int i, px, py, fgG, bgG;
  if (s <= 0 || !d) return;
  memset(&p, 0, sizeof(p)); p.sc = s / 24.0;
  r = sw * p.sc / 2; if (r < 0.9) r = 0.9;                 /* com a mínim un traç de gairebé 2 píxels */
  gp_parse(&p, d);
  cov = (float *)calloc((size_t)s * (size_t)s, sizeof(float));
  if (!cov) { free(p.s); return; }
  for (i = 0; i < p.n; i++) {
    const GSeg *g = &p.s[i]; double ax = g->x1, ay = g->y1, bx = g->x2, by = g->y2, vx = bx - ax, vy = by - ay, L2 = vx * vx + vy * vy;
    int x0 = (int)floor((ax < bx ? ax : bx) - r - 1), x1 = (int)ceil((ax > bx ? ax : bx) + r + 1), y0 = (int)floor((ay < by ? ay : by) - r - 1), y1 = (int)ceil((ay > by ? ay : by) + r + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s - 1) x1 = s - 1;
    if (y1 > s - 1) y1 = s - 1;
    for (py = y0; py <= y1; py++) for (px = x0; px <= x1; px++) {
      double cxp = px + 0.5, cyp = py + 0.5, t = L2 > 1e-9 ? ((cxp - ax) * vx + (cyp - ay) * vy) / L2 : 0, ddx, ddy, dist, c;
      if (t < 0) t = 0; else if (t > 1) t = 1;
      ddx = cxp - (ax + t * vx); ddy = cyp - (ay + t * vy); dist = sqrt(ddx * ddx + ddy * ddy);
      c = r + 0.5 - dist;
      if (c > 1) c = 1;
      if (c > cov[py * s + px]) cov[py * s + px] = (float)c;
    }
  }
  free(p.s);
  fgG = gray_of(fg);
  if (bg >= 0) {
    ibitmap *b = (ibitmap *)malloc(sizeof(ibitmap) + (size_t)s * (size_t)s);
    if (b) {
      bgG = gray_of(bg);
      b->width = (unsigned short)s; b->height = (unsigned short)s; b->depth = 8; b->scanline = (unsigned short)s;
      for (i = 0; i < s * s; i++) b->data[i] = (unsigned char)(bgG + (fgG - bgG) * cov[i] + 0.5f);
      DrawBitmap(x, y, b);
      free(b);
    }
  } else for (py = 0; py < s; py++) for (px = 0; px < s; px++) if (cov[py * s + px] >= 0.5f) DrawPixel(x + px, y + py, fg);
  free(cov);
}
/* El mateix, centrat a cx,cy. */
static UNUSED void ic_pathc(const char *d, int cx, int cy, int s, double sw, int fg, int bg) { ic_path(d, cx - s / 2, cy - s / 2, s, sw, fg, bg); }
#endif
