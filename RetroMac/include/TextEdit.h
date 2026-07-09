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

/* Full TERec layout -- lives here (not RetroMacInternal.h) for the
 * same reason ControlRecord lives in Controls.h: Quickdraw.h embeds
 * `struct TERec teFields[]` inline inside GrafPort, so this file must
 * stay independent of Quickdraw.h to avoid a header cycle. */
struct TERec {
    Boolean   inUse;
    WindowPtr owner;
    Rect      viewRect;    /* local to owner window's content area */
    char     *text;        /* heap buffer, grows via realloc; NUL-terminated */
    short     length;
    short     selStart, selEnd; /* selStart == selEnd => caret, no selection */
    Boolean   active;
    Boolean   caretVisible;
    unsigned long lastBlinkTick;
    short     itemNumber;
};

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

void TECut(TEHandle hTE);
void TECopy(TEHandle hTE);
void TEPaste(TEHandle hTE);
void TEDelete(TEHandle hTE);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_TEXTEDIT_H */
