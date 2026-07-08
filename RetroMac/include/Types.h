/* RetroMac Toolbox shim: Types.h
 *
 * The macOS SDK's own <MacTypes.h> (pulled in transitively by
 * CoreFoundation/CoreGraphics) still ships the basic Mac OS type
 * vocabulary -- Boolean, Point, Rect, Str255, Handle, OSErr, UInt32,
 * etc. -- as a leftover from the Carbon/QuickDraw era. Rather than
 * redefine those (and fight typedef-redefinition errors), we just
 * pull it in and add only what real 64-bit macOS dropped: RGBColor,
 * and the opaque Toolbox object handles (WindowPtr/GrafPtr/
 * MenuHandle/ControlHandle) that used to come from QuickDraw/Windows/
 * Menus/Controls and now live in the RetroMac runtime instead.
 */
#ifndef RETROMAC_TYPES_H
#define RETROMAC_TYPES_H

#include <stddef.h>
#include <stdbool.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

typedef struct RGBColor {
    unsigned short red;
    unsigned short green;
    unsigned short blue;
} RGBColor;

/* Opaque Toolbox object handles. Full definitions live inside the
 * RetroMac runtime (RetroMacInternal.h) -- classic apps only ever
 * carry these around as pointers, never dereference their fields. */
typedef struct GrafPort GrafPort;
typedef GrafPort        *GrafPtr;
typedef GrafPtr          WindowPtr;
typedef WindowPtr        DialogPtr;

typedef struct MenuRecord MenuRecord;
typedef MenuRecord       *MenuHandle;

typedef struct ControlRecord ControlRecord;
typedef ControlRecord    *ControlHandle;

typedef struct TERec TERec;
typedef TERec            *TEHandle;

#define HiWord(x) ((short)(((unsigned long)(x) >> 16) & 0xFFFFU))
#define LoWord(x) ((short)((unsigned long)(x) & 0xFFFFU))

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_TYPES_H */
