/* CocoaBridge.m -- the ONLY file in RetroMac that imports AppKit.
 *
 * See RetroMacBridge.h for why: AppKit transitively drags in legacy
 * Carbon/QuickDraw compatibility headers that redefine WindowPtr,
 * RGBColor, BitMap, etc. with different (opaque) types than
 * RetroMac's own classic-Toolbox headers use. Every function here
 * therefore speaks only Point/Rect/Boolean/int/double/void-pointers/
 * CGContextRef across the boundary -- never RetroMac's own WindowPtr/
 * MenuHandle/ControlHandle/RGBColor types.
 *
 * Windows are deliberately borderless NSWindows with a hand-drawn
 * titlebar + close box (see RMContentView.drawRect:) rather than
 * native chrome -- the classic apps this runtime targets already
 * implement their own FindWindow/DragWindow/TrackGoAway and expect to
 * own that interaction themselves (Toolbox.md section 2/section 6). The one place
 * real AppKit UI is used as-is is the menu bar (NSMenu), since
 * reimplementing menu tracking by hand would be pure busywork with a
 * real Cocoa menu bar sitting right there.
 */
#import <Cocoa/Cocoa.h>
#include "RetroMacBridge.h"
#include <ctype.h>
#include <stdio.h>

/* ==== RMWindow / RMContentView ========================================= */

@interface RMWindow : NSWindow
@end

@implementation RMWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface RMContentView : NSView
@property (nonatomic, assign) void *grafPort;
@end

@implementation RMContentView

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    void *w = self.grafPort;
    if (!w || !RM_WindowInUse(w)) return;

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    int hasTitleBar = RM_WindowHasTitleBar(w);
    int hasGoAway   = RM_WindowHasGoAway(w);
    /* RM_WindowWidth/Height report classic (unscaled) units -- this
     * view itself was sized at RM_DISPLAY_SCALE times that (see
     * RMCocoa_CreateWindow), so all native-view-space chrome geometry
     * here needs the same scale-up. The offscreen `buffer` blitted
     * below is already RM_DISPLAY_SCALE-times-classic pixels natively
     * (WindowManager.c's NewWindow), so its destination rect below
     * needs the same scaled width/height for a crisp 1:1 blit. */
    int width       = RM_WindowWidth(w) * RM_DISPLAY_SCALE;
    int height      = RM_WindowHeight(w) * RM_DISPLAY_SCALE;
    int titleBarH   = hasTitleBar ? RM_TITLEBAR_HEIGHT * RM_DISPLAY_SCALE : 0;

    if (hasTitleBar) {
        NSRect bar = NSMakeRect(0, 0, width, titleBarH);
        [[NSColor colorWithWhite:0.85 alpha:1.0] setFill];
        NSRectFill(bar);

        char titleUTF8[1024];
        RM_WindowTitleUTF8(w, titleUTF8, sizeof(titleUTF8));
        NSString *titleStr = [NSString stringWithUTF8String:titleUTF8];
        NSDictionary *attrs = @{
            NSFontAttributeName: [NSFont boldSystemFontOfSize:12 * RM_DISPLAY_SCALE],
            NSForegroundColorAttributeName: [NSColor blackColor]
        };
        NSSize sz = [titleStr sizeWithAttributes:attrs];
        NSPoint p = NSMakePoint((width - sz.width) / 2.0, (titleBarH - sz.height) / 2.0);
        [titleStr drawAtPoint:p withAttributes:attrs];

        if (hasGoAway) {
            CGFloat boxSide = 12 * RM_DISPLAY_SCALE;
            NSRect box = NSMakeRect(6 * RM_DISPLAY_SCALE, (titleBarH - boxSide) / 2.0, boxSide, boxSide);
            [[NSColor whiteColor] setFill];
            NSRectFill(box);
            [[NSColor blackColor] setStroke];
            NSFrameRect(box);
        }
    }

    CGContextRef buffer = RM_WindowBuffer(w);
    if (buffer) {
        CGImageRef img = CGBitmapContextCreateImage(buffer);
        if (img) {
            /* `buffer` is a plain native (bottom-left origin, y-up)
             * CGBitmapContext; this view is flipped (y-down), and
             * CGContextDrawImage does NOT auto-correct for that, so
             * without a counter-flip here the content renders upside
             * down. */
            CGContextSaveGState(ctx);
            CGContextTranslateCTM(ctx, 0, titleBarH + height);
            CGContextScaleCTM(ctx, 1, -1);
            CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), img);
            CGContextRestoreGState(ctx);
            CGImageRelease(img);
        }
    }

    CGContextSetRGBStrokeColor(ctx, 0, 0, 0, 1);
    CGContextSetLineWidth(ctx, 1);
    CGContextStrokeRect(ctx, CGRectMake(0.5, 0.5, width - 1, height + titleBarH - 1));
}

