# Gameplay added since 2014

The original is Super Crate Box by Vlambeer. Where a decision was made to match
it, the source is cited; where this build diverges, that is called out.

## High score

Kept in cartridge SRAM with a `SCB3` tag, alongside lifetime crates, the
equipped skin, the chosen arena and a best score per arena. Shown on the main
menu.

The tag matters: SRAM that is blank, or absent entirely (a multiboot ROM booted
over a link cable has no cartridge), reads back as garbage. Tag-checking makes
that degrade to "nothing saved yet" instead of a nonsense high score. A
`SRAM_V113` string is embedded in the ROM so emulators detect the save type.

The save format migrates forward through every version it has had: the original
two-byte `SC`, then `SCB2` from before the arenas existed (whose single score
becomes the Construction Yard's), then `SCB3`. Verified against a real `SCB2`
save — best 4, 10 crates — which came back as `arenaBest = [4, 0, 0]`.

**Progress is never written during play.** See [bugs.md](bugs.md) — writing
SRAM on every crate caused a visible stutter.

## Skins

Eight characters, unlocked by score. The original ties unlocks to specific
arenas and game modes ([GameClub guide](https://gameclub.io/stories/super-crate-box-character-unlock-guide)),
which this build has neither of, so the original *numbers* were kept and
retargeted at the score already tracked.

| skin | unlock | original condition |
|---|---|---|
| Guy | default | starting character |
| Astronaut | score 10 | 10 in Moon Temple |
| Ninja | score 20 | 20 in Ambush Mode |
| Crocodile | score 25 | 25 in Construction Yard |
| Robot | score 30 | 30 in Rocket Silo |
| Piklupu | score 35 | 35 in Moon Temple |
| Chicken | score 50 | 100 in SFMT Mode |
| Vlambeer | 1,000 crates | 1,000 crates — kept as-is |

Meat Boy, Mr. Canabalt and the Spelunky Explorer were left out: they are
licensed cameos from other studios' games, and unlike the rest they are shape
changes rather than recolours.

**How a skin works.** The player art uses four palette entries that nothing
else touches — `#f8b070`, `#f88028`, `#d85820`, `#d84018`, a light-to-dark
ramp. So a recolour is four `SetPaletteObj()` calls and recolours every frame
of the animation at once. Only `#d84018` is shared, with three pixels of the
mine sprite.

On top of that each skin has **its own 14 tiles** with silhouette changes — a
snout, a robe, an antenna. Those tiles still use the same four indices, so art
variation and colour variation compose.

The tiles are **generated, not hand-drawn**: `tools/genskins.py` reads
`sprites.c`, applies per-skin pixel operations, and emits `playerskins.c`. This
matters because the frames bob vertically between poses, so features are
anchored to a detected "crown row" (the solid black skull-cap) rather than
fixed coordinates — otherwise a snout drifts a pixel as the character walks.
The three tumbling jump frames are palette-only; they are rolled into a ball
where a snout would not read anyway.

**The skin table in `main.cpp` and the list in `tools/genskins.py` are matched
by index and nothing enforces it.** Keep them in the same order.

Cost: 98 tiles at VRAM 192–289, leaving room for roughly 15 more skins.

## Katana (weapon 9)

Sprite tile 31 existed in the 2014 spritesheet with nothing referencing it —
weapon sprites are `ATTR2_ID8(22+weapon)` and the crate roll was `rand() % 9`,
so slot 9 was unreachable. It is a blade, and the original has a Katana.

Implemented as bullet type 8: a swing that holds in front of the player rather
than travelling, damage 2 (small monsters have 2 HP, large 5), a 3-frame swing,
on the pass-through list so it cuts through a group.

It **turns large monsters around** when it hits, matching the original — the
katana has no knockback but does cause big enemies to about-face
([Steam weapon guide](https://steamcommunity.com/sharedfiles/filedetails/?id=787859694)).
Applied only on the opening frame of the swing, or the slash would flip the
target once per frame it stayed in contact.

Bullet frames are `40+type`, which for type 8 would be tile 48 — that is the
bottom-left quadrant of the large monster under 2D sprite mapping, not free
space. The slash borrows tile 31, the blade itself.

## Grenade launcher (weapon 6)

Was fully implemented in 2014 and deliberately excluded from the crate roll,
marked `//not in game`. Re-enabled. The part that mattered was adding bullet
type 5 to all **three** explosion sites — without that it was a lobbed dud.

## Explosions

Were not explosions. `explode()` sprayed up to 30 machine-gun rounds outward
and whatever they clipped took a point of damage: damage was luck (a large
monster needed five separate fragments to connect), the 30 spawns swamped a
20-slot pool so the player's own weapon had nowhere to go, and range was
unbounded.

Now a real radius — **44 pixels**, measured centre-to-centre so large monsters
are not favoured by their 16px origin, compared squared to keep a square root
out of the frame. Sized from screenshots of the original, where the disc spans
about 37% of the arena width.

Drawn as a black disc that **grows over 8 frames, holds 20 (about a third of a
second), then collapses over 8**. The kill follows the disc outwards rather
than firing instantly at full radius, so nothing dies before the blast visibly
reaches it; walking through the collapsing disc is safe.

An 88px filled circle is far too big for ordinary sprites — roughly 85 objects,
and about 20 are free. It is **one 64x64 sprite magnified by an affine
transform**, so a single object covers every size continuously. The disc is
generated into VRAM at startup rather than shipped as an asset, laid out with a
16-tile stride because objects are in 2D mapping.

It draws behind the player, monsters and bullets, which show on top of the
black. Moving it in front would mean relocating the weapon sprite off object 0.

Debris still flies, but it is cosmetic only — bullet type 9, no damage entry,
8-frame life, ten of them.

## Arenas

Three, matching the original: Construction Yard, Rocket Silo, Moon Temple. Each
is unlocked by **scoring 10 in the one before it**, which is the original's own
rule ([Videogaming Wiki](https://videogaming.fandom.com/wiki/Super_Crate_Box)).
Chosen from a page off the main menu; best score is tracked per arena, since
that is what drives the unlock chain.

**An arena is data, not code.** `checkMapCollision()` reads screenblock 30
directly, so the girder layer *is* the collision map. Everything else that used
to be hardcoded — player spawn, monster spawn, the seven crate positions, the
fire-pit tiles — lives in the `Arena` struct. Adding a fourth arena means
adding a table entry.

The tile conventions are what make this work, and any new layout must respect
them: **16-20 are the deadly fire, 21-25 are passable, 0 is open air, and
everything else is solid.**

Layouts are authored as character grids in `tools/genarenas.py` and turned into
tile indices there, so platform end caps and ladder junctions are worked out
rather than hand-counted:

```
+=============..=============+
|....H..................H....|
|==========........==========|
```

The drop gap and the fire pit both sit at columns 14-15 in every arena, because
monsters spawn at x=116. Platform spacing is kept within about five tiles, since
the player's jump tops out near 50 pixels.

### They share one tileset

The two new arenas reuse the Construction Yard tiles and change **palette**
only. That works because the tile groups turn out to be cleanly separable:

| group | palette entries | |
|---|---|---|
| fire | 1, 8, 9, 14 | **exclusive** — flames stay orange in every arena |
| sky | 5, 6, 7 | exclusive |
| buildings | 3, 4 | exclusive |
| structure | 10-13, 15-21 | shared among girder/brick/ground/ladder |

So an arena's look is ~16 `SetPaletteBG()` calls. Rocket Silo is cold steel,
Moon Temple is pale stone under a night sky.

One ordering trap: **leaving a menu reloads `backgroundPal` wholesale**, which
would put the girders back to red. `applyArenaPalette()` has to run after that,
not just at arena load.

This is a stand-in until a proper tileset arrives. The honest weak point is the
scrolling cloud layer, which reads as dark smudges inside the Rocket Silo
rather than anything intentional.

## Minigun

Reload 5 → 2 frames, 30 shots/sec. Paired with raising the bullet pool to 28;
see [experiments.md](experiments.md) for the measurements behind both numbers.

## Not done

Worth knowing what the original has that this does not:

- **The fire pit.** In the original an enemy that falls in **reappears at the
  top red and faster**, and gold after several falls. Here the pit just kills
  them, so ignoring enemies has no cost — which flattens the whole risk/reward
  loop the game is built on. Feasible as a palette tier: monsters use only two
  exclusive palette indices, so tiers are a two-colour swap, needing duplicate
  tiles remapped to new indices (22 tiles per tier against 320 free).
- **No audio at all.** `gba.h` does not even define the sound registers.
  Probably the largest gap in feel, and the largest amount of work.
- **14 weapons in the original**, 10 here.
- **Arena-specific tilesets.** All three share the Construction Yard tiles,
  recoloured. Real Moon Temple stone and Rocket Silo machinery still to come.
- **No flying enemy** — the original has small, large and flying.
- **No difficulty ramp.** Spawning is a flat `rand()%100` forever.
