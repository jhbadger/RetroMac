/* DialogManager.c -- classic Dialog Manager. Plain C: dialogs are
 * WindowPtrs (DialogPtr == WindowPtr, Types.h) built via the existing
 * WindowManager.c/NewWindow, with items added afterward via
 * NewControl/TENew -- there's no DITL/Resource Manager yet (Phase 2),
 * so item numbers come from creation order instead (see
 * RetroMacInternal.h's itemNumber). See Dialogs.h for the full
 * rationale.
 */
#include "RetroMacInternal.h"
#include "../include/Dialogs.h"
#include "../include/Controls.h"
#include "../include/TextEdit.h"
#include "../include/Events.h"
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

DialogPtr GetNewDialog(short dialogID, void *dStorage, WindowPtr behind)
{
    (void)dStorage; (void)behind;
    static Boolean warned = false;
    if (!warned) {
        fprintf(stderr, "RetroMac: GetNewDialog(%d) requires the Resource Manager (Phase 2); "
                         "use NewDialog instead.\n", (int)dialogID);
        warned = true;
    }
    return NULL;
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

/* ==== ParamText / Alerts ================================================ */

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

static short RM_RunAlert(RM_AlertKind kind)
{
    Rect screen = RMCocoa_MainScreenBoundsClassic();
    short w = 340, h = 120;
    short left = (short)(screen.left + (screen.right - screen.left - w) / 2);
    short top  = (short)(screen.top + (screen.bottom - screen.top - h) / 3);
    Rect bounds;
    SetRect(&bounds, left, top, (short)(left + w), (short)(top + h));

    /* Runtime sources aren't run through retromacc's "\p" literal
     * rewrite (that only applies to user sources -- see retromacc) so
     * Pascal string literals here are plain length-prefixed byte
     * arrays instead. */
    static const unsigned char kEmptyTitle[] = { 0 };
    static const unsigned char kOKTitle[] = { 2, 'O', 'K' };

    DialogPtr d = NewDialog(NULL, &bounds, kEmptyTitle, true, dBoxProc, (WindowPtr)-1L, false, 0, NULL);
    if (!d) return 1;
    SetPort(d);

    Rect iconRect;
    SetRect(&iconRect, 20, 20, 52, 52);
    RM_DrawAlertIcon(kind, &iconRect);

    RGBColor black = {0, 0, 0};
    TextFont(0); TextSize(12); TextFace(0);
    RGBForeColor(&black);
    MoveTo(64, 40);
    DrawString(gParamText[0]);

    Rect btnRect;
    SetRect(&btnRect, (short)(w - 20 - 80), (short)(h - 20 - 24), (short)(w - 20), (short)(h - 20));
    NewControl(d, &btnRect, kOKTitle, true, 0, 0, 1, pushButProc, 0);

    short itemHit = 0;
    ModalDialog(NULL, &itemHit);
    DisposeWindow(d);
    return 1;
}

short Alert(short alertID, void *filterProc)
{
    (void)alertID; (void)filterProc;
    return RM_RunAlert(RM_AlertPlain);
}

short StopAlert(short alertID, void *filterProc)
{
    (void)alertID; (void)filterProc;
    return RM_RunAlert(RM_AlertStop);
}

short NoteAlert(short alertID, void *filterProc)
{
    (void)alertID; (void)filterProc;
    return RM_RunAlert(RM_AlertNote);
}

short CautionAlert(short alertID, void *filterProc)
{
    (void)alertID; (void)filterProc;
    return RM_RunAlert(RM_AlertCaution);
}
