#!/usr/bin/env python3
"""Generate per-skin player tiles for SuperCrateBox-GBA.

Each skin's 14 animation frames are derived from the base player frames
(sprite tiles 0-13) by small silhouette edits, using the same four palette
indices as the original so the per-skin palette swap still applies on top.
"""
import re, sys, zlib, struct

CLEAR, BLACK, LIGHT, MID, DARK, DARKEST = 0, 2, 16, 17, 19, 20
UPRIGHT = range(0, 11)          # 11-13 are the tumbling jump poses

def load_base(path):
    src = open(path).read()
    i = src.index('spritesTiles')
    body = src[src.index('{', i)+1:src.index('};', i)]
    vals = [int(v, 0) for v in re.findall(r'0x[0-9a-fA-F]+', body)]
    data = bytearray()
    for v in vals:
        data += bytes([v & 0xFF, (v >> 8) & 0xFF])
    return [[[data[t*64+y*8+x] for x in range(8)] for y in range(8)] for t in range(14)]

def crown_row(px):
    """Row of the solid black skull-cap; the body bobs between frames."""
    for y in range(8):
        if sum(1 for x in range(7) if px[y][x] == BLACK) >= 6:
            return y
    return None

# ---- features -------------------------------------------------------------

def snout(px, c):                      # crocodile: jaw pushed forward
    px[c+1][6] = LIGHT; px[c+1][7] = BLACK
    px[c+2][6] = LIGHT; px[c+2][7] = LIGHT
    px[c+3][6] = LIGHT; px[c+3][7] = BLACK

def robe(px, c):                       # ninja: legs closed into a skirt
    for y in (c+5, c+6):
        if y > 7: continue
        xs = [x for x in range(7) if px[y][x] != CLEAR]
        if not xs: continue
        for x in range(min(xs), max(xs)+1):
            if px[y][x] == CLEAR: px[y][x] = LIGHT
    if c+6 <= 7:
        for x in range(7):
            if px[c+6][x] == LIGHT: px[c+6][x] = DARK

def headband(px, c):                   # ninja: mask slit across the eyes
    for x in range(1, 6):
        if px[c+1][x] not in (CLEAR, BLACK): px[c+1][x] = DARKEST
    for x in range(1, 6):
        if px[c+2][x] not in (CLEAR, BLACK): px[c+2][x] = DARK

def visor(px, c):                      # astronaut: dark faceplate in a dome
    for y in (c+1, c+2):
        for x in range(3, 6):
            if px[y][x] not in (CLEAR, BLACK): px[y][x] = DARKEST

def scanner(px, c):                    # robot: single eye band across the face
    for x in range(1, 6):
        if px[c+2][x] not in (CLEAR, BLACK): px[c+2][x] = DARKEST

def antenna(px, c):                    # robot: stalk above the head
    if c-1 >= 0 and px[c-1][3] == CLEAR: px[c-1][3] = LIGHT

def beak(px, c):                       # chicken: small pointed bill
    px[c+2][6] = DARKEST; px[c+2][7] = DARKEST

def comb(px, c):                       # chicken: crest
    if c-1 >= 0:
        px[c-1][2] = DARK; px[c-1][3] = DARK; px[c-1][4] = DARK

def ears(px, c):                       # piklupu: tufts either side
    if c-1 >= 0:
        px[c-1][1] = MID; px[c-1][5] = MID

def crown_feature(px, c):              # vlambeer: little crown
    if c-1 >= 0:
        px[c-1][1] = LIGHT; px[c-1][3] = LIGHT; px[c-1][5] = LIGHT

FEATURES = {"snout": snout, "robe": robe, "headband": headband, "visor": visor,
            "scanner": scanner, "antenna": antenna, "beak": beak, "comb": comb,
            "ears": ears, "crown": crown_feature}

SKINS = [
    ("GUY",       []),
    ("ASTRONAUT", ["visor"]),
    ("NINJA",     ["robe", "headband"]),
    ("CROCODILE", ["snout"]),
    ("ROBOT",     ["antenna", "scanner"]),
    ("PIKLUPU",   ["ears"]),
    ("CHICKEN",   ["comb", "beak"]),
    ("VLAMBEER",  ["crown"]),
]

