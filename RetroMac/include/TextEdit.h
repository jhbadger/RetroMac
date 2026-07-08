/* RetroMac Toolbox shim: TextEdit.h
 * Phase 0 only needs TEInit() to satisfy the boot sequence -- full
 * TextEdit (TENew/TEKey/TEClick/...) is Phase 1. */
#ifndef RETROMAC_TEXTEDIT_H
#define RETROMAC_TEXTEDIT_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

void TEInit(void);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_TEXTEDIT_H */
