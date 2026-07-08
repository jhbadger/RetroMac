/* RetroMac Toolbox shim: Memory.h
 * Modern macOS has a flat 64-bit address space, so Handles/Ptrs are
 * thin malloc wrappers here rather than a real relocatable heap. */
#ifndef RETROMAC_MEMORY_H
#define RETROMAC_MEMORY_H

#include "Types.h"

#ifdef __cplusplus
extern "C" {
#endif

Handle NewHandle(long byteCount);
Handle NewHandleClear(long byteCount);
void DisposeHandle(Handle h);
void HLock(Handle h);
void HUnlock(Handle h);
void SetHandleSize(Handle h, long newSize);
long GetHandleSize(Handle h);

Ptr NewPtr(long byteCount);
Ptr NewPtrClear(long byteCount);
void DisposePtr(Ptr p);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_MEMORY_H */
