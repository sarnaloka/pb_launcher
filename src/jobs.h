/* Escriptori — feines de xarxa en segon pla.
   Hi ha un fil de treball que baixa les feines de la cua una rere l'altra amb libcurl, que es carrega en temps
   d'execució (dlopen): si el lector no la té, o si falla el certificat HTTPS, la feina es fa com abans, amb
   QuickDownload al fil principal (bloqueja la pantalla uns segons). El fil de treball no toca mai la pantalla: el
   temporitzador jobs_tick, al fil principal, actualitza la barra d'estat i crida les funcions de final de cada feina.
   Al simulador (ESC_SIM) el fil fa servir el QuickDownload del simulador; ASYNC=1 activa el fil i SIMNETMS=n hi afegeix
   n mil·lisegons de retard per veure el progrés. Sense ASYNC, les feines es fan al moment (com abans), cosa que manté
   vàlids els guions de prova antics. */
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>

enum { J_FREE, J_QUEUED, J_RUNNING, J_DONE, J_ERROR, J_CANCEL };
typedef struct Job {
  int state, sync, seq, timeout;
  char title[80], owner[40], url[600];
  long done, total;
  char *data; int len;
  volatile int cancel;
  void (*cb)(struct Job *j);              /* final, al fil principal (data NULL si ha fallat) */
  void (*pcb)(void *ctx, char *data, int len); void *pctx;   /* final per als paquets (Host.fetch) */
  int arg;
} Job;
#define MAXJOBS 48
static Job jobs[MAXJOBS];
static int jobSeq, jobsAsync = -1, jobsTimer;
static pthread_mutex_t jm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t jc = PTHREAD_COND_INITIALIZER;
static void draw(int full);

