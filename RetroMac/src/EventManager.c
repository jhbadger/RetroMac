/* EventManager.c -- classic Event Manager. Plain C: translates
 * between the classic EventRecord shape and CocoaBridge.m's
 * RMCocoa_WaitNextEvent, and flushes any drawing the app did since
 * the last call to the screen (buffers only hit the screen when a
 * window is told needsDisplay -- see RM_FlushDirtyWindows).
 */
#include "RetroMacInternal.h"
#include "../include/Events.h"
#include <time.h>

Boolean WaitNextEvent(short eventMask, EventRecord *ev, unsigned long sleep, void *mouseRgn)
{
    (void)eventMask; (void)mouseRgn;

    /* Flush whatever the app drew while handling the *previous*
     * event before waiting for the next one -- classic apps pace
     * their own screen updates once per event-loop iteration, and
     * this is the equivalent moment for us. */
    RM_FlushDirtyWindows();

    short what = 0;
    long message = 0;
    Point where = {0, 0};
    short modifiers = 0;
    int got = RMCocoa_WaitNextEvent(sleep, &what, &message, &where, &modifiers);

    if (ev) {
        ev->what = got ? what : nullEvent;
        ev->message = message;
        ev->when = (long)TickCount();
        ev->where = where;
        ev->modifiers = modifiers;
    }
    return got ? true : false;
}

void FlushEvents(short eventMask, short stopMask)
{
    (void)eventMask; (void)stopMask;
    RMCocoa_FlushEvents();
}

void GlobalToLocal(Point *pt)
{
    if (!pt || !gCurrentPort) return;
    pt->h = (short)(pt->h - gCurrentPort->contentOriginGlobal.h);
    pt->v = (short)(pt->v - gCurrentPort->contentOriginGlobal.v);
}

void LocalToGlobal(Point *pt)
{
    if (!pt || !gCurrentPort) return;
    pt->h = (short)(pt->h + gCurrentPort->contentOriginGlobal.h);
    pt->v = (short)(pt->v + gCurrentPort->contentOriginGlobal.v);
}

void SystemClick(const EventRecord *ev, WindowPtr w)
{
    (void)ev; (void)w; /* phase 0 has no desk accessories / system windows to route to */
}

unsigned long TickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long nanos = (unsigned long long)ts.tv_sec * 1000000000ULL + (unsigned long long)ts.tv_nsec;
    return (unsigned long)(nanos / (1000000000ULL / 60ULL));
}
