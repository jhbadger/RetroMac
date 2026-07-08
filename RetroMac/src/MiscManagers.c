/* MiscManagers.c -- the smaller Toolbox pieces that Phase 0 only
 * needs to satisfy the boot sequence (Fonts/TextEdit/Dialogs) or that
 * have no Cocoa dependency at all (Memory Manager: modern macOS has a
 * flat, non-relocating 64-bit heap, so Handles are just a pointer to
 * a pointer to a malloc'd, size-prefixed block).
 */
#include "../include/Fonts.h"
#include "../include/TextEdit.h"
#include "../include/Dialogs.h"
#include "../include/Memory.h"
#include <stdlib.h>
#include <string.h>

void InitFonts(void)
{
    /* Real font/glyph metrics come from Core Text lazily, per draw
     * call (see QuickDraw.c) -- nothing to preload here yet. */
}

void TEInit(void)
{
    /* Full TextEdit (TENew/TEKey/TEClick/...) is Phase 1. */
}

void InitDialogs(void *resumeProc)
{
    (void)resumeProc;
    /* Phase 0 apps build their dialogs as plain dBoxProc windows
     * (see WindowManager.c) rather than via GetNewDialog/DITL. */
}

typedef struct RMHandleHeader {
    long size;
} RMHandleHeader;

Handle NewHandle(long byteCount)
{
    Ptr *slot = (Ptr *)malloc(sizeof(Ptr));
    if (!slot) return NULL;

    size_t payload = byteCount > 0 ? (size_t)byteCount : 0;
    RMHandleHeader *block = (RMHandleHeader *)malloc(sizeof(RMHandleHeader) + payload);
    if (!block) {
        free(slot);
        return NULL;
    }
    block->size = byteCount;
    *slot = (Ptr)(block + 1);
    return (Handle)slot;
}

Handle NewHandleClear(long byteCount)
{
    Handle h = NewHandle(byteCount);
    if (h && *h && byteCount > 0) memset(*h, 0, (size_t)byteCount);
    return h;
}

void DisposeHandle(Handle h)
{
    if (!h) return;
    if (*h) {
        RMHandleHeader *block = ((RMHandleHeader *)(*h)) - 1;
        free(block);
    }
    free(h);
}

void HLock(Handle h) { (void)h; /* flat heap: blocks never move */ }
void HUnlock(Handle h) { (void)h; }

void SetHandleSize(Handle h, long newSize)
{
    if (!h) return;
    RMHandleHeader *old = *h ? (((RMHandleHeader *)(*h)) - 1) : NULL;
    size_t payload = newSize > 0 ? (size_t)newSize : 0;
    RMHandleHeader *block = (RMHandleHeader *)realloc(old, sizeof(RMHandleHeader) + payload);
    if (!block) return;
    block->size = newSize;
    *h = (Ptr)(block + 1);
}

long GetHandleSize(Handle h)
{
    if (!h || !*h) return 0;
    RMHandleHeader *block = ((RMHandleHeader *)(*h)) - 1;
    return block->size;
}

Ptr NewPtr(long byteCount) { return (Ptr)malloc((size_t)(byteCount > 0 ? byteCount : 0)); }
Ptr NewPtrClear(long byteCount) { return (Ptr)calloc(1, (size_t)(byteCount > 0 ? byteCount : 0)); }
void DisposePtr(Ptr p) { free(p); }
