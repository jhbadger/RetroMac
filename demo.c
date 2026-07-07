/*
 * demo.c Classic Mac app with menus and About dialog.
 * Builds in the classic-vibe-mac browser playground (no resource fork).
 * All menus and the About dialog are created programmatically.
 */

#include <Types.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Menus.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Events.h>
#include <Memory.h>
#include <Controls.h>

QDGlobals qd;

enum {
    kAppleMenuID = 128,
    kFileMenuID  = 129,
    kEditMenuID  = 130
};

enum { kAppleAbout = 1 };
enum { kFileQuit = 1 };
enum { kEditUndo = 1, kEditCut = 3, kEditCopy = 4, kEditPaste = 5, kEditClear = 6 };

static MenuHandle gAppleMenu, gFileMenu, gEditMenu;
static WindowPtr  gMainWindow;
static Boolean    gDone = false;

static void SetupMenus(void);
static void SetupWindow(void);
static void DrawMainWindow(void);
static void DoMenuCommand(long menuResult);
static void HandleMouseDown(EventRecord *ev);
static void DoAboutDialog(void);

int main(void)
{
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(0);
    InitCursor();
    FlushEvents(everyEvent, 0);

    SetupMenus();
    SetupWindow();

    {
        EventRecord ev;
        while (!gDone) {
            WaitNextEvent(everyEvent, &ev, 20, NULL);
            switch (ev.what) {
                case mouseDown:
                    HandleMouseDown(&ev);
                    break;
                case keyDown:
                case autoKey:
                    if (ev.modifiers & cmdKey) {
                        long mr = MenuKey((char)(ev.message & charCodeMask));
                        if (HiWord(mr)) DoMenuCommand(mr);
                    }
                    break;
                case updateEvt:
                    BeginUpdate((WindowPtr)ev.message);
                    SetPort((WindowPtr)ev.message);
                    DrawMainWindow();
                    EndUpdate((WindowPtr)ev.message);
                    break;
            }
        }
    }
    return 0;
}

static void SetupMenus(void)
{
    /* \024 is the Apple logo in Mac Roman (0x14) */
    gAppleMenu = NewMenu(kAppleMenuID, "\p\024");
    AppendMenu(gAppleMenu, "\pAbout\311");  /* \311 = ellipsis (0xC9) */
    AppendMenu(gAppleMenu, "\p(-");
    InsertMenu(gAppleMenu, 0);

    gFileMenu = NewMenu(kFileMenuID, "\pFile");
    AppendMenu(gFileMenu, "\pQuit/Q");
    InsertMenu(gFileMenu, 0);

    gEditMenu = NewMenu(kEditMenuID, "\pEdit");
    AppendMenu(gEditMenu, "\pUndo/Z");
    AppendMenu(gEditMenu, "\p(-");
    AppendMenu(gEditMenu, "\pCut/X");
    AppendMenu(gEditMenu, "\pCopy/C");
    AppendMenu(gEditMenu, "\pPaste/V");
    AppendMenu(gEditMenu, "\pClear");
    InsertMenu(gEditMenu, 0);

    DrawMenuBar();
}

static void SetupWindow(void)
{
    Rect bounds;
    SetRect(&bounds, 60, 60, 460, 320);
    gMainWindow = NewWindow(NULL, &bounds, "\pRetro App", true,
                            documentProc, (WindowPtr)-1L, true, 0);
    SetPort(gMainWindow);
    DrawMainWindow();
}

static void DrawMainWindow(void)
{
    MoveTo(20, 30);
    DrawString("\pRetro App - use the menus or try About in the Apple menu.");
}

static void HandleMouseDown(EventRecord *ev)
{
    WindowPtr window;
    short part = FindWindow(ev->where, &window);

    switch (part) {
        case inMenuBar:
            DoMenuCommand(MenuSelect(ev->where));
            break;
        case inDrag:
            {
                Rect screen = qd.screenBits.bounds;
                DragWindow(window, ev->where, &screen);
            }
            break;
        case inGoAway:
            if (TrackGoAway(window, ev->where))
                if (window == gMainWindow) gDone = true;
            break;
        case inContent:
            if (window != FrontWindow())
                SelectWindow(window);
            break;
        case inSysWindow:
            SystemClick(ev, window);
            break;
    }
}

static void DoMenuCommand(long menuResult)
{
    short menuID   = HiWord(menuResult);
    short menuItem = LoWord(menuResult);

    switch (menuID) {
        case kAppleMenuID:
            if (menuItem == kAppleAbout) DoAboutDialog();
            break;
        case kFileMenuID:
            if (menuItem == kFileQuit) gDone = true;
            break;
        /* Edit menu: items are present but inactive in this demo */
    }
    HiliteMenu(0);
}

static void DoAboutDialog(void)
{
    WindowPtr     aboutWin;
    ControlHandle okBtn;
    Rect          winBounds, btnRect, ringRect;
    EventRecord   ev;
    Boolean       done = false;
    Point         localPt;
    short         ctlPart;
    ControlHandle hitCtl;
    char          ch;

    SetRect(&winBounds, 120, 100, 400, 270);
    aboutWin = NewWindow(NULL, &winBounds, "\pAbout Retro App", true,
                         dBoxProc, (WindowPtr)-1L, false, 0);
    SetPort(aboutWin);

    MoveTo(20, 40);  DrawString("\pRetro App 1.0");
    MoveTo(20, 60);  DrawString("\pA classic Mac application.");
    MoveTo(20, 80);  DrawString("\pBuilt in the classic-vibe-mac playground.");
    MoveTo(20, 100); DrawString("\pNo resource fork required.");

    /* OK button centered in a 280px-wide window */
    SetRect(&btnRect, 110, 125, 170, 145);
    okBtn = NewControl(aboutWin, &btnRect, "\pOK", true, 0, 0, 1, pushButProc, 0);

    /* Default button ring: 3px border, 16px corner radius */
    ringRect = btnRect;
    InsetRect(&ringRect, -4, -4);
    PenSize(3, 3);
    FrameRoundRect(&ringRect, 16, 16);
    PenNormal();

    while (!done) {
        WaitNextEvent(everyEvent, &ev, 20, NULL);
        switch (ev.what) {
            case mouseDown:
                localPt = ev.where;
                GlobalToLocal(&localPt);
                ctlPart = FindControl(localPt, aboutWin, &hitCtl);
                if (ctlPart == inButton && hitCtl == okBtn) {
                    TrackControl(hitCtl, localPt, NULL);
                    done = true;
                }
                break;
            case keyDown:
                ch = (char)(ev.message & charCodeMask);
                if (ch == 13 || ch == 3) /* Return or Enter */
                    done = true;
                break;
        }
    }

    DisposeControl(okBtn);
    DisposeWindow(aboutWin);
    SetPort(gMainWindow);
}
