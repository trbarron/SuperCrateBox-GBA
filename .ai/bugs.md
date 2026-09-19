# Bugs found and fixed

## Pre-existing

### Bullet pool exhaustion turned a live bullet around

`spawnBullet()` started `i = 0`. When the pool was full with nothing dead, the
search loop found no slot, fell through, and operated on **whatever was in slot
0** — reassigning its direction and sprite flip. An in-flight bullet silently
reversed.

Not rare: `explode()` called `spawnBullet()` 30 times against a 20-slot pool,
so every bazooka and mine detonation ran the exhausted path ten times.

Fixed with a sentinel:

```c
//Every bullet is still in flight, so there is nowhere to put a new one.
//Without this the caller would be handed slot 0 and turn a live bullet around.
if(i < 0)return -1;
```

That required auditing every caller — three of them fed the result straight into
`bullets.at(...)`, and `.at(-1)` throws, which on this build would abort. The
shotgun, grenade launcher and minigun paths are guarded, `explode()` stops when
the pool is full, and the laser's spawn loop breaks.

### `Bullet::setType` assigned nothing

```c
void setType(int type){ type = type; }   // was
void setType(int type){ this->type = type; }
```

Latent — nothing calls it — but it was a live `-Wself-assign` warning.

### Unbounded text drawing

`drawText()` wrote one hardware object per character with no bounds check. The
credits page uses **exactly** 83 characters against 83 available slots, so it
was one string away from writing past `ObjBuffer`. Now guarded.

## Introduced during this work, then fixed

### SRAM write on every crate caused a visible stutter

Adding lifetime-crate tracking put `saveProgress()` on the pickup path, so every
crate wrote the save. The cost is **not** on the GBA — eleven byte-writes to
SRAM is roughly 100 cycles against 280,896 in a frame, about 0.03%, incapable of
producing a visible hitch. The time went on the *host*: emulators persist SRAM
either by mmap or by detecting changes and writing the file out, and on the
latter every crate triggered a save-file write.

Fixed by deferring. Pickups set `progressDirty`; the write happens on death, and
`saveProgress()` early-returns when nothing changed. Nothing is lost — a run can
only end in death. Equipping a skin still saves immediately, being a one-off
deliberate action outside gameplay.

**General rule for this codebase: never write SRAM during gameplay.**

### Katana did not turn large monsters around

Shipped doing damage only, missing the about-face the original has. See
[features.md](features.md).

### Raising `MAX_BULLETS` would have corrupted the text

Bullets were hardcoded at objects 24–43, the blast at 44, text at 45. Raising
the pool would have silently written over text objects. The object map is now
derived:

```c
const int OBJ_BULLET_BASE = OBJ_ENEMY_BASE + MAX_ENEMIES;
const int OBJ_BLAST       = OBJ_BULLET_BASE + MAX_BULLETS;
const int OBJ_TEXT_GAME   = OBJ_BLAST + 1;
```

Verified behaviour-neutral by benchmarking before and after: identical work
counts, `CPU max` differing by one tick (0.02%).

### Game over text overwrote the frozen arena

Consequence of the above. Menus move the text base down to slot 3, because the
credits page needs 83 characters and only 76 are free above the game objects.
But the game over screen is drawn *over* the frozen arena, so a low base put
text on top of the monsters and bullets. It now keeps the gameplay base — it
needs 42 characters, which fits.

### A finished benchmark run looked identical to a crashed one

Two runs reported nothing and were nearly diagnosed as a hang. The ROM was
finishing in ~35s every time. **Emulators only create a battery file once a
game touches SRAM**, and a benchmark build writes only at the end of the run —
so before the flush there was no file at all, exactly like a crash.

`startBenchmark()` now stamps a `BNC0` placeholder immediately. The reader
requires `BNC2`, so an unfinished run is still rejected, but the file exists.
`-DBENCH_HEARTBEAT` was added to diagnose this and is worth keeping.

### The benchmark was measuring almost nothing

Not a crash, but the most consequential mistake here. The first version ran at
the normal spawn rate with no weapon rotation, so a run saw about six monsters,
took **zero crates**, stayed on the starting machine gun, and triggered **zero
explosions**. A "6x headroom, do not optimise" conclusion was drawn from it.
With the pools full and weapons rotated, worst case was three times higher.

Then the first rotation period (45 frames) was **shorter than the slowest
reload (60)**, so the bazooka, mine and grenade launcher rotated away before
firing — one explosion per run. At 90 frames it is eight.

Lesson: check what a benchmark actually exercised before trusting what it
measured. The `work done in the run` counters exist for that.

## Known and not fixed

- `checkMapCollision()` and `tryMove()` re-read the same tile up to ten times
  per call. Harmless, wasteful, [measured as not worth fixing yet](experiments.md).
- `for(int i = 0; i < vector.size(); i++)` throughout produces sign-compare
  warnings. Pervasive pre-existing style; churning it would obscure real
  changes.
- `gba.cpp:103` produces a `-Wsizeof-array-div` warning. It is a false positive
  — the computation is correct — but it is course-supplied code, left alone.
- The blast disc draws behind sprites. Fixing it means relocating the weapon
  sprite off object 0.
