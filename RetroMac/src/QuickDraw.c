/* QuickDraw.c -- geometry, drawing, text and color, implemented as a
 * software rasterizer onto the current GrafPort's offscreen
 * CGContext (see WindowManager.c/CocoaBridge.m for how that buffer is
 * created and blitted to screen). All calls operate on `gCurrentPort`, exactly
 * like real QuickDraw's "current graphics port" model -- there is no
 * port parameter on any of these calls because classic code doesn't
 * pass one either.
 */
#include "RetroMacInternal.h"
#include <CoreText/CoreText.h>
#include <string.h>
#include <math.h>

/* ==== geometry ======================================================= */

void SetRect(Rect *r, short left, short top, short right, short bottom)
{
    r->left = left; r->top = top; r->right = right; r->bottom = bottom;
}

void InsetRect(Rect *r, short dh, short dv)
{
    r->left += dh; r->top += dv; r->right -= dh; r->bottom -= dv;
}

void OffsetRect(Rect *r, short dh, short dv)
{
    r->left += dh; r->right += dh; r->top += dv; r->bottom += dv;
}

Boolean PtInRect(Point pt, const Rect *r)
{
    return (pt.h >= r->left && pt.h < r->right && pt.v >= r->top && pt.v < r->bottom) ? true : false;
}

/* ==== shared helpers ================================================= */

/* The offscreen buffer is a plain, native (bottom-left origin, y-up)
 * CGBitmapContext -- so its CGImage comes out right-side-up with no
 * extra flip bookkeeping when later composited into a window's
 * (flipped) view. That means every QuickDraw call here has to convert
 * classic top-down y into native bottom-up y itself, using the
 * current port's height. */
static CGRect RM_CGRectFromRect(const Rect *r)
{
    int h = gCurrentPort ? gCurrentPort->height : 0;
    return CGRectMake(r->left, h - r->bottom, r->right - r->left, r->bottom - r->top);
}

static short RM_NativeY(short classicV)
{
    int h = gCurrentPort ? gCurrentPort->height : 0;
    return (short)(h - classicV);
}

static CGFloat RM_C(unsigned short v) { return (CGFloat)v / 65535.0; }

static void RM_SetFillFromRGB(CGContextRef ctx, const RGBColor *c)
{
    CGContextSetRGBFillColor(ctx, RM_C(c->red), RM_C(c->green), RM_C(c->blue), 1.0);
}

static void RM_SetStrokeFromRGB(CGContextRef ctx, const RGBColor *c)
{
    CGContextSetRGBStrokeColor(ctx, RM_C(c->red), RM_C(c->green), RM_C(c->blue), 1.0);
}

/* ==== drawing ========================================================= */

void EraseRect(const Rect *r)
{
    CGContextRef ctx = RM_CurrentBuffer();
    if (!ctx || !gCurrentPort) return;
    RM_SetFillFromRGB(ctx, &gCurrentPort->bgColor);
    CGContextFillRect(ctx, RM_CGRectFromRect(r));
    RM_MarkDirty(gCurrentPort);
}

void PaintRect(const Rect *r)
{
    CGContextRef ctx = RM_CurrentBuffer();
    if (!ctx || !gCurrentPort) return;
    RM_SetFillFromRGB(ctx, &gCurrentPort->fgColor);
    CGContextFillRect(ctx, RM_CGRectFromRect(r));
    RM_MarkDirty(gCurrentPort);
}

void FrameRect(const Rect *r)
{
    CGContextRef ctx = RM_CurrentBuffer();
    if (!ctx || !gCurrentPort) return;
    CGRect cr = CGRectInset(RM_CGRectFromRect(r), gCurrentPort->penH / 2.0, gCurrentPort->penV / 2.0);
    RM_SetStrokeFromRGB(ctx, &gCurrentPort->fgColor);
    CGContextSetLineWidth(ctx, gCurrentPort->penH);
    CGContextStrokeRect(ctx, cr);
    RM_MarkDirty(gCurrentPort);
}

