/* MenuManager.c -- classic Menu Manager backed by a real NSMenu bar
 * (built via CocoaBridge.m). Plain C: owns the menu pool and all
 * AppendMenu item-spec parsing (separators via "-", initial-disabled
 * via a leading "(", Cmd-key equivalents via a trailing "/X" --
 * exactly the conventions Inside Macintosh documents for AppendMenu).
 *
 * Clicking the real system menu bar is intercepted by CocoaBridge.m's
 * WaitNextEvent, which lets AppKit's own (blocking) menu-tracking run
 * via -sendEvent: -- by the time that call returns, RM_MenuItemChosen
 * below has already populated gPendingMenuSelection, and the point
 * that was clicked lands in the classic menu-bar strip, so FindWindow
 * naturally reports inMenuBar and the classic app's own MenuSelect
 * call (right where it already expects to) just needs to read that
 * pending value back out.
 */
#include "RetroMacInternal.h"
#include "../include/Menus.h"
#include "../include/Resources.h"
#include <string.h>
#include <stdio.h>

struct MenuRecord gMenus[RM_MAX_MENUS];
volatile long gPendingMenuSelection = 0;

static struct MenuRecord *RM_AllocMenuSlot(void)
{
    for (int i = 0; i < RM_MAX_MENUS; i++) {
        if (!gMenus[i].inUse) {
            memset(&gMenus[i], 0, sizeof(gMenus[i]));
            gMenus[i].inUse = true;
            return &gMenus[i];
        }
    }
    fprintf(stderr, "RetroMac: out of menu slots (RM_MAX_MENUS=%d)\n", RM_MAX_MENUS);
    return NULL;
}

static void RM_ParseMenuItemSpec(ConstStr255Param pstr, RMMenuItem *item)
{
    short len = RM_PStringLength(pstr);
    short i, textLen = 0;
    Boolean disabled = false;
    char cmdKey = 0;
    unsigned char text[256];

    memset(item, 0, sizeof(*item));

    if (len == 1 && pstr[1] == '-') {
        item->isSeparator = true;
        return;
    }

    i = 0;
    if (len > 0 && pstr[1] == '(') { disabled = true; i = 1; }

    for (; i < len; i++) {
        unsigned char c = pstr[1 + i];
        if (c == '/' && i + 1 < len) {
            cmdKey = (char)pstr[1 + i + 1];
            break;
        }
        text[textLen++] = c;
    }

    item->enabled = disabled ? false : true;
    item->cmdKey = cmdKey;
    item->title[0] = (unsigned char)textLen;
    memcpy(&item->title[1], text, (size_t)textLen);
}

void InitMenus(void)
{
    memset(gMenus, 0, sizeof(gMenus));
    gPendingMenuSelection = 0;
}

MenuHandle NewMenu(short menuID, ConstStr255Param title)
{
    MenuHandle m = RM_AllocMenuSlot();
    if (!m) return NULL;

    m->menuID = menuID;
    RM_PStringCopy(m->title, title);
    m->itemCount = 0;

    /* Byte 0x14 as a menu's entire title is not a printable Mac Roman
     * character -- it is the long-standing Toolbox sentinel for "this
     * is the Apple menu, draw the Apple logo here." */
    m->isAppleMenu = (RM_PStringLength(title) == 1 && title[1] == 0x14) ? true : false;

    char utf8Title[256];
    if (m->isAppleMenu) utf8Title[0] = '\0';
    else RM_MacRomanPStringToUTF8(title, utf8Title, sizeof(utf8Title));

    m->nsMenu = RMCocoa_CreateMenu(utf8Title, m->isAppleMenu ? 1 : 0);
    return m;
}

