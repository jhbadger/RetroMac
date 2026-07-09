/* RetroMac Toolbox shim: Windows.h */
#ifndef RETROMAC_WINDOWS_H
#define RETROMAC_WINDOWS_H

#include "Types.h"
#include "Quickdraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Window definition procIDs */
enum {
    documentProc  = 0,
    dBoxProc      = 1,
    plainDBox     = 2,
    altDBoxProc   = 3,
    noGrowDocProc = 4
};

/* FindWindow part codes -- values match Inside Macintosh */
enum {
    inDesk      = 0,
    inMenuBar   = 1,
    inSysWindow = 2,
    inContent   = 3,
    inDrag      = 4,
    inGrow      = 5,
    inGoAway    = 6,
    inZoomIn    = 7,
    inZoomOut   = 8
};

void InitWindows(void);

WindowPtr NewWindow(void *storage, const Rect *boundsRect, ConstStr255Param title,
                     Boolean visible, short procID, WindowPtr behind,
                     Boolean goAwayFlag, long refCon);

/* Phase 2: unpacks a WIND resource (see Toolbox.md) and calls NewWindow. */
WindowPtr GetNewWindow(short windowID, void *wStorage, WindowPtr behind);

void DisposeWindow(WindowPtr w);

void ShowWindow(WindowPtr w);
void HideWindow(WindowPtr w);

void SetPort(WindowPtr w);
WindowPtr FrontWindow(void);
void SelectWindow(WindowPtr w);
void DragWindow(WindowPtr w, Point startPt, const Rect *boundsRect);
short FindWindow(Point pt, WindowPtr *whichWindow);

void BeginUpdate(WindowPtr w);
void EndUpdate(WindowPtr w);
Boolean TrackGoAway(WindowPtr w, Point pt);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_WINDOWS_H */
