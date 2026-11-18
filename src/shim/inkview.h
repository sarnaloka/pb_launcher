/* Substitut mínim d'inkview per provar la interfície a l'ordinador. */
#ifndef SHIM_INKVIEW_H
#define SHIM_INKVIEW_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdbool.h>
typedef struct ifont_s { int size, bold, serif; char name[80]; } ifont;
typedef int (*iv_handler)(int type, int par1, int par2);
typedef void (*iv_keyboardhandler)(char *text);
typedef void (*iv_dialoghandler)(int button);
#define FLASHDIR "./flash"
#define EVT_INIT 21
#define EVT_EXIT 22
#define EVT_NET_CONNECTED 256
#define EVT_NET_DISCONNECTED 257
#define EVT_SHOW 23
#define EVT_KEYPRESS 25
#define EVT_POINTERUP 29
#define EVT_POINTERDOWN 30
#define EVT_POINTERMOVE 31
#define EVT_POINTERLONG 34
#define EVT_POINTERHOLD 35
#define EVT_POINTERDRAG 44
#define EVT_MTSYNC 39
#define IV_KEY_PREV 0x18
#define IV_KEY_NEXT 0x19
#define IV_KEY_BACK 0x1b
#define BLACK 0x000000
#define LGRAY 0xaaaaaa
#define WHITE 0xffffff
#define DGRAY 0x555555
#define NET_CONNECTED 0x0f00
#define NET_WIFI 0x0002
#define NET_WIFIREADY 0x0200
#define PANEL_DISABLED 0
void SetPanelType(int t); int PanelHeight(void);
int NetConnect(const char *name); int NetDisconnect(void);
int GetFrontlightState(void); void SetFrontlightState(int s); int GetFrontlightEnabled(void); void SetFrontlightEnabled(int e); void *QuickDownload(const char *url, int *retsize, int timeout); int TextRectHeight(int width, const char *s, int flags);
#define EVT_FOREGROUND 151
#define EVT_BACKGROUND 152
typedef void (*iv_timerproc)();
typedef void (*iv_timerprocEx)(void *context);
void SetWeakTimerEx(const char *name, iv_timerprocEx tp, void *context, int ms);
/* Multitàctil (el simulador en té un dit, amb les ordres «mt x y» i «mtup») */
typedef struct iv_mtinfo_s { bool active; int x, y, pressure; int devtype; long long timems; } iv_mtinfo;
iv_mtinfo *GetTouchInfoI(unsigned int index);
void DrawCircle(int x0, int y0, int radius, int color);
void DynamicUpdateA2(int x, int y, int w, int h); int IsInA2Update(void);
void DitherAreaPattern2Level(int dx, int dy, int dw, int dh);
char *GetDeviceModel(void); char *GetSerialNumber(void); char *GetSoftwareVersion(void);
int GetBatteryPower(void); int IsCharging(void); int QueryNetwork(void); int GetNetSignalQuality(void);
void SetHardTimer(const char *n, iv_timerproc tp, int ms);
void SetWeakTimer(const char *name, iv_timerproc tp, int ms); void ClearTimer(iv_timerproc tp); void PartialUpdate(int x, int y, int w, int h);
#define KBD_NORMAL 0
#define ICON_INFORMATION 1
#define ICON_ERROR 4
#define ICON_QUESTION 2
#define ALIGN_LEFT 1
#define ALIGN_CENTER 2
#define ALIGN_RIGHT 4
#define VALIGN_TOP 16
#define VALIGN_MIDDLE 32
#define VALIGN_BOTTOM 64
#define DOTS 512
#define DEFAULTFONT "regular"
#define DEFAULTFONTB "bold"
#define DEFAULTFONTM "mono"
#define DITHER_PATTERN 1
void InkViewMain(iv_handler h); void CloseApp(void); int ScreenWidth(void); int ScreenHeight(void);
void ClearScreen(void); void DrawLine(int, int, int, int, int); void DrawRect(int, int, int, int, int); void FillArea(int, int, int, int, int);
typedef struct ibitmap_s { unsigned short width, height, depth, scanline; unsigned char data[]; } ibitmap;
ibitmap *BitmapFromScreen(int x, int y, int w, int h); void SetClip(int x, int y, int w, int h);
void SimChecker(int x, int y, int w, int h); void DrawBitmap(int x, int y, const ibitmap *b); void DrawPixel(int x, int y, int c); void InvertArea(int x, int y, int w, int h);
char **EnumFonts(void); void CloseFont(ifont *f);
ibitmap *LoadJPEG(const char *path, int w, int h, int br, int co, int proportional); ibitmap *LoadPNGStretch(const char *path, int w, int h, int proportional, int dither);
void DitherArea(int x, int y, int w, int h, int levels, int method);
#define USERFONTDIR FLASHDIR "/system/fonts"
ifont *OpenFont(const char *name, int size, int aa); void SetFont(const ifont *f, int color);
char *DrawTextRect(int x, int y, int w, int h, const char *s, int flags); int StringWidth(const char *s);
void FullUpdate(void); void SoftUpdate(void);
void OpenKeyboard(const char *title, char *buffer, int maxlen, int flags, iv_keyboardhandler hproc);
void Message(int icon, const char *title, const char *text, int timeout);
void Dialog(int icon, const char *title, const char *text, const char *b1, const char *b2, iv_dialoghandler hproc);
void ShowHourglass(void); void HideHourglass(void); int OpenBook(const char *path, const char *parameters, int flags);
#endif
