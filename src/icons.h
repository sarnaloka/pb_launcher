/* Escriptori — icones de traç, les mateixes del prototip de disseny (viewBox 24 × 24, traç rodó).
   Cada icona és un camí SVG; ic_path() (a gfx.h) el dibuixa a qualsevol mida amb les vores suavitzades.
   Per afegir-ne una: dibuixa-la a 24 × 24 amb només traços (sense farciment) i copia aquí l'atribut «d».
   Ordres admeses: M L H V C S Q T A Z (majúscules i minúscules). Un segment de longitud gairebé zero
   («h.01») fa un punt rodó. */
#ifndef ESC_ICONS_H
#define ESC_ICONS_H

/* aplicacions */
#define IC_AGENDA      "M6 5h12a2 2 0 0 1 2 2v11a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V7a2 2 0 0 1 2-2zM4 10h16M8 3v4M16 3v4"
#define IC_BUS         "M7 4h10a2 2 0 0 1 2 2v11H5V6a2 2 0 0 1 2-2zM5 11h14M8 17v3M16 17v3M8.5 14h.01M15.5 14h.01"
#define IC_MINES       "M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8zM12 3v3M12 18v3M3 12h3M18 12h3M6 6l2 2M16 16l2 2M6 18l2-2M16 8l2-2"
#define IC_CALC        "M8 3h8a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2zM9 7h6M9 12h.01M12 12h.01M15 12h.01M9 16h.01M12 16h.01M15 16h.01"
#define IC_CONVERT     "M5 9h14l-3-3M19 15H5l3 3"
#define IC_SUN         "M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8zM12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6l1.4 1.4M17 17l1.4 1.4M5.6 18.4l1.4-1.4M17 7l1.4-1.4"
#define IC_FOLDER      "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"
#define IC_NOTE        "M6 3h9l4 4v14H6zM15 3v4h4M9 12h7M9 16h7"
#define IC_NEWS        "M6 5h12a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V7a2 2 0 0 1 2-2zM8 9h8M8 13h3M8 16h3M14 13h2v3h-2z"
#define IC_CLOCK       "M12 4a8 8 0 1 0 0 16 8 8 0 0 0 0-16zM12 8v4l3 2"
#define IC_SUDOKU      "M6 4h12a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2zM9.3 4v16M14.7 4v16M4 9.3h16M4 14.7h16"
#define IC_CHAT        "M5 6h14a2 2 0 0 1 2 2v7a2 2 0 0 1-2 2h-8l-4 3v-3H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2z"
#define IC_FTP         "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2zM10 11v5M8 13l2-2 2 2M14 11v5M12 14l2 2 2-2"
#define IC_BOOK        "M12 6v14M12 6c-2-1.5-5-2-8-2v14c3 0 6 .5 8 2M12 6c2-1.5 5-2 8-2v14c-3 0-6 .5-8 2"
#define IC_TASKS       "M4 6a2 2 0 0 1 2-2h12a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2zM8 12l3 3 5-6"
#define IC_PACKAGE     "M12 3l8 4.5v9L12 21l-8-4.5v-9zM12 12l8-4.5M12 12v9M12 12L4 7.5"
#define IC_UPDOWN      "M8 18V6M5 9l3-3 3 3M16 6v12M13 15l3 3 3-3"
#define IC_CLOSE       "M6 6l12 12M18 6L6 18"
#define IC_APPS        "M9 9h10v10H9zM5 15V5h10"
#define IC_SPLIT       "M5 4h14a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1zM5 13h14a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1v-5a1 1 0 0 1 1-1z"
#define IC_PULSE       "M3 12h4l3-7 4 14 3-7h4"
#define IC_SLIDERS     "M5 7h8M17 7h2M5 17h2M11 17h8M15 5v4M9 15v4"

