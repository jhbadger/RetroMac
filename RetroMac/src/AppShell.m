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
    }
    return RetroMacUserMain();
}
