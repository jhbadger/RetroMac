/*
 * rezdemo.r -- Phase 2 acceptance test resources for RetroMac's
 * Resource Manager (Toolbox.md section 9's convention: each new
 * sample exercises whatever new Toolbox surface it introduces).
 * Compiled with the real system Rez by retromacc; see Toolbox.md
 * section 12 for how these binary layouts were confirmed.
 */
#include "RetroMac.r"

resource 'WIND' (128) {
    { 60, 60, 320, 460 },
    documentProc,
    visible,
    goAway,
    0,
    "RezDemo",
    noAutoCenter
};

resource 'MENU' (128) {
    128, textMenuProc,
    allEnabled, enabled,
    apple,
    { "About RezDemo...", noIcon, noKey, noMark, plain }
};

resource 'MENU' (129) {
    129, textMenuProc,
    allEnabled, enabled,
    "File",
    { "Quit", noIcon, "Q", noMark, plain }
};

resource 'DITL' (128) {
    {
        { 90, 190, 112, 250 },
        Button {
            enabled,
            "OK"
        },
        { 90, 260, 112, 320 },
        Button {
            enabled,
            "Cancel"
        },
        { 10, 10, 26, 300 },
        StaticText {
            disabled,
            "All of this dialog is resource-driven."
        }
    }
};

resource 'DLOG' (128) {
    { 100, 100, 240, 460 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    128,
    "RezDemo Dialog",
    noAutoCenter
};

resource 'DITL' (129) {
    {
        { 60, 240, 82, 300 },
        Button {
            enabled,
            "OK"
        },
        { 10, 10, 42, 300 },
        StaticText {
            disabled,
            "^0"
        }
    }
};

resource 'ALRT' (128) {
    { 100, 100, 200, 420 },
    129,
    { OK, visible, sound1,
      OK, visible, sound1,
      OK, visible, sound1,
      OK, visible, sound1 },
    noAutoCenter
};