/* sistema */
#define IC_BELL        "M6 16v-5a6 6 0 0 1 12 0v5l1.5 2h-15zM10 20.5a2 2 0 0 0 4 0"
#define IC_BACK        "M15 5l-7 7 7 7"
#define IC_FORWARD     "M9 5l7 7-7 7"
#define IC_UP          "M5 15l7-7 7 7"
#define IC_DOWN        "M5 9l7 7 7-7"
#define IC_HOME        "M4 11l8-7 8 7v8a1 1 0 0 1-1 1h-4v-6H9v6H5a1 1 0 0 1-1-1z"
#define IC_POWER       "M12 4v8M7 7a7 7 0 1 0 10 0"
#define IC_INFO        "M12 4a8 8 0 1 0 0 16 8 8 0 0 0 0-16zM12 11v5M12 8h.01"
#define IC_DOWNLOAD    "M12 4v11M7 10l5 5 5-5M5 20h14"
#define IC_REFRESH     "M19 12a7 7 0 1 1-2.1-5M19 4v4h-4"
#define IC_PLUS        "M6 12h12M12 6v12"
#define IC_MINUS       "M6 12h12"
#define IC_GRID        "M5 5h5v5H5zM14 5h5v5h-5zM5 14h5v5H5zM14 14h5v5h-5z"
#define IC_CHECK       "M5 12.5l4.5 4.5L19 7.5"
#define IC_STAR        "M12 4l2.4 5 5.6.8-4 3.9.9 5.5-4.9-2.6-4.9 2.6.9-5.5-4-3.9 5.6-.8z"
#define IC_BARS        "M4 20v-3M9 20v-7M14 20V9M19 20V5"
#define IC_MOON        "M19 14.5A7.5 7.5 0 0 1 9.5 5a7.5 7.5 0 1 0 9.5 9.5z"
#define IC_IMAGE       "M5 5h14a1 1 0 0 1 1 1v12a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1zM4 16l5-5 4 4 2-2 5 5M15.5 9.5h.01"
#define IC_TYPE        "M5 7V5h14v2M12 5v14M9 19h6"
#define IC_CONTRAST    "M12 4a8 8 0 1 0 0 16 8 8 0 0 0 0-16zM12 4v16"
#define IC_TEXTSIZE    "M3 19l5-13 5 13M4.8 14h6.4M15 19l3-8 3 8M16 16h4"

/* el temps: núvol gran, núvol petit (a dalt, per posar-hi pluja o neu a sota) i detalls */
#define IC_WX_CLOUD    "M7 18a4 4 0 0 1-.6-7.96A6 6 0 0 1 17.6 9.1 4.5 4.5 0 0 1 17 18z"
#define IC_WX_CLOUDS   "M7.75 13.4a3.4 3.4 0 0 1-.51-6.77A5.1 5.1 0 0 1 16.76 5.83 3.83 3.83 0 0 1 16.25 13.4z"
#define IC_WX_PARTLY   "M5.37 9.46A2.8 2.8 0 1 1 10.42 7.1M8 4.2V2.7M3.7 8.5H2.2M5 5.5L3.9 4.4M11 5.5l1.1-1.1M9 20a3.5 3.5 0 0 1-.5-6.96A5 5 0 0 1 18.3 12.3 3.9 3.9 0 0 1 18 20z"
#define IC_WX_PARTLYN  "M7 3a3 3 0 0 0 4 4 4 4 0 1 1-4-4zM9 20a3.5 3.5 0 0 1-.5-6.96A5 5 0 0 1 18.3 12.3 3.9 3.9 0 0 1 18 20z"
#define IC_WX_FOG      IC_WX_CLOUDS "M5 17.5h14M7 20.5h10"
#define IC_WX_DRIZZLE  IC_WX_CLOUDS "M8 17.5h.01M12 17.5h.01M16 17.5h.01M10 20.5h.01M14 20.5h.01"
#define IC_WX_RAIN     IC_WX_CLOUDS "M8.5 16.5l-1.5 4M12.5 16.5l-1.5 4M16.5 16.5l-1.5 4"
#define IC_WX_SNOW     IC_WX_CLOUDS "M7.8 15.8v3.6M6.24 16.7l3.12 1.8M6.24 18.5l3.12 -1.8M16.2 15.8v3.6M14.64 16.7l3.12 1.8M14.64 18.5l3.12 -1.8M12 18.6v3.6M10.44 19.5l3.12 1.8M10.44 21.3l3.12 -1.8"
#define IC_WX_STORM    IC_WX_CLOUDS "M12.5 15l-2.5 3.5h3.5L11 22"
#define IC_WX_THERMO   "M10 13.5V5a2 2 0 0 1 4 0v8.5a4 4 0 1 1-4 0zM12 9v7"
#define IC_WX_DROP     "M12 3.5c3 4 5.5 7 5.5 10a5.5 5.5 0 0 1-11 0c0-3 2.5-6 5.5-10z"
#define IC_WX_WIND     "M3 9h10a3 3 0 1 0-3-3M3 15h14a3 3 0 1 1-3 3M3 12h6"
#define IC_WX_GAUGE    "M5.6 18.4A9 9 0 1 1 18.4 18.4M12 12l4-3.5"
#define IC_WX_EYE      "M2.5 12s3.5-6 9.5-6 9.5 6 9.5 6-3.5 6-9.5 6-9.5-6-9.5-6zM12 9.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z"
#define IC_WX_SUNSET   "M3 18h18M7 18a5 5 0 0 1 10 0M12 4v6M9.5 7.5L12 10l2.5-2.5M4.5 12.5l1.5 1M19.5 12.5l-1.5 1"
#define IC_WX_LIST     "M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"


