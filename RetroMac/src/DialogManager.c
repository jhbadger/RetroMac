/* DialogManager.c -- classic Dialog Manager. Plain C: dialogs are
 * WindowPtrs (DialogPtr == WindowPtr, Types.h) built via the existing
 * WindowManager.c/NewWindow, with items added via NewControl/TENew --
 * either by the app itself in creation order (Phase 1, no Resource
 * Manager) or, as of Phase 2, unpacked from a real DITL resource in
 * DITL order (see RM_InstantiateDITL below). Either way, item numbers
 * come from `nextItemNumber` (RetroMacInternal.h), just driven by a
 * different order depending on which path created the items.
 */
#include "RetroMacInternal.h"
#include "../include/Dialogs.h"
#include "../include/Controls.h"
#include "../include/TextEdit.h"
#include "../include/Events.h"
#include "../include/Resources.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void InitDialogs(void *resumeProc)
{
    (void)resumeProc;
}

DialogPtr NewDialog(void *dStorage, const Rect *boundsRect, ConstStr255Param title,
                     Boolean visible, short procID, WindowPtr behind,
                     Boolean goAwayFlag, long refCon, void *items)
{
    (void)items; /* no DITL Handle support -- apps add items via NewControl/TENew instead */
    return NewWindow(dStorage, boundsRect, title, visible, procID, behind, goAwayFlag, refCon);
}

/* ==== ParamText ========================================================= */

static unsigned char gParamText[4][256];

void ParamText(ConstStr255Param param0, ConstStr255Param param1,
               ConstStr255Param param2, ConstStr255Param param3)
{
    ConstStr255Param params[4] = { param0, param1, param2, param3 };
    for (int i = 0; i < 4; i++) {
        if (params[i]) RM_PStringCopy(gParamText[i], params[i]);
        else gParamText[i][0] = 0;
    }
}

/* ==== DITL instantiation (shared by GetNewDialog and the Alert family) =
 *
 * DITL layout (confirmed by compiling one with the real system Rez and
 * hex-dumping the result -- see Toolbox.md section 12): itemCount-1
 * (short), then per item: a 4-byte placeholder handle (unused, zero on
 * disk), itemRect (4 big-endian shorts: top,left,bottom,right), a type
 * byte (bit 0x80 set = disabled; low 7 bits = item type: 4=button,
 * 5=checkbox, 6=radio, 8=statText, 16=editText -- icon/control/picture/
 * userItem are not supported and are silently skipped, a documented
 * Phase 2 scope trim), a data-length byte, then that many raw data
 * bytes (title text for button/checkbox/radio/statText/editText),
 * plus one padding byte if the data length is odd (word alignment --
 * confirmed empirically: WIND/DLOG's title Pascal string needed the
 * same treatment).
 *
 * Item numbers are assigned by DITL position (1-based), not by
 * NewControl/TENew's own creation-order auto-increment -- a statText
 * item doesn't create a control/TE field at all, so without this fixup
 * a dialog mixing statText with interactive items would number its
 * controls 1,2,... instead of matching their real DITL positions. */
enum {
    kDITL_UserItem = 0,
    kDITL_Button   = 4,
    kDITL_CheckBox = 5,
    kDITL_RadioBtn = 6,
    kDITL_Control  = 7,
    kDITL_StatText = 8,
    kDITL_EditText = 16,
    kDITL_Icon     = 32,
    kDITL_Picture  = 64
};

/* Draws a DITL statText item, expanding ^0-^3 into ParamText's stored
 * strings exactly like a real ALRT/DITL does -- not just for alerts;
 * real Inside Mac substitution applies to any DITL statText item. No
 * word-wrap -- single line, the same simplification Phase 1's original
 * fixed alert box already used. */
static void RM_DrawSubstitutedStatText(const Rect *itemRect, const unsigned char *text, short textLen)
{
    unsigned char out[256];
    short outLen = 0;
    for (short i = 0; i < textLen && outLen < 254; i++) {
        if (text[i] == '^' && i + 1 < textLen && text[i + 1] >= '0' && text[i + 1] <= '3') {
            const unsigned char *sub = gParamText[text[i + 1] - '0'];
            short subLen = RM_PStringLength(sub);
            for (short j = 0; j < subLen && outLen < 254; j++) out[1 + outLen++] = sub[1 + j];
            i++; /* skip the digit */
        } else {
            out[1 + outLen++] = text[i];
        }
    }
    out[0] = (unsigned char)outLen;

    MoveTo((short)(itemRect->left + 2), (short)(itemRect->top + 12));
    DrawString(out);
}

