/* RetroMac Toolbox shim: Controls.h */
#ifndef RETROMAC_CONTROLS_H
#define RETROMAC_CONTROLS_H

#include "Types.h"
#include "Quickdraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Control definition procIDs */
enum {
    pushButProc  = 0,
    checkBoxProc = 1,
    radioButProc = 2
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
