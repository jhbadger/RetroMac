# RetroMac Toolbox — a Classic Mac Toolbox shim for modern macOS

> **Status: Phase 0 and Phase 1 are done and verified.** `wordle.c` and
> `demo.c` both build with `retromacc` and run as real, interactive
> `.app` bundles — windows, colored/text drawing, keyboard input, the
> real system menu bar, and the programmatic About dialog all
> confirmed working by launching the built apps and driving them with
> actual clicks/keystrokes, not just a clean compile. Phase 1 adds a
> real Dialog Manager (`NewDialog`/`ModalDialog`/`Alert` family) and
> TextEdit (`TENew`/`TEClick`/`TEKey`); `dialogdemo.c` is its
> acceptance test, confirmed the same way. The component map in §3 and
> the build-wrapper description in §6 describe what was actually built
> (which diverged from the original plan in a few places); see §10 for
> Phase 0's bring-up notes and §11 for Phase 1's.

## 1. Goal

Let unmodified (or nearly unmodified) classic Mac OS C sources —
`wordle.c`, `demo.c`, and similar System 6–9 style Toolbox programs —
compile and run as **native Apple Silicon/Intel macOS processes**,
with no 68k/PPC emulation and no Retro68 cross-compilation step.

This is achieved by providing:

1. **Header shims** — drop-in replacements for `<Types.h>`,
   `<Quickdraw.h>`, `<Fonts.h>`, `<Windows.h>`, `<Menus.h>`,
   `<TextEdit.h>`, `<Dialogs.h>`, `<Events.h>`, `<Memory.h>`,
   `<Controls.h>` — that declare the same types and function
   signatures the original apps `#include`.
2. **A runtime library** (`libRetroMac.a` / `RetroMac.framework`)
   implementing those functions on top of Cocoa/AppKit + Core
   Graphics + Core Text.
3. **A thin build wrapper** (`retromacc`) so `retromacc wordle.c -o
   Wordle.app` behaves like a classic MPW/THINK C compile-and-link,
   producing a real double-clickable `.app` bundle.

Non-goals (explicitly out of scope for v1): real 68k/PPC binaries,
resource-fork (`.r`/Rez) resources, the Resource Manager, Color
QuickDraw regions/pictures, QuickTime, AppleScript, networking,
printing. These can be layered on later; see §8.

## 2. Why simulate rather than translate to native widgets

`wordle.c` hand-draws every cell with `EraseRect`/`FrameRect`/
`DrawChar` and hand-draws its own default-button focus ring around a
`NewControl` button. If we mapped `NewWindow` → `NSWindow` +
`NSButton` 1:1, the visual output would look like a *modern* Mac
app, not a *classic* one, and pixel-level QuickDraw calls
(`EraseRect`, `FrameRoundRect`, `InsetRect`) would have nothing to
land on between button draws.

So RetroMac implements **QuickDraw as a real (partial) software
rasterizer**, exactly like the original Toolbox did, onto an
off-screen 32-bit bitmap owned by each `WindowPtr`. That bitmap is
what `NSView`/`CALayer` displays. Controls (`NewControl`,
`TrackControl`, `FindControl`) are drawn into the same bitmap using
QuickDraw primitives (rounded rects, frames) rather than becoming
real `NSButton`s. This is the same architectural choice used by
projects like Executor and Advanced Mac Substitute: reimplement the
trap table, not "port to native controls."

Menus are the one exception: `NewMenu`/`InsertMenu`/`DrawMenuBar` map
onto a real `NSMenu` in the host menu bar, because there is no
value in hand-drawing a menu bar when Cocoa already provides a
correct, accessible one — and `MenuSelect`/`MenuKey` just need to
return the same `(menuID, item)` packed `long` either way.

## 3. Component map

