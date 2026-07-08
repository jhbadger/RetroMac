/* RetroMac Toolbox shim: Dialogs.h
 * Phase 1: a real, DITL-less Dialog Manager. DialogPtr is WindowPtr
 * (see Types.h -- exactly like real Inside Mac); dialogs are built
 * programmatically with NewDialog + NewControl/TENew rather than by
 * loading a DLOG/DITL resource, since there is no Resource Manager yet
 * (Toolbox.md Phase 2). GetNewDialog is therefore a documented stub --
 * see Toolbox.md section 10 for the tradeoff.
 */
#ifndef RETROMAC_DIALOGS_H
#define RETROMAC_DIALOGS_H

#include "Types.h"
#include "Windows.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitDialogs(void *resumeProc);

DialogPtr NewDialog(void *dStorage, const Rect *boundsRect, ConstStr255Param title,
                     Boolean visible, short procID, WindowPtr behind,
                     Boolean goAwayFlag, long refCon, void *items);

/* Stub: no Resource Manager exists yet to look up dialogID's DLOG/DITL
 * (Phase 2 work). Logs once to stderr and returns NULL -- use
 * NewDialog instead. */
DialogPtr GetNewDialog(short dialogID, void *dStorage, WindowPtr behind);

/* Blocking modal event loop. itemHit is the 1-based creation order of
 * whichever NewControl/TENew item was hit (see RetroMacInternal.h's
 * itemNumber) -- there's no DITL to assign real item numbers from.
 * Return/Enter resolves to item 1 by convention (the first control
 * created), matching the OK-button-first pattern Phase 0's hand-rolled
 * About dialogs already use. */
void ModalDialog(void *filterProc, short *itemHit);

/* Stores up to 4 Pascal strings substituted for ^0-^3 in real ALRT
 * text; Phase 1's Alert family has no ALRT resource to substitute
 * into, so the message shown is simply param 0 (mirroring how real
 * ALRT templates are very often just "^0"). */
void ParamText(ConstStr255Param param0, ConstStr255Param param1,
               ConstStr255Param param2, ConstStr255Param param3);

/* alertID/filterProc are unused (no ALRT resource to look up); each
 * builds a small programmatic dialog showing ParamText's param0 with a
 * hand-drawn icon and a single OK button, runs ModalDialog, and always
 * returns 1 (ok) -- see Toolbox.md section 10. */
short Alert(short alertID, void *filterProc);
short StopAlert(short alertID, void *filterProc);
short NoteAlert(short alertID, void *filterProc);
short CautionAlert(short alertID, void *filterProc);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_DIALOGS_H */
