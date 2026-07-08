/* RetroMac Toolbox shim: TextEdit.h
 * Phase 1: real editable text fields (TENew/TEClick/TEKey/...), drawn
 * by hand into the owning window's offscreen buffer using Core Text
 * for layout/hit-testing -- consistent with the rest of RetroMac's
 * "reimplement the Toolbox trap" approach (Toolbox.md section 2).
 * There is no auto word-wrap/multi-line layout model (TECalText is a
 * no-op beyond a redraw) -- text just flows on one line; see Toolbox.md
 * section 10 for why.
 */
#ifndef RETROMAC_TEXTEDIT_H
#define RETROMAC_TEXTEDIT_H

#include "Types.h"
#include "Memory.h"

#ifdef __cplusplus
extern "C" {
#endif

void TEInit(void);

TEHandle TENew(const Rect *destRect, const Rect *viewRect);
void TEDispose(TEHandle hTE);

void TESetText(const void *text, long length, TEHandle hTE);
Handle TEGetText(TEHandle hTE);

void TEClick(Point pt, Boolean extend, TEHandle hTE);
void TEKey(short key, TEHandle hTE);

void TEIdle(TEHandle hTE);
void TEActivate(TEHandle hTE);
void TEDeactivate(TEHandle hTE);
void TEUpdate(const Rect *rUpdate, TEHandle hTE);
void TESetSelect(long selStart, long selEnd, TEHandle hTE);
void TECalText(TEHandle hTE);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_TEXTEDIT_H */
