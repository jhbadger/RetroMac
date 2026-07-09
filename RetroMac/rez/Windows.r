/* Windows.r -- compatibility shim for real classic Mac .r sources.
 * Modern Xcode's Rez interfaces renamed the WIND/window-manager
 * template file to MacWindows.r at some point after the classic era;
 * older .r sources (and every real Inside Mac example) still say
 * #include "Windows.r". This is the C-header-shim trick (RetroMac/
 * include/*.h) applied to the Rez side. */
#ifndef __RETROMAC_WINDOWS_R__
#define __RETROMAC_WINDOWS_R__

#include "MacWindows.r"

#endif /* __RETROMAC_WINDOWS_R__ */
