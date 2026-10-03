# Template Snake

A real-time terminal Snake game in which **every game rule is computed by C++
templates at compile time**. The snake moves on a fixed tick, like classic
Snake. Between ticks, the game recompiles itself: the compiler is the CPU.

```
+--------------------------------+
|. . . . . . . . . . . . . . . . |
|. . . . . . . . * . . . . . . . |
|. . . . o o o @ . . . . . . . . |
|. . . . o . . . . . . . . . . . |
|. . . . . . . . . . . . . . . . |
+--------------------------------+
 Score: 2
```

## Requirements

- macOS or Linux
- `clang++` with C++20 support (Apple clang 17 and LLVM clang 15+ work)
- `make`

The engine uses the clang builtins `__type_pack_element` and
`__make_integer_seq`, so GCC is not supported.

## Quick start

```sh
make             # builds build/driver
build/driver     # play
```

Run the driver from the repository root, because it compiles `frame.cpp` and
`initial_state.hpp` from the current directory. It needs a real terminal, so
start it from your shell, not from a tool that pipes stdin.

**Controls:** arrow keys or WASD to steer, `q` or Ctrl-C to quit.

Eat the `*` to grow and score. Hitting a wall or yourself ends the game. Fill
the whole board to win. Pressing the direction opposite to the one you're
heading in is ignored.

## Options

| Option | Meaning |
|---|---|
| `--tick MS` | Tick length. Without it, the driver measures this machine at startup (see below). |
| `--calibrate` | Measure, print the tick length the driver would pick, and exit. |
| `--seed N` | Food sequence seed (default 1). |
| `--naive` | Compile the next state only after the tick ends. Slow and stuttery; for debugging. |
| `--script KEYS` | Non-interactive replay: one input per tick, `U` `D` `L` `R` or `.` for none. Prints every frame. |
| `--no-delay` | With `--script`: don't wait for the tick clock, run as fast as compiles allow. |
| `--root DIR` | Directory holding `frame.cpp` and `initial_state.hpp` (default `.`). |
| `--log FILE` | Timing and stall log (default `tmp/driver.log`). |

Example replay:

```sh
build/driver --script "RRDDDDDDDD" --no-delay --seed 1
```

## How it works

### The state is a type

```cpp
Game<Dir::Right, Food<8,8>, Seed<1015568748u>, Score<0>,
     Snake<P<6,6>, P<5,6>, P<4,6>>>      // head first
GameOver<Score, Won>
```

The rules are metafunctions over these types: `Step` (movement, growth,
collision), `SpawnFood` (a compile-time LCG with bounded retries, then a
row-bitmask scan for a free cell), `Next`, and `Successors<S>`, which holds the
3 possible next states: straight, turn left and turn right.

`KeyTable<S>` maps each input (none, Up, Down, Left, Right) to one of those 3
slots. The mapping depends on the snake's heading, so it's computed by the
templates too. Pressing the reverse direction maps to "straight".

### One binary per state

`frame.cpp` includes a generated `state.hpp` containing `using State = ...;`.
Compiled against it, it produces a binary `B(S)` whose outputs are all string
constants baked in at compile time:

| Command | Output |
|---|---|
| `B frame` | the rendered board and score |
| `B info` | the key table (`keys 0 1 2 0 0`) and `terminal 0` or `1` |
| `B emit DIR` | `succ0.hpp`, `succ1.hpp`, `succ2.hpp`: the three successor states as source |

### The tick loop (speculative compilation)

```
tick n:  B(S) is already built and warmed up
  1. B frame         -> draw the screen
  2. B info          -> key table + terminal flag (terminal: show game over, exit)
  3. B emit tmp/...  -> 3 successor headers
  4. compile all 3 successors in parallel, each in its own directory
  5. record the last key pressed during the tick
end of tick:
  6. slot = keys[last_key]
  7. wait for that compile if needed (logged as a stall); kill the other two
  8. B(successor) becomes the current binary
```

