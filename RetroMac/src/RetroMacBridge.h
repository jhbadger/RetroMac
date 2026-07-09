/* RetroMacBridge.h -- the ONLY header shared between RetroMac's plain
 * C Toolbox logic and its one Objective-C/AppKit file (CocoaBridge.m).
 *
 * Why this exists: importing <AppKit/AppKit.h> transitively drags in
 * ApplicationServices' legacy Carbon/QuickDraw compatibility headers
 * (via HIServices/QD.framework), which redefine WindowPtr, GrafPtr,
 * DialogPtr, RGBColor, BitMap, `struct GrafPort`, TickCount() and the
 * HiWord/LoWord macros -- the exact names RetroMac's own public
 * headers (Types.h/Quickdraw.h/...) define for classic app sources,
 * but with different (opaque/incompatible) underlying types. The two
 * cannot coexist in one translation unit. So: every function crossing
 * the C/Objective-C boundary here uses only names that are safe in
 * *both* worlds -- Point/Rect/Boolean (defined identically by
 * <MacTypes.h>, no collision), plain integers, `void *` opaque
 * handles, and CGContextRef. CocoaBridge.m includes only this header
 * (plus AppKit); every other RetroMac source includes this header via
 * RetroMacInternal.h.
 */
#ifndef RETROMAC_BRIDGE_H
#define RETROMAC_BRIDGE_H

#include <MacTypes.h>
#include <CoreGraphics/CoreGraphics.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared by both sides of the boundary, so window/chrome geometry
 * math agrees whichever side computes it. */
#define RM_TITLEBAR_HEIGHT 20
#define RM_MENUBAR_HEIGHT  20

/* Uniform "everything is bigger on modern screens" scale factor: the
 * entire classic coordinate universe (screen bounds, window
 * positions/sizes, mouse points) is a factor of RM_DISPLAY_SCALE
 * smaller than real screen points -- exactly like a Retina
 * backingScaleFactor, applied at the same classic/native boundary
 * this header exists to isolate. Classic app code and every plain-C
 * Toolbox file (QuickDraw.c/ControlManager.c/TextEditManager.c/
 * DialogManager.c) stay entirely unaware of it: they keep computing
 * in ordinary classic units, and CocoaBridge.m converts at the edges
 * (window frame creation, mouse point conversion) while WindowManager.c's
 * NewWindow pre-scales each window's offscreen CGContext's CTM once at
 * creation so ordinary classic-unit CGContext/Core Text drawing calls
 * land at the right physical pixel density automatically -- see
 * Toolbox.md section 11 for why this needed no changes at all to the
 * actual drawing code in QuickDraw.c/ControlManager.c/TextEditManager.c/
 * DialogManager.c. */
#define RM_DISPLAY_SCALE 2

/* ==== C (Toolbox logic) calling into Objective-C (CocoaBridge.m) ==== */

void RMCocoa_Init(void); /* NSApplication bootstrap; called once from AppShell.m */

/* Windows */
void *RMCocoa_CreateWindow(Rect contentBoundsGlobal, int hasTitleBar, void *ownerGrafPort, void **outView);
void  RMCocoa_DestroyWindow(void *nsWindowRef, void *nsViewRef);
void  RMCocoa_OrderFront(void *nsWindowRef);
void  RMCocoa_OrderOut(void *nsWindowRef);
void  RMCocoa_MarkNeedsDisplayAndFlush(void *nsViewRef);
Rect  RMCocoa_MainScreenBoundsClassic(void); /* classic global (top-down) coords */
Point RMCocoa_WindowContentOriginClassic(void *nsWindowRef, int hasTitleBar); /* re-derive after a drag */

/* Generic mouse tracking: blocks pumping the run loop until the
 * button is released. Points are in classic global (top-down) coords.
 * `onMove`/`onUp` may be NULL. */
void RMCocoa_TrackMouse(void (*onMove)(Point globalPt, void *ctx),
                        void (*onUp)(Point globalPt, void *ctx),
                        void *ctx);

/* Moves an already-created window by a screen-space delta (used by
 * DragWindow's tracking loop, via RMCocoa_TrackMouse's onMove). */
void RMCocoa_MoveWindowByScreenDelta(void *nsWindowRef, double dxScreen, double dyScreen);
Point RMCocoa_CurrentMouseLocationClassic(void);

/* Events */
int  RMCocoa_WaitNextEvent(unsigned long timeoutTicks, short *outWhat, long *outMessage,
                            Point *outWhere, short *outModifiers);
void RMCocoa_FlushEvents(void);

/* Menus */
void *RMCocoa_CreateMenu(const char *utf8Title, int isAppleMenu);
void  RMCocoa_AppendMenuItem(void *nsMenuRef, const char *utf8Title, char cmdKeyOrZero,
                              int isSeparator, int enabled, short menuID, short itemIndex1Based);
void  RMCocoa_InsertMenuIntoBar(void *nsMenuRef);
void  RMCocoa_RebuildMenuBar(void);

/* ==== Objective-C (CocoaBridge.m) calling back into plain C ==== */

/* Implemented in WindowManager.c; called by RMContentView's drawRect:. */
CGContextRef RM_WindowBuffer(void *grafPort);
int  RM_WindowInUse(void *grafPort);
int  RM_WindowHasTitleBar(void *grafPort);
int  RM_WindowHasGoAway(void *grafPort);
int  RM_WindowWidth(void *grafPort);
int  RM_WindowHeight(void *grafPort);
void RM_WindowTitleUTF8(void *grafPort, char *out, unsigned long outSize);

/* Implemented in MenuManager.c; called by an NSMenuItem's action. */
void RM_MenuItemChosen(short menuID, short itemIndex1Based);

/* Real menu-bar clicks are intercepted by AppKit's own internal
 * tracking and never surface as a plain NSEventTypeLeftMouseDown
 * through nextEventMatchingMask -- but the chosen item's target-action
 * (RM_MenuItemChosen above) still fires normally. So RMCocoa_WaitNextEvent
 * polls this to notice a selection happened and synthesizes the
 * mouseDown-in-menu-bar event the classic app's own FindWindow/
 * MenuSelect call chain expects instead of trying to catch a real one. */
int RM_HasPendingMenuSelection(void);

/* Implemented in ResourceManager.c; called once from AppShell.m (after
 * RMCocoa_Init, before RetroMacUserMain) with the .app bundle's
 * Contents/Resources/Resources.rsrc path. Silent no-op if the path
 * doesn't exist -- apps with no sibling .r file (every Phase 0/1
 * sample) are completely unaffected. */
void RM_LoadAppResourceFile(const char *path);

/* The classic app's real main(), renamed to this by retromacc's
 * -Dmain=RetroMacUserMain so AppShell.m can own the real process
 * main() and boot Cocoa first. */
extern int RetroMacUserMain(void);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_BRIDGE_H */
