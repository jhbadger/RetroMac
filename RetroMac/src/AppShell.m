/* AppShell.m -- the real process entry point.
 *
 * Classic Mac apps define their own `main()` and run their own
 * WaitNextEvent loop; Cocoa needs to own process bootstrap first
 * (NSApplication, activation policy, ...). retromacc resolves this by
 * compiling the user's source with -Dmain=RetroMacUserMain, so their
 * `int main(void)` becomes `int RetroMacUserMain(void)` and this file
 * gets to be the *real* main() instead.
 */
#import <Cocoa/Cocoa.h>
#include "RetroMacBridge.h"

int main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    @autoreleasepool {
        RMCocoa_Init();

        /* Phase 2: retromacc embeds a sibling .r file's compiled output
         * (if any) as Contents/Resources/Resources.rsrc. Silent no-op
         * if it doesn't exist -- every app with no .r file (all of
         * Phase 0/1's samples) is unaffected. */
        NSString *rsrcPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"Resources.rsrc"];
        RM_LoadAppResourceFile(rsrcPath.UTF8String);
    }
    return RetroMacUserMain();
}
