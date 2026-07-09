/* RetroMacInternal.h -- private runtime state shared by RetroMac's
 * implementation files. Not installed as a public Toolbox header;
 * classic app sources never see this. Safe to include from both
 * plain C (.c) and Objective-C (.m) translation units -- it only
 * uses C types plus opaque `void *` for Cocoa objects, so the plain
 * C files (QuickDraw.c, ControlManager.c, MacRoman.c) don't need to
 * know about Objective-C at all.
 */
#ifndef RETROMAC_INTERNAL_H
#define RETROMAC_INTERNAL_H

#include "../include/Types.h"
#include "../include/Quickdraw.h"
#include "../include/Events.h"
#include "RetroMacBridge.h"
#include <CoreGraphics/CoreGraphics.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RM_MAX_WINDOWS     16
#define RM_MAX_MENUS       16
#define RM_MAX_MENU_ITEMS  32
/* RM_MAX_CONTROLS / RM_MAX_TE_FIELDS now live in Quickdraw.h (public --
 * GrafPort's inline control/TE-field arrays need them there).
 * RM_TITLEBAR_HEIGHT / RM_MENUBAR_HEIGHT come from RetroMacBridge.h */

/* ---- Windows ----------------------------------------------------------
 * WindowPtr == GrafPtr == &gWindows[i]; these live in a fixed pool
 * rather than being malloc'd individually, so ControlHandles (which
 * point into owner->controls[]) stay valid for the window's lifetime
 * without a separate allocator. struct GrafPort itself is now fully
 * defined in the public Quickdraw.h (see there for why) -- this file
 * just owns the pool. */

extern struct GrafPort gWindows[RM_MAX_WINDOWS];
extern WindowPtr gCurrentPort;     /* SetPort target -- QuickDraw calls go here */
extern WindowPtr gWindowStack[RM_MAX_WINDOWS]; /* front-to-back order, [0] frontmost */
extern int gWindowStackCount;

WindowPtr RM_AllocWindowSlot(void);
void RM_BringToFront(WindowPtr w);     /* stack-to-front + orderFront on screen */
void RM_RemoveFromStack(WindowPtr w);
void RM_SyncContentOriginFromFrame(WindowPtr w); /* re-derive contentOriginGlobal after a drag */
CGContextRef RM_CurrentBuffer(void); /* buffer for gCurrentPort, or NULL */

/* Implemented in TextEditManager.c; used by DialogManager.c's
 * ModalDialog to route a mouseDown to whichever TE field it landed in. */
TEHandle RM_FindTEField(WindowPtr owner, Point localPt);

/* ---- Menus ----------------------------------------------------------- */

typedef struct RMMenuItem {
    unsigned char title[256];  /* Pascal string, as given to AppendMenu (raw) */
    char      cmdKey;          /* 0 if none */
    Boolean   isSeparator;
    Boolean   enabled;
} RMMenuItem;

struct MenuRecord {
    Boolean   inUse;
    short     menuID;
    unsigned char title[256];
    RMMenuItem items[RM_MAX_MENU_ITEMS];
    short     itemCount;
    void     *nsMenu;   /* NSMenu * */
    Boolean   isAppleMenu;
};

extern struct MenuRecord gMenus[RM_MAX_MENUS];

/* Packs a pending "user chose this menu item via the real menu bar"
 * selection; consumed by the synthetic-inMenuBar-mouseDown bridge in
 * EventManager.m / MenuSelect. Single slot: AppKit's menu tracking is
 * modal, so only one selection can be pending at a time. */
extern volatile long gPendingMenuSelection; /* 0 == none, else (menuID<<16)|item */
void RM_MenuItemChosen(short menuID, short itemIndex1Based);

/* ---- Mac Roman <-> UTF-8 --------------------------------------------- */

/* Converts a Pascal string (Mac Roman encoded) into a heap-free,
 * caller-owned UTF-8 buffer written into `out` (must be >= 4*len+1
 * bytes; 256 Mac Roman bytes never exceed 1024 UTF-8 bytes + NUL). */
void RM_MacRomanPStringToUTF8(const unsigned char *pstr, char *out, size_t outSize);
void RM_MacRomanCStringToUTF8(const char *cstr, char *out, size_t outSize);
short RM_PStringLength(const unsigned char *pstr);
void RM_PStringCopy(unsigned char *dst, const unsigned char *src);

/* ---- Cross-manager glue (implemented in EventManager.m) -------------- */

void RM_MarkDirty(WindowPtr w);
void RM_FlushDirtyWindows(void);       /* called at top of WaitNextEvent; flushes buffers to screen */

/* ---- Resource Manager big-endian accessors (ResourceManager.c) -------
 * Resource fork data is always big-endian regardless of host CPU;
 * every resource-backed unpacker (GetNewWindow, GetMenu, GetNewDialog/
 * DITL, Alert/DITL) shares these rather than reimplementing them. */
unsigned short RM_ReadU16BE(const unsigned char *p);
unsigned long RM_ReadU32BE(const unsigned char *p);

/* Enumeration by type (AppendResMenu, MenuManager.c) -- theType's
 * resources in the app's own resource file, 1-based index order. */
short RM_CountResourcesOfType(ResType theType);
Boolean RM_GetIndResourceInfo(ResType theType, short index1Based, short *outResID, unsigned char *outName);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_INTERNAL_H */