static void RM_InstantiateDITL(DialogPtr d, short ditlID)
{
    Handle h = Get1Resource('DITL', ditlID);
    if (!h || !*h) {
        fprintf(stderr, "RetroMac: DITL %d not found\n", (int)ditlID);
        return;
    }

    SetPort(d);
    const unsigned char *p = (const unsigned char *)*h;
    short count = (short)(RM_ReadU16BE(p) + 1);
    long offset = 2;

    for (short i = 0; i < count; i++) {
        short thisItemNumber = (short)(i + 1);

        offset += 4; /* placeholder handle */
        Rect itemRect;
        itemRect.top    = (short)RM_ReadU16BE(p + offset + 0);
        itemRect.left   = (short)RM_ReadU16BE(p + offset + 2);
        itemRect.bottom = (short)RM_ReadU16BE(p + offset + 4);
        itemRect.right  = (short)RM_ReadU16BE(p + offset + 6);
        offset += 8;

        unsigned char typeByte = p[offset]; offset += 1;
        short type = (short)(typeByte & 0x7F);

        unsigned char dataLen = p[offset]; offset += 1;
        const unsigned char *itemData = p + offset;
        offset += dataLen;
        if (dataLen % 2 == 1) offset += 1; /* word-align, per the confirmed layout */

        unsigned char title[256];
        title[0] = dataLen;
        if (dataLen > 0) memcpy(&title[1], itemData, dataLen);

        switch (type) {
            case kDITL_Button: {
                ControlHandle c = NewControl(d, &itemRect, title, true, 0, 0, 1, pushButProc, 0);
                if (c) c->itemNumber = thisItemNumber;
                break;
            }
            case kDITL_CheckBox: {
                ControlHandle c = NewControl(d, &itemRect, title, true, 0, 0, 1, checkBoxProc, 0);
                if (c) c->itemNumber = thisItemNumber;
                break;
            }
            case kDITL_RadioBtn: {
                ControlHandle c = NewControl(d, &itemRect, title, true, 0, 0, 1, radioButProc, 0);
                if (c) c->itemNumber = thisItemNumber;
                break;
            }
            case kDITL_EditText: {
                TEHandle field = TENew(&itemRect, &itemRect);
                if (field) {
                    TESetText(itemData, dataLen, field);
                    field->itemNumber = thisItemNumber;
                }
                break;
            }
            case kDITL_StatText:
                RM_DrawSubstitutedStatText(&itemRect, itemData, dataLen);
                break;
            default:
                break; /* icon/control/picture/userItem: not supported, silently skipped */
        }
    }

    /* Positional numbering above may leave nextItemNumber behind the
     * real DITL count (a statText item doesn't call NewControl/TENew,
     * so it never bumps the counter) -- resync so any item added to
     * this dialog afterward continues from the right base. */
    d->nextItemNumber = count;

    ReleaseResource(h);
}

/* DLOG layout mirrors WIND (see WindowManager.c's GetNewWindow) plus an
 * itemsID pointing at a DITL: boundsRect, procID, visible, goAwayFlag,
 * refCon, itemsID (short), title (Pascal string), then an optional
 * trailing positioning word -- confirmed by the same hands-on Rez
 * compile-and-hexdump pass documented in Toolbox.md section 12. */
DialogPtr GetNewDialog(short dialogID, void *dStorage, WindowPtr behind)
{
    Handle h = Get1Resource('DLOG', dialogID);
    if (!h || !*h) {
        fprintf(stderr, "RetroMac: GetNewDialog: DLOG %d not found\n", (int)dialogID);
        return NULL;
    }

    const unsigned char *p = (const unsigned char *)*h;
    Rect bounds;
    bounds.top    = (short)RM_ReadU16BE(p + 0);
    bounds.left   = (short)RM_ReadU16BE(p + 2);
    bounds.bottom = (short)RM_ReadU16BE(p + 4);
    bounds.right  = (short)RM_ReadU16BE(p + 6);
    short procID     = (short)RM_ReadU16BE(p + 8);
    Boolean visible  = RM_ReadU16BE(p + 10) != 0;
    Boolean goAway   = RM_ReadU16BE(p + 12) != 0;
    long refCon      = (long)RM_ReadU32BE(p + 14);
    short itemsID    = (short)RM_ReadU16BE(p + 18);
    unsigned char title[256];
    RM_PStringCopy(title, p + 20);
    ReleaseResource(h);

    DialogPtr d = NewDialog(dStorage, &bounds, title, visible, procID, behind, goAway, refCon, NULL);
    if (!d) return NULL;

    RM_InstantiateDITL(d, itemsID);
    return d;
}

