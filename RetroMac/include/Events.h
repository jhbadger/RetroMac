/* RetroMac Toolbox shim: Events.h */
#ifndef RETROMAC_EVENTS_H
#define RETROMAC_EVENTS_H

#include "Types.h"
#include "OSUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event "what" codes */
enum {
    nullEvent   = 0,
    mouseDown   = 1,
    mouseUp     = 2,
    keyDown     = 3,
    keyUp       = 4,
    autoKey     = 5,
    updateEvt   = 6,
    diskEvt     = 7,
    activateEvt = 8,
    osEvt       = 15
};

enum { everyEvent = 0xFFFF };

enum {
    charCodeMask = 0x000000FF,
    keyCodeMask  = 0x0000FF00
};

/* Modifier bits */
enum {
    cmdKey     = 0x0100,
    shiftKey   = 0x0200,
    alphaLock  = 0x0400,
    optionKey  = 0x0800,
    controlKey = 0x1000
};

typedef struct EventRecord {
    short what;
    long  message;
    long  when;
    Point where;
    short modifiers;
} EventRecord;

Boolean WaitNextEvent(short eventMask, EventRecord *ev, unsigned long sleep, void *mouseRgn);
void FlushEvents(short eventMask, short stopMask);
void GlobalToLocal(Point *pt);
void LocalToGlobal(Point *pt);
void SystemClick(const EventRecord *ev, WindowPtr w);

#ifdef __cplusplus
}
#endif

#endif /* RETROMAC_EVENTS_H */
