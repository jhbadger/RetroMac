/* RetroMac.r -- one-line convenience include for RetroMac resource
 * (.r) sources, mirroring how classic C sources #include <Windows.h>
 * against RetroMac's own C shims (RetroMac/include). Unlike the C
 * side, there's no need for RetroMac to reimplement its own .r
 * template definitions -- real Rez (present via Xcode or just the
 * Command Line Tools, see retromacc) already ships correct WIND/MENU/
 * DLOG/DITL/ALRT templates in these headers, and RetroMac's Resource
 * Manager (ResourceManager.c) reads the standard binary layout they
 * produce. See Toolbox.md section 12 for how that layout was
 * confirmed.
 */
#ifndef __RETROMAC_R__
#define __RETROMAC_R__

#include "MacTypes.r"
#include "MacWindows.r"
#include "Menus.r"
#include "Dialogs.r"

#endif /* __RETROMAC_R__ */
