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
