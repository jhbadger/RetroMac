/* RetroMac Toolbox shim: Resources.h
 * Phase 2: a minimal Resource Manager. Real Rez (present on any
 * machine with Xcode or just the Command Line Tools, discoverable via
 * `xcrun`) compiles a classic .r source into a real, standard
 * resource-fork-format blob (flat file via `-useDF`, no HFS+
 * resource-fork/xattr involved) -- retromacc embeds that as
 * Contents/Resources/Resources.rsrc in the .app bundle, and
 * AppShell.m loads it once at boot. There is no OpenResFile/
 * UseResFile/CurResFile chain -- a single implicit "the app's own
 * resource file" model is all any sample app here needs.
 */
#ifndef RETROMAC_RESOURCES_H
#define RETROMAC_RESOURCES_H

#include "Types.h"
#include "Memory.h"

#ifdef __cplusplus
extern "C" {
#endif

Handle GetResource(ResType theType, short theID);
Handle Get1Resource(ResType theType, short theID);
void ReleaseResource(Handle theResource);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_RESOURCES_H */