/* ==== ModalDialog ======================================================= */

static TEHandle RM_ActiveTEField(WindowPtr d)
{
    for (int i = 0; i < RM_MAX_TE_FIELDS; i++) {
        TERec *t = &d->teFields[i];
        if (t->inUse && t->active) return t;
    }
    return NULL;
}

void ModalDialog(void *filterProc, short *itemHit)
{
    (void)filterProc;
    if (itemHit) *itemHit = 0;

    WindowPtr d = FrontWindow();
    if (!d) return;

    for (;;) {
        TEHandle active = RM_ActiveTEField(d);
        if (active) TEIdle(active);

        EventRecord ev;
        if (!WaitNextEvent(everyEvent, &ev, 10, NULL)) continue;

        switch (ev.what) {
            case mouseDown: {
                WindowPtr hitWindow = NULL;
                short part = FindWindow(ev.where, &hitWindow);
                if (hitWindow != d) break; /* click outside the modal dialog: ignored in Phase 1 */

                if (part == inGoAway) {
                    if (TrackGoAway(d, ev.where)) return;
                    break;
                }
                if (part != inContent) break;

                SetPort(d);
                Point local = ev.where;
                GlobalToLocal(&local);

                ControlHandle ctrl = NULL;
                short cpart = FindControl(local, d, &ctrl);
                if (cpart == inButton && ctrl) {
                    if (TrackControl(ctrl, local, NULL) == inButton) {
                        if (itemHit) *itemHit = ctrl->itemNumber;
                        return;
                    }
                    break;
                }

                TEHandle field = RM_FindTEField(d, local);
                if (field) {
                    for (int i = 0; i < RM_MAX_TE_FIELDS; i++) {
                        TERec *t = &d->teFields[i];
                        if (t->inUse && t != field && t->active) TEDeactivate(t);
                    }
                    if (!field->active) TEActivate(field);
                    TEClick(local, (ev.modifiers & shiftKey) ? true : false, field);
                }
                break;
            }

            case keyDown:
            case autoKey: {
                char ch = (char)(ev.message & charCodeMask);
                if (ch == 0x0D || ch == 0x03) { /* Return/Enter -> default item, item 1 by convention */
                    if (itemHit) *itemHit = 1;
                    return;
                }
                TEHandle field = RM_ActiveTEField(d);
                if (field) TEKey((short)(unsigned char)ch, field);
                break;
            }

            case updateEvt:
                BeginUpdate((WindowPtr)ev.message);
                EndUpdate((WindowPtr)ev.message);
                break;

            default:
                break;
        }
    }
}

/* ==== Alerts ============================================================= */

typedef enum { RM_AlertPlain, RM_AlertNote, RM_AlertCaution, RM_AlertStop } RM_AlertKind;

static void RM_DrawOctagon(CGContextRef ctx, CGRect r)
{
    CGFloat cx = r.origin.x + r.size.width / 2.0, cy = r.origin.y + r.size.height / 2.0;
    CGFloat rad = r.size.width / 2.0;
    CGContextMoveToPoint(ctx, cx + rad * cos(0.0), cy + rad * sin(0.0));
    for (int i = 1; i <= 8; i++) {
        double angle = i * (M_PI / 4.0);
        CGContextAddLineToPoint(ctx, cx + rad * cos(angle), cy + rad * sin(angle));
    }
    CGContextClosePath(ctx);
}

