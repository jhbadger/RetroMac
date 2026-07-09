# RetroMac Toolbox — a Classic Mac Toolbox shim for modern macOS

> **Status: Phases 0, 1, and 2 are done and verified.** `wordle.c` and
> `demo.c` both build with `retromacc` and run as real, interactive
> `.app` bundles — windows, colored/text drawing, keyboard input, the
> real system menu bar, and the programmatic About dialog all
> confirmed working by launching the built apps and driving them with
> actual clicks/keystrokes, not just a clean compile. Phase 1 adds a
> real Dialog Manager (`NewDialog`/`ModalDialog`/`Alert` family) and
> TextEdit (`TENew`/`TEClick`/`TEKey`); `dialogdemo.c` is its
> acceptance test, confirmed the same way. `DragWindow` (previously
> only reviewed by inspection) and a 2× display scale (§5) are also now
> confirmed working, including fixing a real `DragWindow` bug the
> scaling work surfaced. Phase 2 adds a real Resource Manager
> (`GetResource`/`Get1Resource`) and wires `GetNewWindow`/`GetMenu`/
> `GetNewDialog`/`Alert` family to real `WIND`/`MENU`/`DLOG`+`DITL`/
> `ALRT`+`DITL` resources compiled by the real system `Rez`;
> `rezdemo.c`/`rezdemo.r` is its acceptance test, confirmed the same
> way. The component map in §3 and the build-wrapper description in §6
> describe what was actually built (which diverged from the original
> plan in a few places); see §10 for Phase 0's bring-up notes, §11 for
> Phase 1's, §12 for the display-scale notes, and §13 for Phase 2's.

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
│   │   Dialogs.h, Events.h, Memory.h, Controls.h, OSUtils.h,
│   │   Resources.h (Phase 2: GetResource/Get1Resource/ReleaseResource)
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
│   ├── DialogManager.c           NewDialog/ModalDialog/ParamText/Alert family
│   │                             (Phase 1) -- DialogPtr is just WindowPtr (Types.h).
│   │                             GetNewDialog and DITL-driven Alert layouts (Phase 2)
│   │                             share one DITL-instantiation helper with GetNewDialog
│   ├── ResourceManager.c         Phase 2: GetResource/Get1Resource/ReleaseResource --
│   │                             parses the real resource-fork binary format (header/
│   │                             data/map/type-list/reference-list) that the real
│   │                             system Rez produces; also exports the big-endian
│   │                             read helpers WIND/MENU/DITL/DLOG/ALRT unpacking
│   │                             (in WindowManager.c/MenuManager.c/DialogManager.c)
│   │                             all share
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
├── rez/
│   └── RetroMac.r                Phase 2: one-line convenience #include of the
│                                  real system .r interface headers (MacTypes.r/
│                                  MacWindows.r/Menus.r/Dialogs.r) -- unlike the C
│                                  side, RetroMac doesn't reimplement its own .r
│                                  templates; real Rez already ships correct ones
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

`GetNewDialog` was declared but was a documented stub (§7) through
Phase 1 — there was no Resource Manager yet to look up a dialog ID's
DLOG/DITL against. See §11 for what `NewDialog`/`ModalDialog`/TextEdit
diverge from real Inside Macintosh behavior to work without one.

### 4b. API surface added for Phase 2 (`rezdemo.c`)

| Category | Functions |
|---|---|
| Resource Manager | `GetResource`, `Get1Resource`, `ReleaseResource` |
| Window Manager | `GetNewWindow` (now real — unpacks a `WIND` resource) |
| Menu Manager | `GetMenu` (unpacks a `MENU` resource) |
| Dialog Manager | `GetNewDialog` (now real — unpacks `DLOG`+`DITL`); `Alert`/`StopAlert`/`NoteAlert`/`CautionAlert` now look up a real `ALRT`+`DITL` by ID, falling back to Phase 1's fixed hand-drawn box if none exists |

