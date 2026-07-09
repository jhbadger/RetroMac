/* TextEditManager.c -- classic TextEdit. Phase 1 implements a single
 * unwrapped line of editable text per field (no auto word-wrap/line-
 * break model -- TECalText is a no-op beyond a redraw; see Toolbox.md
 * section 10), drawn by hand into the owning window's offscreen buffer
 * using Core Text for both rendering and hit-testing/caret-offset math,
 * exactly like ControlManager.c's button-title drawing.
 *
 * Real classic TextEdit arrow keys already arrive as charCodes
 * 0x1C-0x1F (left/right/up/down) rather than through keyCodeMask, so
 * TEKey needs no EventRecord/keyCode plumbing -- see CocoaBridge.m's
 * keyDown handling, which maps the Cocoa arrow-key unichars to those
 * same charCodes.
 */
#include "RetroMacInternal.h"
#include "../include/TextEdit.h"
#include "../include/Memory.h"
#include <CoreText/CoreText.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define RM_TE_PADDING     4.0
#define RM_TE_BLINK_TICKS 30 /* ~0.5s at TickCount()'s 60/sec */

void TEInit(void)
{
    /* Per-field state lives in each owner window's teFields[] pool
     * (RetroMacInternal.h) -- nothing global to initialize. */
}

/* ==== layout helpers (Core Text) ======================================= */

typedef struct {
    CTLineRef line;
    CTFontRef font;
    CFStringRef str;
    CFAttributedStringRef attrStr;
} RM_TELayout;

static CFStringRef RM_TE_CreateCFString(TEHandle hTE)
{
    size_t bufSize = (size_t)hTE->length * 4 + 4;
    char *buf = (char *)malloc(bufSize);
    RM_MacRomanCStringToUTF8(hTE->text, buf, bufSize);
    CFStringRef str = CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
    free(buf);
    return str ? str : CFSTR("");
}

static RM_TELayout RM_TE_BuildLayout(TEHandle hTE)
{
    RM_TELayout L;
    L.font = CTFontCreateWithName(CFSTR("Helvetica"), 12, NULL);
    L.str = RM_TE_CreateCFString(hTE);

    CFStringRef keys[1] = { kCTFontAttributeName };
    CFTypeRef values[1] = { L.font };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 1,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    L.attrStr = CFAttributedStringCreate(NULL, L.str, attrs);
    L.line = CTLineCreateWithAttributedString(L.attrStr);
    CFRelease(attrs);
    return L;
}

static void RM_TE_ReleaseLayout(RM_TELayout *L)
{
    CFRelease(L->line);
    CFRelease(L->attrStr);
    CFRelease(L->str);
    CFRelease(L->font);
}

static CGRect RM_TE_ContentRect(TEHandle hTE)
{
    int h = hTE->owner ? hTE->owner->height : 0;
    const Rect *r = &hTE->viewRect;
    return CGRectMake(r->left, h - r->bottom, r->right - r->left, r->bottom - r->top);
}

/* ==== drawing =========================================================== */

static void RM_TE_Draw(TEHandle hTE)
{
    if (!hTE || !hTE->owner || !hTE->owner->buffer) return;
    CGContextRef ctx = (CGContextRef)hTE->owner->buffer;
    CGRect cr = RM_TE_ContentRect(hTE);

    CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
    CGContextFillRect(ctx, cr);
    CGContextSetRGBStrokeColor(ctx, 0, 0, 0, 1);
    CGContextSetLineWidth(ctx, 1);
    CGContextStrokeRect(ctx, CGRectInset(cr, 0.5, 0.5));

    RM_TELayout L = RM_TE_BuildLayout(hTE);
    CGFloat ascent = 0, descent = 0, leading = 0;
    CTLineGetTypographicBounds(L.line, &ascent, &descent, &leading);
    double baselineY = cr.origin.y + (cr.size.height - (ascent + descent)) / 2.0 + descent;
    double textX = cr.origin.x + RM_TE_PADDING;

    CGContextSaveGState(ctx);
    CGRect inner = CGRectInset(cr, 1, 1);
    CGContextClipToRect(ctx, inner);

    if (hTE->selStart != hTE->selEnd) {
        double x0 = textX + CTLineGetOffsetForStringIndex(L.line, hTE->selStart, NULL);
        double x1 = textX + CTLineGetOffsetForStringIndex(L.line, hTE->selEnd, NULL);
        CGContextSetRGBFillColor(ctx, 0.68, 0.82, 1.0, 1.0);
        CGContextFillRect(ctx, CGRectMake(x0, cr.origin.y + 1, x1 - x0, cr.size.height - 2));
    }

    CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
    CGContextSetTextPosition(ctx, textX, baselineY);
    CTLineDraw(L.line, ctx);

    if (hTE->selStart == hTE->selEnd && hTE->active && hTE->caretVisible) {
        double xc = textX + CTLineGetOffsetForStringIndex(L.line, hTE->selStart, NULL);
        CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
        CGContextFillRect(ctx, CGRectMake(xc, cr.origin.y + 2, 1, cr.size.height - 4));
    }

    CGContextRestoreGState(ctx);
    RM_TE_ReleaseLayout(&L);
    RM_MarkDirty(hTE->owner);
}

