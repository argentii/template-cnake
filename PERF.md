# Performance log

Machine: Apple M4 (10 cores), macOS 26 (Darwin 25.6), Apple clang 17.0.0.

## Milestone 0 baseline (empty `main` + `<cstdio>`, `-std=c++20 -O0`, 10 runs)

| Step | Median |
|---|---|
| compile + link | 34 ms |
| compile only (`-c`) | 17 ms |
| link only | 24 ms |

First exec of a freshly linked binary: **~110 ms** (macOS scans new executables);
later execs ~4 ms. The driver must warm each speculative binary (run it once with
no output-producing mode) while the tick is still running, so the scan cost is
hidden behind the tick instead of added to it.

## Milestone 9 findings: macOS first-exec scan serializes

Measured with the default grid, snake length ~3–5:

| | 1 at a time | 3 in parallel |
|---|---|---|
| compile + link `frame.cpp` | 55 ms | 82 ms (all three) |
| first run of the new binaries | 109 ms | **322 ms** (all three) |

The macOS scan of new code is serialized system-wide (~107 ms each), and it also
applies to `dlopen` of a fresh dylib (3 fresh dylibs: 333 ms). So the spec's
3-binaries-per-tick pipeline costs ~330 ms/tick on this machine regardless of
compile speed. Exempting the terminal app under System Settings → Privacy &
Security → Developer Tools removes the scan.
Decision: keep the 3-binary architecture and raise the macOS default tick to
**450 ms** (Linux stays at 200 ms). Measured stall rates with the real tick clock:

| tick | stalls | notes |
|---|---|---|
| 350 ms | 14/39 | worst 94 ms |
| 400 ms | 1/39 | worst 28 ms |
| 450 ms | 0/39, 0/59 | quiet machine |
| 450 ms | 35/87 | right after heavy test activity; scan p90 746 ms, max 1.2 s |

The scan time depends on system state, not on our binary: stripping, dead-strip
and copying an already-scanned binary all leave it at ~107 ms. Unsigned arm64
binaries are killed. Stalls that do happen are logged to `tmp/driver.log`.

## Milestone 10: compile time vs snake length

`frame.cpp` compile + link, `-O0`, median of 7 (serpentine snake, 16x12 grid):

| length | compile + link |
|---|---|
| 3 | 43 ms |
| 20 | 45 ms |
| 50 | 50 ms |
| 100 | 64 ms |
| 150 | 68 ms |

`-ftime-trace` at length 100 (compile only, ~39 ms): template instantiation
23 ms, constexpr evaluation (render + serialize text) 16 ms. No hot spot.

Precompiled header for the engine + `<cstdio>`: saves 3–5 ms per compile. Not
adopted: the ~110 ms per-binary scan is 20x larger, and a PCH adds a build step
that must match flags exactly.
