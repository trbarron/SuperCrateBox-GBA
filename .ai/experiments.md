# Performance experiments

All figures from `tools/bench.sh` — see [benchmark.md](benchmark.md) for what
the numbers mean and how much to trust them. Budget is 4389 ticks per frame.

## Headroom

| config | avg | p95 | max | slow frames |
|---|---|---|---|---|
| typical play (no stress) | 12.2% | 14.8% | 16.7% | 0 |
| pools full, weapons rotated | 21.9% | 29.3% | 47.0% | 0 |

**The first row is misleading and is why the benchmark now always stresses.**
At the normal spawn rate a run sees about six monsters, and the scripted player
takes no crates — so it stayed on the starting machine gun and never fired the
bazooka, minigun, laser or mines. Zero explosions. That run was exercising
maybe a third of the game, and a "6x headroom, do not optimise" conclusion was
drawn from it before the gap was spotted.

## Bullet pool

Original minigun (5 frame reload):

| pool | avg | p95 | max | peak live bullets |
|---|---|---|---|---|
| **20** | 21.9% | 29.3% | 47.0% | **20 — clipping** |
| 40 | 26.7% | 36.3% | 57.1% | 23 |
| 60 | 31.4% | 41.7% | 62.1% | 22 |

Tripling the pool bought three bullets on screen and cost ten points of average
CPU. **Pool slots cost CPU whether or not they hold a live bullet**: the loops
skip dead entries but still walk them, and the collision pass is enemies ×
pool, so 400 inner iterations at 20 becomes 1200 at 60. Roughly **+4.8% average
CPU per 20 empty slots**.

## Faster minigun

Reload 5 → 2 frames (12 → 30 shots/sec).

| config | avg | p95 | max | peak bullets | shots |
|---|---|---|---|---|---|
| 5f minigun, pool 20 | 21.9% | 29.3% | 47.0% | 20 | 88 |
| 2f minigun, pool 20 | 23.0% | 36.6% | 52.2% | 20 — clipping | 130 |
| **2f minigun, pool 28** | **24.9%** | **39.2%** | **56.5%** | **26** | 129 |
| 2f minigun, pool 40 | 26.8% | 38.5% | 56.8% | 23 | 127 |

**Shipped: 2 frame minigun, pool 28.** Demand settles around 23–26 — fire rate
times the roughly 50-frame flight of a round — so 28 clears it with margin. 40
costs another two points of average for nothing.

Note the shape: average moved 21.9 → 24.9, but **p95 moved 29.3 → 39.2**. The
faster minigun does not make the typical frame much worse, it makes the busy
frames much more common. p95 is the number to watch when pushing this further.

## Deferred optimisations

Not done, because nothing is dropping frames and worst case is under 60% of
budget. Listed in the order they would be worth doing:

1. **Bullet pool compaction.** Swap-remove dead bullets so live ones stay
   contiguous and the loops run over a live count instead of the pool size.
   This is the one that unblocks larger pools — right now 28 slots cost 28
   iterations per enemy per frame regardless of how many are in flight.
2. **`checkMapCollision()` reads the same tile up to five times.**
   `REG_VIDEO_BASE` is `volatile`, so the compiler is *forbidden* from
   collapsing them. `tryMove()` then calls the whole function twice with
   identical arguments to test for `1` then `2` — up to ten VRAM reads where
   one would do, about 65 times a frame.
3. **`checkEntityCollision(Entity, Entity)` takes both by value** — two slicing
   copies per call, up to 420 calls a frame.
4. **`UpdateObjects()` copies 1KB of OAM with a halfword loop** every frame.
   DMA3 would be roughly ten times faster. `gba.h`'s DMA defines are commented
   *"untested — probably broken"*, so verify them first.
5. **Move hot code to IWRAM.** Largest available win (EWRAM is a 16-bit bus
   with wait states, IWRAM is 32-bit and zero-wait, worth perhaps 2–3x on
   compute) and the largest risk. Only if measurement demands it.

Re-measure before and after any of these. The point of the benchmark is that
none of this needs to be argued about.