/* ==== text mutation ===================================================== */

static void RM_TE_ReplaceRange(TEHandle hTE, short start, short end, const char *repl, short replLen)
{
    short oldLen = hTE->length;
    short newLen = (short)(oldLen - (end - start) + replLen);
    char *newText = (char *)malloc((size_t)newLen + 1);
    memcpy(newText, hTE->text, (size_t)start);
    if (replLen > 0) memcpy(newText + start, repl, (size_t)replLen);
    memcpy(newText + start + replLen, hTE->text + end, (size_t)(oldLen - end));
    newText[newLen] = 0;
    free(hTE->text);
    hTE->text = newText;
    hTE->length = newLen;
    hTE->selStart = hTE->selEnd = (short)(start + replLen);
}

/* ==== Toolbox entry points ============================================== */

TEHandle TENew(const Rect *destRect, const Rect *viewRect)
{
    (void)destRect;
    if (!gCurrentPort || !viewRect) return NULL;

    TEHandle t = NULL;
    for (int i = 0; i < RM_MAX_TE_FIELDS; i++) {
        if (!gCurrentPort->teFields[i].inUse) { t = &gCurrentPort->teFields[i]; break; }
    }
    if (!t) {
        fprintf(stderr, "RetroMac: out of TextEdit slots on window (RM_MAX_TE_FIELDS=%d)\n", RM_MAX_TE_FIELDS);
        return NULL;
    }

    memset(t, 0, sizeof(*t));
    t->inUse = true;
    t->owner = gCurrentPort;
    t->viewRect = *viewRect;
    t->text = (char *)malloc(1);
    t->text[0] = 0;
    t->caretVisible = true;
    t->lastBlinkTick = TickCount();
    t->itemNumber = ++gCurrentPort->nextItemNumber;

    RM_TE_Draw(t);
    return t;
}

void TEDispose(TEHandle hTE)
{
    if (!hTE) return;
    free(hTE->text);
    hTE->text = NULL;
    hTE->inUse = false;
}

void TESetText(const void *text, long length, TEHandle hTE)
{
    if (!hTE) return;
    free(hTE->text);
    hTE->text = (char *)malloc((size_t)length + 1);
    if (length > 0 && text) memcpy(hTE->text, text, (size_t)length);
    hTE->text[length] = 0;
    hTE->length = (short)length;
    hTE->selStart = hTE->selEnd = (short)length;
    RM_TE_Draw(hTE);
}

Handle TEGetText(TEHandle hTE)
{
    if (!hTE) return NULL;
    Handle h = NewHandle(hTE->length);
    if (h && *h && hTE->length > 0) memcpy(*h, hTE->text, (size_t)hTE->length);
    return h;
}

void TEClick(Point pt, Boolean extend, TEHandle hTE)
{
    if (!hTE) return;
    CGRect cr = RM_TE_ContentRect(hTE);
    double localX = pt.h - (cr.origin.x + RM_TE_PADDING);

    RM_TELayout L = RM_TE_BuildLayout(hTE);
    CFIndex idx = CTLineGetStringIndexForPosition(L.line, CGPointMake(localX, 0));
    RM_TE_ReleaseLayout(&L);

    if (idx == kCFNotFound) idx = hTE->length;
    short pos = (short)idx;
    if (pos < 0) pos = 0;
    if (pos > hTE->length) pos = hTE->length;

    if (extend) {
        if (pos < hTE->selStart) hTE->selStart = pos; else hTE->selEnd = pos;
    } else {
        hTE->selStart = hTE->selEnd = pos;
    }

    hTE->caretVisible = true;
    hTE->lastBlinkTick = TickCount();
    RM_TE_Draw(hTE);
}

void TEKey(short key, TEHandle hTE)
{
    if (!hTE) return;
    unsigned char ch = (unsigned char)key;

    switch (ch) {
        case 8: /* backspace */
            if (hTE->selStart != hTE->selEnd) {
                RM_TE_ReplaceRange(hTE, hTE->selStart, hTE->selEnd, NULL, 0);
            } else if (hTE->selStart > 0) {
                RM_TE_ReplaceRange(hTE, (short)(hTE->selStart - 1), hTE->selStart, NULL, 0);
            }
            break;
        case 0x1C: /* left arrow */
            hTE->selStart = hTE->selEnd = (hTE->selStart != hTE->selEnd) ? hTE->selStart
                                           : (short)(hTE->selStart > 0 ? hTE->selStart - 1 : 0);
            break;
        case 0x1D: /* right arrow */
            hTE->selStart = hTE->selEnd = (hTE->selStart != hTE->selEnd) ? hTE->selEnd
                                           : (short)(hTE->selEnd < hTE->length ? hTE->selEnd + 1 : hTE->length);
            break;
        case 0x1E: /* up arrow -- no line-wrap model, simplifies to start of text */
            hTE->selStart = hTE->selEnd = 0;
            break;
        case 0x1F: /* down arrow -- simplifies to end of text */
            hTE->selStart = hTE->selEnd = hTE->length;
            break;
        default:
            RM_TE_ReplaceRange(hTE, hTE->selStart, hTE->selEnd, (const char *)&ch, 1);
            break;
    }

    hTE->caretVisible = true;
    hTE->lastBlinkTick = TickCount();
    RM_TE_Draw(hTE);
}

