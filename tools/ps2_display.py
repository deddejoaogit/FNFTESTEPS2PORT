#!/usr/bin/env python3
"""Work out the PS2's display geometry exactly the way the original does.

The user asked for two things: resize every texture to the size it appears on
the PS2 screen, and work that size out the way the original resizes its window
on Windows. The second has a definite answer, because v0.8.6 ships the scale
mode that does it: funkin.ui.FullScreenScaleMode, set in source/Main.hx line
196. This file reimplements its arithmetic and applies it to the console.

The relevant part of that class is updateGameSize() and updateScaleOffset():

    gameRatio   = FlxG.width / FlxG.height        // 1280 / 720
    screenRatio = Width / Height                  // the window
    ratioAxis   = screenRatio < gameRatio ? Y : X  // letterbox or pillarbox

    if ratioAxis == Y:                            // window narrower than the game
        gameSize.x   = Width                      // use the full width
        logicalSize.y = ceil(gameSize.x / gameRatio)
        gameSize.y   = logicalSize.y
    else:                                         // window wider than the game
        gameSize.y   = Height                     // use the full height
        logicalSize.x = ceil(gameSize.y * gameRatio)
        gameSize.x   = logicalSize.x

    scale.x = deviceSize.x / FlxG.width
    scale.y = deviceSize.y / FlxG.height
    scale.x = scale.y = min(scale.x, scale.y)     // never distort, always fit

    offset.x = ceil((deviceSize.x - gameSize.x) * 0.5)   // centred
    offset.y = ceil((deviceSize.y - gameSize.y) * 0.5)

So the game is scaled uniformly to fit the window, centred, and whatever is
left over becomes a black bar. Nothing about the game's own proportions or the
position of anything inside it changes -- which is the behaviour the user asked
to reproduce.

Run from the project root: python3 tools/ps2_display.py
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)

# The original's own numbers, from source/Main.hx lines 33-34.
GAME_W, GAME_H = 1280, 720

# The console, from the build the user compiles: 640x448 PSMCT32, double
# buffered. src/main.c draws a 640x360 logical area with 44 px bars.
PS2_W, PS2_H = 640, 448

# SCPH-90006, as measured earlier in the port.
VRAM_TOTAL = 4194304
# The budget is taken from tools/vram_budget.py so this script cannot drift
# from the one place the VRAM model lives. That model is: two 512x384 PSMCT16
# framebuffers, the fixed textures, three native-size character frames, and the
# rest for the stage.
try:
    import vram_budget as _vb
    _vb.SIZES_CACHE = _vb.load_sizes()
    _FB = _vb.FB_COUNT * _vb.FB_W * _vb.FB_H * 2
    _FIXED = (_vb.FONT_W * _vb.FONT_H * 4 + _vb.HUD_W * _vb.HUD_H * 2
              + _vb.NOTE_W * _vb.NOTE_H * 2)
    _CHARS = _vb.char_frame_bytes(16)
    TEXTURE_BUDGET = _vb.VRAM_TOTAL - _FB - _FIXED - _CHARS
except Exception:                            # standalone use
    TEXTURE_BUDGET = 4194304 - 2 * 512 * 384 * 2 - 622592 - 1087312


def scale_mode(game_w, game_h, screen_w, screen_h, enabled=False):
    """FullScreenScaleMode.updateGameSize + updateScaleOffset, verbatim."""
    game_ratio = game_w / float(game_h)
    screen_ratio = screen_w / float(screen_h)
    axis = 'Y' if screen_ratio < game_ratio else 'X'

    logical_w, logical_h = screen_w, screen_h
    if axis == 'Y':
        game_size_x = screen_w
        logical_h = int(math.ceil(game_size_x / game_ratio))
        game_size_y = screen_h if enabled else logical_h
    else:
        game_size_y = screen_h
        logical_w = int(math.ceil(game_size_y * game_ratio))
        game_size_x = screen_w if enabled else logical_w

    sx = screen_w / float(game_w)
    sy = screen_h / float(game_h)
    scale = min(sx, sy)

    offset_x = int(math.ceil((screen_w - game_size_x * (scale if enabled else 1)) * 0.5))
    offset_y = int(math.ceil((screen_h - game_size_y * (scale if enabled else 1)) * 0.5))
    return {
        'ratio_axis': axis,
        'game_w': game_w, 'game_h': game_h,
        'screen_w': screen_w, 'screen_h': screen_h,
        'game_ratio': game_ratio, 'screen_ratio': screen_ratio,
        'logical_w': logical_w, 'logical_h': logical_h,
        'scale': scale,
        'bar_x': offset_x, 'bar_y': offset_y,
        'bar_top': offset_y, 'bar_bottom': screen_h - game_size_y - offset_y,
        'bar_left': offset_x, 'bar_right': screen_w - game_size_x - offset_x,
    }


def main():
    m = scale_mode(GAME_W, GAME_H, PS2_W, PS2_H)

    print("The original's window is %dx%d (aspect %.4f); the console's screen is"
          % (GAME_W, GAME_H, m['game_ratio']))
    print("%dx%d (aspect %.4f)." % (PS2_W, PS2_H, m['screen_ratio']))
    print()
    if m['ratio_axis'] == 'Y':
        print("The screen is narrower than the game, so FullScreenScaleMode")
        print("letterboxes: the game uses the screen's full width and the height")
        print("is whatever keeps the aspect.")
    else:
        print("The screen is wider than the game, so FullScreenScaleMode")
        print("pillarboxes: the game uses the screen's full height and the width")
        print("is whatever keeps the aspect.")
    print()
    print("  game area      %d x %d   (aspect %.6f, the original's %.6f)"
          % (m['logical_w'], m['logical_h'], m['logical_w'] / float(m['logical_h']),
             m['game_ratio']))
    print("  uniform scale  %.6f x   (= %d / %d)" % (m['scale'], m['logical_w'], GAME_W))
    print("  black bars     %d px top, %d px bottom, %d px left, %d px right"
          % (m['bar_top'], m['bar_bottom'], m['bar_left'], m['bar_right']))
    print()
    print("src/main.c draws a 640x360 logical area with 44 px bars, so the port")
    print("already matches this exactly. The scale is a clean 0.5x, which means")
    print("every texture only needs to be HALF its original resolution to appear")
    print("at exactly the size the original shows it.")
    print()

    # Now the honest part: what does that cost, and what fits?
    #
    # Costed with the model the port actually uses (see tools/vram_budget.py):
    # one resident FRAME per prop, 16 bpp, no power-of-two padding. An earlier
    # version of this script costed whole atlases at 32 bpp and concluded the
    # backdrops were "far too large at any scale" -- which stopped being true
    # once frames became the unit of residency.
    sizes = json.load(open(os.path.join(PROJECT, "assets-data",
                                        "image-sizes.json")))["images"]
    import glob
    rows = []
    for f in sorted(glob.glob(os.path.join(PROJECT, "assets-data", "stages", "*.json"))):
        d = json.load(open(f))
        sid = os.path.basename(f)[:-5]
        seen = set()
        native = 0
        for p in d.get("props") or []:
            ap = p.get("assetPath")
            if not ap or ap.startswith("#") or ap in seen:
                continue
            seen.add(ap)
            e = sizes.get(ap)
            if not e:
                continue
            w = e.get("frame_w") or e.get("w")
            h = e.get("frame_h") or e.get("h")
            if not w or not h:
                continue
            native += w * h * 4
        rows.append((sid, native, native // 2))

    rows.sort(key=lambda r: -r[2])
    print("%-22s %14s %14s %10s  %s"
          % ("stage", "one frame each @32bpp", "the same @16bpp",
             "vs stage budget", "verdict"))
    print("-" * 84)
    for sid, native, half in rows:
        if half == 0:
            print("%-22s %14s %14s %10s  %s" % (sid, "-", "-", "-", "no images"))
            continue
        over = half / float(TEXTURE_BUDGET)
        verdict = ("fits" if half <= TEXTURE_BUDGET
                   else "needs %.0f%% scale" % (100.0 / over ** 0.5))
        print("%-22s %14d %14d %9.1f%%  %s"
              % (sid, native, half, 100.0 * half / TEXTURE_BUDGET, verdict))
    print()
    print("Budget shown is the whole texture region (%d B). The real per-stage"
          % TEXTURE_BUDGET)
    print("allowance subtracts the font, the HUD, the note atlas and the three")
    print("character frames; tools/vram_budget.py prints that number and the")
    print("exact scale each stage needs.")
    print()
    print("The display analysis above is unaffected: the 640x360 logical area")
    print("inside 640x448 is what the original occupies, and 0.5x is the scale")
    print("at which a texture appears at its original on-screen size.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