Compiles run one tick ahead, so the player never waits for the compiler
unless a compile is slower than a tick.

### The purity rule

All game logic lives in types and template specializations. The driver
(`driver/driver.cpp`) only:

- reads raw keyboard bytes and decodes them into one of 5 input indices, and
  keeps time;
- spawns, waits for and kills `clang++` and the binaries it produces;
- looks up `keys[input]` in the table `B info` printed, and stops when it says
  `terminal 1`;
- prints the frame text and copies emitted headers into build directories.

It never decides movement, growth, collisions, food placement, randomness,
score, what a key means relative to the snake, or what the board looks like.
`constexpr` functions appear only where finished type-level results are turned
into characters (`text.hpp`, `render.hpp`, `serialize.hpp`).

### Layout

```
engine/core.hpp       state types, list utilities (O(1) template depth)
engine/step.hpp       movement, growth, wall and self collision
engine/rng.hpp        LCG seed, food spawning
engine/game.hpp       Next, Successors, KeyTable, Initial
engine/text.hpp       constexpr strings, int-to-chars, concat
engine/render.hpp     state -> board text
engine/serialize.hpp  state -> state.hpp source text
frame.cpp             B(S): argument dispatch only
initial_state.hpp     tick 0 state (seeded by -DTS_SEED)
driver/driver.cpp     runtime shell: input, timing, compile pipeline
tests/                static tests, serialize round trip, golden replays
PERF.md               measurements
```

The grid size (default 16x12) is set in `engine/core.hpp` and can be
overridden with `-DTS_GRID_W` / `-DTS_GRID_H`.

## Tests

```sh
make test
```

- **Static tests** (`tests/static_tests.cpp`): `static_assert`s for every
  metafunction. If the file compiles, they pass.
- **Serialize round trip**: states are written out as headers, compiled back
  in and checked with `Same<...>` against the original types.
- **Golden replays** (`tests/golden/*.case`): scripted games diffed against
  recorded output. Run them against the naive driver with
  `make golden DRIVER_ARGS=--naive`, and re-record with `make golden-update`.

## Performance

A tick's compile and link takes about 43 ms at snake length 3 and 64 ms at
length 100 on an Apple M4.

**Automatic tick length.** Unless you pass `--tick`, the driver times one
tick's worth of work at startup: three compiles of the initial state in
parallel, each followed by its first run. It does this twice, takes the slower
round, adds 10% plus 25 ms for the per-tick bookkeeping, and rounds up to 10 ms.
The result is clamped to 200–1000 ms: 200 ms is classic Snake speed, and above
1000 ms the game isn't fun anyway. It adds about a second to startup, and one of
the compiles becomes the first frame. Run `build/driver --calibrate` to see
what it picks.

**macOS:** macOS scans every newly built executable on its first run, one at a
time (about 107 ms each, more under load). Three new binaries per tick costs
about 400 ms, so the measured tick on an M4 comes out around 480–500 ms. The
driver runs each fresh binary once during the tick to hide that cost. If you
add your terminal app under System Settings → Privacy & Security → Developer
Tools, the scan goes away and the measured tick should drop toward 200 ms.

Stalls (ticks where the chosen compile wasn't ready) are logged to
`tmp/driver.log`. Full numbers are in `PERF.md`.

## Troubleshooting

- **"stdin is not a terminal"**: run `build/driver` directly in a terminal,
  or use `--script` for non-interactive runs.
- **Compile errors about `frame.cpp` not found**: run from the repository root
  or pass `--root`.
- **The snake pauses now and then**: check `tmp/driver.log` for `STALL` lines
  and try a longer `--tick`. The measured tick reflects the machine at
  startup; heavy load later in the game can still cause stalls.
- **Leftover `tmp/run-*` directories** after a crash or Ctrl-C are removed the
  next time the driver starts.
