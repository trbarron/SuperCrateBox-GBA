# Benchmark

```sh
export PATH="$HOME/.local/opt/arm-none-eabi-gcc/bin:$PATH"
tools/bench.sh <label>
```

Builds an auto-running ROM, runs it, restores the normal `main.gba`, prints the
figures. Takes about two minutes. It needs no interaction with the emulator
window at all.

```
  benchmark: final-28-minigun2f
  1800 frames over 30.14s, budget 4389 ticks/frame

  CPU avg      24.9%   (1093 ticks)
  CPU p95      39.2%   (1720 ticks)
  CPU max      56.5%   (2479 ticks)
  CPU min       7.4%   (325 ticks)

  frame rate  59.73 fps
  slow frames     0  (0.0% of run)
  respawns      113

  work done in the run:
    shots fired        129  (4.3/s)
    explosions           8
    peak bullets        26
    peak monsters       20
    crates taken         0
```

## What it runs

Thirty seconds (1800 frames) of scripted play:

- **Player is invincible** — death respawns and the run continues, so a bad run
  still measures a full thirty seconds of work.
- **Input is synthesised** from a seeded random stream. Directions are held for
  6–26 frames at a time so the player actually crosses the arena instead of
  twitching in place; fire is held about three frames in four.
- **The monster pool is kept full** (`BENCH_SPAWN_ODDS`). At the normal spawn
  rate a thirty second run sees only a handful of monsters, which measures an
  almost empty arena.
- **The weapon rotates** through all ten every `BENCH_WEAPON_HOLD` frames.

It deliberately measures a hard case, not a typical one.

## Why the weapon rotation is 90 frames

The slowest weapons reload in 60. At 45 the bazooka, mine and grenade launcher
rotated away *before they could fire*, so a whole run logged one explosion and
the blast path went essentially unmeasured. At 90 it logs eight. **Any change to
reload times needs to be checked against this number.**

## Why "59.73 fps" is a perfect score

The GBA refreshes at 16777216 / 280896 = **59.7275 Hz**, not 60. A run hitting
59.73 with zero slow frames dropped nothing. Frames are not counted as loop
iterations — `WaitVSync` would make that always read 60. Timers 2 and 3 cascade
into a 32-bit clock over the whole run and the rate comes from elapsed cycles
against frames completed, so a missed vblank actually shows up.

`CPU` is Timer 0 measured from the top of vblank to the end of the next frame's
work: everything except the idle wait. 100% means the frame budget is exactly
consumed. `slow frames` is the count over budget, and is the number that
matters. The live on-screen meter (SELECT, during normal play) is force-disabled
during a run, since its `toString()` calls would otherwise measure themselves.

## Determinism

Two runs of the same build produce **byte-identical** figures. Getting there
needed more than seeding `rand()`:

- `loopSlower`, `reloadTimer`, `shootCoolDown` and `cloudScroll` were locals in
  `main()`, so a second run began on a different animation phase and consumed
  the random stream differently. They are globals now, reset by `gameInit()`.
- `gameInit()` only marked the enemy and bullet pools **dead**, not empty.
  `spawnBullet()` takes a different branch once a pool has been filled once —
  `push_back` versus scanning for a free slot — so run two did measurably
  different work than run one. Both pools are cleared now.

Both also make ordinary restarts more consistent, so this is not scaffolding.

## How results get out

The ROM writes to cartridge SRAM at offset 64 and `tools/benchreport.py` parses
the `.sav`. This is the workaround for mGBA's Qt build having no scriptable
debugger and screen capture being unavailable.

**Emulators only create a battery file once a game touches SRAM.** Nothing else
in a benchmark build writes until the run is over, so for a while a finished run
and a crashed one looked identical — both produced no file. `startBenchmark()`
now stamps a `BNC0` placeholder over the results block immediately. The reader
requires `BNC2`, so an unfinished run is still correctly rejected, but the file
exists to be read.

`-DBENCH_HEARTBEAT` writes the remaining frame count to SRAM offset 128 every
60 frames. That is how the above was diagnosed — poll the `.sav` and watch the
counter move. Leave it off for real measurements: an SRAM write makes the
emulator flush its save file, which is itself a stall (see
[bugs.md](bugs.md)).

## What it will not tell you

- **Absolute numbers are emulated.** Timer 0 reads what real hardware's timer
  would, so build-to-build comparisons are sound, but mGBA's timing is not a
  physical GBA.
- **Runs across different builds are not a pure A/B.** Changing something that
  affects gameplay — pool size, fire rate — changes which enemies die, which
  moves everyone, which changes where the player dies. Respawn counts varied
  87/113/39 across a pool sweep on one seed. Trends are trustworthy; individual
  points carry a percent or so of scenario noise.
- **`crates taken` is always 0.** The scripted player never walks into a crate,
  so the pickup path and natural weapon switching are only reached artificially
  via the rotation. If that path ever gets expensive, this will not catch it.
- **Explosions are thinly sampled** — eight in a run, so `CPU max` often rides
  on very few frames.
