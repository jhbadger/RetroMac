# RetroMac Toolbox — a Classic Mac Toolbox shim for modern macOS

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
├── include/                     (drop-in classic headers)
│   ├── Types.h
│   ├── Quickdraw.h
│   ├── Fonts.h
│   ├── Windows.h
│   ├── Menus.h
│   ├── TextEdit.h
│   ├── Dialogs.h
│   ├── Events.h
│   ├── Memory.h
│   ├── Controls.h
│   └── OSUtils.h                (TickCount, HiWord/LoWord)
├── src/
│   ├── qd_types.c/h              Rect/Point/RGBColor helpers, SetRect/InsetRect
│   ├── qd_port.c/h                GrafPort/CGrafPort + per-window ARGB8888 buffer
│   ├── qd_draw.c                  EraseRect/FrameRect/FrameRoundRect/PenSize/PenNormal
│   ├── qd_text.c                  TextFont/TextSize/TextFace/DrawString/DrawChar/
│   │                              StringWidth/GetFontInfo (via Core Text)
│   ├── qd_color.c                 RGBForeColor/RGBBackColor (RGBColor -> CGColor)
│   ├── win_manager.c              NewWindow/DisposeWindow/ShowWindow/SetPort/
│   │                              FrontWindow/SelectWindow/DragWindow/FindWindow
│   ├── menu_manager.c             NewMenu/AppendMenu/InsertMenu/DrawMenuBar/
│   │                              MenuSelect/MenuKey/HiliteMenu (backed by NSMenu)
│   ├── control_manager.c          NewControl/DisposeControl/FindControl/TrackControl
│   ├── dialog_manager.c           InitDialogs (stub), programmatic dBoxProc windows
│   ├── event_manager.c            WaitNextEvent/FlushEvents/GlobalToLocal/
│   │                              EventRecord translation from NSEvent
│   ├── memory_manager.c           NewHandle/NewPtr/HLock/HUnlock/DisposeHandle
│   │                              (thin malloc wrappers — flat 64-bit memory model)
│   ├── te_manager.c                TEInit (stub; full TextEdit is Phase 2)
│   ├── font_manager.c              InitFonts (loads a bundled bitmap-ish system font)
│   ├── pascal_string.c/h           c2pstr/p2cstr, Str255 helpers
│   └── app_shell.m                 NSApplication bootstrap, run loop pump, main()
│                                    trampoline that calls the user's classic main()
├── tools/
│   └── retromacc                   build-wrapper shell script (see §6)
└── Toolbox.md                      this document
```

`app_shell.m` is the only Objective-C file; everything else is
plain C so classic sources compile against it unchanged. It supplies
the process's *real* `main()`, sets up `NSApplication`, creates an
`NSWindow`/`NSView` per `WindowPtr`, and pumps the Cocoa run loop
from inside `WaitNextEvent` — the classic app's `main()` (in
`wordle.c`) is renamed at build time (see §6) and invoked once
Cocoa is ready.

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

## 5. Coordinate system & rendering pipeline

- QuickDraw's local coordinate space is top-left-origin, y-down, same
  as Core Graphics' *view* space when flipped — so each window's
  backing `NSView` sets `isFlipped = YES`, letting `qd_draw.c` write
  `Rect{top,left,bottom,right}` straight into `CGContext` calls
  without manual axis flipping.
- Each `WindowPtr` owns one off-screen `CGContext` (ARGB8888,
  device RGB) sized to the window's port rect. All `QDDraw*` calls
  render into that context.
- On `updateEvt` / `BeginUpdate`/`EndUpdate`, `app_shell.m`'s
  `NSView.drawRect:` simply blits the window's `CGContext` image to
  screen — the classic app never talks to `NSView` directly, it only
  ever draws into the offscreen buffer via `SetPort`/QuickDraw calls.
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
   extension. `retromacc` runs a small source-to-source pass (a
   `sed`/regex pre-pass, not a full C parser — Pascal-string literals
   in these files are always simple string constants, never built
   with macros) that rewrites `"\pFoo"` into a compound literal
   `(unsigned char[]){4,'F','o','o',0}` — this is the same trick used
   internally by `c2pstr`-style shims. Escapes like `\311` (octal,
   0xC9 ellipsis) and `\024` (0x14, Apple logo) are left to the
   standard C preprocessor, which already understands octal escapes.

Usage:

```sh
retromacc wordle.c -o Wordle.app
```

`retromacc` is a shell script that: pre-processes `\p` literals →
temp file, compiles with `clang -Dmain=RetroMacUserMain -IRetroMac/include`,
links against `libRetroMac.a` + `app_shell.m` + AppKit/CoreGraphics/
CoreText frameworks, and wraps the resulting Mach-O binary in a
minimal `.app` bundle (`Info.plist` + bundle dirs) so it's
double-clickable and gets a normal menu bar/Dock icon.

## 7. Stub policy

Toolbox calls outside the Phase 0 table (§4) that later test
programs need — e.g. `GetNewWindow`, `ParamText`, `Alert`,
`TESetText` — should be added incrementally, in the same
implementation style, rather than stubbed indefinitely. But to keep
*unrelated* undeclared calls from turning into link errors while a
new sample is being brought up, `Memory.h`/`OSUtils.h` provide a
`RETROMAC_STUB(name)` macro that: emits a weak symbol, prints
`"RetroMac: <name> not implemented"` to stderr the first time it's
hit, and returns a zeroed result. This is a development aid, not a
permanent crutch — anything still stubbed when a sample ships is a
bug.

## 8. Phased roadmap

- **Phase 0** — the table in §4. Target: `wordle.c` and `demo.c`
  compile with `retromacc` and are playable/clickable, pixel-for-pixel
  faithful enough that green/yellow/gray cell coloring, the About
  dialog, and menu commands all work.
- **Phase 1** — Dialog Manager proper (`GetNewDialog`, `ModalDialog`,
  `DITL`-less programmatic dialogs already covered by Phase 0's
  `dBoxProc`-style windows), `Alert`/`StopAlert`, `TextEdit` editable
  fields (`TENew`, `TEClick`, `TEKey`) for apps with text input.
  Static Rez-based resources (`GetNewWindow(id, ...)`) 
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

- `wordle.c` and `demo.c` become the Phase 0 acceptance tests: build
  with `retromacc`, launch, and manually verify (per this repo's
  `/verify`-style workflow): menu bar renders and responds to
  Cmd-key equivalents, window drags/closes, About dialog's OK button
  and default-button ring draw correctly, and (for Wordle) a full
  game — typing letters, backspace, Enter, win/loss messaging, File ▸
  New Game — behaves identically to the described rules.
- Once Phase 0 passes, treat each new sample app added to the repo
  as the acceptance test for whatever new Toolbox surface it
  introduces, extending §4's table rather than writing synthetic
  unit tests against Toolbox internals in isolation.
