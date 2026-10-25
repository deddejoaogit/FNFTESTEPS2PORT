#!/usr/bin/env python3
"""Read a PNG's dimensions and bit depth straight from the IHDR chunk.

No Pillow in this environment, and none is needed: the IHDR is the first
chunk after the 8-byte signature, and it always has the same layout. Colour
type tells us how many bytes per pixel the PS2 upload will need, which is what
the vram budget actually cares about (a 4 MB region is 4 MB of texture,
whatever the PNG compressed to on disk).
"""
import struct
import sys

CT_GREY, CT_RGB, CT_INDEXED, CT_GREY_A, CT_RGBA = 0, 2, 3, 4, 6


def png_info(path):
    with open(path, "rb") as f:
        sig = f.read(8)
        if sig != b"\x89PNG\r\n\x1a\n":
            raise ValueError("not a PNG: %s" % path)
        length = struct.unpack(">I", f.read(4))[0]
        ctype = f.read(4)
        if ctype != b"IHDR":
            raise ValueError("no IHDR in %s" % path)
        w, h, depth, color, comp, filt, inter = struct.unpack(">IIBBBBB", f.read(13))
    channels = {CT_GREY: 1, CT_RGB: 3, CT_INDEXED: 1,
                CT_GREY_A: 2, CT_RGBA: 4}[color]
    bpp = channels * depth // 8
    return {"w": w, "h": h, "depth": depth, "color": color,
            "channels": channels, "bytes_per_px": bpp,
            "raw_bytes": w * h * bpp, "interlaced": bool(inter)}


if __name__ == "__main__":
    for p in sys.argv[1:]:
        try:
            i = png_info(p)
        except Exception as e:
            print("%-28s ERROR %s" % (p.split("/")[-1], e))
            continue
        print("%-28s %5dx%-5d depth=%d channels=%d  %d B/px  raw=%9d B"
              % (p.split("/")[-1], i["w"], i["h"], i["depth"], i["channels"],
                 i["bytes_per_px"], i["raw_bytes"]))
