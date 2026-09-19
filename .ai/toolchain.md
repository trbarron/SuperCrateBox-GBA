# Building

```sh
export PATH="$HOME/.local/opt/arm-none-eabi-gcc/bin:$PATH"
make
```

That is the whole thing. The Makefile is unchanged from 2014 apart from one
added object file — the toolchain was assembled to satisfy it as written.

## Why not devkitPro

The Makefile wants `arm-none-eabi-g++`, `-specs=gba_mb.specs` and `gbafix`,
which normally come from devkitARM. Neither usual route works here:

- **`pkg.devkitpro.org` returns Cloudflare 403 to every client** from this
  network — browser user agent, pacman user agent, sandboxed and not. So
  `dkp-pacman -S gba-dev` cannot fetch packages even once the installer is
  in place. Re-test before concluding this is permanent.
- **Homebrew has no Apple-silicon bottle** for `arm-none-eabi-gcc` (only
  x86_64 sonoma), so `brew install` would build GCC from source.

## What replaces it

Everything lives in `~/.local/opt/arm-none-eabi-gcc`. No sudo, nothing outside
your home directory.

| piece | source |
|---|---|
| compiler | [xPack arm-none-eabi-gcc](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack) 15.2.1, darwin-arm64 prebuilt |
| `gba_crt0.o`, `gba_mb.ld`, `gba_mb.specs` | [devkitPro/devkitarm-crtls](https://github.com/devkitPro/devkitarm-crtls), assembled and dropped into the toolchain's `arm-none-eabi/lib/` |
| `gba_sbrk.o` | hand-written, see below |
| `gbafix` | Python re-implementation in the toolchain's `bin/` |

The crt0 and linker script go in `arm-none-eabi/lib/` specifically because that
is where GCC looks for `-specs=` files and startfiles, so the Makefile's
`-specs=gba_mb.specs` resolves with no extra flags.

**`gba_sbrk.o` is the one piece with no upstream.** devkitARM gets `_sbrk` and
the newlib syscall stubs from libsysbase, which vanilla newlib does not ship.
Without a working `_sbrk` there is no heap, and this game uses `std::vector` and
`std::string` throughout, so it would fail at the first allocation. The source
is kept at `tools/gba_sbrk.c`.

**`gbafix`** patches the ROM header: boot logo, the `0x96` fixed value, and the
complement check at `0xBD`. Verified against the committed 2014 ROM — running
it on a copy reproduces the file byte for byte.

## Emulator

`~/.local/opt/mGBA.app`, from the [mGBA releases](https://github.com/mgba-emu/mgba/releases)
macOS dmg. Two things worth knowing, both discovered the hard way:

- **The Qt build ignores `-d`.** There is no CLI debugger to script against.
- **mGBA flushes its battery file periodically while running**, not only on
  exit. This is what makes the benchmark's SRAM output readable without
  touching the UI. But it only *creates* the file once the game first writes
  SRAM — see [benchmark.md](benchmark.md).

## Multiboot

`gba_mb.specs` links for EWRAM at `0x02000000`. Consequences:

- ROM is capped at **256KB** (currently ~175KB).
- Code runs from a 16-bit bus with wait states. Moving hot code to IWRAM would
  be the single largest available speedup — see [experiments.md](experiments.md)
  for why it is not worth doing yet.
- `REG_WAITCNT` tuning is pointless here: nothing executes from cart ROM.
- Cartridge SRAM still works in emulators loading the `.gba` directly, which is
  how the high score and the benchmark results persist. On real hardware booted
  over a link cable there is no cart, so saves degrade to nothing — the code
  handles that by tag-checking what it reads back.