No new types — `ResType`/`Handle` already come from `<MacTypes.h>` via
`Types.h`. See §13 for the exact binary layouts (empirically confirmed
by compiling real resources with the real system Rez and hex-dumping
the result) and the divergences this surfaced.

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
- **Display scale** (`RM_DISPLAY_SCALE`, `RetroMacBridge.h`, default
  2): classic UI conventions (12pt system font, a 20px title bar) are
  tiny on a modern high-resolution, physically-large display, so the
  whole classic coordinate universe — screen bounds, window positions/
  sizes, mouse points — is uniformly `RM_DISPLAY_SCALE` smaller than
  real screen points, exactly like a Retina `backingScaleFactor`. The
  entire feature is confined to the classic/native boundary that
  already existed for other reasons: `WindowManager.c`'s `NewWindow`
  creates each window's offscreen `CGContext` at
  `RM_DISPLAY_SCALE`-times-classic pixel dimensions and pre-scales its
  CTM once (`CGContextScaleCTM`) right at creation, and `CocoaBridge.m`
  scales window frame creation, the hand-drawn title bar/close box, and
  every real-screen-point-to-classic-`Point` conversion
  (`ClassicPointFromScreenPoint` and friends). Because the CTM is
  pre-scaled, ordinary classic-unit `CGContext`/Core Text drawing calls
  in `QuickDraw.c`/`ControlManager.c`/`TextEditManager.c`/
  `DialogManager.c` land at the right physical pixel density
  automatically — **none of those files needed any changes at all**,
  and classic app sources (`wordle.c`/`demo.c`/`dialogdemo.c`) are
  completely unaware the scale exists. See §12 for what this actually
  touched and a pre-existing bug it surfaced.

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

