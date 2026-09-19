# Working notes

Background on changes made to this GBA clone after the original 2014 build —
the things that are not recoverable from reading the code.

| file | what it covers |
|---|---|
| [toolchain.md](toolchain.md) | How to build this. devkitPro does not work here; what replaces it. |
| [benchmark.md](benchmark.md) | The in-ROM benchmark: what it measures, how to run it, what it cannot tell you. |
| [experiments.md](experiments.md) | Performance measurements and the numbers behind the tuning decisions. |
| [features.md](features.md) | Gameplay added since 2014, and how each one fits the original game. |
| [bugs.md](bugs.md) | Bugs found and fixed, including several that predate this work. |

## Orientation

`main.cpp` holds the game loop and every menu. `entity.h` has the
`Entity`/`Player`/`Monster`/`Bullet`/`Crate` hierarchy. `gba.h` and `gba.cpp`
are a course-supplied hardware layer and are best left alone. `background.c`,
`sprites.c`, `font.cpp` and `playerskins.c` are generated data.

Two constraints shape almost every decision in this codebase:

**There are 128 hardware objects, and text is made of them.** Every character
on screen is its own 8x8 sprite. The object map is derived from the pool sizes
in `main.cpp`, so changing `MAX_BULLETS` or `MAX_ENEMIES` moves everything
after it rather than silently overwriting the text. Menus move the text base
down to slot 3 because the credits page needs 83 characters at once, which does
not fit above the game objects.

**An arena is data.** The girder layer doubles as the collision map, so a new
level is a tilemap plus a row in the `Arena` table. Tile conventions: 16-20
deadly, 21-25 passable, 0 open air, everything else solid.

**This is a multiboot build.** `-specs=gba_mb.specs` links everything to run
from EWRAM at `0x02000000`, which caps the ROM at 256KB and means code runs
from a 16-bit bus with wait states rather than from IWRAM. See
[toolchain.md](toolchain.md).
