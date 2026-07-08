/* RetroMac Toolbox shim: Dialogs.h
 * Phase 0 apps build their dialogs programmatically as plain
 * dBoxProc windows, so only the boot call is needed here. */
#ifndef RETROMAC_DIALOGS_H
#define RETROMAC_DIALOGS_H

#include "Types.h"
#include "Windows.h"

#ifdef __cplusplus
extern "C" {
#endif

void InitDialogs(void *resumeProc);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_DIALOGS_H */
