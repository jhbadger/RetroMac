/* ControlManager.c -- classic Control Manager. Phase 0 only needs
 * push buttons (pushButProc), drawn by hand directly into the owning
 * window's offscreen buffer -- consistent with the rest of RetroMac's
 * "reimplement the Toolbox trap, don't bridge to a native widget"
 * approach (see Toolbox.md section 2). Mouse tracking is shared with
 * DragWindow/TrackGoAway via CocoaBridge.m's RMCocoa_TrackMouse.
 */
#include "RetroMacInternal.h"
#include "../include/Controls.h"
#include <CoreText/CoreText.h>
#include <string.h>
#include <stdio.h>

static void RM_DrawControl(ControlHandle c)
{
    if (!c || !c->visible || !c->owner || !c->owner->buffer) return;

    CGContextRef ctx = c->owner->buffer;
    int h = c->owner->height;
    CGRect cr = CGRectMake(c->bounds.left, h - c->bounds.bottom,
                           c->bounds.right - c->bounds.left, c->bounds.bottom - c->bounds.top);

    CGFloat radius = CGRectGetHeight(cr) / 2.0;
    CGPathRef path = CGPathCreateWithRoundedRect(cr, radius, radius, NULL);

    CGContextSetRGBFillColor(ctx, c->hilited ? 0.6 : 0.9, c->hilited ? 0.6 : 0.9, c->hilited ? 0.6 : 0.9, 1.0);
    CGContextAddPath(ctx, path);
    CGContextFillPath(ctx);

    CGContextSetRGBStrokeColor(ctx, 0, 0, 0, 1);
    CGContextSetLineWidth(ctx, 1);
    CGContextAddPath(ctx, path);
    CGContextStrokePath(ctx);
    CGPathRelease(path);

    char utf8[256];
    RM_MacRomanPStringToUTF8(c->title, utf8, sizeof(utf8));

    CTFontRef font = CTFontCreateWithName(CFSTR("Helvetica"), 12, NULL);
    CFStringRef str = CFStringCreateWithCString(NULL, utf8, kCFStringEncodingUTF8);
    CFStringRef keys[1] = { kCTFontAttributeName };
    CFTypeRef values[1] = { font };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 1,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef attrStr = CFAttributedStringCreate(NULL, str, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(attrStr);

    CGFloat ascent = 0, descent = 0, leading = 0;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

    CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
    CGContextSaveGState(ctx);
    CGContextSetTextPosition(ctx, cr.origin.x + (cr.size.width - width) / 2.0,
                              cr.origin.y + (cr.size.height - (ascent + descent)) / 2.0 + descent);
    CTLineDraw(line, ctx);
    CGContextRestoreGState(ctx);

    CFRelease(line);
    CFRelease(attrStr);
    CFRelease(attrs);
    CFRelease(str);
    CFRelease(font);

    RM_MarkDirty(c->owner);
}

ControlHandle NewControl(WindowPtr owner, const Rect *boundsRect, ConstStr255Param title,
                          Boolean visible, short value, short min, short max,
                          short procID, long refCon)
{
    (void)refCon;
    if (!owner) return NULL;

    ControlHandle c = NULL;
    for (int i = 0; i < RM_MAX_CONTROLS; i++) {
        if (!owner->controls[i].inUse) { c = &owner->controls[i]; break; }
    }
    if (!c) {
        fprintf(stderr, "RetroMac: out of control slots on window (RM_MAX_CONTROLS=%d)\n", RM_MAX_CONTROLS);
        return NULL;
    }

    memset(c, 0, sizeof(*c));
    c->inUse = true;
    c->owner = owner;
    c->bounds = *boundsRect;
    if (title) RM_PStringCopy(c->title, title); else c->title[0] = 0;
    c->value = value;
    c->min = min;
    c->max = max;
    c->visible = visible ? true : false;
    c->hilited = false;
    c->procID = procID;
    c->itemNumber = ++owner->nextItemNumber;

    if (c->visible) RM_DrawControl(c);
    return c;
}

void DisposeControl(ControlHandle c)
{
    if (!c) return;
    c->inUse = false;
}

short FindControl(Point pt, WindowPtr w, ControlHandle *control)
{
    if (control) *control = NULL;
    if (!w) return 0;

    for (int i = 0; i < RM_MAX_CONTROLS; i++) {
        ControlHandle c = &w->controls[i];
        if (!c->inUse || !c->visible) continue;
        if (PtInRect(pt, &c->bounds)) {
            if (control) *control = c;
            return inButton; /* phase 0 only implements push buttons */
        }
    }
    return 0;
}

typedef struct {
    ControlHandle c;
    Rect globalBounds;
    Boolean stillInside;
} RMTrackCtx;

static void RM_TrackOnMove(Point globalPt, void *ctxPtr)
{
    RMTrackCtx *ctx = (RMTrackCtx *)ctxPtr;
    Boolean inside = PtInRect(globalPt, &ctx->globalBounds) ? true : false;
    if (ctx->c->hilited != inside) {
        ctx->c->hilited = inside;
        RM_DrawControl(ctx->c);
    }
}

static void RM_TrackOnUp(Point globalPt, void *ctxPtr)
{
    RMTrackCtx *ctx = (RMTrackCtx *)ctxPtr;
    ctx->stillInside = PtInRect(globalPt, &ctx->globalBounds) ? true : false;
    if (ctx->c->hilited) {
        ctx->c->hilited = false;
        RM_DrawControl(ctx->c);
    }
}

short TrackControl(ControlHandle c, Point startPt, void *actionProc)
{
    (void)startPt; (void)actionProc;
    if (!c || !c->owner) return 0;

    RMTrackCtx ctx;
    ctx.c = c;
    ctx.stillInside = false;

    /* RMCocoa_TrackMouse reports points in classic global coords;
     * convert the control's (local, content-area) bounds to match. */
    Point origin = c->owner->contentOriginGlobal;
    SetRect(&ctx.globalBounds,
            (short)(origin.h + c->bounds.left), (short)(origin.v + c->bounds.top),
            (short)(origin.h + c->bounds.right), (short)(origin.v + c->bounds.bottom));

    c->hilited = true;
    RM_DrawControl(c);

    RMCocoa_TrackMouse(RM_TrackOnMove, RM_TrackOnUp, &ctx);

    return ctx.stillInside ? inButton : 0;
}
