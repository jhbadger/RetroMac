/* RetroMac Toolbox shim: Quickdraw.h */
#ifndef RETROMAC_QUICKDRAW_H
#define RETROMAC_QUICKDRAW_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BitMap {
    void  *baseAddr;
    short  rowBytes;
    Rect   bounds;
} BitMap;

/* Field order matches real QDGlobals closely enough that InitGraf can
 * locate the struct base from &qd.thePort the same way real QuickDraw
 * does: by subtracting offsetof(QDGlobals, thePort). */
typedef struct QDGlobals {
    BitMap  screenBits;
    GrafPtr thePort;
} QDGlobals;

typedef struct FontInfo {
    short ascent;
    short descent;
    short widMax;
    short leading;
} FontInfo;

/* QuickDraw character style bits (normal/bold/italic/... come from
 * the Style enum already defined by <MacTypes.h>) */

/* Font IDs */
enum {
    systemFont = 0,
    applFont   = 1
};

void InitGraf(void *globalPtr);
void InitCursor(void);

void SetRect(Rect *r, short left, short top, short right, short bottom);
void InsetRect(Rect *r, short dh, short dv);
void OffsetRect(Rect *r, short dh, short dv);
Boolean PtInRect(Point pt, const Rect *r);

void EraseRect(const Rect *r);
void FrameRect(const Rect *r);
void PaintRect(const Rect *r);
void FrameRoundRect(const Rect *r, short ovalWidth, short ovalHeight);
void PaintRoundRect(const Rect *r, short ovalWidth, short ovalHeight);
void FrameOval(const Rect *r);

void PenSize(short width, short height);
void PenNormal(void);

void MoveTo(short h, short v);
void LineTo(short h, short v);
void DrawString(ConstStr255Param s);
void DrawChar(short ch);
short StringWidth(ConstStr255Param s);
short CharWidth(short ch);
void GetFontInfo(FontInfo *info);

void TextFont(short fontID);
void TextSize(short size);
void TextFace(short face);

void RGBForeColor(const RGBColor *color);
void RGBBackColor(const RGBColor *color);
void ForeColor(long colorCode);
void BackColor(long colorCode);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_QUICKDRAW_H */
