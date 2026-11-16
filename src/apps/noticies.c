/* Escriptori — paquet «Notícies». Generat a partir del codi comú; vegeu mod.h. */
#define ESC_MODULE
#include "../mod.h"
enum { V_RSS, V_RSSITEM, V_FEEDS };
enum { A_RSS_REFRESH = 100, A_RSS_OPEN, A_RSS_FEEDS, A_FEED_ADD, A_FEED_DEL };
/* ═════════════ Notícies (RSS) ═════════════ */
static Store feeds = { 0, 0, 0, CFG "escriptori_canals.txt" };
static Store news = { 0, 0, 0, CFG "escriptori_noticies.txt" };    /* "títol\tresum" */
static void feeds_load(void) {
  st_load(&feeds);
  if (!feeds.n && feeds.it) { st_insert(&feeds, 0, "https://www.vilaweb.cat/feed/"); st_save(&feeds); }
}
static void utf8_put(char *out, int *n, int max, unsigned c) {
  if (*n + 4 >= max) return;
  if (c < 0x80) out[(*n)++] = (char)c;
  else if (c < 0x800) { out[(*n)++] = (char)(0xC0 | (c >> 6)); out[(*n)++] = (char)(0x80 | (c & 63)); }
  else if (c < 0x10000) { out[(*n)++] = (char)(0xE0 | (c >> 12)); out[(*n)++] = (char)(0x80 | ((c >> 6) & 63)); out[(*n)++] = (char)(0x80 | (c & 63)); }
  else { out[(*n)++] = (char)(0xF0 | (c >> 18)); out[(*n)++] = (char)(0x80 | ((c >> 12) & 63)); out[(*n)++] = (char)(0x80 | ((c >> 6) & 63)); out[(*n)++] = (char)(0x80 | (c & 63)); }
}
/* text net: sense etiquetes, CDATA ni entitats; espais compactats */
static void clean_text(const char *s, const char *end, char *out, int max, int pass) {
  int n = 0, sp = 1; char *tmp = NULL;
  while (s < end && n < max - 5) {
    if (!strncmp(s, "<![CDATA[", 9)) { s += 9; continue; }
    if (!strncmp(s, "]]>", 3)) { s += 3; continue; }
    if (*s == '<') { while (s < end && *s != '>') s++; if (s < end) s++; if (!sp) { out[n++] = ' '; sp = 1; } continue; }
    if (*s == '&') {
      const char *sc = memchr(s, ';', (size_t)(end - s > 10 ? 10 : end - s)); unsigned c = 0;
      if (sc) {
        if (s[1] == '#') c = (s[2] == 'x' || s[2] == 'X') ? (unsigned)strtoul(s + 3, NULL, 16) : (unsigned)strtoul(s + 2, NULL, 10);
        else if (!strncmp(s, "&amp;", 5)) c = '&'; else if (!strncmp(s, "&lt;", 4)) c = '<'; else if (!strncmp(s, "&gt;", 4)) c = '>';
        else if (!strncmp(s, "&quot;", 6)) c = '"'; else if (!strncmp(s, "&apos;", 6)) c = '\''; else if (!strncmp(s, "&nbsp;", 6)) c = ' ';
        if (c) { if (c == ' ' || c == '\n' || c == '\t') { if (!sp) { out[n++] = ' '; sp = 1; } } else { utf8_put(out, &n, max, c); sp = 0; } s = sc + 1; continue; }
      }
    }
    if (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') { if (!sp) { out[n++] = ' '; sp = 1; } s++; continue; }
    out[n++] = *s++; sp = 0;
  }
  while (n > 0 && out[n - 1] == ' ') n--;
  out[n] = 0;
  /* el resum sovint porta HTML escapat (&lt;p&gt;): segona passada */
  if (pass == 0 && strchr(out, '<') && (tmp = strdup(out))) { clean_text(tmp, tmp + strlen(tmp), out, max, 1); free(tmp); }
}
static const char *tag_body(const char *from, const char *end, const char *tag, const char **bodyEnd) {
  char open[40], close[40]; const char *p, *q;
  snprintf(open, sizeof(open), "<%s", tag); snprintf(close, sizeof(close), "</%s>", tag);
  for (p = from; (p = strstr(p, open)) && p < end; p += strlen(open)) {
    char nx = p[strlen(open)];
    if (nx != '>' && nx != ' ' && nx != '\t' && nx != '\n') continue;
    q = strchr(p, '>');
    if (!q || q >= end) return NULL;
    if (q[-1] == '/') continue;
    *bodyEnd = strstr(q + 1, close);
    if (!*bodyEnd || *bodyEnd > end) return NULL;
    return q + 1;
  }
  return NULL;
}
static int rss_parse(const char *xml) {
  const char *p = xml, *end = xml + strlen(xml); int added = 0; static char title[400], desc[2400], ln[2900];
  const char *itag = strstr(xml, "<item") ? "item" : "entry";
  while (news.n < MAXL && added < 40) {
    const char *ie, *ib = tag_body(p, end, itag, &ie), *te, *tb, *de, *dbody;
    if (!ib) break;
    tb = tag_body(ib, ie, "title", &te);
    dbody = tag_body(ib, ie, "description", &de);
    if (!dbody) dbody = tag_body(ib, ie, "summary", &de);
    if (!dbody) dbody = tag_body(ib, ie, "content", &de);
    title[0] = desc[0] = 0;
    if (tb) clean_text(tb, te, title, sizeof(title), 0);
    if (dbody) clean_text(dbody, de, desc, sizeof(desc), 0);
    if (title[0]) { snprintf(ln, sizeof(ln), "%s\t%s", title, desc); st_insert(&news, news.n, ln); added++; }
    p = ie + 1;
  }
  return added;
}
static void rss_refresh(void) {
  int i, total = 0, okFeeds = 0;
  feeds_load(); st_load(&news);
  for (i = 0; i < feeds.n; i++) {
    char *b = net_get(feeds.it[i], NULL);
    if (!b) continue;
    if (!okFeeds) st_clear(&news);
    okFeeds++; total += rss_parse(b);
    free(b);
  }
  if (!okFeeds) { say("No s'ha pogut descarregar cap canal. Comprova el Wi-Fi."); return; }
  st_save(&news);
  (void)total;
}
static void v_rss(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U * 3 / 4, per, i, bw = (SW - U - U / 3) / 2; char sub[64], title[400];
  feeds_load(); st_load(&news);
  snprintf(sub, sizeof(sub), "%d titulars · %d canals", news.n, feeds.n);
  header("Notícies", sub);
  button(U / 2, y, bw, bh, "Actualitza (Wi-Fi)", 0, A_RSS_REFRESH, 0); button(U / 2 + bw + U / 3, y, bw, bh, "Canals", 0, A_RSS_FEEDS, 0);
  y += bh + U / 2;
  per = (BOTTOM - y) / rh; if (per < 1) per = 1;
  pages = news.n ? (news.n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  if (!news.n) { SetFont(fSmall, BLACK); DrawTextRect(U / 2, y, SW - U, 2 * U, "Encara no hi ha titulars desats.", ALIGN_LEFT | VALIGN_TOP); }
  for (i = v->page * per; i < news.n && i < (v->page + 1) * per; i++) {
    const char *tab = strchr(news.it[i], '\t'); size_t n = tab ? (size_t)(tab - news.it[i]) : strlen(news.it[i]);
    if (n > sizeof(title) - 1) n = sizeof(title) - 1;
    memcpy(title, news.it[i], n); title[n] = 0;
    SetFont(fText, BLACK); DrawTextRect(U / 2, y, SW - U, rh - U / 4, title, ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    DrawLine(U / 2, y + rh - U / 8, SW - U / 2, y + rh - U / 8, LGRAY);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_RSS_OPEN, i);
    y += rh;
  }
}
static void v_rssitem(View *v) {
  char title[400]; const char *tab; size_t n; int th;
  st_load(&news);
  if (v->a < 0 || v->a >= news.n) { header("Notícia", ""); return; }
  tab = strchr(news.it[v->a], '\t'); n = tab ? (size_t)(tab - news.it[v->a]) : strlen(news.it[v->a]);
  if (n > sizeof(title) - 1) n = sizeof(title) - 1;
  memcpy(title, news.it[v->a], n); title[n] = 0;
  header("Notícia", "");
  SetFont(fBold, BLACK); th = TextRectHeight(SW - U, title, ALIGN_LEFT | VALIGN_TOP);
  if (th < U) th = U + U / 2;
  if (th > 8 * U) th = 8 * U;
  DrawTextRect(U / 2, TOP, SW - U, th, title, ALIGN_LEFT | VALIGN_TOP);
  SetFont(fText, BLACK);
  DrawTextRect(U / 2, TOP + th + U / 2, SW - U, BOTTOM + NAVH - TOP - th - U / 2, tab && tab[1] ? tab + 1 : "(Sense resum.)", ALIGN_LEFT | VALIGN_TOP | DOTS);
}
static int feedDel = -1;
static void feed_del(void) { st_del(&feeds, feedDel); st_save(&feeds); feedDel = -1; }
static void feed_add(const char *t) { if (!strncmp(t, "http://", 7) || !strncmp(t, "https://", 8)) { st_insert(&feeds, feeds.n, t); st_save(&feeds); } else say("L'adreça ha de començar per http:// o https://"); }
static void v_feeds(View *v) {
  int y = TOP, bh = 2 * U + U / 4, rh = 2 * U + U / 4, per, i; char sub[40];
  feeds_load();
  snprintf(sub, sizeof(sub), feeds.n == 1 ? "%d canal" : "%d canals", feeds.n);
  header("Canals de notícies", sub);
  button(U / 2, y, SW - U, bh, "Afegeix un canal RSS", 0, A_FEED_ADD, 0); y += bh + U / 2;
  per = (BOTTOM - y - 2 * U) / rh; if (per < 1) per = 1;
  pages = feeds.n ? (feeds.n + per - 1) / per : 1; if (v->page >= pages) v->page = pages - 1;
  for (i = v->page * per; i < feeds.n && i < (v->page + 1) * per; i++) {
    box(U / 2, y, SW - U, rh - U / 4);
    SetFont(fSmall, BLACK); DrawTextRect(U, y, SW - 2 * U, rh - U / 4, feeds.it[i], ALIGN_LEFT | VALIGN_MIDDLE | DOTS);
    add_hit(U / 2, y, SW - U, rh - U / 4, A_FEED_DEL, i);
    y += rh;
  }
  SetFont(fSmall, BLACK); DrawTextRect(U / 2, y + U / 4, SW - U, U + U / 4, "Toca un canal per esborrar-lo.", ALIGN_LEFT | VALIGN_MIDDLE);
}
/* ───── widget ───── */
static void w_headline(int x, int y, int w, int h) {
  char t[300] = "Encara no hi ha titulars"; char *tab;
  st_load(&news);
  if (news.n > 0) { snprintf(t, sizeof(t), "%s", news.it[0]); if ((tab = strchr(t, '\t'))) *tab = 0; }
  ui_lv(x, y, w, h, "Notícies · últim titular", t);
}
static const Widget WG[] = { { "titular", "Últim titular", 4, 1, w_headline } };
static void mod_icon(int x, int y, int s) {
  int i, j; (void)i; (void)j;
      FillArea(x + s / 6, y + s / 6, s * 2 / 3, s * 2 / 3, BLACK); FillArea(x + s / 6 + 4, y + s / 6 + 4, s * 2 / 3 - 8, s * 2 / 3 - 8, WHITE);
      FillArea(x + s / 4, y + s / 4, s / 2, s / 8, BLACK);
      for (i = 0; i < 3; i++) FillArea(x + s / 4, y + s * 7 / 16 + i * (s / 9), s / 2, s / 32 > 1 ? s / 32 : 2, BLACK);
}
static void mod_draw(View *v) {
  switch (v->type) {
    case V_RSS: v_rss(v); break;
    case V_RSSITEM: v_rssitem(v); break;
    case V_FEEDS: v_feeds(v); break;
  }
}
static void mod_action(View *v, int a, int arg) {
  (void)v; (void)arg;
  switch (a) {
    case A_RSS_REFRESH: rss_refresh(); v->page = 0; draw(1); break;
    case A_RSS_OPEN: push(V_RSSITEM, arg, 0, 0, NULL); break;
    case A_RSS_FEEDS: push(V_FEEDS, 0, 0, 0, NULL); break;
    case A_FEED_ADD: ask("Adreça del canal RSS", "https://", feed_add); break;
    case A_FEED_DEL: feedDel = arg; confirm("Canals", "Vols esborrar aquest canal?", "Esborra", feed_del); break;
  }
}
static void mod_open(View *v) {
  (void)v;
}
static Module M = { ESC_API, "noticies", "Notícies", 0, mod_icon, mod_draw, mod_action, NULL, mod_open, 0, NULL, 1, WG };
__attribute__((visibility("default"))) Module *escriptori_module(const Host *h) { H = h; return &M; }
