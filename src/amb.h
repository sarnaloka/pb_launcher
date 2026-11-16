/* Bus AMB per a PocketBook — lògica portable (dades, calendari, sortides, horari, PDF). */
#ifndef AMB_H
#define AMB_H
#include <stdint.h>
#include <time.h>

typedef struct {
  int nR, nS, nSvc, nEx, nT, nRows, nHeads, made;
  const uint8_t *svcDays; const int32_t *svcStart, *svcEnd, *ex;
  const uint16_t *tRoute, *tSvc, *tHead, *tLen, *sStop, *sDelta, *srList, *routeOrder;
  const uint8_t *tDir; const int32_t *tStart, *tT0, *srStart;
  const char **rShort, **rLong, **rColor, **sCode, **sName, **sQ, **heads;
  int32_t *sTime, *rowTrip, *stopStart, *stopRows;
} DB;
extern DB db;

typedef struct { time_t when; int trip, row, route; } Dep;
typedef struct { int32_t last, next, prev; } PatKey;
typedef struct { int stop, route, n, nkeys; PatKey *keys; const char *dest; } Group;
typedef struct { int time; char note; } TtDep;
typedef struct { int y, m, d; TtDep *deps; int n; } TtCol;
typedef struct { char letter; int stop; char text[160]; } TtNote;
typedef struct { const char *name; int min; } TtStrip;
typedef struct {
  int stop, route, empty, hMin, hMax, nNotes, nStrip;
  const char *dest; TtCol cols[3]; TtNote notes[26]; TtStrip *strip;
} Timetable;

int  amb_load(const uint8_t *blob, long size);            /* 0 = correcte */
void amb_fold(const char *utf8, char *out, int outsz);    /* minúscules sense accents, per a la cerca */
const char *amb_trip_dest(int trip);
int  amb_departures(int stop, time_t now, time_t from, time_t to, Dep **out);   /* retorna n; cal free(*out) */
int  amb_lines_at_stop(int stop, Group **out);            /* retorna n; cal amb_free_groups */
void amb_free_groups(Group *g, int n);
void amb_timetable(const Group *g, time_t ref, Timetable *tt);
void amb_free_timetable(Timetable *tt);
int  amb_write_pdf(const char *path, const Timetable *tt, time_t generated);     /* 0 = correcte */
const char *amb_route_label(int route);
#endif
