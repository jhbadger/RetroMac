/* RetroMac Toolbox shim: Menus.h */
#ifndef RETROMAC_MENUS_H
#define RETROMAC_MENUS_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitMenus(void);

MenuHandle NewMenu(short menuID, ConstStr255Param title);

/* Phase 2: unpacks a MENU resource (see Toolbox.md) and calls
 * NewMenu/AppendMenu -- does not itself InsertMenu, exactly like real
 * GetMenu, so the caller controls menu-bar order. */
MenuHandle GetMenu(short resID);

void AppendMenu(MenuHandle m, ConstStr255Param data);
void InsertMenu(MenuHandle m, short beforeID);
void DrawMenuBar(void);

long MenuSelect(Point startPt);
long MenuKey(short ch);
void HiliteMenu(short menuID);

/* Phase 3: unpacks an MBAR resource (a list of MENU resource IDs),
 * materializes each one via GetMenu, and returns a Handle identifying
 * the set for SetMenuBar -- exactly like real GetNewMBar/SetMenuBar,
 * so a whole menu bar can be installed from resources in two calls
 * instead of one GetMenu/InsertMenu pair per menu. */
Handle GetNewMBar(short mbarID);
void SetMenuBar(Handle mbar);
MenuHandle GetMenuHandle(short menuID);

/* Appends one item per resource of theType found in the app's own
 * resource file, titled with that resource's name -- real Menu
 * Manager's Apple-menu desk-accessory list, generalized to any type.
 * RetroMac has no desk accessories to actually open, so this only
 * matters for apps (like real ones) that call it defensively even
 * when they expect zero matches. */
void AppendResMenu(MenuHandle theMenu, ResType theType);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_MENUS_H */
