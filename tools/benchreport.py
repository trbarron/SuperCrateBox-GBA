#!/usr/bin/env python3
"""Decode the benchmark block a run leaves in cartridge SRAM."""
import struct, sys

FIELDS = ["frames", "avg", "p95", "peak", "low", "slow", "deaths", "budget", "cycles",
          "shots", "explosions", "peak_bullets", "peak_enemies", "crates"]
AT = 64

def main(path, label="run"):
    try:
        raw = open(path, "rb").read()
    except FileNotFoundError:
        sys.exit("no save file at %s - did the emulator run?" % path)

    if raw[AT:AT+4] != b"BNC2":
        sys.exit("no benchmark block in %s (found %r) - the run may not have finished"
                 % (path, raw[AT:AT+4]))

    v = dict(zip(FIELDS, struct.unpack_from("<14i", raw, AT + 4)))
    budget = v["budget"]
    pct = lambda t: 100.0 * t / budget
    # the run clock counts 1024-cycle units; the GBA runs at 16777216Hz
    seconds = v["cycles"] / 16384.0
    fps = v["frames"] / seconds if seconds else 0.0

    print()
    print("  benchmark: %s" % label)
    print("  %d frames over %.2fs, budget %d ticks/frame" % (v["frames"], seconds, budget))
    print()
    print("  CPU avg    %6.1f%%   (%d ticks)" % (pct(v["avg"]), v["avg"]))
    print("  CPU p95    %6.1f%%   (%d ticks)" % (pct(v["p95"]), v["p95"]))
    print("  CPU max    %6.1f%%   (%d ticks)" % (pct(v["peak"]), v["peak"]))
    print("  CPU min    %6.1f%%   (%d ticks)" % (pct(v["low"]), v["low"]))
    print()
    print("  frame rate %6.2f fps" % fps)
    print("  slow frames %5d  (%.1f%% of run)" % (v["slow"], 100.0 * v["slow"] / v["frames"]))
    print("  respawns    %5d" % v["deaths"])
    print()
    print("  work done in the run:")
    print("    shots fired      %5d  (%.1f/s)" % (v["shots"], v["shots"] / seconds))
    print("    explosions       %5d" % v["explosions"])
    print("    peak bullets     %5d" % v["peak_bullets"])
    print("    peak monsters    %5d" % v["peak_enemies"])
    print("    crates taken     %5d" % v["crates"])
    print()

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "run")