/* ── libcurl, carregada en temps d'execució ── */
#ifndef ESC_SIM
#define C_URL 10002
#define C_WRITEFUNCTION 20011
#define C_WRITEDATA 10001
#define C_FOLLOWLOCATION 52
#define C_TIMEOUT 13
#define C_CONNECTTIMEOUT 78
#define C_NOPROGRESS 43
#define C_PROGRESSFUNCTION 20056
#define C_PROGRESSDATA 10057
#define C_NOSIGNAL 99
#define C_USERAGENT 10018
#define C_INFO_RESPONSE_CODE 0x200002
static void *(*c_init)(void); static int (*c_setopt)(void *, int, ...); static int (*c_perform)(void *);
static void (*c_cleanup)(void *); static int (*c_getinfo)(void *, int, ...);
static int curl_load(void) {
  static int st = 0;                      /* 0 sense provar, 1 carregada, -1 no disponible */
  void *lib; int (*gi)(long);
  if (st) return st > 0;
  st = -1;
  lib = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
  if (!lib) lib = dlopen("libcurl.so", RTLD_NOW | RTLD_LOCAL);
  if (!lib) return 0;
  c_init = (void *(*)(void))dlsym(lib, "curl_easy_init"); c_setopt = (int (*)(void *, int, ...))dlsym(lib, "curl_easy_setopt");
  c_perform = (int (*)(void *))dlsym(lib, "curl_easy_perform"); c_cleanup = (void (*)(void *))dlsym(lib, "curl_easy_cleanup");
  c_getinfo = (int (*)(void *, int, ...))dlsym(lib, "curl_easy_getinfo"); gi = (int (*)(long))dlsym(lib, "curl_global_init");
  if (!c_init || !c_setopt || !c_perform || !c_cleanup || !c_getinfo || !gi || gi(3) != 0) return 0;
  st = 1;
  return 1;
}
typedef struct { Job *j; char *b; size_t n, cap; } CBuf;
static size_t cw(char *p, size_t sz, size_t nm, void *ud) {
  CBuf *c = (CBuf *)ud; size_t k = sz * nm;
  if (c->j->cancel) return 0;
  if (c->n + k + 1 > c->cap) { size_t nc = (c->cap ? c->cap * 2 : 65536); char *nb; while (nc < c->n + k + 1) nc *= 2; nb = (char *)realloc(c->b, nc); if (!nb) return 0; c->b = nb; c->cap = nc; }
  memcpy(c->b + c->n, p, k); c->n += k; c->b[c->n] = 0;
  return k;
}
static int cprog(void *ud, double dt, double dn, double ut, double un) {
  Job *j = (Job *)ud; (void)ut; (void)un;
  pthread_mutex_lock(&jm); j->total = (long)dt; j->done = (long)dn; pthread_mutex_unlock(&jm);
  return j->cancel ? 1 : 0;
}
#endif
/* Baixa la feina al fil de treball. 0 bé; -1 error; -2 cal fer-ho al fil principal (certificat o curl no disponible). */
static int job_fetch(Job *j) {
#ifdef ESC_SIM
  int size = 0, k, ms = getenv("SIMNETMS") ? atoi(getenv("SIMNETMS")) : 0; void *b = QuickDownload(j->url, &size, j->timeout);
  for (k = 1; k <= 10 && ms > 0 && !j->cancel; k++) { usleep((useconds_t)ms * 100); pthread_mutex_lock(&jm); j->total = size; j->done = (long)size * k / 10; pthread_mutex_unlock(&jm); }
  if (!b || size <= 0 || j->cancel) { free(b); return -1; }
  { char *nb = (char *)realloc(b, (size_t)size + 1); if (!nb) { free(b); return -1; } j->data = nb; }
  j->data[size] = 0; j->len = size;
  return 0;
#else
  void *h; CBuf c; int rc; long code = 0;
  if (!curl_load() || !(h = c_init())) return -2;
  memset(&c, 0, sizeof(c)); c.j = j;
  c_setopt(h, C_URL, j->url); c_setopt(h, C_WRITEFUNCTION, cw); c_setopt(h, C_WRITEDATA, &c);
  c_setopt(h, C_FOLLOWLOCATION, 1L); c_setopt(h, C_TIMEOUT, (long)j->timeout); c_setopt(h, C_CONNECTTIMEOUT, 20L);
  c_setopt(h, C_NOPROGRESS, 0L); c_setopt(h, C_PROGRESSFUNCTION, cprog); c_setopt(h, C_PROGRESSDATA, j);
  c_setopt(h, C_NOSIGNAL, 1L); c_setopt(h, C_USERAGENT, "Escriptori-PocketBook");
  rc = c_perform(h);
  c_getinfo(h, C_INFO_RESPONSE_CODE, &code);
  c_cleanup(h);
  if (rc == 35 || rc == 51 || rc == 58 || rc == 59 || rc == 60 || rc == 77 || rc == 83) { free(c.b); return -2; }   /* SSL o certificats */
  if (rc != 0 || code != 200 || !c.b) { free(c.b); return -1; }
  j->data = c.b; j->len = (int)c.n;
  return 0;
#endif
}
static void *jworker(void *arg) {
  (void)arg;
  for (;;) {
    Job *j = NULL; int i, best = 0, rc;
    pthread_mutex_lock(&jm);
    for (;;) {
      for (i = 0; i < MAXJOBS; i++) if (jobs[i].state == J_QUEUED && !jobs[i].sync && (!j || jobs[i].seq < best)) { j = &jobs[i]; best = j->seq; }
      if (j) break;
      pthread_cond_wait(&jc, &jm);
    }
    j->state = J_RUNNING; j->done = 0; j->total = 0;
    pthread_mutex_unlock(&jm);
    rc = job_fetch(j);
    pthread_mutex_lock(&jm);
    if (j->cancel) { free(j->data); j->data = NULL; j->state = J_CANCEL; }
    else if (rc == -2) { j->sync = 1; j->state = J_QUEUED; jobsAsync = 0; }   /* a partir d'ara, tot al fil principal */
    else j->state = rc == 0 ? J_DONE : J_ERROR;
    pthread_mutex_unlock(&jm);
  }
  return NULL;
}
static int jobs_use_thread(void) {
  if (jobsAsync < 0) {
    pthread_t t;
#ifdef ESC_SIM
    jobsAsync = getenv("ASYNC") != NULL;
#else
    jobsAsync = curl_load();
#endif
    if (jobsAsync && pthread_create(&t, NULL, jworker, NULL) != 0) jobsAsync = 0;
    if (jobsAsync) pthread_detach(t);
  }
  return jobsAsync > 0;
}
/* Progrés per a la barra d'estat: -1 res; 0..100 la feina en curs; 101 n'hi ha a la cua però cap no informa. */
static int jobs_progress(void) {
  int i, p = -1;
  pthread_mutex_lock(&jm);
  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].state == J_RUNNING) { p = jobs[i].total > 0 ? (int)(jobs[i].done * 100 / jobs[i].total) : 101; break; }
    if (jobs[i].state == J_QUEUED && p < 0) p = 101;
  }
  pthread_mutex_unlock(&jm);
  return p;
}
static int jobs_count(int st) { int i, n = 0; pthread_mutex_lock(&jm); for (i = 0; i < MAXJOBS; i++) if (jobs[i].state == st) n++; pthread_mutex_unlock(&jm); return n; }
static void job_finish(Job *j) {               /* fil principal */
  char *d = j->state == J_DONE ? j->data : NULL; int len = j->len;
  if (j->state != J_CANCEL) {
    if (j->cb) j->cb(j);
    else if (j->pcb) j->pcb(j->pctx, d, d ? len : 0);
  }
  pthread_mutex_lock(&jm); free(j->data); memset(j, 0, sizeof(*j)); pthread_mutex_unlock(&jm);
}
/* Redibuixa després d'una feina: només si es veu una pantalla del sistema i no hi ha cap diàleg obert. */
static void jobs_redraw(void) { if (uiModal) return; if (depth > 0 && stack[depth - 1].mod < 0) draw(0); else sb_refresh(); }
static void jobs_tick(void);
static void jobs_arm(void) { if (!jobsTimer) { jobsTimer = 1; SetWeakTimer("escriptori_jobs", jobs_tick, 400); } }
static int lastProg = -2;
static void jobs_tick(void) {
  int i, any = 0, fin = 0, p;
  jobsTimer = 0;
  for (i = 0; i < MAXJOBS; i++) {
    int st;
    pthread_mutex_lock(&jm); st = jobs[i].state; pthread_mutex_unlock(&jm);
    if (st == J_DONE || st == J_ERROR || st == J_CANCEL) { job_finish(&jobs[i]); fin++; }
  }
  /* sense fil (o després d'un error de certificat): una feina per tic, al fil principal */
  for (i = 0; i < MAXJOBS; i++) if (jobs[i].state == J_QUEUED && jobs[i].sync) {
    int len = 0; char *b;
    jobs[i].state = J_RUNNING;
    b = net_get_t(jobs[i].url, &len, jobs[i].timeout);
    net_activity(0);
    jobs[i].data = b; jobs[i].len = len; jobs[i].state = b ? J_DONE : J_ERROR;
    job_finish(&jobs[i]); fin++;
    break;
  }
  for (i = 0; i < MAXJOBS; i++) if (jobs[i].state != J_FREE) any = 1;
  p = jobs_progress();
  if (fin) jobs_redraw();
  else if (p / 10 != lastProg / 10 || (p < 0) != (lastProg < 0)) sb_refresh();
  lastProg = p;
  if (any) jobs_arm();
}
/* Posa una feina a la cua. Retorna l'índex o -1. Sense fil, la fa al moment (com abans de la 2.3). */
static int jobs_add(const char *url, const char *title, const char *owner, int timeout, void (*cb)(Job *), int arg, void (*pcb)(void *, char *, int), void *pctx) {
  int i;
  if (!(QueryNetwork() & NET_CONNECTED)) NetConnect(NULL);
  if (!(QueryNetwork() & NET_CONNECTED)) {               /* sense xarxa: falla de seguida */
    Job tmp; memset(&tmp, 0, sizeof(tmp)); tmp.state = J_ERROR; tmp.cb = cb; tmp.arg = arg;
    snprintf(tmp.title, sizeof(tmp.title), "%s", title); if (cb) cb(&tmp); else if (pcb) pcb(pctx, NULL, 0);
    return -1;
  }
  pthread_mutex_lock(&jm);
  for (i = 0; i < MAXJOBS && jobs[i].state != J_FREE; i++) {}
  if (i == MAXJOBS) { pthread_mutex_unlock(&jm); if (cb) { Job t2; memset(&t2, 0, sizeof(t2)); t2.state = J_ERROR; t2.arg = arg; cb(&t2); } else if (pcb) pcb(pctx, NULL, 0); return -2; }
  memset(&jobs[i], 0, sizeof(Job));
  snprintf(jobs[i].url, sizeof(jobs[i].url), "%s", url); snprintf(jobs[i].title, sizeof(jobs[i].title), "%s", title);
  snprintf(jobs[i].owner, sizeof(jobs[i].owner), "%s", owner ? owner : "");
  jobs[i].timeout = timeout; jobs[i].cb = cb; jobs[i].arg = arg; jobs[i].pcb = pcb; jobs[i].pctx = pctx; jobs[i].seq = ++jobSeq;
  jobs[i].state = J_QUEUED;
  pthread_mutex_unlock(&jm);
  if (!jobs_use_thread()) {                              /* al moment, com abans */
    int len = 0; char *b = net_get_t(url, &len, timeout);
    net_activity(0);
    jobs[i].state = J_RUNNING; jobs[i].data = b; jobs[i].len = len; jobs[i].state = b ? J_DONE : J_ERROR;
    job_finish(&jobs[i]);
    return i;
  }
  pthread_mutex_lock(&jm); pthread_cond_signal(&jc); pthread_mutex_unlock(&jm);
  sb_refresh();
  jobs_arm();
  return i;
}
static void jobs_cancel(int i) {
  if (i < 0 || i >= MAXJOBS) return;
  pthread_mutex_lock(&jm);
  if (jobs[i].state == J_QUEUED) jobs[i].state = J_CANCEL;
  else if (jobs[i].state == J_RUNNING) jobs[i].cancel = 1;
  pthread_mutex_unlock(&jm);
  jobs_arm();
}
/* El paquet id es descarrega: les seves feines ja no han de cridar codi seu. */
static void jobs_drop_owner(const char *id) {
  int i;
  pthread_mutex_lock(&jm);
  for (i = 0; i < MAXJOBS; i++) if (jobs[i].state != J_FREE && !strcmp(jobs[i].owner, id)) { jobs[i].pcb = NULL; jobs[i].cb = NULL; if (jobs[i].state == J_QUEUED) jobs[i].state = J_CANCEL; else if (jobs[i].state == J_RUNNING) jobs[i].cancel = 1; }
  pthread_mutex_unlock(&jm);
}
