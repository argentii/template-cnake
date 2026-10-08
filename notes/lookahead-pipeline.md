# Lookahead pipeline: one binary per tick

## Why

On macOS, every newly built executable is scanned the first time it runs. The
scans run one at a time and take about 107 ms each. The original speculative
pipeline builds three binaries per tick (one per possible move), which costs
about 400 ms of scanning per tick. On an Apple M4 the driver therefore measured
a tick floor of about 490 ms, half the speed the game asks for (250 ms).

## What it adds

**Lookahead binaries.** Compiled with `-DTS_LOOKAHEAD`, the frame binary for
state `S` also carries the outputs for each of `S`'s three successors (slot 0 =
straight, 1 = left, 2 = right). A new `-cN` flag picks one:

```
B -cN frame          the successor's board
B -cN info           its key table, terminal flag and tick length
B -cN state FILE     its state.hpp
B -cN best FILE      the best-score header if the game ends there
B -cN restart FILE   the next game's first state, if it's a game over
```

Without `-cN`, the binary answers for `S` itself, exactly as before.

**A lookahead loop in the driver.** Each tick, the current state's frame and
key table come from the previous binary (`-c<slot>`). The driver writes that
state's header with `-c<slot> state` and compiles one new binary from it during
the tick. That's one compile and one scan per tick instead of three.

**A `--pipeline` option.** `speculative`, `lookahead` or `naive`
(`--naive` still works). The default is `lookahead` on macOS and `speculative`
everywhere else, so Linux keeps the design in CLAUDE.md.

**Calibration per pipeline.** `--calibrate` and the startup measurement time
one compile per round for lookahead and three for speculative.

## Purity

Unchanged. The templates compute every successor's board, key table, state,
best score and restart state. The driver still only looks up
`slot = keys[key]` and passes it back as `-c<slot>`.

## Results (Apple M4)

| pipeline | work per tick | measured floor |
|---|---|---|
| speculative | 3 compiles + 3 first runs (~420 ms) | ~490 ms |
| lookahead | 1 compile + 1 first run (~200 ms) | ~250 ms |

- A lookahead binary costs 4–6 ms more to compile than a normal one (48 vs
  44 ms at snake length 3, 70 vs 64 ms at length 100).
- A timed 58-tick replay at the game's own speed (240–250 ms) had no stalls.
  One compile plus first run took 192 ms median and 231 ms at worst.
- An interactive game ran at 250 ms per tick, including restart and quit.

The game now runs at its intended starting speed on macOS. The speed-up as the
snake grows (down to 120 ms) is still capped at about 230 ms, because one
compile plus its first run takes that long.

## Tests

- All 6 golden replays produce identical output under all three pipelines.
- `make test` passes. CI now runs the golden replays under each pipeline and
  prints both calibrated floors on Ubuntu and macOS.

## Not done yet

To show the full speed-up on macOS, each binary could also carry the 9 states
two moves ahead, so every compile gets two ticks to finish. My estimate is a
floor around 130 ms. The other option is a slower starting speed.