void AppendMenu(MenuHandle m, ConstStr255Param data)
{
    if (!m || m->itemCount >= RM_MAX_MENU_ITEMS) return;

    RMMenuItem *item = &m->items[m->itemCount];
    RM_ParseMenuItemSpec(data, item);
    short itemIndex1Based = (short)(m->itemCount + 1);
    m->itemCount++;

    if (item->isSeparator) {
        RMCocoa_AppendMenuItem(m->nsMenu, "", 0, 1, 0, m->menuID, itemIndex1Based);
    } else {
        char utf8[1024];
        RM_MacRomanPStringToUTF8(item->title, utf8, sizeof(utf8));
        RMCocoa_AppendMenuItem(m->nsMenu, utf8, item->cmdKey, 0, item->enabled ? 1 : 0,
                                m->menuID, itemIndex1Based);
    }
}

/* MENU resource layout (confirmed by compiling one with the real
 * system Rez and hex-dumping the result -- see Toolbox.md section 12):
 * menuID (short), menuWidth/menuHeight (shorts, ignored -- computed at
 * runtime by real Menu Manager, and RetroMac's NSMenu-backed bar
 * doesn't need them either), a 4-byte menuProc placeholder (ignored --
 * always zero on disk), enableFlags (long, bit 0 = the menu itself,
 * bit N = item N), title (Pascal string -- the `apple` Rez keyword
 * compiles to the same single 0x14 sentinel byte NewMenu already
 * special-cases), then items until a zero-length title ends the list:
 * itemTitle (pstr), icon (byte, ignored), cmdKey (byte), mark (byte,
 * ignored), style (byte, ignored). A title of exactly "-" is a
 * separator, matching AppendMenu's own convention. */
MenuHandle GetMenu(short resID)
{
    Handle h = Get1Resource('MENU', resID);
    if (!h || !*h) {
        fprintf(stderr, "RetroMac: GetMenu: MENU %d not found\n", (int)resID);
        return NULL;
    }

    const unsigned char *p = (const unsigned char *)*h;
    short menuID = (short)RM_ReadU16BE(p + 0);
    unsigned long enableFlags = RM_ReadU32BE(p + 10);
    unsigned char title[256];
    RM_PStringCopy(title, p + 14);

    MenuHandle m = NewMenu(menuID, title);
    if (!m) { ReleaseResource(h); return NULL; }

    long offset = 14 + 1 + RM_PStringLength(title);
    for (short itemNum = 1; ; itemNum++) {
        short len = (short)p[offset];
        if (len == 0) break; /* zero-length title ends the item list */

        unsigned char itemTitle[256];
        RM_PStringCopy(itemTitle, p + offset);
        char cmdKey = (char)p[offset + 1 + len + 1]; /* icon byte, then cmdKey */
        Boolean enabled = (itemNum < 32) ? (((enableFlags >> itemNum) & 1) != 0) : true;

        if (len == 1 && itemTitle[1] == '-') {
            unsigned char sep[] = { 1, '-' };
            AppendMenu(m, sep);
        } else {
            unsigned char spec[258];
            short specLen = 0;
            if (!enabled) spec[1 + specLen++] = '(';
            memcpy(&spec[1 + specLen], &itemTitle[1], (size_t)len);
            specLen = (short)(specLen + len);
            if (cmdKey) {
                spec[1 + specLen++] = '/';
                spec[1 + specLen++] = cmdKey;
            }
            spec[0] = (unsigned char)specLen;
            AppendMenu(m, spec);
        }

        offset += 1 + len + 4; /* title + icon/cmdKey/mark/style */
    }

    ReleaseResource(h);
    return m;
}

void InsertMenu(MenuHandle m, short beforeID)
{
    (void)beforeID; /* phase 0: both sample apps always insert at the end */
    if (!m) return;
    RMCocoa_InsertMenuIntoBar(m->nsMenu);
}

void DrawMenuBar(void)
{
    RMCocoa_RebuildMenuBar();
}

long MenuSelect(Point startPt)
{
    (void)startPt;
    long sel = gPendingMenuSelection;
    gPendingMenuSelection = 0;
    return sel;
}

