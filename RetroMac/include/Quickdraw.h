/* RetroMac Toolbox shim: Quickdraw.h */
#ifndef RETROMAC_QUICKDRAW_H
#define RETROMAC_QUICKDRAW_H

#include "Types.h"
#include "Controls.h"
#include "TextEdit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BitMap {
    void  *baseAddr;
    short  rowBytes;
    Rect   bounds;
} BitMap;

/* Field order matches real QDGlobals closely enough that InitGraf can
 * locate the struct base from &qd.thePort the same way real QuickDraw
 * does: by subtracting offsetof(QDGlobals, thePort). */
typedef struct QDGlobals {
    BitMap  screenBits;
    GrafPtr thePort;
} QDGlobals;

/* Per-window control/TE-field pool sizes -- public because GrafPort
 * (below) embeds these arrays inline; RetroMacInternal.h's window pool
 * sizing (RM_MAX_WINDOWS etc.) stays internal since apps never see it. */
enum {
    RM_MAX_CONTROLS  = 16,
    RM_MAX_TE_FIELDS = 4
};

/* Full GrafPort layout. Real classic apps routinely dereference
 * WindowPtr fields directly (`window->portRect` above all -- see
 * Toolbox.md), so this can't be left opaque the way MenuRecord is.
 * Only `portRect` is meaningful to app code; the rest is RetroMac's
 * own bookkeeping for the Cocoa-backed window (owned by WindowManager.c/
 * CocoaBridge.m) and control/TE-field pools (ControlManager.c/
 * TextEditManager.c) -- apps should treat everything past portRect as
 * private, exactly as real apps were expected to leave most of
 * GrafPort's other legacy fields alone.
 *
 * `buffer` is typed `void *` (really a CGContextRef) rather than
 * pulling CoreGraphics.h into this header -- classic app code never
 * touches it, and RetroMac's own .c files cast it back at each use
 * site (see RM_CurrentBuffer/RM_WindowBuffer in WindowManager.c). */
struct GrafPort {
    Rect       portRect;   /* content area in local coords: {0,0,width,height} */

    Boolean    inUse;
    void      *nsWindow;   /* NSWindow *       */
    void      *nsView;     /* RMContentView *  */
    void      *buffer;     /* CGContextRef -- offscreen content bitmap */
    int        width, height;   /* content area size (excludes titlebar) */
    Boolean    hasTitleBar;     /* documentProc: true, dBoxProc: false   */
    Boolean    hasGoAway;
    Boolean    isVisible;
    Boolean    needsDisplay;
    unsigned char title[256];   /* Pascal string */
    Point      contentOriginGlobal; /* top-left of content area, classic global coords */

    Point      pen;
    short      penH, penV;
    RGBColor   fgColor, bgColor;
    short      txFont, txSize, txFace;

    short      nextItemNumber; /* next dialog item number to assign (see TERec) */

    struct ControlRecord controls[RM_MAX_CONTROLS];
    struct TERec teFields[RM_MAX_TE_FIELDS];
};

typedef struct FontInfo {
    short ascent;
    short descent;
    short widMax;
    short leading;
} FontInfo;

/* QuickDraw character style bits (normal/bold/italic/... come from
 * the Style enum already defined by <MacTypes.h>) */

/* Font IDs */
enum {
    systemFont = 0,
    applFont   = 1
};

void InitGraf(void *globalPtr);
void InitCursor(void);

void SetRect(Rect *r, short left, short top, short right, short bottom);
void InsetRect(Rect *r, short dh, short dv);
void OffsetRect(Rect *r, short dh, short dv);
Boolean PtInRect(Point pt, const Rect *r);

void EraseRect(const Rect *r);
void FrameRect(const Rect *r);
void PaintRect(const Rect *r);
void FrameRoundRect(const Rect *r, short ovalWidth, short ovalHeight);
void PaintRoundRect(const Rect *r, short ovalWidth, short ovalHeight);
void FrameOval(const Rect *r);

void PenSize(short width, short height);
void PenNormal(void);

void MoveTo(short h, short v);
void LineTo(short h, short v);
void DrawString(ConstStr255Param s);
void DrawChar(short ch);
short StringWidth(ConstStr255Param s);
short CharWidth(short ch);
void GetFontInfo(FontInfo *info);

void TextFont(short fontID);
void TextSize(short size);
void TextFace(short face);

void RGBForeColor(const RGBColor *color);
void RGBBackColor(const RGBColor *color);
void ForeColor(long colorCode);
void BackColor(long colorCode);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_QUICKDRAW_H */
