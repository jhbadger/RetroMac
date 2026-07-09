/* RetroMac Toolbox shim: Dialogs.h
 * DialogPtr is WindowPtr (see Types.h -- exactly like real Inside
 * Mac). Dialogs can be built two ways: programmatically with
 * NewDialog + NewControl/TENew in creation order (Phase 1, still
 * supported for apps with no .r file), or with GetNewDialog loading a
 * real DLOG+DITL resource (Phase 2, see Toolbox.md) -- either way,
 * item numbers come from RetroMacInternal.h's nextItemNumber, just
 * driven by whichever order actually created the items.
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

/* Phase 2: unpacks a DLOG resource and its DITL (see Toolbox.md),
 * calling NewDialog + NewControl/TENew per item in real DITL order. */
DialogPtr GetNewDialog(short dialogID, void *dStorage, WindowPtr behind);

/* Blocking modal event loop. itemHit is the 1-based item number of
 * whichever control/TE field was hit -- either DITL position (Phase 2
 * dialogs) or NewControl/TENew creation order (Phase 1 programmatic
 * dialogs), see RetroMacInternal.h's itemNumber. Return/Enter resolves
 * to item 1 by convention (the first/default button), matching the
 * OK-button-first pattern Phase 0's hand-rolled About dialogs already
 * used. */
void ModalDialog(void *filterProc, short *itemHit);

/* Stores up to 4 Pascal strings substituted for ^0-^3 in a DITL
 * statText item (real ALRT/dialog behavior) -- or, for the Phase 1
 * fallback box (no ALRT resource for this alertID), the entire message
 * shown is simply param 0. */
void ParamText(ConstStr255Param param0, ConstStr255Param param1,
               ConstStr255Param param2, ConstStr255Param param3);

/* Looks up an ALRT resource by alertID and builds its real DITL-driven
 * layout; if none exists, falls back to Phase 1's fixed hand-drawn
 * icon + ParamText param0 + single OK button box (so apps with no .r
 * file keep working unchanged). filterProc is unused; always returns 1. */
short Alert(short alertID, void *filterProc);
short StopAlert(short alertID, void *filterProc);
short NoteAlert(short alertID, void *filterProc);
short CautionAlert(short alertID, void *filterProc);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_DIALOGS_H */
