/* RetroMac Toolbox shim: Controls.h */
#ifndef RETROMAC_CONTROLS_H
#define RETROMAC_CONTROLS_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Control definition procIDs */
enum {
    pushButProc  = 0,
    checkBoxProc = 1,
    radioButProc = 2
};

/* Full ControlRecord layout -- lives here (not RetroMacInternal.h) so
 * that Quickdraw.h can embed `struct ControlRecord controls[]` inline
 * inside GrafPort (see Quickdraw.h) without a header cycle: Quickdraw.h
 * includes this file, so this file must not include Quickdraw.h back.
 * Classic apps only ever carry ControlHandles around opaquely, exactly
 * like real Inside Mac code, but the fields need to be visible here for
 * GrafPort's inline array to have a complete element type. */
struct ControlRecord {
    Boolean   inUse;
    WindowPtr owner;
    Rect      bounds;      /* local to owner's content area */
    unsigned char title[256];
    short     value, min, max;
    Boolean   visible;
    Boolean   hilited;     /* true while pressed/tracking   */
    short     procID;
    short     itemNumber;  /* dialog item number -- see TERec's itemNumber */
};

/* FindControl / TrackControl part codes -- values match Inside Macintosh */
enum {
    inButton     = 10,
    inCheckBox   = 11,
    inUpButton   = 20,
    inDownButton = 21,
    inPageUp     = 22,
    inPageDown   = 23,
    inThumb      = 129
};

ControlHandle NewControl(WindowPtr owner, const Rect *boundsRect, ConstStr255Param title,
                          Boolean visible, short value, short min, short max,
                          short procID, long refCon);
void DisposeControl(ControlHandle c);

short FindControl(Point pt, WindowPtr w, ControlHandle *control);
short TrackControl(ControlHandle c, Point startPt, void *actionProc);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_CONTROLS_H */