void PaintRoundRect(const Rect *r, short ovalWidth, short ovalHeight)
{
    CGContextRef ctx = RM_CurrentBuffer();
    if (!ctx || !gCurrentPort) return;
    CGFloat radius = (ovalWidth + ovalHeight) / 4.0;
    CGPathRef path = CGPathCreateWithRoundedRect(RM_CGRectFromRect(r), radius, radius, NULL);
    RM_SetFillFromRGB(ctx, &gCurrentPort->fgColor);
    CGContextAddPath(ctx, path);
    CGContextFillPath(ctx);
    CGPathRelease(path);
    RM_MarkDirty(gCurrentPort);
}

void FrameRoundRect(const Rect *r, short ovalWidth, short ovalHeight)
{
    CGContextRef ctx = RM_CurrentBuffer();
    if (!ctx || !gCurrentPort) return;
    CGRect cr = CGRectInset(RM_CGRectFromRect(r), gCurrentPort->penH / 2.0, gCurrentPort->penV / 2.0);
    CGFloat radius = (ovalWidth + ovalHeight) / 4.0;
    CGPathRef path = CGPathCreateWithRoundedRect(cr, radius, radius, NULL);
    RM_SetStrokeFromRGB(ctx, &gCurrentPort->fgColor);
    CGContextSetLineWidth(ctx, gCurrentPort->penH);
    CGContextAddPath(ctx, path);
    CGContextStrokePath(ctx);
    CGPathRelease(path);
    RM_MarkDirty(gCurrentPort);
}

void FrameOval(const Rect *r)
{
    CGContextRef ctx = RM_CurrentBuffer();
    if (!ctx || !gCurrentPort) return;
    RM_SetStrokeFromRGB(ctx, &gCurrentPort->fgColor);
    CGContextSetLineWidth(ctx, gCurrentPort->penH);
    CGContextStrokeEllipseInRect(ctx, RM_CGRectFromRect(r));
    RM_MarkDirty(gCurrentPort);
}

void PenSize(short width, short height)
{
    if (!gCurrentPort) return;
    gCurrentPort->penH = width;
    gCurrentPort->penV = height;
}

void PenNormal(void)
{
    if (!gCurrentPort) return;
    gCurrentPort->penH = 1;
    gCurrentPort->penV = 1;
}

void MoveTo(short h, short v)
{
    if (!gCurrentPort) return;
    gCurrentPort->pen.h = h;
    gCurrentPort->pen.v = v;
}

void LineTo(short h, short v)
{
    CGContextRef ctx = RM_CurrentBuffer();
    if (!ctx || !gCurrentPort) return;
    RM_SetStrokeFromRGB(ctx, &gCurrentPort->fgColor);
    CGContextSetLineWidth(ctx, gCurrentPort->penH);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, gCurrentPort->pen.h, RM_NativeY(gCurrentPort->pen.v));
    CGContextAddLineToPoint(ctx, h, RM_NativeY(v));
    CGContextStrokePath(ctx);
    gCurrentPort->pen.h = h;
    gCurrentPort->pen.v = v;
    RM_MarkDirty(gCurrentPort);
}

/* ==== text ============================================================ */

static CTFontRef RM_CreateCurrentFont(void)
{
    short size = (gCurrentPort && gCurrentPort->txSize > 0) ? gCurrentPort->txSize : 12;
    short face = gCurrentPort ? gCurrentPort->txFace : 0;
    CTFontRef font = CTFontCreateWithName(CFSTR("Helvetica"), size, NULL);

    CTFontSymbolicTraits traits = 0;
    if (face & bold)   traits |= kCTFontBoldTrait;
    if (face & italic) traits |= kCTFontItalicTrait;
    if (traits != 0) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(font, size, NULL, traits, traits);
        if (styled) {
            CFRelease(font);
            font = styled;
        }
    }
    return font;
}

/* Shared draw-or-measure path for DrawString/DrawChar/StringWidth/
 * CharWidth/GetFontInfo -- they all boil down to laying out one line
 * of the current font and either stroking it at the pen position or
 * just reading back its metrics. */