```
RetroMac/
├── include/                     (drop-in classic headers, unchanged from plan)
│   ├── Types.h                  Boolean/Point/Rect/etc reused from the SDK's own
│   │                            MacTypes.h; only RGBColor + opaque WindowPtr/
│   │                            MenuHandle/ControlHandle are added on top
│   ├── Quickdraw.h, Fonts.h, Windows.h, Menus.h, TextEdit.h,
│   │   Dialogs.h, Events.h, Memory.h, Controls.h, OSUtils.h
├── src/
│   ├── RetroMacBridge.h          the ONLY header shared with CocoaBridge.m -- see
│   │                             §10 for why a hard C/Objective-C split was needed
│   ├── RetroMacInternal.h        private GrafPort/MenuRecord/ControlRecord structs
│   │                             + the window/menu pools, included by every plain-C
│   │                             file below (never by CocoaBridge.m)
│   ├── QuickDraw.c               geometry, drawing, text (Core Text), color --
│   │                             everything qd_types/qd_port/qd_draw/qd_text/
│   │                             qd_color would have been, consolidated
│   ├── MacRoman.c                Mac Roman -> UTF-8 (Pascal string helpers live
│   │                             here too, replacing the planned pascal_string.c)
│   ├── WindowManager.c           NewWindow/DisposeWindow/SetPort/FrontWindow/
│   │                             SelectWindow/DragWindow/FindWindow/BeginUpdate/
│   │                             EndUpdate/TrackGoAway -- pure C, calls CocoaBridge
│   │                             for the actual NSWindow/NSView work
│   ├── MenuManager.c             NewMenu/AppendMenu/InsertMenu/DrawMenuBar/
│   │                             MenuSelect/MenuKey/HiliteMenu (backed by NSMenu
│   │                             via CocoaBridge)
│   ├── ControlManager.c          NewControl/DisposeControl/FindControl/TrackControl
│   │                             (push buttons only; pure C, no Cocoa needed)
│   ├── EventManager.c            WaitNextEvent/FlushEvents/GlobalToLocal/
│   │                             SystemClick/TickCount
│   ├── MiscManagers.c            InitFonts stub + the Memory Manager (NewHandle/
│   │                             NewPtr/HLock/... -- malloc wrappers). TEInit/
│   │                             InitDialogs moved out once TextEditManager.c/
│   │                             DialogManager.c made them real (Phase 1)
│   ├── TextEditManager.c         Phase 1: TENew/TEClick/TEKey/TEIdle/... -- one
│   │                             unwrapped editable line per field, drawn with Core
│   │                             Text for both rendering and hit-testing, embedded
│   │                             in each window's teFields[] pool
│   ├── DialogManager.c           Phase 1: NewDialog/ModalDialog/ParamText/Alert
│   │                             family -- DialogPtr is just WindowPtr (Types.h);
│   │                             GetNewDialog is a documented stub (no Resource
│   │                             Manager until Phase 2)
│   ├── CocoaBridge.m             the ONLY Objective-C / AppKit-importing file --
│   │                             not in the original plan; see §10. Owns RMWindow/
│   │                             RMContentView, NSMenu construction, NSApplication
│   │                             bootstrap, and the WaitNextEvent/mouse-tracking
│   │                             pump, all behind RetroMacBridge.h's primitive-only
│   │                             signatures
│   └── AppShell.m                thin now: just the real main(), boots Cocoa via
│                                  CocoaBridge then calls RetroMacUserMain()
├── tools/
│   ├── retromacc                 build-wrapper shell script (see §6)
│   └── pstr_rewrite.py           the "\p" literal rewrite pass (a real small
│                                  Python tokenizer, not a sed/regex one-liner --
│                                  see §6)
└── Toolbox.md                    this document
```

`CocoaBridge.m` is the only Objective-C file — everything else is
plain C so classic sources compile against it unchanged. It supplies
the process's *real* `main()` (via `AppShell.m`), sets up
`NSApplication`, creates an `NSWindow`/`NSView` per `WindowPtr`, and
pumps the Cocoa run loop from inside `WaitNextEvent` — the classic
app's `main()` (in `wordle.c`) is renamed at build time (see §6) and
invoked once Cocoa is ready.

## 4. API surface required for Phase 0 (`wordle.c` + `demo.c`)

Scanned directly from the two existing sources — this is the
concrete, minimal contract Phase 0 must satisfy:

| Category | Functions |
|---|---|
| Bootstrap | `InitGraf`, `InitFonts`, `InitWindows`, `InitMenus`, `TEInit`, `InitDialogs`, `InitCursor`, `FlushEvents` |
| QuickDraw geometry | `SetRect`, `InsetRect` |
| QuickDraw drawing | `EraseRect`, `FrameRect`, `FrameRoundRect`, `PenSize`, `PenNormal`, `MoveTo` |
| QuickDraw text | `TextFont`, `TextSize`, `TextFace`, `DrawString`, `DrawChar`, `StringWidth`, `GetFontInfo` |
| QuickDraw color | `RGBForeColor`, `RGBBackColor` |
| Window Manager | `NewWindow`, `DisposeWindow`, `SetPort`, `FrontWindow`, `SelectWindow`, `DragWindow`, `FindWindow` |
| Menu Manager | `NewMenu`, `AppendMenu`, `InsertMenu`, `DrawMenuBar`, `MenuSelect`, `MenuKey`, `HiliteMenu` |
| Control Manager | `NewControl`, `DisposeControl`, `FindControl`, `TrackControl` |
| Event Manager | `WaitNextEvent`, `GlobalToLocal`, `SystemClick`, `BeginUpdate`, `EndUpdate`, `TrackGoAway` |
| Utility/macros | `TickCount`, `HiWord`, `LoWord` |
| Types | `WindowPtr`, `MenuHandle`, `ControlHandle`, `EventRecord`, `Rect`, `Point`, `RGBColor`, `FontInfo`, `QDGlobals`, `Boolean`, `Str255`/Pascal-string literals |

Everything in this table gets a real implementation before Phase 0
is "done." Anything a future test program calls that isn't in this
table returns a documented stub (compiles, logs to stderr once,
no-ops) rather than a link error — see §7.

### 4a. API surface added for Phase 1 (`dialogdemo.c`)

| Category | Functions |
|---|---|
| Dialog Manager | `NewDialog`, `ModalDialog`, `ParamText`, `Alert`, `StopAlert`, `NoteAlert`, `CautionAlert` |
| TextEdit | `TENew`, `TEDispose`, `TESetText`, `TEGetText`, `TEClick`, `TEKey`, `TEIdle`, `TEActivate`, `TEDeactivate`, `TEUpdate`, `TESetSelect`, `TECalText` |
| Types | `DialogPtr` (== `WindowPtr`), `TEHandle` |

`GetNewDialog` is declared but is a documented stub (§7) — there's no
Resource Manager yet to look up a dialog ID's DLOG/DITL against
(Phase 2). See §11 for what `NewDialog`/`ModalDialog`/TextEdit
actually diverge from real Inside Macintosh behavior to work without
one.

## 5. Coordinate system & rendering pipeline