long MenuKey(short ch)
{
    char target = (char)ch;
    if (target >= 'a' && target <= 'z') target = (char)(target - 32); /* Cmd-key equivalents are stored uppercase */

    for (int i = 0; i < RM_MAX_MENUS; i++) {
        if (!gMenus[i].inUse) continue;
        for (int j = 0; j < gMenus[i].itemCount; j++) {
            RMMenuItem *item = &gMenus[i].items[j];
            if (!item->isSeparator && item->enabled && item->cmdKey == target) {
                return ((long)gMenus[i].menuID << 16) | (long)(j + 1);
            }
        }
    }
    return 0;
}

void HiliteMenu(short menuID)
{
    (void)menuID; /* AppKit already highlighted the chosen title during its own tracking */
}

MenuHandle GetMenuHandle(short menuID)
{
    for (int i = 0; i < RM_MAX_MENUS; i++) {
        if (gMenus[i].inUse && gMenus[i].menuID == menuID) return &gMenus[i];
    }
    return NULL;
}

/* MBAR layout: count-1 (short) followed by that many menuID shorts --
 * the same "list of MENU resource IDs" real Menu Manager reads. The
 * Handle GetNewMBar returns just stores {count, id0, id1, ...} in
 * native-endian shorts for SetMenuBar to walk; the actual menus are
 * materialized right here via GetMenu, matching real GetNewMBar's
 * behavior of instantiating them up front. */
Handle GetNewMBar(short mbarID)
{
    Handle h = Get1Resource('MBAR', mbarID);
    if (!h || !*h) {
        fprintf(stderr, "RetroMac: GetNewMBar: MBAR %d not found\n", (int)mbarID);
        return NULL;
    }

    const unsigned char *p = (const unsigned char *)*h;
    short count = (short)(RM_ReadU16BE(p) + 1);

    Handle mbar = NewHandle((long)(sizeof(short) * (size_t)(1 + count)));
    if (mbar && *mbar) {
        short *ids = (short *)*mbar;
        ids[0] = count;
        for (short i = 0; i < count; i++) {
            short menuID = (short)RM_ReadU16BE(p + 2 + i * 2);
            ids[1 + i] = menuID;
            if (!GetMenuHandle(menuID)) GetMenu(menuID);
        }
    }
    ReleaseResource(h);
    return mbar;
}

/* Installs every menu named in an MBAR list into the real menu bar, in
 * order -- like real SetMenuBar, but simplified: it only ever appends
 * (matching InsertMenu's own existing simplification), so it's only
 * meant to be called once per app, right after GetNewMBar, exactly
 * like every sample app here does. */
void SetMenuBar(Handle mbar)
{
    if (!mbar || !*mbar) return;
    short *ids = (short *)*mbar;
    short count = ids[0];
    for (short i = 0; i < count; i++) {
        MenuHandle m = GetMenuHandle(ids[1 + i]);
        if (m) InsertMenu(m, 0);
    }
}

/* Appends one item per resource of theType found in the app's own
 * resource file (see ResourceManager.c's RM_CountResourcesOfType/
 * RM_GetIndResourceInfo), titled with that resource's name. RetroMac
 * only ever loads the app's own resource file (no System file), so an
 * app with no resources of theType -- every sample here, calling this
 * with 'DRVR' for the desk-accessory list -- sees zero items appended,
 * same as a real, DA-less System file would produce. */
void AppendResMenu(MenuHandle theMenu, ResType theType)
{
    if (!theMenu) return;
    short count = RM_CountResourcesOfType(theType);
    for (short i = 1; i <= count; i++) {
        short resID = 0;
        unsigned char name[256];
        if (!RM_GetIndResourceInfo(theType, i, &resID, name)) continue;
        if (name[0] == 0) continue; /* unnamed resources don't get a DA-style menu item */
        AppendMenu(theMenu, name);
    }
}

void RM_MenuItemChosen(short menuID, short itemIndex1Based)
{
    gPendingMenuSelection = ((long)menuID << 16) | (long)itemIndex1Based;
}

int RM_HasPendingMenuSelection(void)
{
    return gPendingMenuSelection != 0;
}