**Phase 2: `.r` files** — pass any `.r` sources alongside the `.c`
ones, e.g. `retromacc rezdemo.c rezdemo.r -o RezDemo.app`. Each is
compiled by the *real* system `Rez` (found via `xcrun`, present on any
machine with Xcode or just the Command Line Tools) against
`RetroMac/rez/RetroMac.r`'s umbrella include, producing a flat
resource-format blob (`-useDF`) embedded as `Contents/Resources/
Resources.rsrc`, which `AppShell.m` loads at boot. `.r` files are
**listed explicitly, not auto-detected** by matching a `.c` file's
basename — see §13 for why that distinction turned out to matter in
practice (this repo has a stale, unrelated `demo.r` that happens to
share `demo.c`'s basename purely by coincidence).

## 7. Stub policy

Toolbox calls outside the Phase 0 table (§4) that later test
programs need — e.g. `GetNewWindow`, `ParamText`, `Alert`,
`TESetText` — should be added incrementally, in the same
implementation style, rather than stubbed indefinitely. Phase 1 did
exactly this for `ParamText`/`Alert`/`TESetText` (now real, §4a);
`GetNewDialog` was the one Phase 1 symbol that stayed a real stub,
since it fundamentally needed the Resource Manager, not just more
implementation effort — Phase 2 (§4b) is exactly that, and
`GetNewDialog` is now real too. No stubs remain as of Phase 2.

The planned `RETROMAC_STUB(name)` weak-symbol macro (auto-stubbing
any undeclared call with a stderr warning) was **not** built for
Phase 0 — it wasn't needed, since `wordle.c`/`demo.c` only ever call
things already in the §4 table, and a real link error for a missing
symbol is arguably a clearer signal during bring-up than a silently
no-op'd trap anyway. Still not needed as of Phase 2 — every sample app
only calls things in §4/§4a/§4b.

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
- **Phase 2 — done.** The table in §4b. Real Resource Manager
  (`GetResource`/`Get1Resource`/`ReleaseResource`) reading the actual
  resource-fork binary format that the real system `Rez` produces (no
  reimplemented Rez compiler needed — see §13); `GetNewWindow`,
  `GetMenu`, `GetNewDialog`, and the `Alert` family now all load real
  `WIND`/`MENU`/`DLOG`+`DITL`/`ALRT`+`DITL` resources.
  `rezdemo.c`/`rezdemo.r` compiles with `retromacc` (`.r` files are
  passed explicitly, not auto-detected — the stale `demo.r` already in
  this repo predates RetroMac and is unrelated, see §13) and was
  verified working by launching the built app and driving it with real
  clicks/keystrokes: the `WIND`-placed/titled main window, both
  `MENU`-loaded menus (including a working Cmd-key), the `DLOG`+`DITL`
  dialog's statText/OK/Cancel, Return resolving to the real DITL-order
  default item, and the chained `ALRT`+`DITL` alert's `^0` substitution
  all confirmed on screen.
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
- `rezdemo.c`/`rezdemo.r` is the Phase 2 acceptance test: build with
  `retromacc rezdemo.c rezdemo.r -o RezDemo.app`, launch, and manually
  verify the `WIND`/`MENU`/`DLOG`+`DITL`/`ALRT`+`DITL` resources alone
  (no hand-authored `NewWindow`/`NewMenu`/`NewDialog` calls) drive a
  working window, menu bar, dialog, and alert. **Done for Phase 2** —
  see §13.

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
- ~~Window dragging (`DragWindow`) and the close box (`TrackGoAway`)
  are implemented with the same `RMCocoa_TrackMouse` primitive already
  proven live for `TrackControl`'s button-press tracking... worth a
  manual click-test pass before relying on it.~~ **This hedge was
  correct to be suspicious, and it turned out to hide a real bug, not
  just an untested-but-fine code path** — see §12.

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

## 12. Display-scale bring-up notes

**`DragWindow` was actually broken since Phase 0 — not just
unconfirmed — and it took a real user report to catch it.** While
adding the scale factor, `RMCocoa_WindowContentOriginClassic` (re-
derives `contentOriginGlobal` after a drag) was updated defensively to
add the title bar height back in, on the theory that it had been
returning the *structure* top instead of the *content* top. That
theory was correct, but fixing that one function wasn't the actual
bug, and an initial pass at this section claimed dragging was
"confirmed working" based on that fix plus unrelated content-area
click tests (an About box's OK button, a menu command) — it was not
actually re-tested. When the user reported windows still couldn't be
moved, direct instrumentation (temporary `fprintf`s in `DragWindow`/
`RMCocoa_TrackMouse`) plus a from-scratch coordinate re-derivation
found the real bug: **`RMCocoa_CreateWindow`'s frame-origin formula
had a duplicated `titleBarH` term**
(`screenH - (contentBoundsGlobal.top + titleBarH + height) * SCALE`),
which places the window's on-screen structural top exactly at
`contentBoundsGlobal.top` instead of `titleBarH` pixels *above* it.
That renders every `documentProc` window's content (and its title bar)
`titleBarH` pixels lower on screen than `FindWindow`'s hit-testing
model (`structTop = contentOriginGlobal.v - titleBarH`) expects — so a
real click anywhere on the *visible* title bar always fails
`FindWindow`'s `pt.v < contentOriginGlobal.v` test and gets classified
as `inContent` instead of `inDrag`/`inGoAway`. The close box was
equally broken by the same root cause, for the same reason. Invisible
in Phase 0's own interactive testing because that testing only ever
drove `dBoxProc` About boxes (title bar height 0, so the duplicated
term is zero and the bug vanishes) — exactly what §10's own hedge
("worth a manual click-test pass before relying on it") was
unknowingly flagging. Fixed by removing the duplicated term
(`screenH - (contentBoundsGlobal.top + height) * SCALE`); confirmed via
a clean launch reporting the window at exactly the position the
corrected formula predicts, and by the user's own drag test afterward.
Lesson: a plausible-sounding fix for a *related* function is not the
same as re-testing the actual reported symptom — should have re-driven
an actual drag before saying so.

**The uniform-scale model (same classic coordinates, everything
`RM_DISPLAY_SCALE` bigger in real screen points) turned out simpler
than an earlier "magnify window content in place, keep its screen
position fixed" framing.** The latter would have made the classic-to-
real mapping depend on which window (if any) a click landed in,
requiring hit-testing in real screen space *before* any classic-space
conversion — a real restructuring of the mouse-event pipeline. Scaling
window *position* along with size, uniformly, keeps the existing
"convert real point to classic point, then hit-test in classic space"
pipeline (`ClassicPointFromScreenPoint` → `FindWindow`) completely
intact; only the conversion functions themselves needed the scale
factor, not their calling order.

**Verified** (after the `RMCocoa_CreateWindow` fix above): built
`demo.app`/`dialogdemo.app`/`wordle.app`. A `documentProc` window
placed at classic content-top `(60,60)`, size `400×260` (title bar
height 20) now reports its real on-screen frame at exactly
`(120,80)`/`800×560` — i.e. `RM_DISPLAY_SCALE` × `(60,60-20)`/`(400,280)`,
matching the corrected formula's prediction exactly, not the
`(120,120)` an earlier (buggy) build reported. Text renders crisp at
the larger size (genuine higher-resolution Core Text rendering via the
pre-scaled CTM, not a blurry upscaled blit). A real click on the About
dialog's (now 2×-sized) OK button correctly dismissed it, and — the
actual regression this section exists to document — dragging a
`documentProc` window by its visible title bar, and clicking its
visible close box, both now work, confirmed by the user directly
rather than by automated screenshot alone.

## 13. Phase 2 bring-up notes

**Real `Rez`/`DeRez` are genuinely present and usable** — `/usr/bin/Rez`
(and under both a full Xcode.app and just the Command Line Tools,
discoverable via `xcrun --show-sdk-path` regardless of which is
active), along with the real system `.r` interface headers
(`MacTypes.r`, `MacWindows.r`, `Menus.r`, `Dialogs.r` under the
Carbon/CarbonCore framework headers in the SDK). `Rez -useDF` compiles
ordinary `.r` source into a flat, single-file resource-fork-format blob
— no HFS+ resource-fork/xattr involved, and no reimplemented Rez
compiler needed on RetroMac's side, unlike `retromacc`'s bespoke `\p`-
string rewriter (which exists because no stock tool does that job).

**The exact binary layouts below were confirmed empirically** — by
compiling real resources with the real system Rez and decoding the
output byte-for-byte with a throwaway Python script cross-checked
against the known Rez source, not by trusting memory of Inside
Macintosh. That process caught two real bugs before they shipped:

- `GetMenu`'s first draft read `enableFlags` at offset 8 and the title
  at offset 12 — both off by 2 (the real menuProc placeholder field is
  4 bytes, not 2). A smoke test that only checked "`GetMenu` returns a
  non-NULL handle and doesn't crash" would not have caught this — it
  took decoding a real compiled `MENU` resource field-by-field in
  Python and comparing against the known source to notice. Lesson:
  "didn't crash" is not evidence a byte-offset parser is correct;
  decode a real example against known values before trusting it.
- `retromacc`'s first draft auto-detected a `.r` file to compile by
  matching a `.c` file's basename (`demo.c` → look for `demo.r`). This
  repo already has a `demo.r` — a leftover from before RetroMac existed
  (`git log --follow` traces it to the very first commit) that
  references an unrelated Retro68/MacBinary/WASM "splice" pipeline —
  which happens to share `demo.c`'s basename by pure coincidence.
  Auto-detection silently tried to compile it and broke `demo.c`'s
  previously-working build (`demo.r` references headers, like
  `Windows.r`, that don't exist under that name in the modern SDK).
  Fixed by requiring `.r` files to be listed explicitly on the
  `retromacc` command line instead of inferred from a `.c` file's name
  — caught by rebuilding *all* existing samples after adding the new
  feature, not just the new one.

Confirmed resource-fork container format (all multi-byte fields
big-endian): a 16-byte header (`dataOffset`, `mapOffset`, `dataLength`,
`mapLength`, each a 4-byte length), a data section of
length-prefixed (4-byte length) blobs, and a map section: a 16-byte
copy of the header, a 4-byte next-map handle (unused), a 2-byte file
ref num (unused), a 2-byte attributes word, a 2-byte type-list offset
and 2-byte name-list offset (both relative to the map's start), then
the type list itself (a count-1 word, then per type: 4-byte `ResType`,
count-1 word, reference-list offset relative to the type list's own
start) and each type's reference list (per entry: resID, name offset,
1-byte attributes, 3-byte data offset relative to the data section,
4-byte handle placeholder).

Confirmed resource layouts (all offsets from the start of the
resource's own data, after its 4-byte length prefix):

| Type | Layout |
|---|---|
| `WIND` | boundsRect (4 shorts: top,left,bottom,right), procID (short), visible (short, nonzero=true), goAwayFlag (short), refCon (long), title (pstr), optional trailing positioning word (ignored) |
| `MENU` | menuID (short), width/height (shorts, ignored), a 4-byte menuProc placeholder (ignored), enableFlags (long, bit *N* = item *N*), title (pstr), then items until a zero-length title: itemTitle (pstr), icon/cmdKey/mark/style (1 byte each) |
| `DLOG` | same as `WIND` through refCon, then itemsID (short), title (pstr), optional positioning word |
| `DITL` | itemCount-1 (short), then per item: 4-byte placeholder, itemRect (4 shorts), type byte (0x80 bit = disabled, low 7 bits = type: 4=button, 5=checkbox, 6=radio, 8=statText, 16=editText — icon/control/picture/userItem not supported, silently skipped), 1-byte data length, that many data bytes, +1 pad byte if the data length is odd |
| `ALRT` | boundsRect (4 shorts), itemsID (short), a 2-byte "stages" word (per-click sound/redraw hints — ignored), optional positioning word |

One alignment subtlety: a title Pascal string immediately followed by
another field (the positioning word in `WIND`/`DLOG`) gets a padding
byte inserted when the *total* `1 + titleLen` is odd — confirmed by
comparing two real compiles, one with an odd-length title (no padding
needed) and one with an empty title (padding needed). `DITL` item data
is word-aligned the same way, though the specific items compiled
during bring-up happened to both be even-length, so that path is
implemented per Inside Mac's documented convention rather than having
been directly exercised.

**Modern Rez encodes string literals as UTF-8, not Mac Roman** —
confirmed: a menu item containing "…" compiled to the 3-byte UTF-8
sequence, not the 1-byte Mac Roman 0xC9. `RM_MacRomanPStringToUTF8`
(used throughout to render resource-sourced Pascal strings) assumes
Mac Roman, so any non-ASCII byte in a resource's text will render as
mojibake — ASCII-only text round-trips fine, since ASCII is a subset of
both encodings. `rezdemo.r` sticks to plain ASCII (`"..."` instead of
`"…"`) to sidestep this in its own acceptance test; a real fix would
mean either re-encoding resource strings from UTF-8 at load time or
teaching the renderer to detect which encoding a given string is in —
left as a known limitation rather than fixed, since no sample app here
needs non-ASCII resource text.

**Verified**: built `rezdemo.app` with `retromacc rezdemo.c rezdemo.r
-o RezDemo.app`, confirmed `Contents/Resources/Resources.rsrc` exists,
launched, and drove it with real clicks/keystrokes: the main window
appeared at exactly the `RM_DISPLAY_SCALE`-scaled position/size the
`WIND` resource specified with the right title and body text; both
`MENU`-loaded menus appeared correctly (including the Cmd-Q
equivalent); the `DLOG`+`DITL` dialog rendered its statText and both
buttons correctly positioned from the resource; Return resolved to the
real DITL-order default item (item 1, OK) and dismissed it; the
resulting `NoteAlert` — looked up by ID, falling through to its own
`ALRT`'s `itemsID` → `DITL` 129 — showed the exact message text passed
to `ParamText` substituted for `^0`. Also rebuilt `wordle.app`/
`demo.app`/`dialogdemo.app` (no `.r` files passed) to confirm zero
regression for apps with no resources.