def build(base):
    out = []
    for name, feats in SKINS:
        frames = []
        for t in range(14):
            px = [row[:] for row in base[t]]
            if t in UPRIGHT and feats:
                c = crown_row(px)
                if c is not None:
                    for f in feats:
                        FEATURES[f](px, c)
            frames.append(px)
        out.append((name, frames))
    return out

# ---- preview --------------------------------------------------------------

RAMPS = {
 "GUY":("f8b070","f88028","d85820","d84018"), "ASTRONAUT":("f8f8f8","c8d0e8","8890b8","585880"),
 "NINJA":("7078a8","404870","282848","181830"), "CROCODILE":("a8e060","68b830","388018","205010"),
 "ROBOT":("e0e8f0","a0b0c0","687888","404850"), "PIKLUPU":("f8b8e0","e868b0","a83878","702050"),
 "CHICKEN":("f8f0c0","f8d040","d09818","906008"), "VLAMBEER":("f8e880","f0b828","b87810","805008"),
}
def png(path, skins, frames_shown=(0,6,8,10), scale=13, pad=2):
    hx = lambda h: tuple(int(h[i:i+2],16) for i in (0,2,4))
    W = (len(frames_shown)*(8+pad)-pad)*scale
    H = (len(skins)*(8+pad)-pad)*scale
    img=[[(38,38,46)]*W for _ in range(H)]
    for si,(name,fr) in enumerate(skins):
        r=RAMPS[name]; cmap={LIGHT:hx(r[0]),MID:hx(r[1]),DARK:hx(r[2]),DARKEST:hx(r[3]),BLACK:(0,0,0)}
        for fi,t in enumerate(frames_shown):
            for y in range(8):
                for x in range(8):
                    v=fr[t][y][x]
                    if v==CLEAR: continue
                    col=cmap.get(v,(255,0,255))
                    px0=(fi*(8+pad)+x)*scale; py0=(si*(8+pad)+y)*scale
                    for dy in range(scale):
                        for dx in range(scale): img[py0+dy][px0+dx]=col
    raw=b"".join(b"\0"+bytes(b for p in row for b in p) for row in img)
    ch=lambda t,d: struct.pack(">I",len(d))+t+d+struct.pack(">I",zlib.crc32(t+d)&0xffffffff)
    open(path,"wb").write(b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",W,H,8,2,0,0,0))
                          +ch(b"IDAT",zlib.compress(raw,9))+ch(b"IEND",b""))

# ---- C output -------------------------------------------------------------

def emit_c(path, skins):
    # skin 0 keeps the original tiles 0-13, so only the others need data
    extra = skins[1:]
    lines = ["//Generated by tools/genskins.py - do not edit by hand.",
             "//Player animation tiles for each skin past the first; skin 0 uses the",
             "//original sprite tiles. 14 frames of 8x8 8bpp pixels per skin.",
             "",
             '#include "playerskins.h"', "",
             "const unsigned short playerSkinTiles[%d] = {" % (len(extra)*14*32)]
    for name, fr in extra:
        lines.append("\t//%s" % name)
        for t in range(14):
            flat = [fr[t][y][x] for y in range(8) for x in range(8)]
            words = [flat[i] | (flat[i+1] << 8) for i in range(0, 64, 2)]
            for i in range(0, 32, 8):
                lines.append("\t" + "".join("0x%04X," % w for w in words[i:i+8]))
    lines += ["};", ""]
    open(path, "w").write("\n".join(lines))
    h = ["//Generated by tools/genskins.py - do not edit by hand.", "",
         "#ifndef PLAYERSKINS_H", "#define PLAYERSKINS_H", "",
         "#define PLAYER_SKIN_FRAMES 14",
         "#define PLAYER_SKIN_EXTRA  %d" % len(extra),
         "#define playerSkinTilesLen %d" % (len(extra)*14*64), "",
         "extern const unsigned short playerSkinTiles[%d];" % (len(extra)*14*32), "",
         "#endif", ""]
    open(path.replace(".c", ".h"), "w").write("\n".join(h))

if __name__ == "__main__":
    base = load_base(sys.argv[1])
    skins = build(base)
    png(sys.argv[2], skins)
    if len(sys.argv) > 3: emit_c(sys.argv[3], skins)
    print("skins:", ", ".join(n for n, _ in skins))