/* barra d'estat i tauler ràpid (estil Kobo / Kindle, dibuixats per a la 2.3) */
#define IC_SB_SUN      "M12 8.5a3.5 3.5 0 1 0 0 7 3.5 3.5 0 0 0 0-7zM12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"
#define IC_WIFI_A3     "M3.5 10.5a12 12 0 0 1 17 0"
#define IC_WIFI_A2     "M6.3 13.3a8 8 0 0 1 11.4 0"
#define IC_WIFI_A1     "M9.2 16.2a4 4 0 0 1 5.6 0M12 19.5h.01"
#define IC_WIFI        IC_WIFI_A3 IC_WIFI_A2 IC_WIFI_A1
#define IC_WIFI_X      "M2 2.5l4 4M6 2.5l-4 4"
#define IC_BATT_H      "M3.1 7h15.8a.6.6 0 0 1 .6.6v8.8a.6.6 0 0 1-.6.6H3.1a.6.6 0 0 1-.6-.6V7.6a.6.6 0 0 1 .6-.6z"
#define IC_BATT_TIP    "M20.2 10h1.6v4h-1.6z"
#define IC_BOLT        "M12 8.5l-2.5 3.5h4L11 15.5"
#define IC_SYNC        "M19.5 10a7.7 7.7 0 0 0-14-2.5M4.5 14a7.7 7.7 0 0 0 14 2.5M5.5 3.5v4h4M18.5 20.5v-4h-4"
#define IC_SEARCH      "M10.5 4.5a6 6 0 1 0 0 12 6 6 0 0 0 0-12zM15 15l5 5"
#define IC_DOT         "M18 3.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z"
#define IC_CONTRAST_F  "M12 4a8 8 0 0 1 0 16z"
#define IC_DISC15      "M12-3a15 15 0 1 0 0 30a15 15 0 1 0 0-30z"   /* cercle de fons de 30 (amb marge 3) */
#define IC_DOOM        "M12 4a8 8 0 1 0 0 16 8 8 0 0 0 0-16zM12 2v5M12 17v5M2 12h5M17 12h5M12 11.5v1"   /* punt de mira (Doom) */
#define IC_CLEAN       "M5 5h14v14H5zM9 15l6-6M12 16l4-4M8 12l4-4"
#endif
