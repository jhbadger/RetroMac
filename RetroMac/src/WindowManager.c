/* WindowManager.c -- classic Window Manager. Plain C: owns the window
 * pool and all FindWindow/DragWindow/TrackGoAway hit-testing math,
 * and calls into CocoaBridge.m (via RetroMacBridge.h) for the actual
 * NSWindow/NSView work. See RetroMacBridge.h for why this split
 * exists (AppKit's legacy QuickDraw compat headers collide with our
 * own classic-Toolbox type names).
 */
#include "RetroMacInternal.h"
#include "../include/Windows.h"
#include "../include/Resources.h"
#include <string.h>
#include <stdio.h>

struct GrafPort gWindows[RM_MAX_WINDOWS];
WindowPtr gCurrentPort = NULL;
WindowPtr gWindowStack[RM_MAX_WINDOWS];
int gWindowStackCount = 0;

/* ==== accessors called by CocoaBridge.m's RMContentView drawRect: ===== */

CGContextRef RM_WindowBuffer(void *grafPort) { return (CGContextRef)((WindowPtr)grafPort)->buffer; }
int RM_WindowInUse(void *grafPort)      { return ((WindowPtr)grafPort)->inUse ? 1 : 0; }
int RM_WindowHasTitleBar(void *grafPort) { return ((WindowPtr)grafPort)->hasTitleBar ? 1 : 0; }
int RM_WindowHasGoAway(void *grafPort)   { return ((WindowPtr)grafPort)->hasGoAway ? 1 : 0; }
int RM_WindowWidth(void *grafPort)      { return ((WindowPtr)grafPort)->width; }
int RM_WindowHeight(void *grafPort)     { return ((WindowPtr)grafPort)->height; }

void RM_WindowTitleUTF8(void *grafPort, char *out, unsigned long outSize)
{
    RM_MacRomanPStringToUTF8(((WindowPtr)grafPort)->title, out, (size_t)outSize);
}

/* ==== pool / stack management ========================================== */

CGContextRef RM_CurrentBuffer(void)
{
    return gCurrentPort ? (CGContextRef)gCurrentPort->buffer : NULL;
}

WindowPtr RM_AllocWindowSlot(void)
{
    for (int i = 0; i < RM_MAX_WINDOWS; i++) {
        if (!gWindows[i].inUse) {
            memset(&gWindows[i], 0, sizeof(gWindows[i]));
            gWindows[i].inUse = true;
            return &gWindows[i];
        }
    }
    fprintf(stderr, "RetroMac: out of window slots (RM_MAX_WINDOWS=%d)\n", RM_MAX_WINDOWS);
    return NULL;
}

void RM_RemoveFromStack(WindowPtr w)
{
    for (int i = 0; i < gWindowStackCount; i++) {
        if (gWindowStack[i] == w) {
            for (int j = i; j < gWindowStackCount - 1; j++) gWindowStack[j] = gWindowStack[j + 1];
            gWindowStackCount--;
            return;
        }
    }
}

void RM_BringToFront(WindowPtr w)
{
    RM_RemoveFromStack(w);
    if (gWindowStackCount < RM_MAX_WINDOWS) {
        for (int i = gWindowStackCount; i > 0; i--) gWindowStack[i] = gWindowStack[i - 1];
        gWindowStack[0] = w;
        gWindowStackCount++;
    }
    if (w->nsWindow) RMCocoa_OrderFront(w->nsWindow);
}

void RM_SyncContentOriginFromFrame(WindowPtr w)
{
    if (!w || !w->nsWindow) return;
    w->contentOriginGlobal = RMCocoa_WindowContentOriginClassic(w->nsWindow, w->hasTitleBar);
}

void RM_MarkDirty(WindowPtr w)
{
    if (w) w->needsDisplay = true;
}

void RM_FlushDirtyWindows(void)
{
    for (int i = 0; i < RM_MAX_WINDOWS; i++) {
        WindowPtr w = &gWindows[i];
        if (w->inUse && w->needsDisplay) {
            if (w->nsView) RMCocoa_MarkNeedsDisplayAndFlush(w->nsView);
            w->needsDisplay = false;
        }
    }
}

/* ==== Toolbox entry points ============================================== */

void InitWindows(void)
{
    gWindowStackCount = 0;
    gCurrentPort = NULL;
}

