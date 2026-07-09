/* ResourceManager.c -- classic Resource Manager, Phase 2. Plain C, no
 * Cocoa dependency: this is pure byte parsing of a real resource-fork-
 * format file (see Resources.h for how that file gets there). Resource
 * data on disk is big-endian; this file is the only place that does
 * big-endian byte-swapping -- everything downstream (GetNewWindow,
 * GetMenu, GetNewDialog, DITL unpacking) works with already-native
 * shorts/longs via the accessor helpers below.
 *
 * Single implicit resource file, loaded once at boot -- no
 * OpenResFile/UseResFile/CurResFile chain, since no sample app here
 * needs more than "my own app's resources" (see Resources.h).
 */
#include "../include/Resources.h"
#include "RetroMacBridge.h" /* declares RM_LoadAppResourceFile, called from AppShell.m */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *gResFileData = NULL;
static long gResFileSize = 0;

/* ==== big-endian accessors ==============================================
 * Exported (declared in RetroMacInternal.h) since every resource-backed
 * unpacker (GetNewWindow, GetMenu, GetNewDialog/DITL, Alert/DITL) needs
 * to read the same big-endian fields RetroMac's resource data holds
 * (resource fork format has always been big-endian, regardless of the
 * host CPU). */

unsigned short RM_ReadU16BE(const unsigned char *p)
{
    return (unsigned short)((p[0] << 8) | p[1]);
}

unsigned long RM_ReadU32BE(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

/* 3-byte big-endian, as used for a reference list entry's data offset */
static unsigned long RM_ReadU24BE(const unsigned char *p)
{
    return ((unsigned long)p[0] << 16) | ((unsigned long)p[1] << 8) | (unsigned long)p[2];
}

/* ==== boot-time load ==================================================== */

void RM_LoadAppResourceFile(const char *path)
{
    if (!path) return;

    FILE *f = fopen(path, "rb");
    if (!f) return; /* no .r file for this app -- silent no-op, exactly like Phase 0/1 apps expect */

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return; }

    unsigned char *buf = (unsigned char *)malloc((size_t)size);
    if (!buf) { fclose(f); return; }

    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return;
    }
    fclose(f);

    gResFileData = buf;
    gResFileSize = size;
}

/* ==== lookup ============================================================= */

Handle Get1Resource(ResType theType, short theID)
{
    if (!gResFileData || gResFileSize < 16) return NULL;

    unsigned long dataOffset = RM_ReadU32BE(gResFileData + 0);
    unsigned long mapOffset  = RM_ReadU32BE(gResFileData + 4);
    if (mapOffset + 30 > (unsigned long)gResFileSize) return NULL;

    const unsigned char *map = gResFileData + mapOffset;
    /* map: 16-byte reserved header copy, 4-byte next-map handle (unused),
     * 2-byte file ref num (unused), 2-byte attributes, then the type
     * list (offset relative to map start) and name list (ditto). */
    unsigned short typeListOff = RM_ReadU16BE(map + 24);
    const unsigned char *typeList = map + typeListOff;

    unsigned short typeCount = (unsigned short)(RM_ReadU16BE(typeList) + 1);
    const unsigned char *typeEntry = typeList + 2;

    for (unsigned short i = 0; i < typeCount; i++) {
        unsigned long entryType = RM_ReadU32BE(typeEntry);
        unsigned short refCount = (unsigned short)(RM_ReadU16BE(typeEntry + 4) + 1);
        unsigned short refListOff = RM_ReadU16BE(typeEntry + 6);

        if (entryType == (unsigned long)theType) {
            const unsigned char *refEntry = typeList + refListOff;
            for (unsigned short j = 0; j < refCount; j++) {
                short resID = (short)RM_ReadU16BE(refEntry);
                if (resID == theID) {
                    unsigned long dataRelOffset = RM_ReadU24BE(refEntry + 5);
                    const unsigned char *resData = gResFileData + dataOffset + dataRelOffset;
                    unsigned long len = RM_ReadU32BE(resData);

                    Handle h = NewHandle((long)len);
                    if (h && *h && len > 0) memcpy(*h, resData + 4, (size_t)len);
                    return h;
                }
                refEntry += 12; /* resID(2) + nameOffset(2) + attributes(1) + dataOffset(3) + handle(4) */
            }
            return NULL;
        }
        typeEntry += 8; /* type(4) + count-1(2) + refListOffset(2) */
    }
    return NULL;
}

Handle GetResource(ResType theType, short theID)
{
    return Get1Resource(theType, theID);
}

void ReleaseResource(Handle theResource)
{
    DisposeHandle(theResource);
}
