/* RetroMac Toolbox shim: OSUtils.h */
#ifndef RETROMAC_OSUTILS_H
#define RETROMAC_OSUTILS_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned long TickCount(void);
void SysBeep(short duration);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_OSUTILS_H */