static void RM_DrawAlertIcon(RM_AlertKind kind, const Rect *r)
{
    CGContextRef ctx = RM_CurrentBuffer();
    if (!ctx || !gCurrentPort || kind == RM_AlertPlain) return;

    int h = gCurrentPort->height;
    CGRect cr = CGRectMake(r->left, h - r->bottom, r->right - r->left, r->bottom - r->top);

    switch (kind) {
        case RM_AlertStop:
            CGContextSetRGBFillColor(ctx, 0.82, 0.1, 0.1, 1.0);
            RM_DrawOctagon(ctx, cr);
            CGContextFillPath(ctx);
            break;
        case RM_AlertCaution:
            CGContextSetRGBFillColor(ctx, 0.95, 0.75, 0.1, 1.0);
            CGContextMoveToPoint(ctx, cr.origin.x + cr.size.width / 2.0, cr.origin.y + cr.size.height);
            CGContextAddLineToPoint(ctx, cr.origin.x, cr.origin.y);
            CGContextAddLineToPoint(ctx, cr.origin.x + cr.size.width, cr.origin.y);
            CGContextClosePath(ctx);
            CGContextFillPath(ctx);
            break;
        case RM_AlertNote:
            CGContextSetRGBFillColor(ctx, 0.2, 0.45, 0.9, 1.0);
            CGContextFillEllipseInRect(ctx, cr);
            break;
        default:
            break;
    }
    RM_MarkDirty(gCurrentPort);
}

/* ALRT layout: boundsRect, itemsID (short), a 2-byte "stages" word
 * (per-click sound/redraw/bold-item hints -- ignored, same simplification
 * as Phase 1's fixed box: only one OK-equivalent button is ever shown),
 * then an optional positioning word. Confirmed via the same hands-on
 * Rez compile-and-hexdump pass as WIND/DLOG/DITL (Toolbox.md section 12). */
static short RM_RunAlert(RM_AlertKind kind, short alertID)
{
    Handle h = Get1Resource('ALRT', alertID);
    Rect bounds;
    short itemsID = 0;

    if (h && *h) {
        const unsigned char *p = (const unsigned char *)*h;
        bounds.top    = (short)RM_ReadU16BE(p + 0);
        bounds.left   = (short)RM_ReadU16BE(p + 2);
        bounds.bottom = (short)RM_ReadU16BE(p + 4);
        bounds.right  = (short)RM_ReadU16BE(p + 6);
        itemsID = (short)RM_ReadU16BE(p + 8);
        ReleaseResource(h);
    } else {
        /* No ALRT resource for this ID -- fall back to Phase 1's fixed
         * hand-drawn box, so apps with no .r file (every Phase 1
         * sample) keep working exactly as before. */
        Rect screen = RMCocoa_MainScreenBoundsClassic();
        short w = 340, boxH = 120;
        short left = (short)(screen.left + (screen.right - screen.left - w) / 2);
        short top  = (short)(screen.top + (screen.bottom - screen.top - boxH) / 3);
        SetRect(&bounds, left, top, (short)(left + w), (short)(top + boxH));
    }

    static const unsigned char kEmptyTitle[] = { 0 };
    DialogPtr d = NewDialog(NULL, &bounds, kEmptyTitle, true, dBoxProc, (WindowPtr)-1L, false, 0, NULL);
    if (!d) return 1;
    SetPort(d);

    if (itemsID != 0) {
        RM_InstantiateDITL(d, itemsID);
    } else {
        Rect iconRect;
        SetRect(&iconRect, 20, 20, 52, 52);
        RM_DrawAlertIcon(kind, &iconRect);

        RGBColor black = {0, 0, 0};
        TextFont(0); TextSize(12); TextFace(0);
        RGBForeColor(&black);
        MoveTo(64, 40);
        DrawString(gParamText[0]);

        short w = (short)(bounds.right - bounds.left), boxH = (short)(bounds.bottom - bounds.top);
        Rect btnRect;
        SetRect(&btnRect, (short)(w - 20 - 80), (short)(boxH - 20 - 24), (short)(w - 20), (short)(boxH - 20));
        static const unsigned char kOKTitle[] = { 2, 'O', 'K' };
        NewControl(d, &btnRect, kOKTitle, true, 0, 0, 1, pushButProc, 0);
    }

    short itemHit = 0;
    ModalDialog(NULL, &itemHit);
    DisposeWindow(d);
    return 1;
}

short Alert(short alertID, void *filterProc)
{
    (void)filterProc;
    return RM_RunAlert(RM_AlertPlain, alertID);
}

short StopAlert(short alertID, void *filterProc)
{
    (void)filterProc;
    return RM_RunAlert(RM_AlertStop, alertID);
}

short NoteAlert(short alertID, void *filterProc)
{
    (void)filterProc;
    return RM_RunAlert(RM_AlertNote, alertID);
}

short CautionAlert(short alertID, void *filterProc)
{
    (void)filterProc;
    return RM_RunAlert(RM_AlertCaution, alertID);
}