void TEIdle(TEHandle hTE)
{
    if (!hTE || !hTE->active) return;
    unsigned long now = TickCount();
    if (now - hTE->lastBlinkTick >= RM_TE_BLINK_TICKS) {
        hTE->caretVisible = !hTE->caretVisible;
        hTE->lastBlinkTick = now;
        if (hTE->selStart == hTE->selEnd) RM_TE_Draw(hTE);
    }
}

void TEActivate(TEHandle hTE)
{
    if (!hTE) return;
    hTE->active = true;
    hTE->caretVisible = true;
    hTE->lastBlinkTick = TickCount();
    RM_TE_Draw(hTE);
}

void TEDeactivate(TEHandle hTE)
{
    if (!hTE) return;
    hTE->active = false;
    RM_TE_Draw(hTE);
}

void TEUpdate(const Rect *rUpdate, TEHandle hTE)
{
    (void)rUpdate;
    RM_TE_Draw(hTE);
}

void TESetSelect(long selStart, long selEnd, TEHandle hTE)
{
    if (!hTE) return;
    short s = (short)selStart, e = (short)selEnd;
    if (s < 0) s = 0;
    if (e > hTE->length) e = hTE->length;
    if (s > e) { short tmp = s; s = e; e = tmp; }
    hTE->selStart = s;
    hTE->selEnd = e;
    RM_TE_Draw(hTE);
}

void TECalText(TEHandle hTE)
{
    /* No auto word-wrap/line-break model in Phase 1 -- nothing to
     * recompute beyond a redraw. */
    RM_TE_Draw(hTE);
}

/* ==== Cut/Copy/Paste/Clear ==============================================
 * Real TECut/TECopy/TEPaste route through the shared system scrap
 * (ZeroScrap/PutScrap/GetScrap), so any two TE fields -- even in
 * different windows -- can exchange text. RetroMac doesn't implement
 * the rest of the Scrap Manager (no sample app calls PutScrap/GetScrap
 * directly), so this is a private buffer sized for exactly TE's own
 * use: one heap-allocated run of text, shared by every TE field in the
 * process, same as the classic scrap is shared by every window. */
static char *gScrapText = NULL;
static short gScrapLen = 0;

static void RM_SetScrap(const char *text, short len)
{
    free(gScrapText);
    gScrapText = (char *)malloc((size_t)(len > 0 ? len : 1));
    if (len > 0 && text) memcpy(gScrapText, text, (size_t)len);
    gScrapLen = len;
}

void TECut(TEHandle hTE)
{
    if (!hTE || hTE->selStart == hTE->selEnd) return;
    RM_SetScrap(hTE->text + hTE->selStart, (short)(hTE->selEnd - hTE->selStart));
    RM_TE_ReplaceRange(hTE, hTE->selStart, hTE->selEnd, NULL, 0);
    hTE->caretVisible = true;
    hTE->lastBlinkTick = TickCount();
    RM_TE_Draw(hTE);
}

void TECopy(TEHandle hTE)
{
    if (!hTE || hTE->selStart == hTE->selEnd) return;
    RM_SetScrap(hTE->text + hTE->selStart, (short)(hTE->selEnd - hTE->selStart));
}

void TEPaste(TEHandle hTE)
{
    if (!hTE || !gScrapText || gScrapLen <= 0) return;
    RM_TE_ReplaceRange(hTE, hTE->selStart, hTE->selEnd, gScrapText, gScrapLen);
    hTE->caretVisible = true;
    hTE->lastBlinkTick = TickCount();
    RM_TE_Draw(hTE);
}

void TEDelete(TEHandle hTE)
{
    if (!hTE || hTE->selStart == hTE->selEnd) return;
    RM_TE_ReplaceRange(hTE, hTE->selStart, hTE->selEnd, NULL, 0);
    hTE->caretVisible = true;
    hTE->lastBlinkTick = TickCount();
    RM_TE_Draw(hTE);
}

/* ==== Dialog Manager support (DialogManager.c) ========================= */

TEHandle RM_FindTEField(WindowPtr owner, Point localPt)
{
    if (!owner) return NULL;
    for (int i = 0; i < RM_MAX_TE_FIELDS; i++) {
        TERec *t = &owner->teFields[i];
        if (t->inUse && PtInRect(localPt, &t->viewRect)) return t;
    }
    return NULL;
}