static double RM_LayoutLine(const char *utf8, Boolean draw, short *outAscent, short *outDescent, short *outLeading)
{
    CTFontRef font = RM_CreateCurrentFont();
    CFStringRef str = CFStringCreateWithCString(NULL, utf8, kCFStringEncodingUTF8);
    if (!str) str = CFSTR("");

    CFStringRef keys[1]   = { kCTFontAttributeName };
    CFTypeRef   values[1] = { font };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 1,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef attrStr = CFAttributedStringCreate(NULL, str, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(attrStr);

    CGFloat ascent = 0, descent = 0, leading = 0;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    if (outAscent)  *outAscent  = (short)ceil(ascent);
    if (outDescent) *outDescent = (short)ceil(descent);
    if (outLeading) *outLeading = (short)ceil(leading);

    if (draw) {
        CGContextRef ctx = RM_CurrentBuffer();
        if (ctx && gCurrentPort) {
            /* CoreText's baseline/y-up model already matches this
             * native (bottom-left origin, y-up) buffer directly --
             * no flip needed, just place the baseline at the pen. */
            RM_SetFillFromRGB(ctx, &gCurrentPort->fgColor);
            CGContextSaveGState(ctx);
            CGContextSetTextPosition(ctx, gCurrentPort->pen.h, RM_NativeY(gCurrentPort->pen.v));
            CTLineDraw(line, ctx);
            CGContextRestoreGState(ctx);
            RM_MarkDirty(gCurrentPort);
        }
    }

    CFRelease(line);
    CFRelease(attrStr);
    CFRelease(attrs);
    CFRelease(str);
    CFRelease(font);
    return width;
}

void DrawString(ConstStr255Param s)
{
    char buf[1024];
    RM_MacRomanPStringToUTF8(s, buf, sizeof(buf));
    double width = RM_LayoutLine(buf, true, NULL, NULL, NULL);
    if (gCurrentPort) gCurrentPort->pen.h += (short)ceil(width);
}

void DrawChar(short ch)
{
    unsigned char pstr[2];
    pstr[0] = 1;
    pstr[1] = (unsigned char)ch;
    DrawString(pstr);
}

short StringWidth(ConstStr255Param s)
{
    char buf[1024];
    RM_MacRomanPStringToUTF8(s, buf, sizeof(buf));
    return (short)ceil(RM_LayoutLine(buf, false, NULL, NULL, NULL));
}

short CharWidth(short ch)
{
    unsigned char pstr[2];
    pstr[0] = 1;
    pstr[1] = (unsigned char)ch;
    return StringWidth(pstr);
}

void GetFontInfo(FontInfo *info)
{
    short ascent = 0, descent = 0, leading = 0;
    double widM = RM_LayoutLine("M", false, &ascent, &descent, &leading);
    if (!info) return;
    info->ascent = ascent;
    info->descent = descent;
    info->leading = leading;
    info->widMax = (short)ceil(widM);
}

void TextFont(short fontID) { if (gCurrentPort) gCurrentPort->txFont = fontID; }
void TextSize(short size)   { if (gCurrentPort) gCurrentPort->txSize = size; }
void TextFace(short face)   { if (gCurrentPort) gCurrentPort->txFace = face; }

/* ==== color ============================================================ */

void RGBForeColor(const RGBColor *color) { if (gCurrentPort && color) gCurrentPort->fgColor = *color; }
void RGBBackColor(const RGBColor *color) { if (gCurrentPort && color) gCurrentPort->bgColor = *color; }

void ForeColor(long colorCode)
{
    if (!gCurrentPort) return;
    RGBColor black = {0, 0, 0}, white = {0xFFFF, 0xFFFF, 0xFFFF};
    gCurrentPort->fgColor = (colorCode == 30 /* whiteColor */) ? white : black;
}

void BackColor(long colorCode)
{
    if (!gCurrentPort) return;
    RGBColor black = {0, 0, 0}, white = {0xFFFF, 0xFFFF, 0xFFFF};
    gCurrentPort->bgColor = (colorCode == 33 /* blackColor */) ? black : white;
}

/* ==== bootstrap ========================================================= */

void InitGraf(void *globalPtr)
{
    /* globalPtr == &qd.thePort; walk back to the QDGlobals base the
     * same way real QuickDraw did, using the fixed offset of
     * `thePort` within the struct (see Quickdraw.h). */
    QDGlobals *g = (QDGlobals *)((char *)globalPtr - offsetof(QDGlobals, thePort));
    g->screenBits.baseAddr = NULL;
    g->screenBits.rowBytes = 0;
    g->screenBits.bounds = RMCocoa_MainScreenBoundsClassic();
    g->thePort = NULL;
}

void InitCursor(void)
{
    /* NSCursor already defaults to the arrow; real cursor management
     * (SetCursor/custom cursors) is Phase 1+. */
}