@end

/* ==== Menu item action bridge =========================================== */

@interface RMMenuTarget : NSObject
- (void)itemChosen:(id)sender;
@end

@implementation RMMenuTarget
- (void)itemChosen:(id)sender
{
    NSMenuItem *item = (NSMenuItem *)sender;
    long tag = item.tag; /* packed (menuID << 16) | itemIndex1Based */
    RM_MenuItemChosen((short)((tag >> 16) & 0xFFFF), (short)(tag & 0xFFFF));
}
@end

static RMMenuTarget *sMenuTarget = nil;

/* ==== coordinate helpers ================================================ */

static double MainScreenHeight(void)
{
    NSScreen *s = [NSScreen mainScreen];
    return s ? s.frame.size.height : 900.0;
}

static Point ClassicPointFromScreenPoint(NSPoint p)
{
    Point pt;
    pt.h = (short)(p.x / RM_DISPLAY_SCALE);
    pt.v = (short)((MainScreenHeight() - p.y) / RM_DISPLAY_SCALE);
    return pt;
}

/* Translates Cocoa's modifierFlags into classic EventRecord.modifiers
 * bits (Events.h) -- shared by every event type RMCocoa_WaitNextEvent
 * reports, so shift-click-to-extend (TEClick's `extend` param, driven
 * by shiftKey) and Cmd-key menu equivalents both see the real state
 * instead of just Command. */
static short RMClassicModifiers(NSEventModifierFlags flags)
{
    short m = 0;
    if (flags & NSEventModifierFlagCommand)   m |= 0x0100; /* cmdKey */
    if (flags & NSEventModifierFlagShift)     m |= 0x0200; /* shiftKey */
    if (flags & NSEventModifierFlagOption)    m |= 0x0800; /* optionKey */
    if (flags & NSEventModifierFlagControl)   m |= 0x1000; /* controlKey */
    return m;
}

/* ==== bootstrap ========================================================= */

void RMCocoa_Init(void)
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        sMenuTarget = [RMMenuTarget new];
        [NSApp finishLaunching];
        /* finishLaunching auto-populates mainMenu with a placeholder
         * "application menu" item (bold app name, empty submenu) --
         * discard it so the classic app's own InsertMenu calls land
         * at index 0, matching classic Menu Manager ordering exactly
         * (Apple menu first, as these apps expect). */
        NSApp.mainMenu = [[NSMenu alloc] initWithTitle:@""];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

void RMCocoa_Beep(void)
{
    NSBeep();
}

/* ==== windows ============================================================ */

Rect RMCocoa_MainScreenBoundsClassic(void)
{
    NSScreen *s = [NSScreen mainScreen];
    NSRect f = s ? s.frame : NSMakeRect(0, 0, 1440, 900);
    Rect r;
    r.top = 0;
    r.left = 0;
    /* Reported in classic units -- RM_DISPLAY_SCALE smaller than the
     * real screen -- so apps centering a window against qd.screenBits.
     * bounds land correctly once RMCocoa_CreateWindow scales their
     * result back up. */
    r.right = (short)(f.size.width / RM_DISPLAY_SCALE);
    r.bottom = (short)(f.size.height / RM_DISPLAY_SCALE);
    return r;
}

