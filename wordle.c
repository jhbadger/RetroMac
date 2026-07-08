/*
 * wordle.c — Wordle clone for classic Mac (System 6-9).
 * No resource fork: all UI built programmatically.
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

#define ROWS   6
#define COLS   5
#define BOX_SZ 52
#define GAP    6
#define GRID_X 58
#define GRID_Y 60

#define EMPTY   0
#define ABSENT  1
#define PRESENT 2
#define CORRECT 3

static const char *kWords[] = {
    "CRANE","SLATE","AUDIO","RAISE","TEARS","CRATE","TRACE","STARE",
    "PLANT","BLOOD","FLOOR","CLOUD","LIGHT","NIGHT","BRAIN","TRAIN",
    "PLAIN","BLACK","BLEND","CLOCK","CLOSE","DREAM","EARTH","EARLY",
    "EAGLE","FABLE","FAIRY","GHOST","HASTE","IMAGE","JOUST","KNIFE",
    "LANCE","LASER","MAPLE","MARCH","MATCH","NERVE","OLIVE","PANIC",
    "PASTA","PIANO","PLACE","PLANE","POKER","POWER","PRESS","PRICE",
    "PRIDE","PRIME","PROBE","PROSE","PULSE","QUEEN","QUEST","QUIET",
    "RADAR","RALLY","RAVEN","REACH","REBEL","RIDER","RIFLE","RIVER",
    "ROUGH","ROUND","ROUTE","ROYAL","SAINT","SAUCE","SCALE","SCOPE",
    "SCORE","SEIZE","SERVE","SEVEN","SHADE","SHAKE","SHAME","SHAPE",
    "SHARE","SHARK","SHARP","SHEEP","SHELF","SHELL","SHOCK","SHORE",
    "SHORT","SHOUT","SINCE","SKILL","SLASH","SLAVE","SLEEP","SLICE",
    "SMALL","SMILE","SNACK","SNAKE","SNEAK","SOLAR","SORRY","SOUTH",
    "SPACE","SPARE","SPARK","SPEAK","SPEED","SPEND","SPICE","SPORT",
    "STACK","STAGE","STAIN","STAND","STARK","START","STEAK","STEAL",
    "STEAM","STEEL","STERN","STICK","STILL","STONE","STORE","STORM",
    "STORY","STOUT","STRAP","STRAW","STRIP","SUGAR","SUPER","SWAMP",
    "SWEAR","SWEEP","SWEET","SWIFT","TABLE","TEASE","TEMPO","TENSE",
    "THIEF","THING","THINK","THREE","TIGER","TIGHT","TITLE","TODAY",
    "TOKEN","TORCH","TOUCH","TOUGH","TOWEL","TOWER","TRAIL","TRAIT",
    "TRASH","TREND","TRICK","TROUT","TRUCK","TRULY","TRUNK","TRUST",
    "TRUTH","TULIP","TWICE","TWIST","UNCLE","UNDER","UNION","UNTIL",
    "UPPER","UPSET","VALID","VAULT","VENOM","VERSE","VIRAL","VIRUS",
    "VISIT","VISTA","VITAL","VIVID","VOCAL","VOTER","WASTE","WATCH",
    "WATER","WEARY","WEAVE","WEIRD","WHALE","WHEAT","WHEEL","WHILE",
    "WHOLE","WITCH","WORSE","WORST","WORTH","WOUND","WRONG","YACHT",
    "YIELD","YOUNG","YOUTH","ZESTY"
};
#define WORD_COUNT (sizeof(kWords)/sizeof(kWords[0]))

static const char *kWinMsgs[] = {
    "Genius!", "Magnificent!", "Impressive!", "Splendid!", "Great!", "Phew!"
};

static char    gAnswer[6];
static char    gGuesses[ROWS][COLS];
static int     gStates[ROWS][COLS];
static int     gRow, gCol;
static Boolean gDone, gGameOver;
static unsigned char gMsgBuf[64];

static WindowPtr  gWindow;
static MenuHandle gAppleMenu, gFileMenu;

static void InitGame(void);
static void SetupMenus(void);
static void SetupWindow(void);
static void DrawAll(void);
static void DrawCell(int row, int col);
static void DrawMessage(void);
static void SetMessage(const char *s);
static void HandleKey(char ch);
static void SubmitGuess(void);
static void DoMenuCommand(long mr);
static void HandleMouseDown(EventRecord *ev);
static void DoAboutDialog(void);
static void SetFG(unsigned short r, unsigned short g, unsigned short b);
static void SetBG(unsigned short r, unsigned short g, unsigned short b);

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

    InitGame();
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
                    {
                        char ch = (char)(ev.message & charCodeMask);
                        if (ev.modifiers & cmdKey) {
                            long mr = MenuKey(ch);
                            if (HiWord(mr)) DoMenuCommand(mr);
                        } else {
                            HandleKey(ch);
                        }
                    }
                    break;
                case updateEvt:
                    BeginUpdate((WindowPtr)ev.message);
                    SetPort((WindowPtr)ev.message);
                    DrawAll();
                    EndUpdate((WindowPtr)ev.message);
                    break;
            }
        }
    }
    return 0;
}

static void InitGame(void)
{
    int i, j;
    int idx = (int)((unsigned long)TickCount() % WORD_COUNT);

    for (i = 0; i < 5; i++) gAnswer[i] = kWords[idx][i];
    gAnswer[5] = '\0';

    for (i = 0; i < ROWS; i++)
        for (j = 0; j < COLS; j++) {
            gGuesses[i][j] = 0;
            gStates[i][j] = EMPTY;
        }

    gRow = gCol = 0;
    gGameOver = false;
    gDone = false;
    gMsgBuf[0] = 0;
}

static void SetupMenus(void)
{
    gAppleMenu = NewMenu(128, "\p\024");
    AppendMenu(gAppleMenu, "\pAbout Wordle\311");
    AppendMenu(gAppleMenu, "\p(-");
    InsertMenu(gAppleMenu, 0);

    gFileMenu = NewMenu(129, "\pFile");
    AppendMenu(gFileMenu, "\pNew Game/N");
    AppendMenu(gFileMenu, "\p(-");
    AppendMenu(gFileMenu, "\pQuit/Q");
    InsertMenu(gFileMenu, 0);

    DrawMenuBar();
}

static void SetupWindow(void)
{
    Rect b;
    SetRect(&b, 50, 40, 450, 510);
    gWindow = NewWindow(NULL, &b, "\pWordle", true,
                        documentProc, (WindowPtr)-1L, true, 0);
    SetPort(gWindow);
    TextFont(0);
    TextSize(24);
    TextFace(bold);
    DrawAll();
}

static void SetFG(unsigned short r, unsigned short g, unsigned short b)
{
    RGBColor c; c.red = r; c.green = g; c.blue = b; RGBForeColor(&c);
}

static void SetBG(unsigned short r, unsigned short g, unsigned short b)
{
    RGBColor c; c.red = r; c.green = g; c.blue = b; RGBBackColor(&c);
}

static void DrawAll(void)
{
    int r, c;
    unsigned char title[] = {6,'W','O','R','D','L','E'};
    short tw;
    Rect wr;

    SetBG(0xFFFF, 0xFFFF, 0xFFFF);
    SetFG(0, 0, 0);
    SetRect(&wr, 0, 0, 400, 470);
    EraseRect(&wr);

    TextSize(22); TextFace(bold);
    tw = StringWidth(title);
    MoveTo((400 - tw) / 2, 45);
    DrawString(title);

    TextSize(24); TextFace(bold);
    for (r = 0; r < ROWS; r++)
        for (c = 0; c < COLS; c++)
            DrawCell(r, c);

    DrawMessage();
}

static void DrawCell(int row, int col)
{
    Rect cr;
    int  x = GRID_X + col * (BOX_SZ + GAP);
    int  y = GRID_Y + row * (BOX_SZ + GAP);
    int  state = gStates[row][col];
    char letter = gGuesses[row][col];

    SetRect(&cr, x, y, x + BOX_SZ, y + BOX_SZ);

    switch (state) {
        case CORRECT: SetBG(0x3333, 0xAAAA, 0x3333); break;
        case PRESENT: SetBG(0xCCCC, 0xAAAA, 0x1111); break;
        case ABSENT:  SetBG(0x7777, 0x7777, 0x7777); break;
        default:      SetBG(0xFFFF, 0xFFFF, 0xFFFF); break;
    }
    EraseRect(&cr);

    if (state == EMPTY)
        SetFG(0xAAAA, 0xAAAA, 0xAAAA);
    else
        SetFG(0, 0, 0);
    FrameRect(&cr);

    if (letter) {
        FontInfo fi;
        unsigned char pch[2];
        short cw;

        GetFontInfo(&fi);
        pch[0] = 1; pch[1] = (unsigned char)letter;
        cw = StringWidth(pch);

        if (state == EMPTY)
            SetFG(0, 0, 0);
        else
            SetFG(0xFFFF, 0xFFFF, 0xFFFF);

        MoveTo(x + (BOX_SZ - cw) / 2,
               y + (BOX_SZ + fi.ascent - fi.descent) / 2);
        DrawChar(letter);
    }

    SetFG(0, 0, 0);
    SetBG(0xFFFF, 0xFFFF, 0xFFFF);
}

static void DrawMessage(void)
{
    int msgY = GRID_Y + ROWS * (BOX_SZ + GAP) + 20;
    Rect mr;
    SetRect(&mr, 0, msgY - 20, 400, msgY + 20);
    SetBG(0xFFFF, 0xFFFF, 0xFFFF);
    EraseRect(&mr);

    if (gMsgBuf[0] > 0) {
        short tw;
        TextSize(14); TextFace(0);
        tw = StringWidth(gMsgBuf);
        SetFG(0, 0, 0);
        MoveTo((400 - tw) / 2, msgY);
        DrawString(gMsgBuf);
        TextSize(24); TextFace(bold);
    }
}

static void SetMessage(const char *s)
{
    int i = 0;
    while (s[i] && i < 62) { gMsgBuf[i+1] = (unsigned char)s[i]; i++; }
    gMsgBuf[0] = (unsigned char)i;
    SetPort(gWindow);
    DrawMessage();
}

static void HandleKey(char ch)
{
    if (gGameOver) return;

    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
        if (gCol < COLS) {
            char uch = (ch >= 'a') ? ch - 32 : ch;
            gGuesses[gRow][gCol] = uch;
            DrawCell(gRow, gCol);
            gCol++;
            gMsgBuf[0] = 0;
            DrawMessage();
        }
    } else if (ch == 8) {   /* backspace */
        if (gCol > 0) {
            gCol--;
            gGuesses[gRow][gCol] = 0;
            gStates[gRow][gCol] = EMPTY;
            DrawCell(gRow, gCol);
            gMsgBuf[0] = 0;
            DrawMessage();
        }
    } else if (ch == 13 || ch == 3) {  /* return / enter */
        if (gCol == COLS)
            SubmitGuess();
        else
            SetMessage("Not enough letters");
    }
}

