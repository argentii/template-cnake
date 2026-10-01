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