void *RMCocoa_CreateWindow(Rect contentBoundsGlobal, int hasTitleBar, void *ownerGrafPort, void **outView)
{
    int width  = contentBoundsGlobal.right - contentBoundsGlobal.left;
    int height = contentBoundsGlobal.bottom - contentBoundsGlobal.top;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    int titleBarH = hasTitleBar ? RM_TITLEBAR_HEIGHT : 0;

    /* contentBoundsGlobal/width/height/titleBarH are all classic
     * units; the real on-screen frame is RM_DISPLAY_SCALE times
     * bigger in both position and size, uniformly, so the classic
     * app's own placement math (e.g. NewWindow bounds literals) scales
     * along with everything else -- see RetroMacBridge.h.
     *
     * contentBoundsGlobal.top is the CONTENT top (WindowManager.c's
     * contentOriginGlobal, per NewWindow), and the title bar sits
     * ABOVE that -- so the frame's structural top has to be placed at
     * (contentBoundsGlobal.top - titleBarH), not at contentBoundsGlobal.top
     * itself. Do not add titleBarH again here (on top of it already
     * being folded into frame.size.height below) -- that was a real
     * bug: it pushed the whole frame titleBarH-classic-units further
     * down the screen than FindWindow's hit-testing model (which does
     * expect the content to start exactly at contentBoundsGlobal.top)
     * assumes, so a real click on the visible title bar always fell
     * outside the pt.v < contentOriginGlobal.v test and got
     * classified as inContent instead of inDrag/inGoAway -- see
     * Toolbox.md section 12. */
    double screenH = MainScreenHeight();
    NSRect frame = NSMakeRect(contentBoundsGlobal.left * RM_DISPLAY_SCALE,
                              screenH - (contentBoundsGlobal.top + height) * RM_DISPLAY_SCALE,
                              width * RM_DISPLAY_SCALE, (height + titleBarH) * RM_DISPLAY_SCALE);

    RMWindow *win = [[RMWindow alloc] initWithContentRect:frame
                                                  styleMask:NSWindowStyleMaskBorderless
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
    win.opaque = YES;
    win.hasShadow = YES;
    win.backgroundColor = [NSColor whiteColor];
    win.releasedWhenClosed = NO;

    RMContentView *view = [[RMContentView alloc] initWithFrame:NSMakeRect(0, 0, width * RM_DISPLAY_SCALE,
                                                                           (height + titleBarH) * RM_DISPLAY_SCALE)];
    view.grafPort = ownerGrafPort;
    win.contentView = view;

    if (outView) *outView = (void *)CFBridgingRetain(view);
    return (void *)CFBridgingRetain(win);
}

void RMCocoa_DestroyWindow(void *nsWindowRef, void *nsViewRef)
{
    if (nsWindowRef) {
        NSWindow *win = (__bridge NSWindow *)nsWindowRef;
        [win orderOut:nil];
        win.contentView = nil;
        CFBridgingRelease(nsWindowRef);
    }
    if (nsViewRef) {
        CFBridgingRelease(nsViewRef);
    }
}

void RMCocoa_OrderFront(void *nsWindowRef)
{
    if (!nsWindowRef) return;
    [(__bridge NSWindow *)nsWindowRef makeKeyAndOrderFront:nil];
}

void RMCocoa_OrderOut(void *nsWindowRef)
{
    if (!nsWindowRef) return;
    [(__bridge NSWindow *)nsWindowRef orderOut:nil];
}

void RMCocoa_MarkNeedsDisplayAndFlush(void *nsViewRef)
{
    if (!nsViewRef) return;
    NSView *view = (__bridge NSView *)nsViewRef;
    view.needsDisplay = YES;
    [view displayIfNeeded];
}

Point RMCocoa_WindowContentOriginClassic(void *nsWindowRef, int hasTitleBar)
{
    Point pt = {0, 0};
    if (!nsWindowRef) return pt;
    NSWindow *win = (__bridge NSWindow *)nsWindowRef;
    NSRect f = win.frame;
    double screenH = MainScreenHeight();
    /* contentOriginGlobal (what this feeds) is documented as the
     * CONTENT area's top-left -- f.size.height is the whole frame
     * (content + titlebar), so the titlebar strip has to be added back
     * to land on the content top, not the structure top. */
    double titleBarHReal = hasTitleBar ? RM_TITLEBAR_HEIGHT * RM_DISPLAY_SCALE : 0;
    pt.h = (short)(f.origin.x / RM_DISPLAY_SCALE);
    pt.v = (short)((screenH - f.origin.y - f.size.height + titleBarHReal) / RM_DISPLAY_SCALE);
    return pt;
}

void RMCocoa_MoveWindowByScreenDelta(void *nsWindowRef, double dxScreen, double dyScreen)
{
    if (!nsWindowRef) return;
    NSWindow *win = (__bridge NSWindow *)nsWindowRef;
    NSRect f = win.frame;
    /* Despite the name, callers (WindowManager.c's DragWindow) compute
     * this delta between two already-classic-unit points, so it has to
     * be scaled back up to move the real (RM_DISPLAY_SCALE-times-bigger)
     * NSWindow by the equivalent real distance. */
    f.origin.x += dxScreen * RM_DISPLAY_SCALE;
    f.origin.y += dyScreen * RM_DISPLAY_SCALE;
    [win setFrameOrigin:f.origin];
}

Point RMCocoa_CurrentMouseLocationClassic(void)
{
    return ClassicPointFromScreenPoint([NSEvent mouseLocation]);
}

/* ==== mouse tracking ===================================================== */

void RMCocoa_TrackMouse(void (*onMove)(Point globalPt, void *ctx),
                        void (*onUp)(Point globalPt, void *ctx),
                        void *ctx)
{
    for (;;) {
        NSEvent *e = [NSApp nextEventMatchingMask:(NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp)
                                         untilDate:[NSDate distantFuture]
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES];
        if (!e) continue;
        Point gp = ClassicPointFromScreenPoint([NSEvent mouseLocation]);
        if (e.type == NSEventTypeLeftMouseUp) {
            if (onUp) onUp(gp, ctx);
            break;
        } else if (onMove) {
            onMove(gp, ctx);
        }
    }
}

/* ==== events ============================================================== */

int RMCocoa_WaitNextEvent(unsigned long timeoutTicks, short *outWhat, long *outMessage,
                           Point *outWhere, short *outModifiers)
{
    NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:(double)timeoutTicks / 60.0];
    const NSTimeInterval sliceInterval = 0.05; /* poll in short slices so a menu
                                                 * selection completed by AppKit's
                                                 * own tracking (see below) is
                                                 * noticed promptly rather than
                                                 * only after a long single wait */

    for (;;) {
        if (RM_HasPendingMenuSelection()) {
            /* Real menu-bar clicks are intercepted by AppKit's own
             * internal tracking and never surface as a plain
             * NSEventTypeLeftMouseDown here -- but the chosen item's
             * target-action (RM_MenuItemChosen) still fires normally,
             * which is what set this. Synthesize the mouseDown-in-
             * menu-bar event the classic app's own FindWindow/
             * MenuSelect call chain expects.
             *
             * If the current event loop never calls MenuSelect (e.g. a
             * modal dialog's own simplified loop, which classic apps
             * often write without menu handling) this value never gets
             * consumed/cleared, and we'd otherwise re-synthesize it on
             * every call with zero delay -- a 100%-CPU spin. Pace this
             * path to the same ~20Hz slice rate as normal polling so a
             * never-consumed selection just idles harmlessly instead. */
            [NSThread sleepForTimeInterval:sliceInterval];
            if (outWhat) *outWhat = 1 /* mouseDown */;
            if (outMessage) *outMessage = 0;
            if (outWhere) { outWhere->h = 0; outWhere->v = 0; }
            if (outModifiers) *outModifiers = 0;
            return 1;
        }

        if ([deadline timeIntervalSinceNow] <= 0) return 0; /* timed out: nullEvent */

        NSDate *sliceDeadline = [NSDate dateWithTimeIntervalSinceNow:sliceInterval];
        if ([sliceDeadline compare:deadline] == NSOrderedDescending) sliceDeadline = deadline;

        NSEvent *e = [NSApp nextEventMatchingMask:NSEventMaskAny
                                         untilDate:sliceDeadline
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES];
        if (!e) continue; /* slice elapsed; loop re-checks pending selection + deadline */

        if (e.type == NSEventTypeLeftMouseDown) {
            Point gp = ClassicPointFromScreenPoint([NSEvent mouseLocation]);
            if (outWhat) *outWhat = 1 /* mouseDown */;
            if (outMessage) *outMessage = 0;
            if (outWhere) *outWhere = gp;
            if (outModifiers) *outModifiers = RMClassicModifiers(e.modifierFlags);
            return 1;
        }

        if (e.type == NSEventTypeKeyDown) {
            NSString *chars = e.characters;
            unichar ch = chars.length > 0 ? [chars characterAtIndex:0] : 0;
            /* Cocoa reports Return as CR (13) already; map Delete (0x7F)
             * back to the classic backspace code (8) that these apps
             * check for. Real classic TextEdit/Toolbox apps already
             * receive arrow keys as charCodes 0x1C-0x1F (left/right/
             * up/down) rather than via keyCodeMask, so map Cocoa's
             * arrow-key function-key unichars to those same codes
             * instead of plumbing keyCode through EventRecord. */
            if (ch == 0x7F) ch = 8;
            else if (ch == NSLeftArrowFunctionKey)  ch = 0x1C;
            else if (ch == NSRightArrowFunctionKey) ch = 0x1D;
            else if (ch == NSUpArrowFunctionKey)    ch = 0x1E;
            else if (ch == NSDownArrowFunctionKey)  ch = 0x1F;
            if (outWhat) *outWhat = 3 /* keyDown */;
            if (outMessage) *outMessage = (long)(unsigned char)ch;
            if (outWhere) *outWhere = ClassicPointFromScreenPoint([NSEvent mouseLocation]);
            if (outModifiers) *outModifiers = RMClassicModifiers(e.modifierFlags);
            return 1;
        }

        /* Anything else (mouse moved, app-activation, real menu-bar
         * tracking's own internal events, etc.) -- let AppKit process
         * it normally and keep waiting for something the classic
         * event loop cares about. */
        [NSApp sendEvent:e];
    }
}