static void SubmitGuess(void)
{
    int i;
    int ansCount[26] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    Boolean won;

    for (i = 0; i < COLS; i++)
        ansCount[(int)(gAnswer[i] - 'A')]++;

    /* First pass: exact matches */
    for (i = 0; i < COLS; i++) {
        if (gGuesses[gRow][i] == gAnswer[i]) {
            gStates[gRow][i] = CORRECT;
            ansCount[(int)(gAnswer[i] - 'A')]--;
        }
    }

    /* Second pass: present / absent */
    for (i = 0; i < COLS; i++) {
        if (gStates[gRow][i] == CORRECT) continue;
        if (ansCount[(int)(gGuesses[gRow][i] - 'A')] > 0) {
            gStates[gRow][i] = PRESENT;
            ansCount[(int)(gGuesses[gRow][i] - 'A')]--;
        } else {
            gStates[gRow][i] = ABSENT;
        }
    }

    for (i = 0; i < COLS; i++) DrawCell(gRow, i);

    won = true;
    for (i = 0; i < COLS; i++)
        if (gStates[gRow][i] != CORRECT) { won = false; break; }

    if (won) {
        SetMessage(kWinMsgs[gRow]);
        gGameOver = true;
        return;
    }

    gRow++;
    gCol = 0;

    if (gRow >= ROWS) {
        char buf[32];
        int k = 0;
        const char *pre = "The word was ";
        while (pre[k]) { buf[k] = pre[k]; k++; }
        for (i = 0; i < 5; i++) buf[k++] = gAnswer[i];
        buf[k] = '\0';
        SetMessage(buf);
        gGameOver = true;
    } else {
        gMsgBuf[0] = 0;
        DrawMessage();
    }
}

