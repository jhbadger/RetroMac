/* RetroMac Toolbox shim: Menus.h */
#ifndef RETROMAC_MENUS_H
#define RETROMAC_MENUS_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitMenus(void);

MenuHandle NewMenu(short menuID, ConstStr255Param title);
void AppendMenu(MenuHandle m, ConstStr255Param data);
void InsertMenu(MenuHandle m, short beforeID);
void DrawMenuBar(void);

long MenuSelect(Point startPt);
long MenuKey(short ch);
void HiliteMenu(short menuID);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_MENUS_H */