void RMCocoa_FlushEvents(void)
{
    for (;;) {
        NSEvent *e = [NSApp nextEventMatchingMask:NSEventMaskAny
                                         untilDate:[NSDate distantPast]
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES];
        if (!e) break;
    }
}

/* ==== menus =============================================================== */

void *RMCocoa_CreateMenu(const char *utf8Title, int isAppleMenu)
{
    NSString *title = isAppleMenu ? @"" : [NSString stringWithUTF8String:utf8Title];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:title];
    menu.autoenablesItems = NO;
    return (void *)CFBridgingRetain(menu);
}

void RMCocoa_AppendMenuItem(void *nsMenuRef, const char *utf8Title, char cmdKeyOrZero,
                             int isSeparator, int enabled, short menuID, short itemIndex1Based)
{
    if (!nsMenuRef) return;
    NSMenu *menu = (__bridge NSMenu *)nsMenuRef;

    if (isSeparator) {
        [menu addItem:[NSMenuItem separatorItem]];
        return;
    }

    NSString *title = [NSString stringWithUTF8String:utf8Title];
    NSString *key = cmdKeyOrZero ? [[NSString stringWithFormat:@"%c", (char)tolower((int)cmdKeyOrZero)] lowercaseString] : @"";
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(itemChosen:) keyEquivalent:key];
    item.target = sMenuTarget;
    item.enabled = enabled ? YES : NO;
    item.tag = ((long)menuID << 16) | (long)itemIndex1Based;
    [menu addItem:item];
}

void RMCocoa_InsertMenuIntoBar(void *nsMenuRef)
{
    if (!nsMenuRef) return;
    NSMenu *menu = (__bridge NSMenu *)nsMenuRef;

    if (!NSApp.mainMenu) {
        NSApp.mainMenu = [[NSMenu alloc] initWithTitle:@""];
    }
    NSMenuItem *carrier = [[NSMenuItem alloc] initWithTitle:menu.title action:NULL keyEquivalent:@""];
    carrier.submenu = menu;
    [NSApp.mainMenu addItem:carrier];
}

void RMCocoa_RebuildMenuBar(void)
{
    /* NSApp.mainMenu is already live the moment each menu is inserted;
     * nothing to flush explicitly. */
}
