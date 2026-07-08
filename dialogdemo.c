/*
 * dialogdemo.c -- Phase 1 acceptance test for RetroMac's Dialog
 * Manager + TextEdit (Toolbox.md section 9's convention: each new
 * sample app exercises whatever new Toolbox surface it introduces).
 *
 * Demo menu:
 *   Edit Text...  -- opens a NewDialog with a TENew text field
 *                    (pre-filled via TESetText) plus OK/Cancel
 *                    buttons; ModalDialog + TEGetText round-trips the
 *                    typed text back into the main window.
 *   Show Alert    -- fires NoteAlert with a ParamText-supplied message.
 * Apple menu's About box is itself built with NewDialog/ModalDialog
 * rather than a hand-rolled event loop, to exercise the plain (no
 * TextEdit) dialog path too.
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
#include <string.h>

QDGlobals qd;

enum {
    kAppleMenuID = 128,
    kFileMenuID  = 129,
    kDemoMenuID  = 130
};

enum { kAppleAbout = 1 };
enum { kFileQuit = 1 };
enum { kDemoEditText = 1, kDemoShowAlert = 3 }; /* item 2 is the separator */

static MenuHandle gAppleMenu, gFileMenu, gDemoMenu;
static WindowPtr  gMainWindow;
static Boolean    gDone = false;
static char       gSavedText[256] = "Hello, RetroMac!";

static void SetupMenus(void);
static void SetupWindow(void);
static void DrawMainWindow(void);
static void DoMenuCommand(long menuResult);
static void HandleMouseDown(EventRecord *ev);
static void DoAboutDialog(void);
static void DoEditTextDialog(void);
static void DoShowAlert(void);

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
    AppendMenu(gAppleMenu, "\pAbout DialogDemo\311");  /* \311 = ellipsis (0xC9) */
    AppendMenu(gAppleMenu, "\p(-");
    InsertMenu(gAppleMenu, 0);

    gFileMenu = NewMenu(kFileMenuID, "\pFile");
    AppendMenu(gFileMenu, "\pQuit/Q");
    InsertMenu(gFileMenu, 0);

    gDemoMenu = NewMenu(kDemoMenuID, "\pDemo");
    AppendMenu(gDemoMenu, "\pEdit Text\311/E");
    AppendMenu(gDemoMenu, "\p(-");
    AppendMenu(gDemoMenu, "\pShow Alert/A");
    InsertMenu(gDemoMenu, 0);

    DrawMenuBar();
}

static void SetupWindow(void)
{
    Rect bounds;
    SetRect(&bounds, 60, 60, 460, 320);
    gMainWindow = NewWindow(NULL, &bounds, "\pDialogDemo", true,
                            documentProc, (WindowPtr)-1L, true, 0);
    SetPort(gMainWindow);
    DrawMainWindow();
}

static void DrawMainWindow(void)
{
    Rect content;
    unsigned char pstr[256];
    short len;

    SetRect(&content, 0, 0, 400, 260);
    EraseRect(&content);

    MoveTo(20, 30);
    DrawString("\pDialogDemo -- try the Demo menu's Edit Text... and Show Alert.");

    MoveTo(20, 60);
    DrawString("\pCurrent text:");

    len = (short)strlen(gSavedText);
    pstr[0] = (unsigned char)len;
    memcpy(&pstr[1], gSavedText, (size_t)len);
    MoveTo(20, 80);
    DrawString(pstr);
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
        case kDemoMenuID:
            if (menuItem == kDemoEditText) DoEditTextDialog();
            else if (menuItem == kDemoShowAlert) DoShowAlert();
            break;
    }
    HiliteMenu(0);
}

static void DoAboutDialog(void)
{
    Rect winBounds, btnRect;
    DialogPtr aboutWin;
    short itemHit = 0;

    SetRect(&winBounds, 120, 100, 420, 270);
    aboutWin = NewDialog(NULL, &winBounds, "\pAbout DialogDemo", true,
                          dBoxProc, (WindowPtr)-1L, false, 0, NULL);
    SetPort(aboutWin);

    MoveTo(20, 40); DrawString("\pDialogDemo 1.0");
    MoveTo(20, 60); DrawString("\pExercises RetroMac's Phase 1 Dialog Manager + TextEdit.");
    MoveTo(20, 80); DrawString("\pNo resource fork required.");

    SetRect(&btnRect, 130, 105, 190, 127);
    NewControl(aboutWin, &btnRect, "\pOK", true, 0, 0, 1, pushButProc, 0);

    ModalDialog(NULL, &itemHit);

    DisposeWindow(aboutWin);
    SetPort(gMainWindow);
}

static void DoEditTextDialog(void)
{
    Rect winBounds, fieldRect, okRect, cancelRect;
    DialogPtr dlg;
    TEHandle field;
    short itemHit = 0;

    SetRect(&winBounds, 100, 100, 460, 240);
    dlg = NewDialog(NULL, &winBounds, "\pEdit Text", true,
                     dBoxProc, (WindowPtr)-1L, false, 0, NULL);
    SetPort(dlg);

    MoveTo(20, 30);
    DrawString("\pType some text, then click OK or Cancel:");

    SetRect(&fieldRect, 20, 45, 340, 67);
    field = TENew(&fieldRect, &fieldRect);
    TESetText(gSavedText, (long)strlen(gSavedText), field);
    TEActivate(field);

    SetRect(&okRect, 190, 90, 250, 112);
    NewControl(dlg, &okRect, "\pOK", true, 0, 0, 1, pushButProc, 0);       /* item 1 -- default */
    SetRect(&cancelRect, 260, 90, 340, 112);
    NewControl(dlg, &cancelRect, "\pCancel", true, 0, 0, 1, pushButProc, 0); /* item 2 */

    ModalDialog(NULL, &itemHit);

    if (itemHit == 1) {
        Handle h = TEGetText(field);
        long len = GetHandleSize(h);
        if (len > (long)sizeof(gSavedText) - 1) len = (long)sizeof(gSavedText) - 1;
        memcpy(gSavedText, *h, (size_t)len);
        gSavedText[len] = 0;
        DisposeHandle(h);
    }

    TEDispose(field);
    DisposeWindow(dlg);
    SetPort(gMainWindow);
    DrawMainWindow();
}

static void DoShowAlert(void)
{
    unsigned char msg[256];
    const char *text = "This message came from ParamText -- no ALRT resource needed.";
    short len = (short)strlen(text);

    msg[0] = (unsigned char)len;
    memcpy(&msg[1], text, (size_t)len);
    ParamText(msg, "\p", "\p", "\p");
    NoteAlert(0, NULL);
}
