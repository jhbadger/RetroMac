# RetroMac

RetroMac lets classic Mac OS (System 6–9) C programs — written
against the real Toolbox headers (`<Quickdraw.h>`, `<Windows.h>`,
`<Menus.h>`, `<Controls.h>`, `<Events.h>`, ...) — compile and run as
**native, interactive Apple Silicon/Intel macOS apps**. No 68k/PPC
emulator, no cross-compiler, no resource fork required: `retromacc`
compiles your source directly against a from-scratch reimplementation
of the pieces of the Toolbox it needs, and packages the result as a
real double-clickable `.app`.

Two sample apps are included:

- `wordle.c` — a Wordle clone with programmatically-built menus, a
  hand-drawn 5×6 letter grid, and an About dialog.
- `demo.c` — a minimal window-plus-menus starting point.

## Quick start

Requires macOS with Xcode's command line tools (`clang`) and
`python3`, both already present on any modern Mac with Xcode
installed.

```sh
./RetroMac/tools/retromacc wordle.c -o Wordle
open Wordle.app
```

That's it — `Wordle.app` is a normal, self-contained macOS app.
Type letters, press Return to submit a guess, use File ▸ New Game,
and check the Apple menu for About.

## How it works

RetroMac provides three things:

1. **Header shims** (`RetroMac/include/`) — drop-in replacements for
   the classic Toolbox headers, declaring the same types
   (`WindowPtr`, `Rect`, `RGBColor`, `EventRecord`, ...) and function
   signatures the original Mac apps expect.
2. **A runtime** (`RetroMac/src/`) — implements those functions on
   top of Cocoa/AppKit, Core Graphics, and Core Text. QuickDraw is a
   real software rasterizer onto an offscreen bitmap per window (not
   a translation to native `NSButton`/etc.), so hand-drawn classic UI
   looks and behaves like it did originally. The system menu bar is
   the one exception — it's backed by a real `NSMenu`.
3. **A build wrapper** (`RetroMac/tools/retromacc`) — rewrites the
   nonstandard `"\pFoo"` Pascal-string literals classic sources use,
   compiles against the runtime, links Cocoa/CoreGraphics/CoreText,
   and wraps the result in an ad-hoc-codesigned `.app` bundle.

See **[Toolbox.md](Toolbox.md)** for the full architecture writeup,
including the non-obvious problems that came up building this (AppKit
headers colliding with the Toolbox's own type names, image-flip
gotchas, and how real menu-bar clicks are intercepted below the level
a normal event loop can see) and the phased roadmap for what's not
built yet (Dialog Manager, TextEdit, the Resource Manager, Color
QuickDraw).

## Status

Phase 0 (the table of Toolbox calls `wordle.c` and `demo.c` actually
use) is done and verified interactively — see Toolbox.md §10 for
bring-up notes and known limitations. Later phases (dialog manager,
resource forks, color regions, ...) are not implemented yet.

## Repository layout

```
wordle.c, demo.c, demo.r   sample classic Mac apps
RetroMac/
  include/                 drop-in classic Toolbox headers
  src/                     the runtime implementing them
  tools/                   retromacc build wrapper + Pascal-string rewriter
Toolbox.md                 architecture, design rationale, roadmap
```

## License

MIT — see [LICENSE](LICENSE).