- Each `WindowPtr` owns one off-screen `CGContext` (8-bit RGBA,
  device RGB) sized to the window's port rect, but it's a *plain
  native* (bottom-left-origin, y-up) bitmap context, not a flipped
  one — `QuickDraw.c` converts classic top-down `Rect`/`Point`
  coordinates to native bottom-up ones itself on every call (using
  the port's height), rather than fighting a CTM flip. See §10 for
  why: a flipped buffer's `CGImage` doesn't composite correctly into
  a flipped `NSView` without extra correction, so keeping the buffer
  native and doing the flip once at blit time (in `CocoaBridge.m`)
  turned out simpler than flipping twice.
- `CocoaBridge.m`'s `RMContentView.drawRect:` blits the window's
  `CGContext` image to screen on every AppKit repaint — the classic
  app never talks to `NSView` directly, it only ever draws into the
  offscreen buffer via `SetPort`/QuickDraw calls, and `RM_MarkDirty`/
  `RM_FlushDirtyWindows` (called at the top of `WaitNextEvent`) is
  what actually triggers that repaint once per event-loop iteration.
- Text: `DrawString`/`DrawChar`/`StringWidth`/`GetFontInfo` are
  implemented with Core Text, using the current `TextFont`/`TextSize`/
  `TextFace` as a lookup into a small table mapping classic font IDs
  (0 = system font, etc.) and style bits (`bold`, `italic`, ...) to a
  host `CTFont`. Metrics won't be pixel-identical to the original
  Chicago/Geneva bitmap fonts, but `ascent`/`descent`/`StringWidth`
  stay internally consistent so layout math in the app (e.g.
  `wordle.c`'s centering via `StringWidth`) still centers correctly.
- Colors: `RGBColor` (`unsigned short r,g,b`, 0–0xFFFF range) convert
  to `CGColor` by dividing by 65535.0.

## 6. Build wrapper (`retromacc`)

Two problems classic sources hit on a stock `clang`:

1. **`main()` collision** — the classic app defines `main()`, but
   Cocoa needs to own the real entry point (to call
   `NSApplicationMain`-equivalent setup) before the app's game loop
   runs. `retromacc` compiles the user's file with
   `-Dmain=RetroMacUserMain`, so `app_shell.m`'s real `main()` calls
   `RetroMacUserMain()` after Cocoa is initialized.
2. **`"\p..."` Pascal-string literals** — MPW/Retro68 GCC has a
   nonstandard escape (`\p` at the *start* of a string literal turns
   it into a length-prefixed Pascal string). Stock clang has no such
   extension. `retromacc` runs every user source through
   `pstr_rewrite.py` first, which rewrites `"\pFoo"` into a compound
   literal `((unsigned char[]){3,70,111,111,0})` — a length byte, the
   raw Mac-Roman byte values (not re-escaped characters, so the
   rewrite never has to worry about quoting a stray `"` or `\`), and a
   trailing NUL. This turned out to need a real (if small) hand-written
   tokenizer rather than a regex one-liner, specifically so a stray
   `"\p"`-looking sequence inside a `/* comment */` or `'c'` char
   literal doesn't get misrewritten — see §10.

Usage:

```sh
retromacc wordle.c -o Wordle.app
```

`retromacc` rewrites each user source with `pstr_rewrite.py`, compiles
it with `clang -Dmain=RetroMacUserMain -I RetroMac/include`, compiles
the runtime sources normally (no `-Dmain`, since `AppShell.m` needs to
keep its own real `main()`), links everything against AppKit/
CoreGraphics/CoreText, and wraps the resulting Mach-O binary in a
minimal `.app` bundle (`Info.plist` + bundle dirs) so it's
double-clickable and gets a normal menu bar/Dock icon. On Apple
Silicon it also ad-hoc codesigns the bundle (`codesign --sign -`) —
an unsigned Mach-O simply won't launch there.

## 7. Stub policy

Toolbox calls outside the Phase 0 table (§4) that later test
programs need — e.g. `GetNewWindow`, `ParamText`, `Alert`,
`TESetText` — should be added incrementally, in the same
implementation style, rather than stubbed indefinitely. Phase 1 did
exactly this for `ParamText`/`Alert`/`TESetText` (now real, §4a);
`GetNewDialog` is the one Phase 1 symbol that stayed a real stub,
since it fundamentally needs the Resource Manager (Phase 2), not just
more implementation effort.

The planned `RETROMAC_STUB(name)` weak-symbol macro (auto-stubbing
any undeclared call with a stderr warning) was **not** built for
Phase 0 — it wasn't needed, since `wordle.c`/`demo.c` only ever call
things already in the §4 table, and a real link error for a missing
symbol is arguably a clearer signal during bring-up than a silently
no-op'd trap anyway. Still not needed as of Phase 1 — `dialogdemo.c`
only calls things in §4/§4a, plus the one explicit `GetNewDialog`
stub above.

## 8. Phased roadmap

- **Phase 0 — done.** The table in §4. `wordle.c` and `demo.c` both
  compile with `retromacc` and are playable/clickable: green/yellow/
  gray cell coloring, the About dialog, and menu commands all
  verified working by launching the built apps and driving them with
  real clicks/keystrokes. See §10 for the bring-up notes.
- **Phase 1 — done.** The table in §4a. Dialog Manager (`NewDialog`,
  `ModalDialog`, DITL-less — items are `NewControl`/`TENew` calls on
  the dialog in creation order rather than a DITL resource),
  `Alert`/`StopAlert`/`NoteAlert`/`CautionAlert`, and `TextEdit`
  editable fields (`TENew`, `TEClick`, `TEKey`, caret blink via
  `TEIdle`). `dialogdemo.c` compiles with `retromacc` and was verified
  working by launching the built app and driving it with real clicks/
  keystrokes: typing/backspacing/arrow-key-navigating a text field,
  click-to-reposition the caret, OK vs. Cancel, Return-triggers-
  default-item, and the `NoteAlert` icon/message. `GetNewDialog`
  (resource-ID based) is a documented stub — see §4a/§11.
- **Phase 2** — Resource Manager subset: parse `.rsrc`/Rez-compiled
  data so `demo.r`-style resource-driven apps (not just
  programmatic ones) can load `WIND`/`MENU`/`DITL`/`ALRT` resources
  at runtime instead of requiring hand-written C.
- **Phase 3** — Color QuickDraw regions/`PICT`, offscreen `GWorld`s,
  `Random`/sound (`SysBeep`), scrap (clipboard) via `TextEdit`
  `TECopy`/`TEPaste` → macOS pasteboard.
- **Out of scope indefinitely**: real PowerPC/68k execution,
  QuickTime, AppleTalk/networking, printing (`PrintManager`).

## 9. Testing plan

- `wordle.c` and `demo.c` are the Phase 0 acceptance tests: build
  with `retromacc`, launch, and manually verify (per this repo's
  `/verify`-style workflow): menu bar renders and responds to real
  clicks, About dialog's OK button and default-button ring draw
  correctly, and (for Wordle) a full game — typing letters,
  backspace, Enter, win/loss messaging, File ▸ New Game — behaves
  identically to the described rules. **Done for Phase 0** — see §10
  for exactly what was and wasn't screenshot-confirmed.
- Once Phase 0 passes, treat each new sample app added to the repo
  as the acceptance test for whatever new Toolbox surface it
  introduces, extending §4's table rather than writing synthetic
  unit tests against Toolbox internals in isolation.
- `dialogdemo.c` is the Phase 1 acceptance test, per that same
  convention: build with `retromacc`, launch, and manually verify a
  text-entry dialog (typing, backspace, left/right arrow-key caret
  movement, click-to-reposition the caret, OK vs. Cancel round-
  tripping the text via `TESetText`/`TEGetText`), Return triggering
  the default item, and a `NoteAlert` built from `ParamText`.
  **Done for Phase 1** — see §11.

## 10. Phase 0 bring-up notes

Three things about the real Cocoa/AppKit interaction didn't match
the plan's assumptions, and cost most of the implementation time.
Recording them here so Phase 1 doesn't re-discover them the hard way.

**AppKit and RetroMac's own headers can't coexist in one file.**
`#import <Cocoa/Cocoa.h>` transitively drags in
`ApplicationServices/HIServices/QD.framework`'s legacy Carbon/
QuickDraw compatibility headers (still shipped in the macOS SDK for
old source compat), which redefine `WindowPtr`, `GrafPtr`,
`DialogPtr`, `RGBColor`, `BitMap`, `struct GrafPort`, `TickCount()`,
and the `HiWord`/`LoWord` macros — the *exact* names RetroMac's own
public headers define, but with different (opaque/incompatible)
underlying types. No amount of include-guard trickery fixes this
(tried predefining `__QUICKDRAW__` to suppress it — breaks unrelated
declarations elsewhere in the SDK that depend on it, like `Icons.h`
needing `RGBColor`). The only real fix: **exactly one file
(`CocoaBridge.m`) is allowed to import AppKit**, and it never
includes RetroMac's own `Types.h`/`Quickdraw.h`/etc. Every function
crossing that boundary (`RetroMacBridge.h`) uses only names that are
safe on both sides — `Point`/`Rect`/`Boolean` (identical in both
worlds via `<MacTypes.h>`, no collision), plain integers, `void *`
opaque handles, and `CGContextRef`. This is *the* architectural
change from the original plan (which assumed a handful of `.m` files
each owning one manager).

**A freshly-created offscreen `CGBitmapContext`'s image is *not*
auto-oriented when drawn into a flipped `NSView`.** The plan assumed
`CGContextDrawImage` "just handles" flipped-context orientation.
Empirically, it doesn't — content rendered upside down (confirmed via
screenshot: "WORDLE" came out looking like "ꟽOꓤ⊐ГE") until
`RMContentView.drawRect:` wraps the blit in an explicit counter-flip
(`CGContextTranslateCTM` + `CGContextScaleCTM(1,-1)`). Text drawn via
Core Text directly into the buffer needed no such flip, since the
buffer itself is a plain native (bottom-left-origin, y-up)
`CGBitmapContext` and Core Text's baseline model already matches
that; only the *image compositing step* into the flipped view needed
correction.

**Real menu-bar clicks never reach the app's own event queue at
all.** The plan assumed a menu-bar `mouseDown` would show up via
`[NSApp nextEventMatchingMask:...]` like any other event, letting
`WaitNextEvent` detect it, forward it to `-sendEvent:` for real
tracking, and translate the *same* event for the classic app's
`FindWindow`/`MenuSelect` call chain. Instrumented with debug
logging: zero mouseDown events were ever observed for real menu-bar
clicks, yet the `NSMenuItem`'s target-action fired correctly every
time — AppKit intercepts and fully owns menu-bar click tracking below
the level `nextEventMatchingMask` can see, invoking the item's action
via its own internal mechanism regardless. Fix: `RMCocoa_WaitNextEvent`
polls `RM_HasPendingMenuSelection()` (backed by the same
`gPendingMenuSelection` slot the `NSMenuItem` action already writes
to) and *synthesizes* a `mouseDown` in the menu-bar strip once a
selection appears, letting the classic app's existing
`FindWindow → inMenuBar → MenuSelect` code run unmodified. This in
turn caused a **100%-CPU busy-spin**: if the current event loop never
calls `MenuSelect` to consume the pending value (e.g. `DoAboutDialog`'s
own simplified nested loop, which classic apps commonly write without
menu handling), the synthetic event gets re-manufactured on every
call with zero delay forever. Fixed by pacing that path to the same
~20 Hz slice used for normal polling (`[NSThread sleepForTimeInterval:
sliceInterval]` before returning) — a never-consumed selection now
just idles harmlessly instead of pegging a core.

**Known Phase 0 limitations, accepted rather than fixed:**
- The Apple-menu's title shows the bold running-app name ("Wordle")
  instead of the classic Apple-logo glyph. Cocoa forces `mainMenu`
  item 0's *displayed* title to the process name regardless of what
  string the `NSMenuItem` was given — a real, documented Cocoa
  convention, not a bug in the byte-0x14-sentinel handling (which
  correctly identifies the Apple menu internally; it just can't win
  the display-title fight for slot 0).
- Window dragging (`DragWindow`) and the close box (`TrackGoAway`)
  are implemented with the same `RMCocoa_TrackMouse` primitive already
  proven live for `TrackControl`'s button-press tracking, and reviewed
  by inspection, but weren't independently screenshot-confirmed in a
  dedicated interactive test the way window rendering, keyboard input,
  game logic, the menu bar, and the About dialog were — AppleScript UI
  scripting can't reliably hit-test RetroMac's borderless,
  accessibility-invisible windows by position for this specific
  gesture (menu bar items live in a separate, reliably-queryable
  accessibility tree; window content does not, since `NSWindow`
  instances with `NSWindowStyleMaskBorderless` don't expose the same
  `AXWindow` surface a titled window would). Worth a manual click-test
  pass before relying on it.

## 11. Phase 1 bring-up notes

Two assumptions from the Phase 1 plan held up cleanly, and one testing
technique is worth recording for later phases.

**Arrow keys needed zero `EventRecord`/bridge plumbing.** The plan
worried that `TEKey`-driven caret navigation would need `keyCodeMask`
(currently always 0 — `RMCocoa_WaitNextEvent` never populates it)
threaded through from `CocoaBridge.m`. It doesn't: real classic
TextEdit already receives arrow keys as ordinary charCodes `0x1C`-`0x1F`
(left/right/up/down) in the *low* byte, the same byte
`wordle.c`/`demo.c` already mask out with `charCodeMask`. So the only
change needed was four more `unichar` special cases (alongside the
existing Delete→8 mapping) in `CocoaBridge.m`'s keyDown handling —
`TEKey` itself needed no new plumbing to see them.

**Item numbering by creation order (no DITL) needed no new API
surface.** `ModalDialog`'s `itemHit` comes from `nextItemNumber`,
incremented on the dialog by both `NewControl` and `TENew` at creation
time (`RetroMacInternal.h`). Apps just create their OK button before
their Cancel button, exactly like Phase 0's About boxes already did by
convention — no "register item N" call had to be invented.

**AppleScript UI-scripting's window-content limitation from §10 is
still real, but `cliclick` (raw `CGEvent` clicks by screen coordinate,
independent of the accessibility tree) works around it.** Menu bar
commands were driven via `System Events`' real `NSMenu` accessibility
tree as before; clicking *inside* a dialog's content area (the text
field, OK/Cancel buttons) needed `cliclick c:<x>,<y>` at the window's
known screen position instead, since RetroMac's borderless windows
still don't expose an `AXWindow` surface. Worth reaching for `cliclick`
first, rather than fighting accessibility APIs, for any future
interactive verification of window content.