WindowPtr NewWindow(void *storage, const Rect *boundsRect, ConstStr255Param title,
                     Boolean visible, short procID, WindowPtr behind,
                     Boolean goAwayFlag, long refCon)
{
    (void)storage; (void)behind; (void)refCon;

    WindowPtr w = RM_AllocWindowSlot();
    if (!w) return NULL;

    int width  = boundsRect->right - boundsRect->left;
    int height = boundsRect->bottom - boundsRect->top;
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    w->hasTitleBar = (procID == documentProc || procID == noGrowDocProc) ? true : false;
    w->hasGoAway   = (w->hasTitleBar && goAwayFlag) ? true : false;
    w->width  = width;
    w->height = height;
    /* portRect is always local-origin, matching width/height above --
     * there's no SetOrigin/SizeWindow yet, so this is set once here
     * and never touched again. */
    SetRect(&w->portRect, 0, 0, (short)width, (short)height);
    w->penH = 1; w->penV = 1;
    w->fgColor = (RGBColor){0, 0, 0};
    w->bgColor = (RGBColor){0xFFFF, 0xFFFF, 0xFFFF};
    w->txFont = 0; w->txSize = 12; w->txFace = 0;
    w->isVisible = visible ? true : false;
    if (title) RM_PStringCopy(w->title, title); else w->title[0] = 0;
    w->contentOriginGlobal.h = boundsRect->left;
    w->contentOriginGlobal.v = boundsRect->top;

    /* The buffer's actual pixel dimensions are RM_DISPLAY_SCALE times
     * the classic width/height, but every other file (QuickDraw.c,
     * ControlManager.c, TextEditManager.c, DialogManager.c) keeps
     * drawing in plain classic units against gCurrentPort->width/height
     * -- pre-scaling the CTM here once, right at creation, is what
     * transparently maps those classic-unit CGContext/Core Text calls
     * onto the right physical pixel density, so none of them need to
     * know RM_DISPLAY_SCALE exists. See RetroMacBridge.h. */
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    w->buffer = CGBitmapContextCreate(NULL, (size_t)(width * RM_DISPLAY_SCALE),
                                       (size_t)(height * RM_DISPLAY_SCALE), 8, 0, cs,
                                       kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    CGContextScaleCTM(w->buffer, RM_DISPLAY_SCALE, RM_DISPLAY_SCALE);
    CGContextSetRGBFillColor(w->buffer, 1, 1, 1, 1);
    CGContextFillRect(w->buffer, CGRectMake(0, 0, width, height));

    void *view = NULL;
    w->nsWindow = RMCocoa_CreateWindow(*boundsRect, w->hasTitleBar ? 1 : 0, w, &view);
    w->nsView = view;

    if (visible) {
        RM_BringToFront(w);
        RM_MarkDirty(w);
    } else if (gWindowStackCount < RM_MAX_WINDOWS) {
        gWindowStack[gWindowStackCount++] = w;
    }

    return w;
}

/* WIND resource layout (confirmed by compiling one with the real
 * system Rez and hex-dumping the result -- see Toolbox.md section 12):
 * boundsRect (4 big-endian shorts: top,left,bottom,right -- matches
 * Rect's own in-memory field order, no reordering needed), procID
 * (short), visible (short, nonzero = true), goAwayFlag (short, ditto),
 * refCon (long), title (Pascal string), then an optional trailing
 * "positioning" word (a Carbon-era addition some Rez output includes)
 * that RetroMac doesn't need and ignores. */
WindowPtr GetNewWindow(short windowID, void *wStorage, WindowPtr behind)
{
    Handle h = Get1Resource('WIND', windowID);
    if (!h || !*h) {
        fprintf(stderr, "RetroMac: GetNewWindow: WIND %d not found\n", (int)windowID);
        return NULL;
    }

    const unsigned char *p = (const unsigned char *)*h;
    Rect bounds;
    bounds.top    = (short)RM_ReadU16BE(p + 0);
    bounds.left   = (short)RM_ReadU16BE(p + 2);
    bounds.bottom = (short)RM_ReadU16BE(p + 4);
    bounds.right  = (short)RM_ReadU16BE(p + 6);
    short procID    = (short)RM_ReadU16BE(p + 8);
    Boolean visible  = RM_ReadU16BE(p + 10) != 0;
    Boolean goAway   = RM_ReadU16BE(p + 12) != 0;
    long refCon      = (long)RM_ReadU32BE(p + 14);
    unsigned char title[256];
    RM_PStringCopy(title, p + 18);

    WindowPtr w = NewWindow(wStorage, &bounds, title, visible, procID, behind, goAway, refCon);
    ReleaseResource(h);
    return w;
}

void DisposeWindow(WindowPtr w)
{
    if (!w || !w->inUse) return;

    RM_RemoveFromStack(w);
    if (gCurrentPort == w) gCurrentPort = NULL;

    if (w->nsWindow || w->nsView) {
        RMCocoa_DestroyWindow(w->nsWindow, w->nsView);
        w->nsWindow = NULL;
        w->nsView = NULL;
    }
    if (w->buffer) {
        CGContextRelease(w->buffer);
        w->buffer = NULL;
    }
    w->inUse = false;
}

void ShowWindow(WindowPtr w)
{
    if (!w) return;
    w->isVisible = true;
    RM_BringToFront(w);
    RM_MarkDirty(w);
}

void HideWindow(WindowPtr w)
{
    if (!w) return;
    w->isVisible = false;
    if (w->nsWindow) RMCocoa_OrderOut(w->nsWindow);
}

void SetPort(WindowPtr w)
{
    gCurrentPort = w;
}

WindowPtr FrontWindow(void)
{
    return gWindowStackCount > 0 ? gWindowStack[0] : NULL;
}

void SelectWindow(WindowPtr w)
{
    if (!w) return;
    RM_BringToFront(w);
    RM_MarkDirty(w);
}

typedef struct {
    WindowPtr w;
    Point lastGlobal;
} RMDragCtx;

static void RM_DragOnMove(Point globalPt, void *ctxPtr)
{
    RMDragCtx *ctx = (RMDragCtx *)ctxPtr;
    double dx = (double)(globalPt.h - ctx->lastGlobal.h);
    /* classic v increases downward; screen space increases upward */
    double dy = -(double)(globalPt.v - ctx->lastGlobal.v);
    if (dx != 0 || dy != 0) {
        RMCocoa_MoveWindowByScreenDelta(ctx->w->nsWindow, dx, dy);
        ctx->lastGlobal = globalPt;
    }
}

static void RM_DragOnUp(Point globalPt, void *ctxPtr)
{
    RM_DragOnMove(globalPt, ctxPtr);
}

void DragWindow(WindowPtr w, Point startPt, const Rect *boundsRect)
{
    (void)boundsRect;
    if (!w || !w->nsWindow) return;

    RMDragCtx ctx;
    ctx.w = w;
    ctx.lastGlobal = startPt;
    RMCocoa_TrackMouse(RM_DragOnMove, RM_DragOnUp, &ctx);

    RM_SyncContentOriginFromFrame(w);
    RM_MarkDirty(w);
}

short FindWindow(Point pt, WindowPtr *whichWindow)
{
    if (whichWindow) *whichWindow = NULL;
    if (pt.v < RM_MENUBAR_HEIGHT) return inMenuBar;

    for (int i = 0; i < gWindowStackCount; i++) {
        WindowPtr w = gWindowStack[i];
        if (!w->isVisible) continue;

        int titleBarH = w->hasTitleBar ? RM_TITLEBAR_HEIGHT : 0;
        short structTop    = (short)(w->contentOriginGlobal.v - titleBarH);
        short structLeft   = w->contentOriginGlobal.h;
        short structBottom = (short)(w->contentOriginGlobal.v + w->height);
        short structRight  = (short)(w->contentOriginGlobal.h + w->width);

        if (pt.h < structLeft || pt.h >= structRight || pt.v < structTop || pt.v >= structBottom)
            continue;

        if (whichWindow) *whichWindow = w;

        if (w->hasTitleBar && pt.v < w->contentOriginGlobal.v) {
            if (w->hasGoAway) {
                short boxLeft = (short)(structLeft + 6);
                short boxTop  = (short)(structTop + (RM_TITLEBAR_HEIGHT - 12) / 2);
                if (pt.h >= boxLeft && pt.h < boxLeft + 12 && pt.v >= boxTop && pt.v < boxTop + 12)
                    return inGoAway;
            }
            return inDrag;
        }
        return inContent;
    }
    return inDesk;
}

void BeginUpdate(WindowPtr w)
{
    SetPort(w);
}

void EndUpdate(WindowPtr w)
{
    RM_MarkDirty(w);
}

typedef struct {
    Rect globalBox;
    Boolean stillInside;
} RMGoAwayCtx;

static void RM_GoAwayOnUp(Point globalPt, void *ctxPtr)
{
    RMGoAwayCtx *ctx = (RMGoAwayCtx *)ctxPtr;
    ctx->stillInside = PtInRect(globalPt, &ctx->globalBox) ? true : false;
}

Boolean TrackGoAway(WindowPtr w, Point pt)
{
    (void)pt;
    if (!w) return false;

    int titleBarH = w->hasTitleBar ? RM_TITLEBAR_HEIGHT : 0;
    short structTop  = (short)(w->contentOriginGlobal.v - titleBarH);
    short structLeft = w->contentOriginGlobal.h;

    RMGoAwayCtx ctx;
    ctx.stillInside = false;
    SetRect(&ctx.globalBox, (short)(structLeft + 6), (short)(structTop + (RM_TITLEBAR_HEIGHT - 12) / 2),
            (short)(structLeft + 6 + 12), (short)(structTop + (RM_TITLEBAR_HEIGHT - 12) / 2 + 12));

    RMCocoa_TrackMouse(NULL, RM_GoAwayOnUp, &ctx);
    return ctx.stillInside;
}