static void DoMenuCommand(long mr)
{
    short menuID   = HiWord(mr);
    short menuItem = LoWord(mr);

    switch (menuID) {
        case 128:
            if (menuItem == 1) DoAboutDialog();
            break;
        case 129:
            if (menuItem == 1) {
                InitGame();
                SetPort(gWindow);
                DrawAll();
            } else if (menuItem == 3) {
                gDone = true;
            }
            break;
    }
    HiliteMenu(0);
}

static void HandleMouseDown(EventRecord *ev)
{
    WindowPtr wp;
    short part = FindWindow(ev->where, &wp);

    switch (part) {
        case inMenuBar:
            DoMenuCommand(MenuSelect(ev->where));
            break;
        case inDrag:
            {
                Rect screen = qd.screenBits.bounds;
                DragWindow(wp, ev->where, &screen);
            }
            break;
        case inGoAway:
            if (TrackGoAway(wp, ev->where))
                if (wp == gWindow) gDone = true;
            break;
        case inContent:
            if (wp != FrontWindow()) SelectWindow(wp);
            break;
        case inSysWindow:
            SystemClick(ev, wp);
            break;
    }
}

static void DoAboutDialog(void)
{
    WindowPtr     aboutWin;
    ControlHandle okBtn;
    Rect          wb, btnRect, ringRect;
    EventRecord   ev;
    Boolean       done = false;
    Point         pt;
    short         part;
    ControlHandle hit;
    char          ch;

    SetRect(&wb, 120, 100, 390, 275);
    aboutWin = NewWindow(NULL, &wb, "\p", true,
                         dBoxProc, (WindowPtr)-1L, false, 0);
    SetPort(aboutWin);
    TextFont(0); TextSize(12); TextFace(0);

    MoveTo(20, 25);  DrawString("\pWordle for Classic Mac");
    MoveTo(20, 45);  DrawString("\pGuess the 5-letter word in 6 tries.");
    MoveTo(20, 65);  DrawString("\pGreen  = correct letter and position.");
    MoveTo(20, 81);  DrawString("\pYellow = right letter, wrong position.");
    MoveTo(20, 97);  DrawString("\pGray   = letter not in the word.");
    MoveTo(20, 113); DrawString("\pFile > New Game starts a fresh puzzle.");

    SetRect(&btnRect, 107, 130, 163, 150);
    okBtn = NewControl(aboutWin, &btnRect, "\pOK", true, 0, 0, 1, pushButProc, 0);
    ringRect = btnRect; InsetRect(&ringRect, -4, -4);
    PenSize(3, 3); FrameRoundRect(&ringRect, 16, 16); PenNormal();

    while (!done) {
        WaitNextEvent(everyEvent, &ev, 20, NULL);
        switch (ev.what) {
            case mouseDown:
                pt = ev.where; GlobalToLocal(&pt);
                part = FindControl(pt, aboutWin, &hit);
                if (part == inButton && hit == okBtn) {
                    TrackControl(hit, pt, NULL);
                    done = true;
                }
                break;
            case keyDown:
                ch = (char)(ev.message & charCodeMask);
                if (ch == 13 || ch == 3) done = true;
                break;
        }
    }

    DisposeControl(okBtn);
    DisposeWindow(aboutWin);
    SetPort(gWindow);
    TextSize(24); TextFace(bold);
}
