/*
 * rezdemo.c -- Phase 2 acceptance test for RetroMac's Resource
 * Manager (Toolbox.md section 9's convention: each new sample
 * exercises whatever new Toolbox surface it introduces).
 *
 * Unlike wordle.c/demo.c/dialogdemo.c, this app builds no UI
 * programmatically at all -- GetNewWindow, GetMenu, GetNewDialog, and
 * NoteAlert all load their layout from rezdemo.r's WIND/MENU/DLOG+
 * DITL/ALRT+DITL resources.
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
#include <Resources.h>
#include <string.h>

QDGlobals qd;

enum { kAppleMenuID = 128, kFileMenuID = 129 };
enum { kAppleAbout = 1 };
enum { kFileQuit = 1 };

static WindowPtr gMainWindow;
static Boolean   gDone = false;

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
    MenuHandle appleMenu = GetMenu(kAppleMenuID);
    InsertMenu(appleMenu, 0);

    MenuHandle fileMenu = GetMenu(kFileMenuID);
    InsertMenu(fileMenu, 0);

    DrawMenuBar();
}

static void SetupWindow(void)
{
    gMainWindow = GetNewWindow(128, NULL, (WindowPtr)-1L);
    SetPort(gMainWindow);
    DrawMainWindow();
}

static void DrawMainWindow(void)
{
    Rect content;
    SetRect(&content, 0, 0, 400, 260);
    EraseRect(&content);

    MoveTo(20, 30);
    DrawString("\pThis window came from a WIND resource.");
    MoveTo(20, 50);
    DrawString("\pTry About RezDemo... in the Apple menu.");
}

static void HandleMouseDown(EventRecord *ev)
{
    WindowPtr window;
    short part = FindWindow(ev->where, &window);

    switch (part) {
        case inMenuBar:
            DoMenuCommand(MenuSelect(ev->where));
            break;
        case inDrag: {
            Rect screen = qd.screenBits.bounds;
            DragWindow(window, ev->where, &screen);
            break;
        }
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
    }
    HiliteMenu(0);
}

static void DoAboutDialog(void)
{
    DialogPtr dlg = GetNewDialog(128, NULL, (WindowPtr)-1L);
    short itemHit = 0;
    ModalDialog(NULL, &itemHit);
    DisposeWindow(dlg);
    SetPort(gMainWindow);

    if (itemHit == 1) { /* OK -- DITL 128's item 1, per its resource order */
        ParamText("\pYou clicked OK in a resource-driven dialog.", "\p", "\p", "\p");
        NoteAlert(128, NULL);
    }
}
